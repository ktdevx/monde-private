/* Formatter: スタイルの維持の判定、各スタイルの整形、ダブルクオートへの切り替え */

#include "yaml/yaml_format.h"

#include "foundation/utf8.h"

#include <stddef.h>
#include <string.h>

/* 文字の分類（判定は読み取りだけで副作用を持たない） */

static bool format_is_white(char c)
{
    return (c == ' ') || (c == '\t');
}

static bool format_is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

static bool format_is_hex_digit(char c)
{
    return format_is_digit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
}

static bool format_is_flow_indicator(char c)
{
    return (c == ',') || (c == '[') || (c == ']') || (c == '{') || (c == '}');
}

/* プレーンスカラーを始められない指示文字（整形の規則の表: -?:,[]{}#&*!|>'"%@ と `） */
static bool format_is_leading_indicator(char c)
{
    return (c != '\0') && (strchr("-?:,[]{}#&*!|>'\"%@`", c) != NULL);
}

/*
 * text[i] から始まる制御文字のバイト数（制御文字でなければ 0）。
 * 制御文字は C0（タブ・LF・CR を含む）、DEL、C1（U+0080〜U+009F。UTF-8 では C2 80〜C2 9F）。
 * text は妥当な UTF-8 であること
 */
static size_t format_control_len(const char *text, size_t len, size_t i)
{
    unsigned char c = (unsigned char)text[i];

    if ((c < 0x20U) || (c == 0x7FU))
    {
        return 1;
    }
    if ((c == 0xC2U) && ((i + 1) < len))
    {
        c = (unsigned char)text[i + 1];
        if ((c >= 0x80U) && (c <= 0x9FU))
        {
            return 2;
        }
    }

    return 0;
}

/* text[0..len) がタブ以外の制御文字（改行と CR を含む）を含むか */
static bool format_has_control_except_tab(const char *text, size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i)
    {
        if ((text[i] != '\t') && (format_control_len(text, len, i) > 0))
        {
            return true;
        }
    }

    return false;
}

static bool format_equals(const char *text, size_t len, const char *word)
{
    return (strlen(word) == len) && (memcmp(text, word, len) == 0);
}

/*
 * text[0..len) が Core Schema で文字列以外に解決される形か:
 * null（null、Null、NULL、~）、真偽値（true、false の 3 つの書き方）、整数（10 進、0o、0x）、
 * 小数（[-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?）、[-+]?.inf、.nan
 */
static bool format_is_core_schema_non_string(const char *text, size_t len)
{
    static const char *const words[] = {
        "null", "Null", "NULL", "~", "true", "True", "TRUE", "false", "False", "FALSE",
        ".nan", ".NaN", ".NAN"
    };
    size_t i = 0;
    size_t j;
    size_t digits = 0;

    for (j = 0; j < (sizeof(words) / sizeof(words[0])); ++j)
    {
        if (format_equals(text, len, words[j]))
        {
            return true;
        }
    }
    if ((len > 2) && (text[0] == '0') && ((text[1] == 'o') || (text[1] == 'x')))
    {
        for (j = 2; j < len; ++j)
        {
            if ((text[1] == 'o') && ((text[j] < '0') || (text[j] > '7')))
            {
                break;
            }
            if ((text[1] == 'x') && !format_is_hex_digit(text[j]))
            {
                break;
            }
        }
        if (j == len)
        {
            return true;
        }
    }
    if ((len > 0) && ((text[0] == '-') || (text[0] == '+')))
    {
        i = 1;
    }
    if (format_equals(text + i, len - i, ".inf") || format_equals(text + i, len - i, ".Inf")
        || format_equals(text + i, len - i, ".INF"))
    {
        return true;
    }
    while ((i < len) && format_is_digit(text[i]))
    {
        ++i;
        ++digits;
    }
    if ((i < len) && (text[i] == '.'))
    {
        ++i;
        while ((i < len) && format_is_digit(text[i]))
        {
            ++i;
            ++digits;
        }
    }
    if (digits == 0)
    {
        return false;
    }
    if ((i < len) && ((text[i] == 'e') || (text[i] == 'E')))
    {
        ++i;
        if ((i < len) && ((text[i] == '-') || (text[i] == '+')))
        {
            ++i;
        }
        if ((i >= len) || !format_is_digit(text[i]))
        {
            return false;
        }
        while ((i < len) && format_is_digit(text[i]))
        {
            ++i;
        }
    }

    return (i == len);
}

/* ASCII の英字の大文字と小文字を区別せずに text[0..len) が word と一致するか */
static bool format_equals_nocase(const char *text, size_t len, const char *word)
{
    size_t i;
    char a;
    char b;

    if (strlen(word) != len)
    {
        return false;
    }
    for (i = 0; i < len; ++i)
    {
        a = text[i];
        b = word[i];
        if ((a >= 'A') && (a <= 'Z'))
        {
            a = (char)(a - 'A' + 'a');
        }
        if (a != b)
        {
            return false;
        }
    }

    return true;
}

/* text[i..len) の先頭から、数字と extra の文字の並びを読み飛ばした位置 */
static size_t format_skip_digits(const char *text, size_t len, size_t i, const char *extra)
{
    while ((i < len) && (format_is_digit(text[i]) || ((text[i] != '\0') && (strchr(extra, text[i]) != NULL))))
    {
        ++i;
    }

    return i;
}

/* text[*i..len) の先頭の 1 桁以上 max 桁以下の数字を読む（*i を進める）。1 桁もなければ false */
static bool format_take_digits(const char *text, size_t len, size_t *i, size_t max)
{
    size_t n = 0;

    while ((*i < len) && (n < max) && format_is_digit(text[*i]))
    {
        ++*i;
        ++n;
    }

    return (n > 0);
}

/*
 * text[0..len) が YAML 1.1 の整数・小数の形か。https://yaml.org/type/int.html と float.html の正規表現より広く、
 * 符号、`_` と `,`（Psych の区切り）、2 進・8 進（0o を含む）・16 進、60 進（1:30、1:30.5）、
 * 指数の符号の省略、小数点の後ろの `.` と `_`、.inf と .nan の大文字小文字の変種を含める
 */
static bool format_is_yaml11_number(const char *text, size_t len)
{
    size_t i = 0;
    size_t start;
    size_t digits;
    bool integral = false;
    bool dot = false;

    if ((len > 0) && ((text[0] == '-') || (text[0] == '+')))
    {
        i = 1;
    }
    if (format_equals_nocase(text + i, len - i, ".inf") || format_equals_nocase(text + i, len - i, ".nan"))
    {
        return true;
    }
    if (((len - i) > 2) && (text[i] == '0'))
    {
        if (text[i + 1] == 'b')
        {
            for (start = i + 2; start < len; ++start)
            {
                if ((text[start] != '0') && (text[start] != '1') && (text[start] != '_') && (text[start] != ','))
                {
                    break;
                }
            }
            return (start == len);
        }
        if (text[i + 1] == 'o')
        {
            for (start = i + 2; start < len; ++start)
            {
                if (((text[start] < '0') || (text[start] > '7')) && (text[start] != '_'))
                {
                    break;
                }
            }
            return (start == len);
        }
        if (text[i + 1] == 'x')
        {
            for (start = i + 2; start < len; ++start)
            {
                if (!format_is_hex_digit(text[start]) && (text[start] != '_') && (text[start] != ','))
                {
                    break;
                }
            }
            return (start == len);
        }
    }
    if ((i < len) && format_is_digit(text[i]))
    {
        integral = true;
        i = format_skip_digits(text, len, i, "_,");
    }
    if (integral && (i < len) && (text[i] == ':'))
    {
        /* 60 進: (:[0-5]?[0-9])+ の後ろに、小数なら .[0-9_]* */
        while ((i < len) && (text[i] == ':'))
        {
            ++i;
            start = i;
            if (!format_take_digits(text, len, &i, 2))
            {
                return false;
            }
            digits = i - start;
            if ((digits == 2) && (text[start] > '5'))
            {
                return false;
            }
        }
        if ((i < len) && (text[i] == '.'))
        {
            i = format_skip_digits(text, len, i + 1, "_");
        }
        return (i == len);
    }
    if ((i < len) && (text[i] == '.'))
    {
        dot = true;
        i = format_skip_digits(text, len, i + 1, "._,");
    }
    if (!integral && !dot)
    {
        return false;
    }
    if ((i < len) && ((text[i] == 'e') || (text[i] == 'E')))
    {
        ++i;
        if ((i < len) && ((text[i] == '-') || (text[i] == '+')))
        {
            ++i;
        }
        if (!format_take_digits(text, len, &i, len))
        {
            return false;
        }
        i = format_skip_digits(text, len, i, "");
    }

    return (i == len);
}

/*
 * text[0..len) が YAML 1.1 の日付と時刻の形か（https://yaml.org/type/timestamp.html。
 * 日付だけの形も月と日の 1 桁を許し、タイムゾーンの前の空白と `+0900` の形を含める）
 */
static bool format_is_yaml11_timestamp(const char *text, size_t len)
{
    size_t i = 0;
    size_t start;

    if (!format_take_digits(text, len, &i, 4) || (i != 4))
    {
        return false;
    }
    if ((i >= len) || (text[i] != '-'))
    {
        return false;
    }
    ++i;
    if (!format_take_digits(text, len, &i, 2) || (i >= len) || (text[i] != '-'))
    {
        return false;
    }
    ++i;
    if (!format_take_digits(text, len, &i, 2))
    {
        return false;
    }
    if (i == len)
    {
        return true;
    }
    if ((text[i] == 'T') || (text[i] == 't'))
    {
        ++i;
    }
    else if (format_is_white(text[i]))
    {
        while ((i < len) && format_is_white(text[i]))
        {
            ++i;
        }
    }
    else
    {
        return false;
    }
    if (!format_take_digits(text, len, &i, 2) || (i >= len) || (text[i] != ':'))
    {
        return false;
    }
    ++i;
    start = i;
    if (!format_take_digits(text, len, &i, 2) || ((i - start) != 2) || (i >= len) || (text[i] != ':'))
    {
        return false;
    }
    ++i;
    start = i;
    if (!format_take_digits(text, len, &i, 2) || ((i - start) != 2))
    {
        return false;
    }
    if ((i < len) && (text[i] == '.'))
    {
        i = format_skip_digits(text, len, i + 1, "");
    }
    while ((i < len) && format_is_white(text[i]))
    {
        ++i;
    }
    if (i == len)
    {
        return true;
    }
    if (text[i] == 'Z')
    {
        return ((i + 1) == len);
    }
    if ((text[i] != '-') && (text[i] != '+'))
    {
        return false;
    }
    ++i;
    if (!format_take_digits(text, len, &i, 2))
    {
        return false;
    }
    if ((i < len) && (text[i] == ':'))
    {
        ++i;
    }
    if (i < len)
    {
        start = i;
        if (!format_take_digits(text, len, &i, 2) || ((i - start) != 2))
        {
            return false;
        }
    }

    return (i == len);
}

/*
 * text[0..len) が YAML 1.1 で文字列以外に解決されうる形か（整形の規則「YAML 1.1 の非文字列の形」）:
 * 真偽値と null（大文字小文字を区別しない。Psych に合わせて広く取る）、整数、小数、日付と時刻、<<、=
 */
static bool format_is_yaml11_non_string(const char *text, size_t len)
{
    static const char *const words[] = {
        "y", "n", "yes", "no", "true", "false", "on", "off", "null", "~", "<<", "="
    };
    size_t j;

    for (j = 0; j < (sizeof(words) / sizeof(words[0])); ++j)
    {
        if (format_equals_nocase(text, len, words[j]))
        {
            return true;
        }
    }

    return format_is_yaml11_number(text, len) || format_is_yaml11_timestamp(text, len);
}

/* text[0..len) が U+2028（E2 80 A8）または U+2029（E2 80 A9）を含むか。U+0085 は C1 の制御文字として別に扱う */
static bool format_has_line_separator(const char *text, size_t len)
{
    size_t i;

    for (i = 0; (i + 2) < len; ++i)
    {
        if (((unsigned char)text[i] == 0xE2U) && ((unsigned char)text[i + 1] == 0x80U)
            && (((unsigned char)text[i + 2] == 0xA8U) || ((unsigned char)text[i + 2] == 0xA9U)))
        {
            return true;
        }
    }

    return false;
}

/*
 * 訳文をプレーンのまま表現できるか（整形の規則の表のプレーンの行）。
 * `: ` と ` #` の空白は、解析と同じく空白とタブの両方とする。
 * 行頭に置かれたとき文書の区切りの行と読まれないよう、`...` の後ろが空白・タブのものも除く
 */
static bool format_plain_keepable(const char *text, size_t len, bool flow)
{
    size_t i;

    if (len == 0)
    {
        return false;
    }
    if (format_has_control_except_tab(text, len))
    {
        return false;
    }
    if (format_is_white(text[0]) || format_is_leading_indicator(text[0]))
    {
        return false;
    }
    if (format_is_white(text[len - 1]) || (text[len - 1] == ':'))
    {
        return false;
    }
    for (i = 0; (i + 1) < len; ++i)
    {
        if ((text[i] == ':') && format_is_white(text[i + 1]))
        {
            return false;
        }
        if (format_is_white(text[i]) && (text[i + 1] == '#'))
        {
            return false;
        }
    }
    if (flow)
    {
        for (i = 0; i < len; ++i)
        {
            if (format_is_flow_indicator(text[i]))
            {
                return false;
            }
        }
    }
    if (format_equals(text, len, "---") || format_equals(text, len, "..."))
    {
        return false;
    }
    if ((len > 3) && (memcmp(text, "...", 3) == 0) && format_is_white(text[3]))
    {
        return false;
    }

    /* YAML 1.1 のパーサとの互換（5.9）: タブ、フロー形式の中の ?、U+2028・U+2029、非文字列の形 */
    if ((memchr(text, '\t', len) != NULL) || (flow && (memchr(text, '?', len) != NULL))
        || format_has_line_separator(text, len) || format_is_yaml11_non_string(text, len))
    {
        return false;
    }

    return !format_is_core_schema_non_string(text, len);
}

/*
 * 訳文をシングルクオートのまま表現できるか（改行・CR・タブ以外の制御文字を含まない。
 * U+0085 は C1 の制御文字に含まれ、U+2028・U+2029 も YAML 1.1 のパーサが改行として読みうるため除く: 5.9）
 */
static bool format_single_keepable(const char *text, size_t len)
{
    if (format_has_line_separator(text, len))
    {
        return false;
    }

    return !format_has_control_except_tab(text, len);
}

/* 各スタイルの整形（out の後ろに追加する。メモリ不足なら false） */

static bool format_emit_single(const char *text, size_t len, mnd_buf_t *out)
{
    size_t i;
    size_t run = 0;

    if (!mnd_buf_append_byte(out, (unsigned char)'\''))
    {
        return false;
    }
    for (i = 0; i < len; ++i)
    {
        if (text[i] == '\'')
        {
            if (!mnd_buf_append(out, text + run, (i + 1) - run))
            {
                return false;
            }
            run = i;
        }
    }
    if (!mnd_buf_append(out, text + run, len - run))
    {
        return false;
    }

    return mnd_buf_append_byte(out, (unsigned char)'\'');
}

/*
 * ダブルクオートの整形: \ と " を \\ と \" に、改行を \n、タブを \t、CR を \r、その他の制御文字を \xXX にする。
 * 非 ASCII はそのまま（C1 の制御文字は、そのコードポイントを \xXX で表す）
 */
static bool format_emit_double(const char *text, size_t len, mnd_buf_t *out)
{
    static const char hex[] = "0123456789ABCDEF";
    char escape[4];
    size_t escape_len;
    size_t control;
    size_t i = 0;
    size_t run = 0;
    unsigned char code;

    if (!mnd_buf_append_byte(out, (unsigned char)'"'))
    {
        return false;
    }
    while (i < len)
    {
        control = format_control_len(text, len, i);
        escape_len = 0;
        if (text[i] == '\\')
        {
            escape[0] = '\\';
            escape[1] = '\\';
            escape_len = 2;
        }
        else if (text[i] == '"')
        {
            escape[0] = '\\';
            escape[1] = '"';
            escape_len = 2;
        }
        else if (control > 0)
        {
            escape[0] = '\\';
            if (text[i] == '\n')
            {
                escape[1] = 'n';
                escape_len = 2;
            }
            else if (text[i] == '\t')
            {
                escape[1] = 't';
                escape_len = 2;
            }
            else if (text[i] == '\r')
            {
                escape[1] = 'r';
                escape_len = 2;
            }
            else
            {
                code = (unsigned char)text[i + control - 1];
                escape[1] = 'x';
                escape[2] = hex[(code >> 4) & 0x0FU];
                escape[3] = hex[code & 0x0FU];
                escape_len = 4;
            }
        }
        if (escape_len == 0)
        {
            ++i;
            continue;
        }
        if (!mnd_buf_append(out, text + run, i - run) || !mnd_buf_append(out, escape, escape_len))
        {
            return false;
        }
        i += (control > 0) ? control : 1;
        run = i;
    }
    if (!mnd_buf_append(out, text + run, len - run))
    {
        return false;
    }

    return mnd_buf_append_byte(out, (unsigned char)'"');
}

/*
 * ブロックスカラーの範囲とヘッダの行の位置（原文から読み取る）。
 * 範囲が内容の行まで続くとき、範囲はヘッダの行のコメントを含む。内容の行がなければ範囲は
 * 指示子の直後で終わり、ヘッダの行の残りは範囲の外にある
 */
typedef struct format_block_source
{
    const char *header;     /* ヘッダの `|` か `>` */
    size_t header_len;      /* 出力するヘッダのバイト数（範囲の中にあるヘッダの行の内容） */
    size_t rest_start;      /* ヘッダの中の指示子の直後（header からのバイト数） */
    bool header_ok;         /* ヘッダの行の範囲の外に、行末記号の前の内容がない */
    bool eol_after;         /* 範囲の直後に行末記号がある（最後の行で文書が終わらない） */
} format_block_source_t;

static void format_block_source_of(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *scalar,
                                   format_block_source_t *src)
{
    const char *data = mnd_yaml_doc_data(doc);
    size_t content_end = mnd_yaml_doc_content(doc).end;
    size_t start = scalar->span.start;
    size_t end = scalar->span.end;
    size_t line_end = start;
    size_t i = start + 1;

    while ((line_end < content_end) && (data[line_end] != '\n') && (data[line_end] != '\r'))
    {
        ++line_end;
    }
    while ((i < line_end) && (data[i] != '\0') && (strchr("-+123456789", data[i]) != NULL))
    {
        ++i;
    }
    src->header = data + start;
    src->rest_start = i - start;
    src->header_ok = (end >= line_end);
    src->header_len = (end >= line_end) ? (line_end - start) : (end - start);
    src->eol_after = (end < content_end);
}

/* text[0..len) の末尾の改行の数 */
static size_t format_trailing_newlines(const char *text, size_t len)
{
    size_t count = 0;

    while ((count < len) && (text[len - 1 - count] == '\n'))
    {
        ++count;
    }

    return count;
}

/*
 * 訳文をリテラル・折り返しのまま表現できるか（整形の規則の表のリテラルと折り返しの行）:
 * CR とタブ・改行以外の制御文字を含まない。U+0085（C1 の制御文字）・U+2028・U+2029 を含まない
 * （YAML 1.1 のパーサが改行として読み、ブロックスカラーの構造が壊れるため: 5.9）。
 * 空白だけの行を含まない。空でない行が 1 つ以上ある。
 * インデント指示子がなければ、最初の空でない行が空白・タブで始まらない。折り返しでは、空白・タブで
 * 始まる行を含まない。末尾の改行の数 t がチョンプと合う: strip は 0、clip は範囲の直後の行末記号の数
 * （通常は 1。文書が最後の行で終わるなら 0）、keep はその数以上。
 * ヘッダの行の範囲の外に内容がある（内容の行のない元のスカラーで、指示子の後ろに空白やコメントがある）
 * ときは、内容の行を置けないため維持できない
 */
static bool format_block_keepable(const mnd_yaml_scalar_t *s, const format_block_source_t *src,
                                  const char *text, size_t len)
{
    size_t eol_count = src->eol_after ? 1U : 0U;
    size_t trailing = format_trailing_newlines(text, len);
    size_t body_len = len - trailing;
    size_t line_start = 0;
    size_t i;
    size_t j;
    bool first = true;
    bool white_only;

    if (!src->header_ok || (body_len == 0))
    {
        return false;
    }
    for (i = 0; i < len; ++i)
    {
        if ((text[i] != '\t') && (text[i] != '\n') && (format_control_len(text, len, i) > 0))
        {
            return false;
        }
    }
    if (format_has_line_separator(text, len))
    {
        return false;
    }
    if (((s->chomp == MND_YAML_CHOMP_STRIP) && (trailing != 0))
        || ((s->chomp == MND_YAML_CHOMP_CLIP) && (trailing != eol_count))
        || ((s->chomp == MND_YAML_CHOMP_KEEP) && (trailing < eol_count)))
    {
        return false;
    }
    for (i = 0; i <= body_len; ++i)
    {
        if ((i < body_len) && (text[i] != '\n'))
        {
            continue;
        }
        if (i > line_start)
        {
            white_only = true;
            for (j = line_start; (j < i) && white_only; ++j)
            {
                white_only = format_is_white(text[j]);
            }
            if (white_only)
            {
                return false;
            }
            if (format_is_white(text[line_start])
                && ((s->style == MND_YAML_FOLDED) || (first && !s->has_indent_indicator)))
            {
                return false;
            }
            first = false;
        }
        line_start = i + 1;
    }

    return true;
}

static bool format_append_spaces(mnd_buf_t *out, size_t count)
{
    static const char spaces[] = "                                ";
    size_t chunk;

    while (count > 0)
    {
        chunk = (count < (sizeof(spaces) - 1)) ? count : (sizeof(spaces) - 1);
        if (!mnd_buf_append(out, spaces, chunk))
        {
            return false;
        }
        count -= chunk;
    }

    return true;
}

static bool format_append_eols(mnd_buf_t *out, const char *eol, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i)
    {
        if (!mnd_buf_append_str(out, eol))
        {
            return false;
        }
    }

    return true;
}

/*
 * リテラル・折り返しの整形（format_block_keepable が true のとき）: ヘッダを原文のまま出し、
 * 訳文の各行を content_indent 個の空白で始めて出力する。行末記号は eol。
 * 先頭の改行 j 個は j 個の空行にする。内容の行の間の改行 k 個は、リテラルでは k - 1 個、
 * 折り返しでは k 個の空行にする。最後の内容の行の行末記号は範囲の外にあるため出力せず、
 * keep では残りの末尾の改行を空行にする
 */
static bool format_emit_block(const mnd_yaml_scalar_t *s, const format_block_source_t *src, const char *eol,
                              const char *text, size_t len, mnd_buf_t *out)
{
    size_t trailing = format_trailing_newlines(text, len);
    size_t body_len = len - trailing;
    size_t breaks = 1;
    size_t i = 0;
    size_t line_end;
    bool has_content = false;

    if (!mnd_buf_append(out, src->header, src->header_len))
    {
        return false;
    }
    while (i < body_len)
    {
        if (text[i] == '\n')
        {
            ++breaks;
            ++i;
            continue;
        }
        if (has_content && (s->style == MND_YAML_FOLDED))
        {
            ++breaks;
        }
        line_end = i;
        while ((line_end < body_len) && (text[line_end] != '\n'))
        {
            ++line_end;
        }
        if (!format_append_eols(out, eol, breaks) || !format_append_spaces(out, s->content_indent)
            || !mnd_buf_append(out, text + i, line_end - i))
        {
            return false;
        }
        has_content = true;
        breaks = 0;
        i = line_end;
    }
    if (s->chomp == MND_YAML_CHOMP_KEEP)
    {
        return format_append_eols(out, eol, trailing - (src->eol_after ? 1U : 0U));
    }

    return true;
}

/*
 * ブロックスカラーの整形。維持できなければダブルクオートに切り替え、範囲の中にあるヘッダの行の
 * コメントを値の後ろに残す（`| # c` → `"..." # c`）
 */
static bool format_block(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *scalar, const char *text, size_t len,
                         mnd_buf_t *out)
{
    const mnd_yaml_scalar_t *s = &scalar->as.scalar;
    format_block_source_t src;
    const char *rest = NULL;
    size_t rest_len;

    format_block_source_of(doc, scalar, &src);
    if (format_block_keepable(s, &src, text, len))
    {
        return format_emit_block(s, &src, (mnd_yaml_doc_eol(doc) == MND_EOL_CRLF) ? "\r\n" : "\n", text, len,
                                 out);
    }
    if (!format_emit_double(text, len, out))
    {
        return false;
    }
    rest = src.header + src.rest_start;
    rest_len = src.header_len - src.rest_start;
    if ((rest_len > 0) && (memchr(rest, '#', rest_len) != NULL))
    {
        return mnd_buf_append(out, rest, rest_len);
    }

    return true;
}

/*
 * 空の範囲（値のないキー）の前に付ける内容。通常は 1 つの空白（`key:` → `key: 訳文`）。
 * フロー形式の `{k}` の値（範囲がキーの直後）では、値の指示文字がないため `: ` を付ける
 */
static const char *format_empty_prefix(const mnd_yaml_node_t *scalar)
{
    const mnd_yaml_node_t *parent = scalar->parent;
    size_t i;

    if (scalar->as.scalar.flow && (parent != NULL) && (parent->kind == MND_YAML_MAPPING))
    {
        for (i = 0; i < parent->as.mapping.count; ++i)
        {
            if ((parent->as.mapping.pairs[i].value == scalar)
                && (parent->as.mapping.pairs[i].key->span.end == scalar->span.start))
            {
                return ": ";
            }
        }
    }

    return " ";
}

bool mnd_yaml_format_value(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *scalar,
                           const char *text, size_t len, const char *path,
                           mnd_buf_t *out, mnd_diag_list_t *diags)
{
    const mnd_yaml_scalar_t *s = &scalar->as.scalar;
    size_t invalid_offset = 0;
    bool ok;

    if (len == 0)
    {
        text = "";
    }
    if (!mnd_utf8_validate(text, len, &invalid_offset))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "invalid UTF-8 sequence");
        return false;
    }
    if ((len > 0) && (memchr(text, '\0', len) != NULL))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "NUL in value");
        return false;
    }

    ok = true;
    if (scalar->span.start == scalar->span.end)
    {
        ok = mnd_buf_append_str(out, format_empty_prefix(scalar));
    }
    if (ok)
    {
        switch (s->style)
        {
        case MND_YAML_PLAIN:
            if (format_plain_keepable(text, len, s->flow))
            {
                ok = mnd_buf_append(out, text, len);
            }
            else
            {
                ok = format_emit_double(text, len, out);
            }
            break;
        case MND_YAML_SINGLE_QUOTED:
            if (format_single_keepable(text, len))
            {
                ok = format_emit_single(text, len, out);
            }
            else
            {
                ok = format_emit_double(text, len, out);
            }
            break;
        case MND_YAML_LITERAL:
        case MND_YAML_FOLDED:
            ok = format_block(doc, scalar, text, len, out);
            break;
        case MND_YAML_DOUBLE_QUOTED:
        default:
            ok = format_emit_double(text, len, out);
            break;
        }
    }
    if (!ok)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "out of memory");
        return false;
    }

    return true;
}
