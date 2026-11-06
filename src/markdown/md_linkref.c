/* LinkRef: リンク参照定義の解析、ラベルの正規化、定義の表 */

#include "markdown/md.h"
#include "markdown/md_internal.h"
#include "markdown/md_linkref.h"
#include "markdown/md_unicode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* カーソルが行の終わり（最後の行の内容の終わり）に達した */
#define CURSOR_EOF (-1)

/* 表の最初の要素数（2 のべき乗） */
#define TABLE_INITIAL_CAP 16

/* U+FFFD の UTF-8 */
static const char replacement_utf8[] = "\xEF\xBF\xBD";

/*
 * 段落の行の上のカーソル。lines[li] の内容の中の位置 pos を指す（pos は内容の終わりでもよい）。
 * 行の内容の終わりでは、次の行があれば論理的な改行 '\n' を読み、次の行の内容の先頭へ進む。
 * 行の内容は行末記号を含まないので、内容のバイトが '\n' になることはない
 */
typedef struct cursor
{
    const char *data;
    const mnd_md_line_t *lines;
    size_t count;           /* 1 以上 */
    size_t li;              /* 現在の行の添字 */
    size_t pos;             /* 原文のオフセット */
} cursor_t;

/* 1 つの定義の解析の結果 */
typedef struct def_scan
{
    mnd_span_t label;
    mnd_span_t dest;
    bool dest_angle;
    bool has_title;
    mnd_span_t title;
    char title_delim;
    size_t used;            /* 定義を構成する行の数（1 以上） */
} def_scan_t;

static bool is_space_or_tab(int c)
{
    return ((c == ' ') || (c == '\t'));
}

static bool is_ascii_punct(int c)
{
    return (((c >= 0x21) && (c <= 0x2F)) || ((c >= 0x3A) && (c <= 0x40))
            || ((c >= 0x5B) && (c <= 0x60)) || ((c >= 0x7B) && (c <= 0x7E)));
}

/* 現在の文字（バイト）。行の境目は '\n'、最後の行の終わりは CURSOR_EOF */
static int cursor_peek(const cursor_t *cur)
{
    if (cur->pos < cur->lines[cur->li].content.end)
    {
        return (unsigned char)cur->data[cur->pos];
    }

    if ((cur->li + 1) < cur->count)
    {
        return '\n';
    }

    return CURSOR_EOF;
}

/* 1 文字（バイトか行の境目）進める。CURSOR_EOF では何もしない */
static void cursor_advance(cursor_t *cur)
{
    if (cur->pos < cur->lines[cur->li].content.end)
    {
        cur->pos++;
    }
    else if ((cur->li + 1) < cur->count)
    {
        cur->li++;
        cur->pos = cur->lines[cur->li].content.start;
    }
}

/* 空白とタブを読み飛ばす */
static void cursor_skip_spaces(cursor_t *cur)
{
    while (is_space_or_tab(cursor_peek(cur)))
    {
        cursor_advance(cur);
    }
}

/* 空白とタブ、高々 1 つの行の境目、空白とタブを読み飛ばす */
static void cursor_skip_spnl(cursor_t *cur)
{
    cursor_skip_spaces(cur);
    if (cursor_peek(cur) == '\n')
    {
        cursor_advance(cur);
        cursor_skip_spaces(cur);
    }
}

/* 行の残りが空白とタブだけなら、それを読み飛ばして true（カーソルは行の終わり） */
static bool cursor_rest_blank(cursor_t *cur)
{
    int c = 0;

    cursor_skip_spaces(cur);
    c = cursor_peek(cur);

    return ((c == '\n') || (c == CURSOR_EOF));
}

/* バックスラッシュの位置で、バックスラッシュと、ASCII の句読点が続けばそれも読み進める。進めた文字数を返す */
static size_t cursor_skip_escape(cursor_t *cur)
{
    int c = 0;

    cursor_advance(cur);
    c = cursor_peek(cur);
    if ((c != '\n') && is_ascii_punct(c))
    {
        cursor_advance(cur);
        return 2;
    }

    return 1;
}

/*
 * `[` の位置からリンクラベルを読み、`]` の後ろへ進める。ラベル（角括弧の内側）の範囲を *label に入れる。
 * 内側は MND_MD_LINKREF_LABEL_MAX 文字まで、エスケープされない角括弧を含まず、空白・タブ・行の境目でない
 * 文字を 1 つ以上含む
 */
static bool scan_label(cursor_t *cur, mnd_span_t *label)
{
    size_t chars = 0;
    bool nonblank = false;

    if (cursor_peek(cur) != '[')
    {
        return false;
    }

    cursor_advance(cur);
    label->start = cur->pos;

    for (;;)
    {
        int c = cursor_peek(cur);

        if ((c == CURSOR_EOF) || (c == '['))
        {
            return false;
        }

        if (c == ']')
        {
            break;
        }

        if (c == '\\')
        {
            /* エスケープした文字は 1 バイトの ASCII なので、進めたバイトの数が文字数 */
            chars += cursor_skip_escape(cur);
            nonblank = true;
        }
        else
        {
            if ((c & 0xC0) != 0x80)
            {
                chars++;
            }

            if ((c != ' ') && (c != '\t') && (c != '\n'))
            {
                nonblank = true;
            }

            cursor_advance(cur);
        }

        if (chars > MND_MD_LINKREF_LABEL_MAX)
        {
            return false;
        }
    }

    label->end = cur->pos;
    cursor_advance(cur);

    return nonblank;
}

/*
 * リンク先を読む。`<` で始まれば、改行とエスケープされない `<` を含まない `>` までの内側（空でもよい）。
 * そうでなければ、ASCII の制御文字と空白を含まず、エスケープされない括弧が釣り合う、空でない並び
 */
static bool scan_destination(cursor_t *cur, mnd_span_t *dest, bool *angle)
{
    size_t depth = 0;
    int c = cursor_peek(cur);

    if (c == '<')
    {
        cursor_advance(cur);
        dest->start = cur->pos;

        for (;;)
        {
            c = cursor_peek(cur);
            if ((c == CURSOR_EOF) || (c == '\n') || (c == '<'))
            {
                return false;
            }

            if (c == '>')
            {
                break;
            }

            if (c == '\\')
            {
                (void)cursor_skip_escape(cur);
            }
            else
            {
                cursor_advance(cur);
            }
        }

        dest->end = cur->pos;
        cursor_advance(cur);
        *angle = true;

        return true;
    }

    dest->start = cur->pos;

    for (;;)
    {
        c = cursor_peek(cur);
        if ((c == CURSOR_EOF) || (c == '\n') || (c <= 0x20) || (c == 0x7F))
        {
            break;
        }

        if (c == '\\')
        {
            (void)cursor_skip_escape(cur);
            continue;
        }

        if (c == '(')
        {
            depth++;
        }
        else if (c == ')')
        {
            if (depth == 0)
            {
                break;
            }

            depth--;
        }

        cursor_advance(cur);
    }

    dest->end = cur->pos;
    *angle = false;

    return ((dest->end > dest->start) && (depth == 0));
}

/*
 * リンクタイトルを読む（`"..."`、`'...'`、`(...)`。行をまたいでよい。括弧の形はエスケープされない `(` を
 * 含まない）。閉じの記号の後ろへ進め、内側の範囲を *title、開きの記号を *delim に入れる
 */
static bool scan_title(cursor_t *cur, mnd_span_t *title, char *delim)
{
    int open = cursor_peek(cur);
    int close = 0;

    if ((open == '"') || (open == '\''))
    {
        close = open;
    }
    else if (open == '(')
    {
        close = ')';
    }
    else
    {
        return false;
    }

    cursor_advance(cur);
    title->start = cur->pos;

    for (;;)
    {
        int c = cursor_peek(cur);

        if (c == CURSOR_EOF)
        {
            return false;
        }

        if (c == close)
        {
            break;
        }

        if ((open == '(') && (c == '('))
        {
            return false;
        }

        if (c == '\\')
        {
            (void)cursor_skip_escape(cur);
        }
        else
        {
            cursor_advance(cur);
        }
    }

    title->end = cur->pos;
    cursor_advance(cur);
    *delim = (char)open;

    return true;
}

/*
 * lines[0..count) の先頭から 1 つのリンク参照定義を解析する（CommonMark の Link reference definitions）。
 * 定義なら *out に範囲と使った行の数を入れて true。定義でなければ false。
 * タイトルの候補の後ろに空白でない文字があれば、タイトルのない定義として読み直す（cmark と同じ）
 */
static bool scan_definition(const char *data, const mnd_md_line_t *lines, size_t count, def_scan_t *out)
{
    cursor_t cur;
    cursor_t before_title;

    cur.data = data;
    cur.lines = lines;
    cur.count = count;
    cur.li = 0;
    cur.pos = lines[0].content.start;
    memset(out, 0, sizeof(*out));

    if (!scan_label(&cur, &out->label) || (cursor_peek(&cur) != ':'))
    {
        return false;
    }

    cursor_advance(&cur);
    cursor_skip_spnl(&cur);
    if (!scan_destination(&cur, &out->dest, &out->dest_angle))
    {
        return false;
    }

    before_title = cur;
    cursor_skip_spnl(&cur);
    if ((cur.li != before_title.li) || (cur.pos != before_title.pos))
    {
        /* タイトルはリンク先と空白か改行で区切られていなければならない */
        out->has_title = scan_title(&cur, &out->title, &out->title_delim);
    }

    if (out->has_title && !cursor_rest_blank(&cur))
    {
        out->has_title = false;
    }

    if (!out->has_title)
    {
        cur = before_title;
        if (!cursor_rest_blank(&cur))
        {
            return false;
        }
    }

    out->used = cur.li + 1;

    return true;
}

/* UTF-8 の列 s[pos..len) の先頭のコードポイントを *cp に入れ、バイト数を返す。不正なら U+FFFD と 1 */
static size_t decode_utf8(const unsigned char *s, size_t pos, size_t len, uint32_t *cp)
{
    unsigned char b = s[pos];
    size_t n = 0;
    size_t i = 0;
    uint32_t v = 0;
    uint32_t min = 0;

    if (b < 0x80)
    {
        *cp = b;
        return 1;
    }

    if ((b >= 0xC2) && (b <= 0xDF))
    {
        n = 2;
        v = b & 0x1Fu;
        min = 0x80u;
    }
    else if ((b >= 0xE0) && (b <= 0xEF))
    {
        n = 3;
        v = b & 0x0Fu;
        min = 0x800u;
    }
    else if ((b >= 0xF0) && (b <= 0xF4))
    {
        n = 4;
        v = b & 0x07u;
        min = 0x10000u;
    }
    else
    {
        *cp = 0xFFFDu;
        return 1;
    }

    if (n > (len - pos))
    {
        *cp = 0xFFFDu;
        return 1;
    }

    for (i = 1; i < n; i++)
    {
        if ((s[pos + i] & 0xC0) != 0x80)
        {
            *cp = 0xFFFDu;
            return 1;
        }

        v = (v << 6) | (s[pos + i] & 0x3Fu);
    }

    if ((v < min) || (v > 0x10FFFFu) || ((v >= 0xD800u) && (v <= 0xDFFFu)))
    {
        *cp = 0xFFFDu;
        return 1;
    }

    *cp = v;

    return n;
}

/* CommonMark の Unicode の空白（Zs、タブ、LF、FF、CR） */
static bool is_unicode_whitespace(uint32_t cp)
{
    return ((cp == '\t') || (cp == '\n') || (cp == '\f') || (cp == '\r') || mnd_md_unicode_is_space(cp));
}

/* bytes[0..len) を out の後ろに追加する。メモリ不足なら false（out は変更しない） */
static bool array_append_bytes(mnd_md_array_t *out, const char *bytes, size_t len)
{
    if (!mnd_md_array_reserve(out, len))
    {
        return false;
    }

    if (len > 0)
    {
        memcpy((char *)out->items + out->count, bytes, len);
        out->count += len;
    }

    return true;
}

bool mnd_md_linkref_normalize(const char *text, size_t len, mnd_md_array_t *out)
{
    const unsigned char *s = (const unsigned char *)text;
    size_t start = out->count;
    size_t pos = 0;
    bool pending_space = false;

    while (pos < len)
    {
        uint32_t cp = 0;
        char folded[MND_MD_UNICODE_FOLD_MAX];
        size_t n = 0;

        pos += decode_utf8(s, pos, len, &cp);
        if (is_unicode_whitespace(cp))
        {
            /* 先頭の空白は捨て、途中の空白の並びは次の文字の前に 1 つの空白として書く（末尾は捨てる） */
            pending_space = (out->count > start);
            continue;
        }

        if (pending_space)
        {
            if (!array_append_bytes(out, " ", 1))
            {
                return false;
            }

            pending_space = false;
        }

        if (cp == 0)
        {
            cp = 0xFFFDu;
        }

        n = mnd_md_unicode_fold(cp, folded);
        if (n == 0)
        {
            /* decode_utf8 はサロゲートと範囲外を返さないので、ここには来ない */
            if (!array_append_bytes(out, replacement_utf8, sizeof(replacement_utf8) - 1))
            {
                return false;
            }
        }
        else if (!array_append_bytes(out, folded, n))
        {
            return false;
        }
    }

    return true;
}

/* 正規化したラベルのハッシュ値（FNV-1a） */
static size_t label_hash(const char *norm, size_t len)
{
    uint32_t h = 2166136261u;
    size_t i = 0;

    for (i = 0; i < len; i++)
    {
        h ^= (unsigned char)norm[i];
        h *= 16777619u;
    }

    return (size_t)h;
}

/* 表の中の、ラベル norm[0..len)（ハッシュ値 hash）の要素か、それがなければ探索が止まった空きの要素 */
static mnd_md_linkref_slot_t *table_probe(const mnd_md_linkref_table_t *table, const char *norm, size_t len,
                                          size_t hash)
{
    size_t mask = table->cap - 1;
    size_t i = hash & mask;

    for (;;)
    {
        mnd_md_linkref_slot_t *slot = &table->slots[i];

        if (slot->def == NULL)
        {
            return slot;
        }

        if ((slot->hash == hash) && (slot->len == len)
            && (memcmp(slot->def->as.link_ref_def.norm_label, norm, len) == 0))
        {
            return slot;
        }

        i = (i + 1) & mask;
    }
}

/* 要素数を倍にして作り直す（要素の配列は文書のアリーナに確保する）。メモリ不足なら false（表は変更しない） */
static bool table_grow(mnd_md_doc_t *doc)
{
    mnd_md_linkref_table_t *table = &doc->refs;
    mnd_md_linkref_table_t grown;
    size_t i = 0;

    grown.cap = (table->cap == 0) ? TABLE_INITIAL_CAP : (table->cap * 2);
    if (grown.cap < table->cap)
    {
        return false;
    }

    grown.count = table->count;
    grown.slots = (mnd_md_linkref_slot_t *)mnd_md_arena_alloc_array(&doc->arena, grown.cap,
                                                                     sizeof(*grown.slots));
    if (grown.slots == NULL)
    {
        return false;
    }

    for (i = 0; i < table->cap; i++)
    {
        const mnd_md_linkref_slot_t *slot = &table->slots[i];

        if (slot->def != NULL)
        {
            *table_probe(&grown, slot->def->as.link_ref_def.norm_label, slot->len, slot->hash) = *slot;
        }
    }

    *table = grown;

    return true;
}

/*
 * 定義 def（norm_label の長さは len）を表に登録する。同じラベルが登録済みなら何もしない（6.3）。
 * メモリ不足なら false
 */
static bool table_add(mnd_md_doc_t *doc, const mnd_md_node_t *def, size_t len)
{
    mnd_md_linkref_table_t *table = &doc->refs;
    const char *norm = def->as.link_ref_def.norm_label;
    size_t hash = label_hash(norm, len);
    mnd_md_linkref_slot_t *slot = NULL;

    /* 使用率を 1/2 以下に保つ */
    if ((table->cap == 0) || ((table->count + 1) > (table->cap / 2)))
    {
        if (!table_grow(doc))
        {
            return false;
        }
    }

    slot = table_probe(table, norm, len, hash);
    if (slot->def == NULL)
    {
        slot->hash = hash;
        slot->len = len;
        slot->def = def;
        table->count++;
    }

    return true;
}

const mnd_md_node_t *mnd_md_linkref_lookup(const mnd_md_doc_t *doc, const char *norm, size_t len)
{
    const mnd_md_linkref_slot_t *slot = NULL;

    if ((doc == NULL) || (doc->refs.cap == 0))
    {
        return NULL;
    }

    slot = table_probe(&doc->refs, norm, len, label_hash(norm, len));

    return slot->def;
}

/*
 * 定義の行 lines[0..count) の上のラベルの範囲 label の論理的な文字列（行の境目は "\n"）を
 * text の後ろに追加する。メモリ不足なら false
 */
static bool label_logical_text(const char *data, const mnd_md_line_t *lines, size_t count, mnd_span_t label,
                               mnd_md_array_t *text)
{
    size_t i = 0;

    for (i = 0; i < count; i++)
    {
        size_t from = (label.start > lines[i].content.start) ? label.start : lines[i].content.start;
        size_t to = (label.end < lines[i].content.end) ? label.end : lines[i].content.end;

        if ((from < to) && !array_append_bytes(text, data + from, to - from))
        {
            return false;
        }

        if (((i + 1) >= count) || (lines[i + 1].content.start > label.end))
        {
            break;
        }

        if (!array_append_bytes(text, "\n", 1))
        {
            return false;
        }
    }

    return true;
}

/*
 * 解析した定義 scan（lines[0..scan->used) が構成する行）のノードを作り、paragraph の直前に置き、
 * 正規化したラベルを付けて表に登録する。text と norm は作業用の配列。メモリ不足なら false
 */
static bool add_definition(mnd_md_parser_t *parser, mnd_md_node_t *paragraph, const mnd_md_line_t *lines,
                           const def_scan_t *scan, mnd_md_array_t *text, mnd_md_array_t *norm)
{
    mnd_md_doc_t *doc = parser->doc;
    mnd_md_link_ref_def_t *def = NULL;
    mnd_md_node_t *node = NULL;
    char *label = NULL;
    mnd_span_t span;

    span.start = lines[0].content.start;
    span.end = lines[scan->used - 1].content.end;
    node = mnd_md_node_new(doc, MND_MD_LINK_REF_DEF, span);
    if (node == NULL)
    {
        return false;
    }

    node->lines = (const mnd_md_line_t *)mnd_md_arena_dup(&doc->arena, lines, scan->used * sizeof(*lines));
    if (node->lines == NULL)
    {
        return false;
    }

    node->line_count = scan->used;

    mnd_md_array_clear(text);
    mnd_md_array_clear(norm);
    if (!label_logical_text(parser->data, lines, scan->used, scan->label, text)
        || !mnd_md_linkref_normalize((const char *)text->items, text->count, norm))
    {
        return false;
    }

    label = (char *)mnd_md_arena_alloc(&doc->arena, norm->count + 1);
    if (label == NULL)
    {
        return false;
    }

    if (norm->count > 0)
    {
        memcpy(label, norm->items, norm->count);
    }

    label[norm->count] = '\0';

    def = &node->as.link_ref_def;
    def->label = scan->label;
    def->dest = scan->dest;
    def->dest_angle = scan->dest_angle;
    def->has_title = scan->has_title;

    if (scan->has_title)
    {
        def->title = scan->title;
        def->title_delim = scan->title_delim;
    }
    else
    {
        /* タイトルがなければ、空の範囲をリンク先の終わりに置く */
        def->title.start = scan->dest.end;
        def->title.end = scan->dest.end;
    }

    def->norm_label = label;
    mnd_md_node_insert_before(paragraph, node);
    mnd_md_parser_set_node_lines(parser, node);

    return table_add(doc, node, norm->count);
}

bool mnd_md_linkref_take(mnd_md_parser_t *parser, mnd_md_node_t *paragraph, const mnd_md_line_t *lines,
                         size_t count, size_t *taken)
{
    mnd_md_array_t text;
    mnd_md_array_t norm;
    size_t i = 0;
    bool ok = true;

    *taken = 0;
    mnd_md_array_init(&text, 1, parser->allocator);
    mnd_md_array_init(&norm, 1, parser->allocator);

    while (i < count)
    {
        def_scan_t scan;

        if ((lines[i].content.start >= lines[i].content.end)
            || !scan_definition(parser->data, lines + i, count - i, &scan))
        {
            break;
        }

        if (!add_definition(parser, paragraph, lines + i, &scan, &text, &norm))
        {
            ok = false;
            break;
        }

        i += scan.used;
    }

    mnd_md_array_free(&text);
    mnd_md_array_free(&norm);
    *taken = i;

    return ok;
}
