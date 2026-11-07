/*
 * Scalar: プレーン・引用符付き・ブロックスカラーの走査と論理値、非対応の指示文字の検出。
 * 解析の作業状態の補助関数（エラーの報告、行番号、範囲の設定）もここに置く（Scalar が最下位の解析の部品）
 */

#include "yaml/yaml_scalar.h"

#include "foundation/buf.h"

#include <stddef.h>

/* 解析の作業状態の補助関数 */

bool mnd_yaml_parser_claim_error(mnd_yaml_parser_t *p)
{
    if (p->failed)
    {
        return false;
    }
    p->failed = true;

    return true;
}

void mnd_yaml_parser_error(mnd_yaml_parser_t *p, size_t line, const char *message)
{
    if (mnd_yaml_parser_claim_error(p))
    {
        mnd_diag_add(p->diags, MND_SEVERITY_ERROR, p->path, line, "%s", message);
    }
}

void mnd_yaml_parser_oom(mnd_yaml_parser_t *p)
{
    mnd_yaml_parser_error(p, 0, "out of memory");
}

size_t mnd_yaml_parser_line_of(const mnd_yaml_parser_t *p, size_t offset)
{
    return mnd_yaml_lines_line_of(&p->lines, offset);
}

void mnd_yaml_parser_set_span(const mnd_yaml_parser_t *p, mnd_yaml_node_t *node, mnd_span_t span)
{
    node->span = span;
    node->start_line = mnd_yaml_parser_line_of(p, span.start);
    node->end_line = mnd_yaml_parser_line_of(p, span.end);
}

/* 文字の分類 */

static bool scalar_is_white(char c)
{
    return (c == ' ') || (c == '\t');
}

static bool scalar_is_flow_indicator(char c)
{
    return (c == ',') || (c == '[') || (c == ']') || (c == '{') || (c == '}');
}

/*
 * data[pos] の後ろが区切り（行の内容の終わり、空白、フロー文脈ではフロー形式の指示文字）か
 */
static bool scalar_followed_by_separator(const char *data, size_t pos, size_t end, bool flow)
{
    if ((pos + 1) >= end)
    {
        return true;
    }
    if (scalar_is_white(data[pos + 1]))
    {
        return true;
    }

    return flow && scalar_is_flow_indicator(data[pos + 1]);
}

/* エラーの報告（false を返す） */

static bool scalar_fail(mnd_yaml_parser_t *p, size_t line, const char *message)
{
    mnd_yaml_parser_error(p, line, message);

    return false;
}

/*
 * スカラーとして始められない位置のエラー。フロー文脈では空の項目またはフロー形式の中で
 * 使えない値、ブロック文脈では値の位置の余分な内容として報告する
 */
static bool scalar_fail_start(mnd_yaml_parser_t *p, size_t line, bool flow)
{
    if (flow)
    {
        return scalar_fail(p, line, "invalid flow collection entry");
    }

    return scalar_fail(p, line, "unexpected content after value");
}

/* 論理値の組み立て（失敗はメモリ不足） */

static bool scalar_append_repeat(mnd_buf_t *buf, unsigned char byte, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i)
    {
        if (!mnd_buf_append_byte(buf, byte))
        {
            return false;
        }
    }

    return true;
}

/*
 * 行の境目の折り返し: 間の空行がなければ 1 つの空白（suppress_space なら何も入れない）、
 * 空行が n 個あれば n 個の改行（LF）
 */
static bool scalar_append_fold(mnd_buf_t *buf, size_t empty_lines, bool suppress_space)
{
    if (empty_lines == 0)
    {
        if (suppress_space)
        {
            return true;
        }

        return mnd_buf_append_byte(buf, (unsigned char)' ');
    }

    return scalar_append_repeat(buf, (unsigned char)'\n', empty_lines);
}

/* コードポイント cp（U+10FFFF 以下）を UTF-8 で追加する */
static bool scalar_append_utf8(mnd_buf_t *buf, unsigned long cp)
{
    unsigned char bytes[4];
    size_t n;

    if (cp < 0x80UL)
    {
        bytes[0] = (unsigned char)cp;
        n = 1;
    }
    else if (cp < 0x800UL)
    {
        bytes[0] = (unsigned char)(0xC0UL | (cp >> 6));
        bytes[1] = (unsigned char)(0x80UL | (cp & 0x3FUL));
        n = 2;
    }
    else if (cp < 0x10000UL)
    {
        bytes[0] = (unsigned char)(0xE0UL | (cp >> 12));
        bytes[1] = (unsigned char)(0x80UL | ((cp >> 6) & 0x3FUL));
        bytes[2] = (unsigned char)(0x80UL | (cp & 0x3FUL));
        n = 3;
    }
    else
    {
        bytes[0] = (unsigned char)(0xF0UL | (cp >> 18));
        bytes[1] = (unsigned char)(0x80UL | ((cp >> 12) & 0x3FUL));
        bytes[2] = (unsigned char)(0x80UL | ((cp >> 6) & 0x3FUL));
        bytes[3] = (unsigned char)(0x80UL | (cp & 0x3FUL));
        n = 4;
    }

    return mnd_buf_append(buf, bytes, n);
}

/* ノードの作成 */

/*
 * 組み立てた論理値と範囲 [start, end) でスカラーのノードを作り、*out と *at（end_index の行の end）を設定する。
 * メモリ不足なら診断を追加して false
 */
static bool scalar_finish(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, mnd_yaml_style_t style, bool flow,
                          size_t start, size_t end, size_t end_index, const mnd_buf_t *value,
                          mnd_yaml_node_t **out)
{
    mnd_yaml_node_t *node = NULL;
    char *copy = NULL;
    mnd_span_t span;

    node = mnd_yaml_node_new(p->doc, MND_YAML_SCALAR);
    if (node == NULL)
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    copy = mnd_yaml_arena_strdup(p->doc, value->data, value->len);
    if (copy == NULL)
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    node->as.scalar.style = style;
    node->as.scalar.value = copy;
    node->as.scalar.value_len = value->len;
    node->as.scalar.flow = flow;
    span.start = start;
    span.end = end;
    mnd_yaml_parser_set_span(p, node, span);

    *out = node;
    at->line_index = end_index;
    at->offset = end;

    return true;
}

/* プレーンスカラー */

typedef enum scalar_plain_stop
{
    SCALAR_PLAIN_STOP_EOL,      /* 行の内容の終わりまで読んだ（次の行に続けられる） */
    SCALAR_PLAIN_STOP_COLON,    /* : と区切り（マッピングの値の指示文字）の前で止まった */
    SCALAR_PLAIN_STOP_OTHER,    /* コメント、またはフロー形式の指示文字の前で止まった */
    SCALAR_PLAIN_STOP_CR        /* 単独の CR を見つけた */
} scalar_plain_stop_t;

/*
 * data[start, end) をプレーンスカラーの 1 行分として読む。*text_end に最後の空白でない文字の直後を入れる
 * （何も読めなければ start）
 */
static scalar_plain_stop_t scalar_plain_scan_line(const char *data, size_t start, size_t end, bool flow,
                                                  size_t *text_end)
{
    size_t i = start;
    char c;

    *text_end = start;
    while (i < end)
    {
        c = data[i];
        if (c == '\r')
        {
            return SCALAR_PLAIN_STOP_CR;
        }
        if ((c == ':') && scalar_followed_by_separator(data, i, end, flow))
        {
            return SCALAR_PLAIN_STOP_COLON;
        }
        if ((c == '#') && (i > start) && scalar_is_white(data[i - 1]))
        {
            return SCALAR_PLAIN_STOP_OTHER;
        }
        if (flow && scalar_is_flow_indicator(c))
        {
            return SCALAR_PLAIN_STOP_OTHER;
        }
        ++i;
        if (!scalar_is_white(c))
        {
            *text_end = i;
        }
    }

    return SCALAR_PLAIN_STOP_EOL;
}

/*
 * at（最初の文字は検査済み）からプレーンスカラーを読む。行の内容の終わりまで読めたときは、
 * 次の空でない行を継続行として読めるか調べる: コメント行でなく、ブロック文脈では親のインデントより深く
 * 構造の行（- と key:）でなく、読める文字があること
 */
static bool scalar_build_plain(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent, bool flow,
                               mnd_buf_t *value, mnd_yaml_node_t **out)
{
    const char *data = p->data;
    const mnd_yaml_line_t *line = &p->lines.items[at->line_index];
    const mnd_yaml_line_t *next = NULL;
    scalar_plain_stop_t stop;
    size_t text_end = at->offset;
    size_t end;
    size_t end_index = at->line_index;
    size_t index;
    size_t empty_lines;
    size_t s;

    stop = scalar_plain_scan_line(data, at->offset, line->content.end, flow, &text_end);
    if (stop == SCALAR_PLAIN_STOP_CR)
    {
        return scalar_fail(p, line->line, "carriage return without line feed");
    }
    if (!mnd_buf_append(value, data + at->offset, text_end - at->offset))
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    end = text_end;

    while (stop == SCALAR_PLAIN_STOP_EOL)
    {
        index = end_index + 1;
        empty_lines = 0;
        while ((index < p->lines.count) && p->lines.items[index].blank)
        {
            ++empty_lines;
            ++index;
        }
        if (index >= p->lines.count)
        {
            break;
        }
        next = &p->lines.items[index];
        if (next->comment)
        {
            break;
        }
        s = next->content.start;
        while ((s < next->content.end) && scalar_is_white(data[s]))
        {
            ++s;
        }
        if (!flow)
        {
            if (next->tab_in_indent)
            {
                return scalar_fail(p, next->line, "tab character in indentation");
            }
            if (next->indent <= parent_indent)
            {
                break;
            }
            if ((data[s] == '-') && scalar_followed_by_separator(data, s, next->content.end, false))
            {
                break;
            }
        }
        stop = scalar_plain_scan_line(data, s, next->content.end, flow, &text_end);
        if (stop == SCALAR_PLAIN_STOP_CR)
        {
            return scalar_fail(p, next->line, "carriage return without line feed");
        }
        if ((text_end == s) || (!flow && (stop == SCALAR_PLAIN_STOP_COLON)))
        {
            break;
        }
        if (!scalar_append_fold(value, empty_lines, false)
            || !mnd_buf_append(value, data + s, text_end - s))
        {
            mnd_yaml_parser_oom(p);
            return false;
        }
        end = text_end;
        end_index = index;
    }

    return scalar_finish(p, at, MND_YAML_PLAIN, flow, at->offset, end, end_index, value, out);
}

static bool scalar_parse_plain(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent, bool flow,
                               mnd_yaml_node_t **out)
{
    mnd_buf_t value;
    bool ok;

    mnd_buf_init(&value);
    ok = scalar_build_plain(p, at, parent_indent, flow, &value, out);
    mnd_buf_free(&value);

    return ok;
}

/* 引用符付きのスカラー */

/* 16 進数の 1 文字の値。16 進数でなければ -1 */
static int scalar_hex_value(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f'))
    {
        return (c - 'a') + 10;
    }
    if ((c >= 'A') && (c <= 'F'))
    {
        return (c - 'A') + 10;
    }

    return -1;
}

/* 1 文字のエスケープ（\ の次の文字とコードポイント） */
typedef struct scalar_simple_escape
{
    char c;
    unsigned long cp;
} scalar_simple_escape_t;

static const scalar_simple_escape_t scalar_simple_escapes[] =
{
    { 'a', 0x07UL }, { 'b', 0x08UL }, { 't', 0x09UL }, { 'n', 0x0AUL }, { 'v', 0x0BUL }, { 'f', 0x0CUL },
    { 'r', 0x0DUL }, { 'e', 0x1BUL }, { ' ', 0x20UL }, { '"', 0x22UL }, { '/', 0x2FUL }, { '\\', 0x5CUL },
    { 'N', 0x85UL }, { '_', 0xA0UL }, { 'L', 0x2028UL }, { 'P', 0x2029UL }
};

/*
 * data[pos]（\ の次の文字。pos < end）から始まるエスケープを解決してコードポイントを *cp に、
 * \ の後ろで消費したバイト数を *consumed に入れる。YAML 1.2 が定めない、桁が足りない、U+0000、
 * サロゲート、U+10FFFF を超えるなら false
 */
static bool scalar_decode_escape(const char *data, size_t pos, size_t end, unsigned long *cp, size_t *consumed)
{
    size_t digits = 0;
    size_t i;
    unsigned long value = 0;
    int v;

    *consumed = 1;
    for (i = 0; i < (sizeof(scalar_simple_escapes) / sizeof(scalar_simple_escapes[0])); ++i)
    {
        if (scalar_simple_escapes[i].c == data[pos])
        {
            *cp = scalar_simple_escapes[i].cp;
            return true;
        }
    }
    if (data[pos] == 'x')
    {
        digits = 2;
    }
    else if (data[pos] == 'u')
    {
        digits = 4;
    }
    else if (data[pos] == 'U')
    {
        digits = 8;
    }
    else
    {
        return false;
    }

    if ((end - (pos + 1)) < digits)
    {
        return false;
    }
    for (i = 0; i < digits; ++i)
    {
        v = scalar_hex_value(data[pos + 1 + i]);
        if (v < 0)
        {
            return false;
        }
        value = (value * 16UL) + (unsigned long)v;
    }
    /*
     * U+0000 は論理値に NUL を入れることになるため、エスケープの誤りとして扱う
     */
    if ((value == 0UL) || (value > 0x10FFFFUL) || ((value >= 0xD800UL) && (value <= 0xDFFFUL)))
    {
        return false;
    }
    *cp = value;
    *consumed = 1 + digits;

    return true;
}

/*
 * at（開きの引用符）から引用符付きのスカラーを読む。行の境目では、行末の空白（エスケープでない）と
 * 次の行の行頭の空白を除き、折り返しを入れる。ダブルクオートの行末の \ は、その前の空白を残し、
 * 空白を入れずに次の行に続ける。
 * ブロック文脈では、継続行のインデントのタブをエラーにする
 */
static bool scalar_build_quoted(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, bool flow, bool double_quoted,
                                mnd_buf_t *value, mnd_yaml_node_t **out)
{
    const char *data = p->data;
    const char quote = double_quoted ? '"' : '\'';
    size_t index = at->line_index;
    size_t open_line = p->lines.items[index].line;
    size_t i = at->offset + 1;
    size_t e = p->lines.items[index].content.end;
    size_t ws_start = 0;
    size_t empty_lines;
    size_t next_index;
    size_t consumed;
    unsigned long cp;
    bool has_ws = false;
    bool escaped_break = false;
    const mnd_yaml_line_t *next = NULL;
    char c;

    for (;;)
    {
        if (i >= e)
        {
            /*
             * 行の境目: 行末の空白を捨て、間の空行を数えて次の行の空白でない位置から続ける
             */
            has_ws = false;
            next_index = index + 1;
            empty_lines = 0;
            while ((next_index < p->lines.count) && p->lines.items[next_index].blank)
            {
                ++empty_lines;
                ++next_index;
            }
            if (next_index >= p->lines.count)
            {
                return scalar_fail(p, open_line, "unterminated quoted scalar");
            }
            next = &p->lines.items[next_index];
            if (!flow && next->tab_in_indent)
            {
                return scalar_fail(p, next->line, "tab character in indentation");
            }
            if (!scalar_append_fold(value, empty_lines, escaped_break))
            {
                mnd_yaml_parser_oom(p);
                return false;
            }
            escaped_break = false;
            index = next_index;
            i = next->content.start;
            e = next->content.end;
            while ((i < e) && scalar_is_white(data[i]))
            {
                ++i;
            }
            continue;
        }

        c = data[i];
        if (c == '\r')
        {
            return scalar_fail(p, p->lines.items[index].line, "carriage return without line feed");
        }
        if (scalar_is_white(c))
        {
            if (!has_ws)
            {
                has_ws = true;
                ws_start = i;
            }
            ++i;
            continue;
        }
        if (double_quoted && (c == '\\') && ((i + 1) >= e))
        {
            /*
             * 行末の \: その前の空白は内容として残し（YAML 1.2 の例 7.5）、次の行の境目で空白を入れない
             */
            if (has_ws)
            {
                if (!mnd_buf_append(value, data + ws_start, i - ws_start))
                {
                    mnd_yaml_parser_oom(p);
                    return false;
                }
                has_ws = false;
            }
            escaped_break = true;
            i = e;
            continue;
        }
        if (has_ws)
        {
            if (!mnd_buf_append(value, data + ws_start, i - ws_start))
            {
                mnd_yaml_parser_oom(p);
                return false;
            }
            has_ws = false;
        }
        if (c == quote)
        {
            if (!double_quoted && ((i + 1) < e) && (data[i + 1] == '\''))
            {
                if (!mnd_buf_append_byte(value, (unsigned char)'\''))
                {
                    mnd_yaml_parser_oom(p);
                    return false;
                }
                i += 2;
                continue;
            }
            break;
        }
        if (double_quoted && (c == '\\'))
        {
            if (!scalar_decode_escape(data, i + 1, e, &cp, &consumed))
            {
                return scalar_fail(p, p->lines.items[index].line, "invalid escape sequence in double-quoted scalar");
            }
            if (!scalar_append_utf8(value, cp))
            {
                mnd_yaml_parser_oom(p);
                return false;
            }
            i += 1 + consumed;
            continue;
        }
        if (!mnd_buf_append_byte(value, (unsigned char)c))
        {
            mnd_yaml_parser_oom(p);
            return false;
        }
        ++i;
    }

    return scalar_finish(p, at, double_quoted ? MND_YAML_DOUBLE_QUOTED : MND_YAML_SINGLE_QUOTED, flow, at->offset,
                         i + 1, index, value, out);
}

static bool scalar_parse_quoted(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, bool flow, bool double_quoted,
                                mnd_yaml_node_t **out)
{
    mnd_buf_t value;
    bool ok;

    mnd_buf_init(&value);
    ok = scalar_build_quoted(p, at, flow, double_quoted, &value, out);
    mnd_buf_free(&value);

    return ok;
}

/* ブロックスカラー（| と >） */

/* ヘッダの内容 */
typedef struct scalar_block_header
{
    mnd_yaml_style_t style;
    mnd_yaml_chomp_t chomp;
    unsigned indicator;     /* インデント指示子（0 はなし） */
    size_t end;             /* 指示子の直後のオフセット（範囲の終わりの候補） */
} scalar_block_header_t;

/*
 * line の offset（| か >）からヘッダを読む: チョンプ指示子とインデント指示子（1〜9）を任意の順で
 * 0〜1 個ずつ。後ろは空白と、空白の後ろのコメントだけ。不正なら診断を追加して false
 */
static bool scalar_block_read_header(mnd_yaml_parser_t *p, const mnd_yaml_line_t *line, size_t offset,
                                     scalar_block_header_t *h)
{
    const char *data = p->data;
    size_t i = offset + 1;
    size_t e = line->content.end;
    bool has_chomp = false;
    char c;

    h->style = (data[offset] == '|') ? MND_YAML_LITERAL : MND_YAML_FOLDED;
    h->chomp = MND_YAML_CHOMP_CLIP;
    h->indicator = 0;
    while (i < e)
    {
        c = data[i];
        if (((c == '-') || (c == '+')) && !has_chomp)
        {
            has_chomp = true;
            h->chomp = (c == '-') ? MND_YAML_CHOMP_STRIP : MND_YAML_CHOMP_KEEP;
        }
        else if ((c >= '1') && (c <= '9') && (h->indicator == 0))
        {
            h->indicator = (unsigned)(c - '0');
        }
        else
        {
            break;
        }
        ++i;
    }
    h->end = i;

    /*
     * ヘッダの後ろ: 空白のあとのコメントは行末まで読み飛ばす。単独の CR はどこでもエラー
     */
    while (i < e)
    {
        c = data[i];
        if (c == '\r')
        {
            return scalar_fail(p, line->line, "carriage return without line feed");
        }
        if ((c == '#') && (i > h->end) && scalar_is_white(data[i - 1]))
        {
            for (; i < e; ++i)
            {
                if (data[i] == '\r')
                {
                    return scalar_fail(p, line->line, "carriage return without line feed");
                }
            }
            break;
        }
        if (!scalar_is_white(c))
        {
            return scalar_fail(p, line->line, "invalid block scalar");
        }
        ++i;
    }

    return true;
}

/*
 * 空白だけの行で、インデントの空白の後ろにタブがあるか（行頭の空白のあとに空白以外が残る）
 */
static bool scalar_blank_has_tab(const mnd_yaml_line_t *line)
{
    return (line->content.start + line->indent) < line->content.end;
}

/*
 * 指示子がないときの内容のインデントを決める: 先行の空白だけの行を読み飛ばし、最初の空でない行が
 * 親より深ければそのインデント。なければ親 + 1 と先行の行の空白の数の大きい方。
 * 先行の行のインデントのタブ、最初の空でない行より深い先行の行はエラー
 */
static bool scalar_block_detect_indent(mnd_yaml_parser_t *p, size_t first, size_t parent_indent,
                                       size_t header_line, size_t *content_indent)
{
    const mnd_yaml_line_t *line = NULL;
    size_t i = first;
    size_t max_lead = 0;

    while ((i < p->lines.count) && p->lines.items[i].blank)
    {
        line = &p->lines.items[i];
        if (scalar_blank_has_tab(line))
        {
            return scalar_fail(p, line->line, "tab character in indentation");
        }
        if (line->indent > max_lead)
        {
            max_lead = line->indent;
        }
        ++i;
    }
    if ((i < p->lines.count) && (p->lines.items[i].indent > parent_indent))
    {
        *content_indent = p->lines.items[i].indent;
        if (max_lead > *content_indent)
        {
            return scalar_fail(p, header_line, "invalid block scalar");
        }
        return true;
    }
    *content_indent = parent_indent + 1;
    if (max_lead > *content_indent)
    {
        *content_indent = max_lead;
    }

    return true;
}

/*
 * 保留した改行を出力する。折り返しで、前後の行がどちらも空白で始まらない行なら、
 * 改行 1 つ（間の空行なし）を空白に、n 個を n - 1 個の改行にする
 */
static bool scalar_block_flush(mnd_buf_t *buf, size_t breaks, bool fold)
{
    if (fold && (breaks > 0))
    {
        return scalar_append_fold(buf, breaks - 1, false);
    }

    return scalar_append_repeat(buf, (unsigned char)'\n', breaks);
}

/*
 * at（| か >）からブロックスカラーを読む。内容の行は、内容のインデント以上の空でない行と、
 * 内容のインデントより深い空白だけの行（内容は空白）。空白だけの行は、内容のインデント以下なら空行。
 * 内容のインデントより浅い空でない行で終わり、カーソルはその行（または範囲の終わり）に進める
 */
static bool scalar_build_block(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent,
                               mnd_buf_t *value, mnd_yaml_node_t **out)
{
    const char *data = p->data;
    const mnd_yaml_line_t *header_line = &p->lines.items[at->line_index];
    const mnd_yaml_line_t *line = NULL;
    const mnd_yaml_line_t *last = NULL;
    scalar_block_header_t h;
    size_t first = at->line_index + 1;
    size_t n = 0;
    size_t i;
    size_t k;
    size_t text;
    size_t breaks = 0;
    size_t last_content = 0;
    size_t end;
    bool has_content = false;
    bool prev_more = false;
    bool more;
    mnd_yaml_cursor_t next;

    if (!scalar_block_read_header(p, header_line, at->offset, &h))
    {
        return false;
    }
    if (h.indicator > 0)
    {
        n = parent_indent + h.indicator;
    }
    else if (!scalar_block_detect_indent(p, first, parent_indent, header_line->line, &n))
    {
        return false;
    }

    for (i = first; i < p->lines.count; ++i)
    {
        line = &p->lines.items[i];
        if (line->blank && (line->indent < n) && scalar_blank_has_tab(line))
        {
            return scalar_fail(p, line->line, "tab character in indentation");
        }
        if (!line->blank && (line->indent < n))
        {
            break;
        }
        if (line->blank && ((line->content.start + n) >= line->content.end))
        {
            /* 空行: 改行を保留する */
            if (line->eol_len > 0)
            {
                ++breaks;
            }
            continue;
        }

        /* 内容の行: 保留した改行を出力してから、内容のインデントのあとを追加する */
        text = line->content.start + n;
        for (k = text; k < line->content.end; ++k)
        {
            if (data[k] == '\r')
            {
                return scalar_fail(p, line->line, "carriage return without line feed");
            }
        }
        more = scalar_is_white(data[text]);
        if (!scalar_block_flush(value, breaks,
                                has_content && (h.style == MND_YAML_FOLDED) && !prev_more && !more)
            || !mnd_buf_append(value, data + text, line->content.end - text))
        {
            mnd_yaml_parser_oom(p);
            return false;
        }
        breaks = (line->eol_len > 0) ? 1 : 0;
        has_content = true;
        prev_more = more;
        last_content = i;
    }

    /*
     * チョンプ: strip は末尾の改行を残さず、clip は最後の内容の行の改行だけを残し、
     * keep は末尾の空行の改行もすべて残す（内容の行がなければ clip も残さない）
     */
    if (h.chomp == MND_YAML_CHOMP_CLIP)
    {
        breaks = (has_content && (breaks > 0)) ? 1 : 0;
    }
    else if (h.chomp == MND_YAML_CHOMP_STRIP)
    {
        breaks = 0;
    }
    if (!scalar_append_repeat(value, (unsigned char)'\n', breaks))
    {
        mnd_yaml_parser_oom(p);
        return false;
    }

    /*
     * 範囲の終わり: 最後の内容の行の終わり。keep では末尾の空行のうち最後の行の終わり。
     * 内容の行がなければヘッダの指示子の直後（keep で空行があれば最後の空行の終わり）
     */
    last = &p->lines.items[i - 1];
    end = h.end;
    if (has_content)
    {
        end = p->lines.items[last_content].content.end;
    }
    if ((h.chomp == MND_YAML_CHOMP_KEEP) && (i > first))
    {
        end = last->content.end;
    }

    /* 消費した最後の行の次の行の先頭（範囲の終わりでは最後の行の行末記号の直後） */
    next.line_index = i;
    next.offset = (i < p->lines.count) ? p->lines.items[i].start : (last->content.end + last->eol_len);
    if (!scalar_finish(p, at, h.style, false, at->offset, end, at->line_index, value, out))
    {
        return false;
    }
    (*out)->as.scalar.chomp = h.chomp;
    (*out)->as.scalar.has_indent_indicator = (h.indicator > 0);
    (*out)->as.scalar.indent_indicator = h.indicator;
    (*out)->as.scalar.content_indent = n;
    *at = next;

    return true;
}

static bool scalar_parse_block(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent,
                               mnd_yaml_node_t **out)
{
    mnd_buf_t value;
    bool ok;

    mnd_buf_init(&value);
    ok = scalar_build_block(p, at, parent_indent, &value, out);
    mnd_buf_free(&value);

    return ok;
}

bool mnd_yaml_scalar_parse(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent, bool flow,
                           mnd_yaml_node_t **out)
{
    const char *data = p->data;
    const mnd_yaml_line_t *line = NULL;
    size_t offset = at->offset;
    size_t end;
    char c;

    if (at->line_index >= p->lines.count)
    {
        return scalar_fail_start(p, mnd_yaml_parser_line_of(p, offset), flow);
    }
    line = &p->lines.items[at->line_index];
    end = line->content.end;
    if (offset >= end)
    {
        return scalar_fail_start(p, line->line, flow);
    }

    c = data[offset];
    switch (c)
    {
    case '&':
        return scalar_fail(p, line->line, "unsupported YAML syntax: anchor");
    case '*':
        return scalar_fail(p, line->line, "unsupported YAML syntax: alias");
    case '!':
        return scalar_fail(p, line->line, "unsupported YAML syntax: tag");
    case '@':
    case '`':
        if (mnd_yaml_parser_claim_error(p))
        {
            mnd_diag_add(p->diags, MND_SEVERITY_ERROR, p->path, line->line,
                         "reserved indicator \"%c\" cannot start a scalar", c);
        }
        return false;
    case '\'':
        return scalar_parse_quoted(p, at, flow, false, out);
    case '"':
        return scalar_parse_quoted(p, at, flow, true, out);
    case '|':
    case '>':
        if (flow)
        {
            return scalar_fail_start(p, line->line, true);
        }
        return scalar_parse_block(p, at, parent_indent, out);
    case '?':
        if (scalar_followed_by_separator(data, offset, end, flow))
        {
            return scalar_fail(p, line->line, "unsupported YAML syntax: complex mapping key");
        }
        break;
    case '-':
    case ':':
        if (scalar_followed_by_separator(data, offset, end, flow))
        {
            return scalar_fail_start(p, line->line, flow);
        }
        break;
    case ',':
    case '[':
    case ']':
    case '{':
    case '}':
    case '#':
    case '%':
    case ' ':
    case '\t':
        return scalar_fail_start(p, line->line, flow);
    case '\r':
        return scalar_fail(p, line->line, "carriage return without line feed");
    default:
        break;
    }

    return scalar_parse_plain(p, at, parent_indent, flow, out);
}

bool mnd_yaml_scalar_is_block_header(const mnd_yaml_parser_t *p, mnd_yaml_cursor_t at)
{
    const mnd_yaml_line_t *line = NULL;

    if (at.line_index >= p->lines.count)
    {
        return false;
    }
    line = &p->lines.items[at.line_index];
    if ((at.offset < line->content.start) || (at.offset >= line->content.end))
    {
        return false;
    }

    return (p->data[at.offset] == '|') || (p->data[at.offset] == '>');
}
