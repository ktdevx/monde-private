/*
 * InlineParser: インラインの段階（テキスト、エスケープ、実体参照、コードスパン、自動リンク、生の HTML、改行、
 * 強調の区切り記号、リンクと画像）
 */

#include "markdown/md.h"
#include "markdown/md_delim.h"
#include "markdown/md_entity.h"
#include "markdown/md_html.h"
#include "markdown/md_inline.h"
#include "markdown/md_internal.h"
#include "markdown/md_linkref.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* 位置がないことを表す値（保留中のテキストがない、HTML の字句の走査で失敗していない） */
#define NO_POS ((size_t)-1)

/* 数値文字参照の 10 進の最大の桁数と、16 進の最大の桁数（spec 0.31.2） */
#define DEC_DIGITS_MAX 7
#define HEX_DIGITS_MAX 6

/* 自動リンクのスキームの最小と最大の文字数 */
#define SCHEME_MIN 2
#define SCHEME_MAX 32

/* メールアドレスのドメインの 1 つのラベルの最大の文字数 */
#define DOMAIN_LABEL_MAX 63

/* 置換文字 U+FFFD */
#define REPLACEMENT_CP 0xFFFDUL

/*
 * 角括弧で囲まないリンク先の、エスケープされない括弧の入れ子の最大の深さ（spec は少なくとも 3 を求め、
 * 上限を設けてよいとする。cmark と同じ 32）。閉じの括弧のない `(` の繰り返しを短く打ち切る（8.3）
 */
#define DEST_PAREN_DEPTH_MAX 32

/* リンクタイトルの開きの記号の種類の数（`"`、`'`、`(`） */
#define TITLE_KINDS 3

/* バッククォートの並び（論理的な文字列の中の、前後にバッククォートがない最長の並び） */
typedef struct backtick_run
{
    size_t start;
    size_t len;
} backtick_run_t;

/*
 * 角括弧のスタックの要素（spec の付録の bracket）。`[` か `![` の位置と、それを表すテキストのノードを持つ。
 * スタックは要素の大きさが sizeof(bracket_t) の作業用の配列で、積んだ順に並ぶ
 */
typedef struct bracket
{
    mnd_md_node_t *node;            /* `[` または `![` のテキストのノード */
    size_t pos;                     /* `[` または `!` の text の位置 */
    size_t delim_bottom;            /* 積んだときの区切り記号のスタックの要素数（強調の処理の下限） */
    bool image;                     /* `![` */
    bool bracket_after;             /* この後ろに角括弧を積んだ（省略参照とショートカット参照を試さない） */
} bracket_t;

/*
 * 1 つのリーフのインラインの解析の作業状態（行のカーソル）。
 *
 * リーフの各行の内容を、行の境目を '\n' 1 バイトにしてつないだ論理的な文字列 text を作り、その上の位置 pos を
 * 進めて読む。最後の行の末尾の空白とタブは text に入れない（どのノードにも含めない）。text の位置は
 * map_pos で原文のオフセットに戻し、ノードの範囲はすべて原文のオフセットで記録する。行の内容は行末記号を
 * 含まないので、text の '\n' は行の境目だけである。段落と見出しの行の pad は 0 なので、補う空白は入れない。
 *
 * 作業用の配列は、すべてのリーフで使い回す（mnd_md_inline_run が最後に解放する）
 */
typedef struct inline_state
{
    mnd_md_parser_t *parser;
    mnd_md_node_t *leaf;            /* 解析中のリーフ（インラインのノードの親） */
    mnd_md_array_t text;            /* 論理的な文字列（char） */
    mnd_md_array_t line_starts;     /* leaf->lines[i] の内容が text の中で始まる位置（size_t） */
    mnd_md_array_t runs;            /* バッククォートの並び（backtick_run_t。長さ、開始の順に並べる） */
    bool runs_ready;                /* runs をこのリーフについて作った */
    mnd_md_array_t delims;          /* 区切り記号のスタック（mnd_md_delim_t。md_delim.h） */
    mnd_md_array_t brackets;        /* 角括弧のスタック（bracket_t） */
    mnd_md_array_t label;           /* 正規化したラベル（char。参照の解決に使う） */
    /*
     * 角括弧のスタックの添字がこの値より小さい `[`（画像でないもの）は、使えない（リンクの中のリンクを作らない）。
     * リンクを作るたびに、残っている要素の数にする。要素を 1 つずつ印を付けて回らないので、リンクの数と
     * 角括弧の数の積の時間にならない（8.3）。積むときは、その位置より上の古い値を残さないように縮める
     */
    size_t inactive_below;
    const char *s;                  /* text の先頭 */
    size_t len;                     /* text の長さ */
    size_t pos;                     /* 次に読む位置 */
    size_t text_start;              /* 保留中のテキストの開始（text の位置）。なければ NO_POS */
    /*
     * 閉じのない HTML の字句の記録（8.3）。それぞれ、その位置から始まる字句の閉じの記号（`-->`、`?>`、`]]>`、
     * 宣言の `>`）を探して、text の終わりまでに見つからなかった最初の `<` の位置。後ろの `<` から探す範囲は
     * その部分集合なので、探さずに失敗とする（同じ範囲を何度も走査しない）
     */
    size_t comment_fail;
    size_t pi_fail;
    size_t cdata_fail;
    size_t decl_fail;
    /*
     * 閉じのないリンクタイトルの記録（8.3）。開きの記号の種類（`"`、`'`、`(`）ごとに、閉じの記号を探して text の
     * 終わりまで見つからなかった開きの位置のうち、最も前のもの。タイトルの開きの位置は単調に増えない
     * （`<>` のリンク先は `](` と空白を含められる）ので、記録より後ろ（同じ位置を含む）の開きだけを失敗とし、
     * 前の開きからは探す（scan_link_title）
     */
    size_t title_fail[TITLE_KINDS];
} inline_state_t;

/* 特別な文字の処理の結果 */
typedef enum match_result
{
    MATCH_NONE,     /* 当てはまらない（その文字はテキスト） */
    MATCH_DONE,     /* ノードを作り、pos を進めた */
    MATCH_OOM       /* メモリ不足 */
} match_result_t;

static bool is_ascii_letter(unsigned char c)
{
    return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
}

static bool is_ascii_digit(unsigned char c)
{
    return ((c >= '0') && (c <= '9'));
}

static bool is_ascii_alnum(unsigned char c)
{
    return (is_ascii_letter(c) || is_ascii_digit(c));
}

static bool is_ascii_punct(unsigned char c)
{
    return (((c >= 0x21) && (c <= 0x2F)) || ((c >= 0x3A) && (c <= 0x40))
            || ((c >= 0x5B) && (c <= 0x60)) || ((c >= 0x7B) && (c <= 0x7E)));
}

/* 16 進の数字の値。数字でなければ -1 */
static int hex_value(unsigned char c)
{
    if (is_ascii_digit(c))
    {
        return c - '0';
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        return c - 'a' + 10;
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        return c - 'A' + 10;
    }

    return -1;
}

/*
 * インラインの特別な文字か（テキストの読み飛ばしを止める文字）。方言の開始文字（5.2）は、
 * そのタスクがここと parse_leaf の分岐に加える
 */
static bool is_special(unsigned char c)
{
    return ((c == '\n') || (c == '\\') || (c == '`') || (c == '&') || (c == '<') || (c == '*') || (c == '_')
            || (c == '[') || (c == ']') || (c == '!'));
}

/* 位置の対応 */

/* text の位置 k（0 <= k <= len）を含む行の添字（line_starts[i] <= k である最後の i） */
static size_t line_of(const inline_state_t *st, size_t k)
{
    const size_t *starts = (const size_t *)st->line_starts.items;
    size_t lo = 0;
    size_t hi = st->line_starts.count;

    /* starts[0] は 0 なので、答えは必ずある */
    while ((hi - lo) > 1)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (starts[mid] <= k)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }

    return lo;
}

/*
 * text の位置 k を原文のオフセットにする。行 i の内容の位置はそのまま対応し、行の境目の '\n' の位置は
 * 行 i の内容の終わり（line_end）に対応する
 */
static size_t map_pos(const inline_state_t *st, size_t k)
{
    size_t i = line_of(st, k);
    const size_t *starts = (const size_t *)st->line_starts.items;

    return st->leaf->lines[i].content.start + (k - starts[i]);
}

/* text の範囲 [a, b)（b > a。最後のバイトは行の内容のバイト）の原文の範囲 */
static mnd_span_t map_span(const inline_state_t *st, size_t a, size_t b)
{
    mnd_span_t span;

    span.start = map_pos(st, a);
    span.end = map_pos(st, b - 1) + 1;

    return span;
}

/* ノードの作成 */

/* 原文の範囲 span の kind のノードを作ってリーフの最後の子にし、行番号を確定する。メモリ不足なら NULL */
static mnd_md_node_t *emit_node(inline_state_t *st, mnd_md_kind_t kind, mnd_span_t span)
{
    mnd_md_node_t *node = mnd_md_node_new(st->parser->doc, kind, span);

    if (node == NULL)
    {
        return NULL;
    }

    mnd_md_parser_set_node_lines(st->parser, node);
    mnd_md_node_append_child(st->leaf, node);

    return node;
}

/* text の範囲 [a, b)（b > a）の kind のノードを作る。メモリ不足なら NULL */
static mnd_md_node_t *emit_logical(inline_state_t *st, mnd_md_kind_t kind, size_t a, size_t b)
{
    return emit_node(st, kind, map_span(st, a, b));
}

/* 保留中のテキストがなければ、位置 k から始める */
static void text_begin(inline_state_t *st, size_t k)
{
    if (st->text_start == NO_POS)
    {
        st->text_start = k;
    }
}

/*
 * 保留中のテキストを end まででテキストのノードにする（空なら作らない）。保留中のテキストは '\n' を含まないので、
 * 1 つの行の内容の中の連続したバイトを指す。メモリ不足なら false
 */
static bool text_flush(inline_state_t *st, size_t end)
{
    size_t start = st->text_start;

    st->text_start = NO_POS;
    if ((start == NO_POS) || (end <= start))
    {
        return true;
    }

    return (emit_logical(st, MND_MD_TEXT, start, end) != NULL);
}

/* 改行 */

/*
 * text の位置 k（'\n'）の行の境目を処理する。保留中のテキストの末尾の空白（タブを除く）は改行に含め、
 * 2 つ以上ならハード改行、そうでなければソフト改行にする。範囲は末尾の空白の開始（なければ行の内容の終わり）
 * から、次の行の物理的な先頭（行末記号の後ろ）まで
 */
static match_result_t handle_newline(inline_state_t *st, size_t k)
{
    size_t i = line_of(st, k);
    size_t spaces_start = k;
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    if (st->text_start != NO_POS)
    {
        while ((spaces_start > st->text_start) && (st->s[spaces_start - 1] == ' '))
        {
            spaces_start--;
        }
    }

    if (!text_flush(st, spaces_start))
    {
        return MATCH_OOM;
    }

    span.start = map_pos(st, spaces_start);
    span.end = st->leaf->lines[i + 1].prefix.start;
    node = emit_node(st, ((k - spaces_start) >= 2) ? MND_MD_HARD_BREAK : MND_MD_SOFT_BREAK, span);
    if (node == NULL)
    {
        return MATCH_OOM;
    }

    st->pos = k + 1;

    return MATCH_DONE;
}

/* バックスラッシュ */

/*
 * text の位置 k（`\`）。ASCII の句読点が続けばエスケープ、行の境目が続けばハード改行（範囲はバックスラッシュから
 * 次の行の物理的な先頭まで）。それ以外（最後の行の末尾を含む）はテキスト
 */
static match_result_t handle_backslash(inline_state_t *st, size_t k)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;
    unsigned char next = 0;

    if ((k + 1) >= st->len)
    {
        return MATCH_NONE;
    }

    next = (unsigned char)st->s[k + 1];
    if (next == '\n')
    {
        if (!text_flush(st, k))
        {
            return MATCH_OOM;
        }

        span.start = map_pos(st, k);
        span.end = st->leaf->lines[line_of(st, k) + 1].prefix.start;
        node = emit_node(st, MND_MD_HARD_BREAK, span);
        if (node == NULL)
        {
            return MATCH_OOM;
        }

        node->as.hard_break.backslash = true;
        st->pos = k + 2;

        return MATCH_DONE;
    }

    if (!is_ascii_punct(next))
    {
        return MATCH_NONE;
    }

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    node = emit_logical(st, MND_MD_ESCAPE, k, k + 2);
    if (node == NULL)
    {
        return MATCH_OOM;
    }

    node->as.escape.ch = (char)next;
    st->pos = k + 2;

    return MATCH_DONE;
}

/* 実体参照と数値文字参照 */

/* コードポイント cp の UTF-8 を out に書き、バイト数を返す。0、サロゲート、U+10FFFF を超える値は U+FFFD */
static unsigned char encode_utf8(unsigned long cp, char out[8])
{
    if ((cp == 0) || ((cp >= 0xD800UL) && (cp <= 0xDFFFUL)) || (cp > 0x10FFFFUL))
    {
        cp = REPLACEMENT_CP;
    }

    if (cp < 0x80UL)
    {
        out[0] = (char)cp;

        return 1;
    }
    else if (cp < 0x800UL)
    {
        out[0] = (char)(0xC0UL | (cp >> 6));
        out[1] = (char)(0x80UL | (cp & 0x3FUL));

        return 2;
    }
    else if (cp < 0x10000UL)
    {
        out[0] = (char)(0xE0UL | (cp >> 12));
        out[1] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
        out[2] = (char)(0x80UL | (cp & 0x3FUL));

        return 3;
    }

    out[0] = (char)(0xF0UL | (cp >> 18));
    out[1] = (char)(0x80UL | ((cp >> 12) & 0x3FUL));
    out[2] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
    out[3] = (char)(0x80UL | (cp & 0x3FUL));

    return 4;
}

/*
 * text の位置 k（`&`）から数値文字参照（`&#d{1,7};`、`&#[xX]h{1,6};`）を認識する。
 * 認識したら値を *cp に、`;` の後ろの位置を *next に入れて true
 */
static bool scan_numeric_ref(const inline_state_t *st, size_t k, unsigned long *cp, size_t *next)
{
    const unsigned char *s = (const unsigned char *)st->s;
    unsigned long value = 0;
    size_t p = k + 2;
    size_t digits = 0;
    int digit = 0;
    bool hex = false;

    if (((k + 2) >= st->len) || (s[k + 1] != '#'))
    {
        return false;
    }

    if ((s[p] == 'x') || (s[p] == 'X'))
    {
        hex = true;
        p++;
    }

    while ((p < st->len) && (digits <= (hex ? HEX_DIGITS_MAX : DEC_DIGITS_MAX)))
    {
        digit = hex ? hex_value(s[p]) : (is_ascii_digit(s[p]) ? (s[p] - '0') : -1);
        if (digit < 0)
        {
            break;
        }

        value = (value * (hex ? 16UL : 10UL)) + (unsigned long)digit;
        digits++;
        p++;
    }

    if ((digits == 0) || (digits > (hex ? HEX_DIGITS_MAX : DEC_DIGITS_MAX)))
    {
        return false;
    }

    if ((p >= st->len) || (s[p] != ';'))
    {
        return false;
    }

    *cp = value;
    *next = p + 1;

    return true;
}

/*
 * text の位置 k（`&`）。数値文字参照か、表にある実体名の実体参照（`&name;`）なら、表す文字を持つ文字参照の
 * ノードにする（5.8）。不正な数値は U+FFFD
 */
static match_result_t handle_entity(inline_state_t *st, size_t k)
{
    const unsigned char *utf8 = NULL;
    size_t utf8_len = 0;
    unsigned long cp = 0;
    size_t next = 0;
    size_t p = k + 1;
    mnd_md_node_t *node = NULL;
    mnd_md_char_ref_t ref;

    memset(&ref, 0, sizeof(ref));
    if (scan_numeric_ref(st, k, &cp, &next))
    {
        ref.numeric = true;
        ref.len = encode_utf8(cp, ref.text);
    }
    else
    {
        while ((p < st->len) && ((p - (k + 1)) <= MND_MD_ENTITY_NAME_MAX)
               && is_ascii_alnum((unsigned char)st->s[p]))
        {
            p++;
        }

        if ((p >= st->len) || (st->s[p] != ';')
            || !mnd_md_entity_lookup(st->s + k + 1, p - (k + 1), &utf8, &utf8_len))
        {
            return MATCH_NONE;
        }

        ref.numeric = false;
        ref.len = (unsigned char)utf8_len;
        memcpy(ref.text, utf8, utf8_len);
        next = p + 1;
    }

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    node = emit_logical(st, MND_MD_CHAR_REF, k, next);
    if (node == NULL)
    {
        return MATCH_OOM;
    }

    node->as.char_ref = ref;
    st->pos = next;

    return MATCH_DONE;
}

/* コードスパン */

/* バッククォートの並びの順序（長さ、開始の順） */
static int compare_runs(const void *a, const void *b)
{
    const backtick_run_t *x = (const backtick_run_t *)a;
    const backtick_run_t *y = (const backtick_run_t *)b;

    if (x->len != y->len)
    {
        return (x->len < y->len) ? -1 : 1;
    }

    if (x->start != y->start)
    {
        return (x->start < y->start) ? -1 : 1;
    }

    return 0;
}

/*
 * text のすべてのバッククォートの並び（前後にバッククォートがない最長の並び）を集め、長さ、開始の順に並べる。
 * リーフで最初にバッククォートに当たったときに 1 回だけ行い、閉じの並びの検索を二分探索にする
 * （閉じのないバッククォートの繰り返しで二乗の時間にしない。8.3）。メモリ不足なら false
 */
static bool collect_runs(inline_state_t *st)
{
    size_t p = 0;
    size_t start = 0;
    backtick_run_t *run = NULL;

    mnd_md_array_clear(&st->runs);
    while (p < st->len)
    {
        if (st->s[p] != '`')
        {
            p++;
            continue;
        }
    
        start = p;
        while ((p < st->len) && (st->s[p] == '`'))
        {
            p++;
        }

        run = (backtick_run_t *)mnd_md_array_push(&st->runs);
        if (run == NULL)
        {
            return false;
        }

        run->start = start;
        run->len = p - start;
    }

    if (st->runs.count > 1)
    {
        qsort(st->runs.items, st->runs.count, sizeof(backtick_run_t), compare_runs);
    }

    st->runs_ready = true;

    return true;
}

/* 長さ len で、from 以後に始まる最初のバッククォートの並びの開始。なければ NO_POS */
static size_t find_closing_run(const inline_state_t *st, size_t len, size_t from)
{
    const backtick_run_t *runs = (const backtick_run_t *)st->runs.items;
    size_t lo = 0;
    size_t hi = st->runs.count;

    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if ((runs[mid].len < len) || ((runs[mid].len == len) && (runs[mid].start < from)))
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }

    if ((lo < st->runs.count) && (runs[lo].len == len))
    {
        return runs[lo].start;
    }

    return NO_POS;
}

/*
 * text の位置 k（`` ` ``）。同じ長さの閉じの並びがあればコードスパンにする（内容は並びの内側で、行をまたぐときは
 * 途中のプレフィックスと行末記号を含む。5.6）。なければ、開きの並び全体をテキストにする
 */
static match_result_t handle_backticks(inline_state_t *st, size_t k)
{
    size_t open_end = k;
    size_t ticks = 0;
    size_t close = NO_POS;
    mnd_md_node_t *node = NULL;

    while ((open_end < st->len) && (st->s[open_end] == '`'))
    {
        open_end++;
    }

    ticks = open_end - k;

    if (!st->runs_ready && !collect_runs(st))
    {
        return MATCH_OOM;
    }

    close = find_closing_run(st, ticks, open_end);
    if (close == NO_POS)
    {
        text_begin(st, k);
        st->pos = open_end;

        return MATCH_DONE;
    }

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    node = emit_logical(st, MND_MD_CODE_SPAN, k, close + ticks);
    if (node == NULL)
    {
        return MATCH_OOM;
    }

    node->as.code_span.ticks = ticks;
    node->as.code_span.content.start = map_pos(st, open_end);
    node->as.code_span.content.end = map_pos(st, close);
    st->pos = close + ticks;

    return MATCH_DONE;
}

/* 自動リンクと生の HTML */

/* メールアドレスの @ の前に使える文字 */
static bool is_email_local_char(unsigned char c)
{
    return (is_ascii_alnum(c) || ((c != '\0') && (strchr(".!#$%&'*+/=?^_`{|}~-", c) != NULL)));
}

/*
 * text の位置 k（`<`）からメールアドレスの自動リンクを認識し、`>` の後ろの位置を返す。なければ 0
 */
static size_t scan_email_autolink(const inline_state_t *st, size_t k)
{
    const unsigned char *s = (const unsigned char *)st->s;
    size_t p = k + 1;
    size_t label_start = 0;

    while ((p < st->len) && is_email_local_char(s[p]))
    {
        p++;
    }

    if ((p == (k + 1)) || (p >= st->len) || (s[p] != '@'))
    {
        return 0;
    }

    p++;

    for (;;)
    {
        label_start = p;
        while ((p < st->len) && (is_ascii_alnum(s[p]) || (s[p] == '-')))
        {
            p++;
        }

        if ((p == label_start) || ((p - label_start) > DOMAIN_LABEL_MAX) || (s[label_start] == '-')
            || (s[p - 1] == '-') || (p >= st->len))
        {
            return 0;
        }

        if (s[p] == '>')
        {
            return p + 1;
        }

        if (s[p] != '.')
        {
            return 0;
        }

        p++;
    }
}

/*
 * text の位置 k（`<`）から URI の自動リンク（スキーム、`:`、ASCII の制御文字・空白・`<`・`>` を含まない並び、`>`）を
 * 認識し、`>` の後ろの位置を返す。なければ 0。行の境目（'\n'）は制御文字なので、行をまたがない
 */
static size_t scan_uri_autolink(const inline_state_t *st, size_t k)
{
    const unsigned char *s = (const unsigned char *)st->s;
    size_t p = k + 1;

    if ((p >= st->len) || !is_ascii_letter(s[p]))
    {
        return 0;
    }

    p++;

    while ((p < st->len) && ((p - (k + 1)) < SCHEME_MAX)
           && (is_ascii_alnum(s[p]) || (s[p] == '+') || (s[p] == '.') || (s[p] == '-')))
    {
        p++;
    }

    if (((p - (k + 1)) < SCHEME_MIN) || (p >= st->len) || (s[p] != ':'))
    {
        return 0;
    }

    p++;

    while ((p < st->len) && (s[p] > 0x20) && (s[p] != 0x7F) && (s[p] != '<') && (s[p] != '>'))
    {
        p++;
    }

    if ((p >= st->len) || (s[p] != '>'))
    {
        return 0;
    }

    return p + 1;
}

/* text が位置 k から s で始まるか */
static bool text_starts_with(const inline_state_t *st, size_t k, const char *s)
{
    size_t n = strlen(s);

    return ((k <= st->len) && ((st->len - k) >= n) && (memcmp(st->s + k, s, n) == 0));
}

/*
 * 閉じのない字句の記録 *fail を使って、HTML の字句の関数 scan を位置 k から呼ぶ。*fail が記録済み（k より前の
 * `<` で、閉じの記号が終わりまでなかった）なら、探さずに 0。scan が 0 を返したら、*fail に k を記録する。
 * 呼び出し側は、scan が閉じの記号を探す開始が k とともに後ろへずれ、前の失敗の範囲に含まれる字句にだけ使う
 */
static size_t scan_html_memo(const inline_state_t *st, size_t k, size_t *fail,
                             size_t (*scan)(const char *, size_t, size_t))
{
    size_t len = 0;

    if (*fail != NO_POS)
    {
        return 0;
    }

    len = scan(st->s, k, st->len);
    if (len == 0)
    {
        *fail = k;
    }

    return len;
}

/*
 * text の位置 k（`<`）から生の HTML の字句を認識し、その長さを返す。なければ 0。
 * 論理的な文字列の上で認識するので、行をまたぐ字句の途中のプレフィックスは見えない（2.4 の約束）。
 * mnd_md_html_tag と同じ字句を、2 文字目で分けて呼ぶ。閉じの記号を終わりまで探すコメント・処理命令・CDATA・
 * 宣言は、閉じのない記録で二度目からの走査を省く（8.3）。開始タグの引用符の値は、閉じの引用符が値ごとに
 * 異なるので、走査の合計は文字列の長さに比例する
 */
static size_t scan_html(inline_state_t *st, size_t k)
{
    unsigned char c = 0;

    if ((k + 1) >= st->len)
    {
        return 0;
    }

    c = (unsigned char)st->s[k + 1];
    if (is_ascii_letter(c))
    {
        return mnd_md_html_open_tag(st->s, k, st->len);
    }
    else if (c == '/')
    {
        return mnd_md_html_closing_tag(st->s, k, st->len);
    }
    else if (c == '?')
    {
        return scan_html_memo(st, k, &st->pi_fail, mnd_md_html_processing_instruction);
    }
    else if (c != '!')
    {
        return 0;
    }

    if (text_starts_with(st, k, "<!--"))
    {
        return scan_html_memo(st, k, &st->comment_fail, mnd_md_html_comment);
    }

    if (text_starts_with(st, k, "<![CDATA["))
    {
        return scan_html_memo(st, k, &st->cdata_fail, mnd_md_html_cdata);
    }

    if (((k + 2) < st->len) && is_ascii_letter((unsigned char)st->s[k + 2]))
    {
        return scan_html_memo(st, k, &st->decl_fail, mnd_md_html_declaration);
    }

    return 0;
}

/*
 * text の位置 k（`<`）。メールアドレスの自動リンク、URI の自動リンク、生の HTML の順に試す（5.1、5.7）。
 * 自動リンクの dest は `<` と `>` の内側
 */
static match_result_t handle_lt(inline_state_t *st, size_t k)
{
    mnd_md_node_t *node = NULL;
    size_t end = 0;
    bool email = false;

    end = scan_email_autolink(st, k);
    email = (end != 0);
    if (end == 0)
    {
        end = scan_uri_autolink(st, k);
    }

    if (end != 0)
    {
        if (!text_flush(st, k))
        {
            return MATCH_OOM;
        }
    
        node = emit_logical(st, MND_MD_AUTOLINK, k, end);
        if (node == NULL)
        {
            return MATCH_OOM;
        }
    
        node->as.autolink.email = email;
        node->as.autolink.dest = map_span(st, k + 1, end - 1);
        st->pos = end;

        return MATCH_DONE;
    }

    end = scan_html(st, k);
    if (end == 0)
    {
        return MATCH_NONE;
    }

    end += k;

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    if (emit_logical(st, MND_MD_HTML_INLINE, k, end) == NULL)
    {
        return MATCH_OOM;
    }

    st->pos = end;

    return MATCH_DONE;
}

/* 強調の区切り記号 */

/*
 * text の位置 k（`*` または `_`）。同じ文字の連なりの開きと閉じの可否を判定し（5.9）、どちらかができれば、
 * 連なりをテキストのノードにして区切り記号のスタックに積む。どちらもできなければ、保留中のテキストに含める。
 * 連なりは同じ文字だけからなるので、行の境目をまたがない
 */
static match_result_t handle_delims(inline_state_t *st, size_t k)
{
    unsigned char ch = (unsigned char)st->s[k];
    size_t end = k;
    bool can_open = false;
    bool can_close = false;
    mnd_md_node_t *node = NULL;

    while ((end < st->len) && ((unsigned char)st->s[end] == ch))
    {
        end++;
    }

    mnd_md_delim_classify(st->s, st->len, k, end, (ch == '_'), &can_open, &can_close);
    if (!can_open && !can_close)
    {
        text_begin(st, k);
        st->pos = end;

        return MATCH_DONE;
    }

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    node = emit_logical(st, MND_MD_TEXT, k, end);
    if ((node == NULL) || !mnd_md_delim_push(&st->delims, node, ch, end - k, can_open, can_close))
    {
        return MATCH_OOM;
    }

    st->pos = end;

    return MATCH_DONE;
}

/* リンクと画像 */

/* text の範囲 [a, b)（a <= b。空でもよい）の原文の範囲。b は文字の位置か、行の境目か、text の終わり */
static mnd_span_t map_range(const inline_state_t *st, size_t a, size_t b)
{
    mnd_span_t span;

    span.start = map_pos(st, a);
    span.end = map_pos(st, b);

    return span;
}

/* 空白、タブ、行の境目か（リンクの中の空白。段落の行の境目は 2 つ続かないので、高々 1 つの行の境目になる） */
static bool is_link_space(unsigned char c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n'));
}

/* text の位置 p から、空白・タブ・行の境目を読み飛ばした位置 */
static size_t skip_link_spaces(const inline_state_t *st, size_t p)
{
    while ((p < st->len) && is_link_space((unsigned char)st->s[p]))
    {
        p++;
    }

    return p;
}

/* text の位置 p がエスケープ（`\` と ASCII の句読点）の始まりか */
static bool is_escape_at(const inline_state_t *st, size_t p)
{
    return ((st->s[p] == '\\') && ((p + 1) < st->len) && is_ascii_punct((unsigned char)st->s[p + 1]));
}

/*
 * text の位置 p から角括弧で囲まないリンク先を読み、その後ろの位置を *end に入れる。ASCII の制御文字と空白を
 * 含まず、エスケープされない括弧が釣り合う並び（深さは DEST_PAREN_DEPTH_MAX まで）。NUL は spec のとおり
 * U+FFFD として扱い、リンク先に含めてよい。空の並びは、直後が `)` のときだけ認める（commonmark.js と同じ）
 */
static bool scan_bare_dest(const inline_state_t *st, size_t p, size_t *end)
{
    size_t q = p;
    size_t depth = 0;
    unsigned char c = 0;

    while (q < st->len)
    {
        c = (unsigned char)st->s[q];
        if (is_escape_at(st, q))
        {
            q += 2;
            continue;
        }

        if (c == '(')
        {
            depth++;
            if (depth > DEST_PAREN_DEPTH_MAX)
            {
                return false;
            }
        }
        else if (c == ')')
        {
            if (depth == 0)
            {
                break;
            }

            depth--;
        }
        else if ((c == ' ') || ((c < 0x20) && (c != 0)) || (c == 0x7F))
        {
            break;
        }

        q++;
    }

    if ((depth != 0) || ((q == p) && ((q >= st->len) || (st->s[q] != ')'))))
    {
        return false;
    }

    *end = q;

    return true;
}

/*
 * text の位置 p（`<`）から角括弧で囲んだリンク先を読む。改行とエスケープされない `<` を含まない `>` までで、
 * 内側の終わり（`>` の位置）を *close に入れる
 */
static bool scan_angle_dest(const inline_state_t *st, size_t p, size_t *close)
{
    size_t q = p + 1;
    unsigned char c = 0;

    while (q < st->len)
    {
        c = (unsigned char)st->s[q];
        if ((c == '\n') || (c == '<'))
        {
            return false;
        }

        if (c == '>')
        {
            *close = q;
            return true;
        }

        q += is_escape_at(st, q) ? 2 : 1;
    }

    return false;
}

/*
 * text の位置 p からリンクタイトル（`"..."`、`'...'`、`(...)`。行をまたいでよく、括弧の形はエスケープされない
 * `(` を含まない）を読み、閉じの記号の位置を *close に入れる。閉じの記号が終わりまでないことを記録し、
 * 記録より後ろの同じ種類の開きからは探さない（8.3）。
 * 位置 f から終わりまで探して閉じの記号がなかったとき、f より後ろの開き p は、f からの走査で閉じの記号として
 * 見つからなかったのでエスケープされた位置にあり、f からの走査は p の直後で字句の区切りがそろう。したがって
 * p からの走査は f からの走査の後半と同じになり、やはり終わりまで閉じの記号がない。f より前の開きからの走査は
 * 範囲が広いので、記録を使わずに探す
 */
static bool scan_link_title(inline_state_t *st, size_t p, size_t *close)
{
    unsigned char open = (unsigned char)st->s[p];
    unsigned char closer = 0;
    size_t kind = 0;
    size_t q = p + 1;
    unsigned char c = 0;

    if (open == '"')
    {
        closer = '"';
        kind = 0;
    }
    else if (open == '\'')
    {
        closer = '\'';
        kind = 1;
    }
    else if (open == '(')
    {
        closer = ')';
        kind = 2;
    }
    else
    {
        return false;
    }

    if ((st->title_fail[kind] != NO_POS) && (p >= st->title_fail[kind]))
    {
        return false;
    }

    while (q < st->len)
    {
        c = (unsigned char)st->s[q];
        if (c == closer)
        {
            *close = q;
            return true;
        }

        if ((open == '(') && (c == '('))
        {
            return false;
        }

        q += is_escape_at(st, q) ? 2 : 1;
    }

    if ((st->title_fail[kind] == NO_POS) || (p < st->title_fail[kind]))
    {
        st->title_fail[kind] = p;
    }

    return false;
}

/*
 * text の位置 k（`]`）の後ろのインライン形式 `(dest "title")` を読む。認めたら、リンク先・タイトルを link に
 * 入れ、`)` の後ろの位置を *end に入れる
 */
static bool scan_inline_link(inline_state_t *st, size_t k, mnd_md_link_t *link, size_t *end)
{
    size_t p = k + 1;
    size_t dest_start = 0;
    size_t dest_end = 0;
    size_t after_dest = 0;
    size_t close = 0;

    if ((p >= st->len) || (st->s[p] != '('))
    {
        return false;
    }

    p = skip_link_spaces(st, p + 1);
    if ((p < st->len) && (st->s[p] == '<'))
    {
        if (!scan_angle_dest(st, p, &close))
        {
            return false;
        }

        dest_start = p + 1;
        dest_end = close;
        after_dest = close + 1;
        link->dest_angle = true;
    }
    else
    {
        if (!scan_bare_dest(st, p, &dest_end))
        {
            return false;
        }

        dest_start = p;
        after_dest = dest_end;
        link->dest_angle = false;
    }

    link->dest = map_range(st, dest_start, dest_end);
    link->has_title = false;
    link->title.start = link->dest.end;
    link->title.end = link->dest.end;
    link->title_delim = 0;

    p = skip_link_spaces(st, after_dest);
    if ((p > after_dest) && (p < st->len) && scan_link_title(st, p, &close))
    {
        link->has_title = true;
        link->title = map_range(st, p + 1, close);
        link->title_delim = st->s[p];
        p = skip_link_spaces(st, close + 1);
    }

    if ((p >= st->len) || (st->s[p] != ')'))
    {
        return false;
    }

    *end = p + 1;

    return true;
}

/*
 * text の位置 p（`[`）からリンクラベルを読む。内側は MND_MD_LINKREF_LABEL_MAX 文字（コードポイント）まで、
 * エスケープされない角括弧を含まない。`]` の位置を *close に入れる（内側は空や空白だけでもよい）
 */
static bool scan_ref_label(const inline_state_t *st, size_t p, size_t *close)
{
    size_t q = p + 1;
    size_t chars = 0;
    unsigned char c = 0;

    while (q < st->len)
    {
        c = (unsigned char)st->s[q];
        if (c == ']')
        {
            *close = q;
            return true;
        }

        if (c == '[')
        {
            return false;
        }

        if (is_escape_at(st, q))
        {
            chars += 2;
            q += 2;
        }
        else
        {
            if ((c & 0xC0) != 0x80)
            {
                chars++;
            }

            q++;
        }

        if (chars > MND_MD_LINKREF_LABEL_MAX)
        {
            return false;
        }
    }

    return false;
}

/* text の範囲 [a, b) のコードポイントの数が MND_MD_LINKREF_LABEL_MAX 以下か（数えるのは上限の少し先まで） */
static bool label_within_limit(const inline_state_t *st, size_t a, size_t b)
{
    size_t chars = 0;
    size_t q = 0;

    for (q = a; q < b; q++)
    {
        if (((unsigned char)st->s[q] & 0xC0) != 0x80)
        {
            chars++;
            if (chars > MND_MD_LINKREF_LABEL_MAX)
            {
                return false;
            }
        }
    }

    return true;
}

/*
 * text の範囲 [a, b) をラベルとして正規化し、定義の表から引く。見つからなければ *def に NULL。
 * メモリ不足なら false
 */
static bool lookup_label(inline_state_t *st, size_t a, size_t b, const mnd_md_node_t **def)
{
    *def = NULL;
    if (!label_within_limit(st, a, b))
    {
        return true;
    }

    mnd_md_array_clear(&st->label);
    if (!mnd_md_linkref_normalize(st->s + a, b - a, &st->label))
    {
        return false;
    }

    if (st->label.count > 0)
    {
        *def = mnd_md_linkref_lookup(st->parser->doc, (const char *)st->label.items, st->label.count);
    }

    return true;
}

/*
 * text の位置 k（`]`）の後ろの参照形式を、完全参照、省略参照、ショートカット参照の順に判定する。
 * 開きの bracket の内側は [text_start, k)。完全参照のラベルがあれば、その定義だけを試す（ないならリンクに
 * しない）。省略参照とショートカット参照は、内側に角括弧を積んでいないときだけ試す（ラベルが角括弧を含まない）。
 * 認めたら、形式・ラベル・定義とその範囲を link に入れ、リンクの終わりの位置を *end に入れて *found を真にする。
 * メモリ不足なら false
 */
static bool resolve_reference(inline_state_t *st, const bracket_t *opener, size_t text_start, size_t k,
                              mnd_md_link_t *link, size_t *end, bool *found)
{
    const mnd_md_node_t *def = NULL;
    size_t close = 0;
    size_t label_start = text_start;
    size_t label_end = k;
    size_t link_end = k + 1;
    mnd_md_ref_kind_t kind = MND_MD_REF_SHORTCUT;

    *found = false;
    if (((k + 1) < st->len) && (st->s[k + 1] == '[') && scan_ref_label(st, k + 1, &close))
    {
        link_end = close + 1;
        if (close > (k + 2))
        {
            kind = MND_MD_REF_FULL;
            label_start = k + 2;
            label_end = close;
        }
        else
        {
            kind = MND_MD_REF_COLLAPSED;
        }
    }

    if ((kind != MND_MD_REF_FULL) && opener->bracket_after)
    {
        return true;
    }

    if (!lookup_label(st, label_start, label_end, &def))
    {
        return false;
    }

    if (def == NULL)
    {
        return true;
    }

    link->ref_kind = kind;
    link->label = map_range(st, label_start, label_end);
    link->def = def;
    link->dest = def->as.link_ref_def.dest;
    link->dest_angle = def->as.link_ref_def.dest_angle;
    link->has_title = def->as.link_ref_def.has_title;
    link->title = def->as.link_ref_def.title;
    link->title_delim = def->as.link_ref_def.title_delim;
    *end = link_end;
    *found = true;

    return true;
}

/*
 * text の位置 k（`[`）、または `![`（k は `!`）。開きのテキストのノードを作って角括弧のスタックに積む。
 * `!` の後ろが `[` でなければ、`!` はテキスト
 */
static match_result_t handle_open_bracket(inline_state_t *st, size_t k)
{
    bool image = (st->s[k] == '!');
    size_t width = image ? 2 : 1;
    mnd_md_node_t *node = NULL;
    bracket_t *last = NULL;
    bracket_t *b = NULL;

    if (image && (((k + 1) >= st->len) || (st->s[k + 1] != '[')))
    {
        return MATCH_NONE;
    }

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    node = emit_logical(st, MND_MD_TEXT, k, k + width);
    if (node == NULL)
    {
        return MATCH_OOM;
    }

    if (st->inactive_below > st->brackets.count)
    {
        st->inactive_below = st->brackets.count;
    }

    last = (bracket_t *)mnd_md_array_last(&st->brackets);
    if (last != NULL)
    {
        last->bracket_after = true;
    }

    b = (bracket_t *)mnd_md_array_push(&st->brackets);
    if (b == NULL)
    {
        return MATCH_OOM;
    }

    b->node = node;
    b->pos = k;
    b->delim_bottom = st->delims.count;
    b->image = image;
    b->bracket_after = false;
    st->pos = k + width;

    return MATCH_DONE;
}

/*
 * text の位置 k（`]`）。spec の look for link or image。角括弧のスタックの最上位の開きを見て、使えないか
 * （リンクの中のリンク）、インライン形式・参照形式のどれにもならなければ、開きを取り除いて `]` をテキストに
 * する。リンクか画像になれば、開きのテキストの後ろの兄弟を子にしたノードで開きのテキストを置き換え、
 * 開きの位置を下限として強調を処理する。リンクなら、残っているすべての `[` を使えなくする
 */
static match_result_t handle_close_bracket(inline_state_t *st, size_t k)
{
    bracket_t opener;
    bracket_t *top = (bracket_t *)mnd_md_array_last(&st->brackets);
    mnd_md_node_t *node = NULL;
    mnd_md_node_t *child = NULL;
    mnd_md_node_t *next = NULL;
    mnd_md_link_t link;
    size_t text_start = 0;
    size_t end = 0;
    bool found = false;

    if (top == NULL)
    {
        return MATCH_NONE;
    }

    opener = *top;
    mnd_md_array_pop(&st->brackets);
    if (!opener.image && (st->brackets.count < st->inactive_below))
    {
        /* pop のあとの要素数は、取り除いた開きの添字 */
        return MATCH_NONE;
    }

    memset(&link, 0, sizeof(link));
    text_start = opener.pos + (opener.image ? 2 : 1);
    if (scan_inline_link(st, k, &link, &end))
    {
        link.ref_kind = MND_MD_REF_INLINE;
        link.label.start = map_pos(st, k);
        link.label.end = link.label.start;
        found = true;
    }
    else
    {
        memset(&link, 0, sizeof(link));
        if (!resolve_reference(st, &opener, text_start, k, &link, &end, &found))
        {
            return MATCH_OOM;
        }
    }

    if (!found)
    {
        return MATCH_NONE;
    }

    if (!text_flush(st, k))
    {
        return MATCH_OOM;
    }

    node = mnd_md_node_new(st->parser->doc, opener.image ? MND_MD_IMAGE : MND_MD_LINK,
                           map_range(st, opener.pos, end));
    if (node == NULL)
    {
        return MATCH_OOM;
    }

    link.text = map_range(st, text_start, k);
    node->as.link = link;
    mnd_md_parser_set_node_lines(st->parser, node);

    for (child = opener.node->next; child != NULL; child = next)
    {
        next = child->next;
        mnd_md_node_append_child(node, child);
    }

    mnd_md_node_insert_after(opener.node, node);
    mnd_md_node_unlink(opener.node);

    if (!mnd_md_delim_process(st->parser, &st->delims, opener.delim_bottom))
    {
        return MATCH_OOM;
    }

    if (!opener.image)
    {
        st->inactive_below = st->brackets.count;
    }

    st->pos = end;

    return MATCH_DONE;
}

/* リーフ */

/* 前順で node の部分木の次のノード（root の部分木を出たら NULL） */
static mnd_md_node_t *next_after_subtree(mnd_md_node_t *node, const mnd_md_node_t *root)
{
    while ((node != NULL) && (node != root))
    {
        if (node->next != NULL)
        {
            return node->next;
        }

        node = node->parent;
    }

    return NULL;
}

/*
 * leaf の子孫の、原文で隣り合う（前の終わりが後ろの開始に等しい）テキストの兄弟を 1 つにまとめる。
 * 区切り記号として積んだが対にならなかった記号や、使い残した記号のテキストを、隣のテキストとつなぐ
 * （4.1 のテキストと同じく、行の中の最長のテキストにする）。テキストは '\n' を含まないので、
 * 原文で隣り合うテキストは同じ行にある
 */
static void merge_texts(inline_state_t *st, mnd_md_node_t *leaf)
{
    mnd_md_node_t *node = leaf->first_child;
    mnd_md_node_t *next = NULL;

    while (node != NULL)
    {
        if (node->kind == MND_MD_TEXT)
        {
            next = node->next;
            while ((next != NULL) && (next->kind == MND_MD_TEXT) && (next->span.start == node->span.end))
            {
                node->span.end = next->span.end;
                mnd_md_node_unlink(next);
                next = node->next;
            }

            mnd_md_parser_set_node_lines(st->parser, node);
        }

        if (node->first_child != NULL)
        {
            node = node->first_child;
        }
        else
        {
            node = next_after_subtree(node, leaf);
        }
    }
}

/*
 * leaf の行の内容から論理的な文字列と各行の開始の位置を作る。最後の行の末尾の空白とタブは除く。
 * メモリ不足なら false
 */
static bool build_text(inline_state_t *st)
{
    const mnd_md_line_t *lines = st->leaf->lines;
    size_t count = st->leaf->line_count;
    const char *data = st->parser->data;
    size_t i = 0;
    size_t end = 0;
    size_t n = 0;
    size_t *start = NULL;

    mnd_md_array_clear(&st->text);
    mnd_md_array_clear(&st->line_starts);

    for (i = 0; i < count; i++)
    {
        start = (size_t *)mnd_md_array_push(&st->line_starts);
        if (start == NULL)
        {
            return false;
        }

        *start = st->text.count;
        end = lines[i].content.end;

        if ((i + 1) == count)
        {
            while ((end > lines[i].content.start) && ((data[end - 1] == ' ') || (data[end - 1] == '\t')))
            {
                end--;
            }
        }

        n = end - lines[i].content.start;
        if (!mnd_md_array_reserve(&st->text, n + 1))
        {
            return false;
        }

        memcpy((char *)st->text.items + st->text.count, data + lines[i].content.start, n);
        st->text.count += n;
        if (((i + 1) < count) && !mnd_md_array_append(&st->text, "\n"))
        {
            return false;
        }
    }

    st->s = (const char *)st->text.items;
    st->len = st->text.count;

    return true;
}

/* leaf のインラインを解析して子にする。メモリ不足なら false */
static bool parse_leaf(inline_state_t *st, mnd_md_node_t *leaf)
{
    match_result_t result = MATCH_NONE;
    size_t k = 0;

    st->leaf = leaf;
    st->runs_ready = false;
    st->text_start = NO_POS;
    st->comment_fail = NO_POS;
    st->pi_fail = NO_POS;
    st->cdata_fail = NO_POS;
    st->decl_fail = NO_POS;
    for (k = 0; k < TITLE_KINDS; k++)
    {
        st->title_fail[k] = NO_POS;
    }
    st->inactive_below = 0;
    st->pos = 0;
    mnd_md_array_clear(&st->delims);
    mnd_md_array_clear(&st->brackets);
    if (!build_text(st))
    {
        return false;
    }

    while (st->pos < st->len)
    {
        k = st->pos;
        switch ((unsigned char)st->s[k])
        {
        case '\n':
            result = handle_newline(st, k);
            break;
        case '\\':
            result = handle_backslash(st, k);
            break;
        case '`':
            result = handle_backticks(st, k);
            break;
        case '&':
            result = handle_entity(st, k);
            break;
        case '<':
            result = handle_lt(st, k);
            break;
        case '*':
        case '_':
            result = handle_delims(st, k);
            break;
        case '[':
        case '!':
            result = handle_open_bracket(st, k);
            break;
        case ']':
            result = handle_close_bracket(st, k);
            break;
        default:
            /* 特別でない文字の並びはまとめてテキストにする */
            text_begin(st, k);
            k++;
            while ((k < st->len) && !is_special((unsigned char)st->s[k]))
            {
                k++;
            }
            st->pos = k;
            result = MATCH_DONE;
            break;
        }

        if (result == MATCH_OOM)
        {
            return false;
        }

        if (result == MATCH_NONE)
        {
            text_begin(st, k);
            st->pos = k + 1;
        }
    }

    if (!text_flush(st, st->len) || !mnd_md_delim_process(st->parser, &st->delims, 0))
    {
        return false;
    }

    merge_texts(st, leaf);

    return true;
}

/* インラインを解析するリーフか（段落と見出し。方言の inline_content のリーフは 5.1 が加える） */
static bool is_inline_leaf(const mnd_md_node_t *node)
{
    return (((node->kind == MND_MD_PARAGRAPH) || (node->kind == MND_MD_HEADING)) && (node->line_count > 0));
}

bool mnd_md_inline_run(mnd_md_parser_t *parser)
{
    mnd_md_node_t *root = parser->doc->root;
    mnd_md_node_t *node = root;
    inline_state_t st;
    bool ok = true;

    memset(&st, 0, sizeof(st));
    st.parser = parser;
    mnd_md_array_init(&st.text, 1, parser->allocator);
    mnd_md_array_init(&st.line_starts, sizeof(size_t), parser->allocator);
    mnd_md_array_init(&st.runs, sizeof(backtick_run_t), parser->allocator);
    mnd_md_array_init(&st.delims, sizeof(mnd_md_delim_t), parser->allocator);
    mnd_md_array_init(&st.brackets, sizeof(bracket_t), parser->allocator);
    mnd_md_array_init(&st.label, 1, parser->allocator);

    /* 前順に再帰なしでたどる。インラインのリーフの子（作ったインラインのノード）には降りない */
    while (node != NULL)
    {
        if (is_inline_leaf(node))
        {
            if (!parse_leaf(&st, node))
            {
                ok = false;
                break;
            }

            node = next_after_subtree(node, root);
        }
        else if (node->first_child != NULL)
        {
            node = node->first_child;
        }
        else
        {
            node = next_after_subtree(node, root);
        }
    }

    mnd_md_array_free(&st.text);
    mnd_md_array_free(&st.line_starts);
    mnd_md_array_free(&st.runs);
    mnd_md_array_free(&st.delims);
    mnd_md_array_free(&st.brackets);
    mnd_md_array_free(&st.label);

    return ok;
}
