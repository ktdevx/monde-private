/*
 * html_normalize: CommonMark spec 0.31.2 の test/normalize.py（normalize_html）の C への移植。
 *
 * normalize.py は、入力を正規表現 `(<!\[CDATA\[.*?\]\]>|<[^>]*>|[^<]+)` で塊に分け、`<![CDATA` で
 * 始まる塊はそのまま出力に足し、ほかの塊を Python の html.parser.HTMLParser（convert_charrefs = False）に
 * 1 つずつ渡す。HTMLParser の派生クラスが、受け取った字句ごとに出力を組み立てる。
 * ここでは、塊の分け方、HTMLParser の feed・goahead・close の手順（未完の構文を次の塊まで持ち越す
 * こと、何も読めなかったときに次の解析を遅らせるしきい値を含む）、各字句の認識に使う正規表現の
 * 一致の仕方（後戻りを含む）、派生クラスの各ハンドラを、Python 3.13.18 の html/parser.py に
 * 合わせて再現する。データはハンドラの呼び出しごとに空白をまとめるため、呼び出しの区切りまで
 * 合わせる必要がある。
 *
 * normalize.py との意図した違い:
 * - 実体名は、name2codepoint（HTML4）の代わりに、本モジュールの Entity の表（HTML5。HTML4 の名前を
 *   含む）で文字に戻す（設計のとおり）。HTML4 にない名前（`&ngE;` など）は文字に戻り、HTML4 と
 *   HTML5 で値が異なる `&lang;` と `&rang;` は HTML5 の値になる
 * - 大文字と小文字の変換（タグ名、属性名、`<!doctype` の判定）は ASCII だけを扱う
 * - 数値文字参照の数値の読み取り（Python の int()）は ASCII の数字だけを扱う
 * - Python では例外で止まる入力（値が `href` または `src` の属性、値のある属性と値のない属性が
 *   同じ名前で並ぶ属性の並べ替え、C の int に収まらない数値文字参照）も止めずに扱う。
 *   それぞれ、通常の属性と同じ書き直し、値のない属性を先にする並べ替え、参照をそのまま残す扱いにする
 * - サロゲートの範囲の数値文字参照（Python では単独のサロゲートの文字になる）は、3 バイトの
 *   UTF-8 と同じ形のバイト列にする
 * 入力は正しい UTF-8 を前提とする。再帰は使わない
 */

#include "html_normalize.h"

#include "markdown/md_entity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* MyHTMLParser.last の値 */
typedef enum last_kind
{
    LAST_STARTTAG,
    LAST_ENDTAG,
    LAST_DATA,
    LAST_COMMENT,
    LAST_DECL,
    LAST_PI,
    LAST_REF
} last_kind_t;

/* 属性 1 つ。名前と値は attr_text の中のオフセットで表す */
typedef struct norm_attr
{
    size_t name_off;
    size_t name_len;
    size_t value_off;
    size_t value_len;
    bool has_value; /* false なら Python の None */
} norm_attr_t;

/* HTMLParser と MyHTMLParser の状態 */
typedef struct norm
{
    mnd_buf_t *out;       /* MyHTMLParser.output は out の base バイト目から */
    size_t base;          /* 呼び出し前の out の長さ（rstrip はこれより前を消さない） */
    mnd_buf_t raw;        /* HTMLParser.rawdata（まだ読んでいない入力） */
    mnd_buf_t pending;    /* HTMLParser._pending を連結したもの */
    size_t pending_len;   /* HTMLParser._pending_len（コードポイントの数） */
    size_t threshold;     /* HTMLParser._parse_threshold（コードポイントの数） */
    last_kind_t last;     /* MyHTMLParser.last */
    mnd_buf_t last_tag;   /* MyHTMLParser.last_tag */
    bool in_pre;          /* MyHTMLParser.in_pre */
    bool cdata;           /* HTMLParser.cdata_elem が None でない */
    bool escapable;       /* HTMLParser._escapable（cdata のときだけ意味を持つ） */
    bool plaintext;       /* cdata_elem が plaintext */
    mnd_buf_t cdata_elem; /* HTMLParser.cdata_elem */
    mnd_buf_t tag;        /* 解析中のタグ名（小文字） */
    mnd_buf_t text;       /* ハンドラに渡す文字列の作業領域 */
    mnd_buf_t attr_text;  /* 属性の名前と値のバイト列 */
    norm_attr_t *attrs;
    size_t attr_count;
    size_t attr_cap;
    bool ok; /* 確保に失敗したら false */
} norm_t;

/* ---- 文字の分類 ---- */

static bool is_ascii_alpha(unsigned char c)
{
    return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
}

static bool is_ascii_digit(unsigned char c)
{
    return ((c >= '0') && (c <= '9'));
}

static bool is_ascii_hex(unsigned char c)
{
    return (is_ascii_digit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')));
}

static bool is_ascii_alnum(unsigned char c)
{
    return (is_ascii_alpha(c) || is_ascii_digit(c));
}

static unsigned char ascii_lower(unsigned char c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return (unsigned char)(c - 'A' + 'a');
    }

    return c;
}

/* 正規表現の `[\t\n\r\f ]` */
static bool is_tag_space(unsigned char c)
{
    return ((c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == ' '));
}

/* 正規表現の `[\t\n\r\f /]` */
static bool is_tag_space_or_slash(unsigned char c)
{
    return (is_tag_space(c) || (c == '/'));
}

/* 正規表現の `['"\t\n\r\f /]`（属性名の後読みの条件） */
static bool is_attr_lookbehind(unsigned char c)
{
    return (is_tag_space_or_slash(c) || (c == '\'') || (c == '"'));
}

/*
 * s[i..n) の先頭が Python の空白（str.isspace() が真の文字。Unicode の正規表現の `\s` と同じ）なら、
 * そのバイト数を返す。そうでなければ 0 を返す
 */
static size_t space_len_at(const unsigned char *s, size_t n, size_t i)
{
    unsigned char c = s[i];

    if (((c >= 0x09) && (c <= 0x0d)) || ((c >= 0x1c) && (c <= 0x20)))
    {
        return 1;
    }
    if ((c == 0xc2) && ((i + 1) < n) && ((s[i + 1] == 0x85) || (s[i + 1] == 0xa0)))
    {
        return 2;
    }
    if ((i + 2) >= n)
    {
        return 0;
    }
    if ((c == 0xe1) && (s[i + 1] == 0x9a) && (s[i + 2] == 0x80))
    {
        return 3; /* U+1680 */
    }
    if ((c == 0xe2) && (s[i + 1] == 0x80)
        && ((s[i + 2] <= 0x8a) || (s[i + 2] == 0xa8) || (s[i + 2] == 0xa9) || (s[i + 2] == 0xaf)))
    {
        return 3; /* U+2000..U+200A、U+2028、U+2029、U+202F */
    }
    if ((c == 0xe2) && (s[i + 1] == 0x81) && (s[i + 2] == 0x9f))
    {
        return 3; /* U+205F */
    }
    if ((c == 0xe3) && (s[i + 1] == 0x80) && (s[i + 2] == 0x80))
    {
        return 3; /* U+3000 */
    }

    return 0;
}

/* s[0..end) の末尾が Python の空白なら、そのバイト数を返す。そうでなければ 0 を返す */
static size_t space_len_before(const unsigned char *s, size_t end)
{
    size_t back = 1;

    while ((back <= 3) && (back <= end))
    {
        size_t start = end - back;

        if ((s[start] & 0xc0) != 0x80)
        {
            return (space_len_at(s, end, start) == back) ? back : 0;
        }
        back++;
    }

    return 0;
}

/* s[0..len) のコードポイントの数（Python の len()） */
static size_t code_point_count(const char *s, size_t len)
{
    size_t count = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if ((((unsigned char)s[i]) & 0xc0) != 0x80)
        {
            count++;
        }
    }

    return count;
}

/* ---- 出力の補助 ---- */

static void put(norm_t *p, mnd_buf_t *buf, const void *bytes, size_t len)
{
    if (p->ok && !mnd_buf_append(buf, bytes, len))
    {
        p->ok = false;
    }
}

static void put_str(norm_t *p, mnd_buf_t *buf, const char *str)
{
    put(p, buf, str, strlen(str));
}

/* コードポイントを UTF-8 で追加する（サロゲートも 3 バイトの形にする） */
static void put_code_point(norm_t *p, mnd_buf_t *buf, uint32_t cp)
{
    unsigned char bytes[4];
    size_t len;

    if (cp < 0x80)
    {
        bytes[0] = (unsigned char)cp;
        len = 1;
    }
    else if (cp < 0x800)
    {
        bytes[0] = (unsigned char)(0xc0 | (cp >> 6));
        bytes[1] = (unsigned char)(0x80 | (cp & 0x3f));
        len = 2;
    }
    else if (cp < 0x10000)
    {
        bytes[0] = (unsigned char)(0xe0 | (cp >> 12));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | (cp & 0x3f));
        len = 3;
    }
    else
    {
        bytes[0] = (unsigned char)(0xf0 | (cp >> 18));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[3] = (unsigned char)(0x80 | (cp & 0x3f));
        len = 4;
    }
    put(p, buf, bytes, len);
}

/* self.output = self.output.rstrip() */
static void rstrip_output(norm_t *p)
{
    const unsigned char *s = (const unsigned char *)p->out->data;
    size_t len = p->out->len;
    size_t back;

    if (s == NULL)
    {
        return;
    }
    back = space_len_before(s + p->base, len - p->base);
    while (back > 0)
    {
        len -= back;
        back = space_len_before(s + p->base, len - p->base);
    }
    p->out->len = len;
    p->out->data[len] = '\0';
}

/* MyHTMLParser.output_char: c が 1 文字の `<`、`>`、`&`、`"` ならエスケープし、それ以外はそのまま */
static void output_char(norm_t *p, const unsigned char *c, size_t len)
{
    if ((len == 1) && (c[0] == '<'))
    {
        put_str(p, p->out, "&lt;");
    }
    else if ((len == 1) && (c[0] == '>'))
    {
        put_str(p, p->out, "&gt;");
    }
    else if ((len == 1) && (c[0] == '&'))
    {
        put_str(p, p->out, "&amp;");
    }
    else if ((len == 1) && (c[0] == '"'))
    {
        put_str(p, p->out, "&quot;");
    }
    else
    {
        put(p, p->out, c, len);
    }
}

static bool tag_equals(const char *tag, size_t len, const char *name)
{
    return ((strlen(name) == len) && (memcmp(tag, name, len) == 0));
}

/* MyHTMLParser.is_block_tag */
static bool is_block_tag(const char *tag, size_t len)
{
    static const char *const block_tags[] = {
        "article", "header", "aside", "hgroup", "blockquote", "hr", "iframe", "body", "li", "map", "button",
        "object", "canvas", "ol", "caption", "output", "col", "p", "colgroup", "pre", "dd", "progress", "div",
        "section", "dl", "table", "td", "dt", "tbody", "embed", "textarea", "fieldset", "tfoot", "figcaption",
        "th", "figure", "thead", "footer", "tr", "form", "ul", "h1", "h2", "h3", "h4", "h5", "h6", "video",
        "script", "style",
    };
    size_t i;

    for (i = 0; i < (sizeof(block_tags) / sizeof(block_tags[0])); i++)
    {
        if (tag_equals(tag, len, block_tags[i]))
        {
            return true;
        }
    }

    return false;
}

/* ---- MyHTMLParser のハンドラ ---- */

static void handle_data(norm_t *p, const char *data, size_t len)
{
    bool after_tag = ((p->last == LAST_ENDTAG) || (p->last == LAST_STARTTAG));
    bool after_block_tag = (after_tag && is_block_tag(p->last_tag.data, p->last_tag.len));
    const unsigned char *s = (const unsigned char *)data;
    size_t start = 0;
    size_t i;
    const unsigned char *t;
    size_t tlen;
    size_t back;

    if (after_tag && tag_equals(p->last_tag.data, p->last_tag.len, "br"))
    {
        while ((start < len) && (s[start] == '\n'))
        {
            start++;
        }
    }
    mnd_buf_clear(&p->text);
    if (!p->in_pre)
    {
        /* whitespace_re.sub(' ', data) */
        i = start;
        while (i < len)
        {
            size_t sp = space_len_at(s, len, i);

            if (sp == 0)
            {
                put(p, &p->text, s + i, 1);
                i++;
                continue;
            }
            while ((i < len) && (sp > 0))
            {
                i += sp;
                sp = (i < len) ? space_len_at(s, len, i) : 0;
            }
            put(p, &p->text, " ", 1);
        }
    }
    else
    {
        put(p, &p->text, s + start, len - start);
    }
    if (!p->ok)
    {
        return;
    }
    t = (const unsigned char *)p->text.data;
    tlen = p->text.len;
    start = 0;
    if (after_block_tag && !p->in_pre && (t != NULL))
    {
        /* starttag の後は lstrip()、endtag の後は strip() */
        size_t sp = (tlen > 0) ? space_len_at(t, tlen, 0) : 0;

        while (sp > 0)
        {
            start += sp;
            sp = (start < tlen) ? space_len_at(t, tlen, start) : 0;
        }
        if (p->last == LAST_ENDTAG)
        {
            back = space_len_before(t, tlen);
            while ((back > 0) && (tlen > start))
            {
                tlen -= back;
                back = space_len_before(t, tlen);
            }
        }
    }
    if (t != NULL)
    {
        put(p, p->out, t + start, tlen - start);
    }
    p->last = LAST_DATA;
}

static void set_last_tag(norm_t *p, const char *tag, size_t len)
{
    mnd_buf_clear(&p->last_tag);
    put(p, &p->last_tag, tag, len);
}

static void handle_endtag(norm_t *p, const char *tag, size_t len)
{
    if (tag_equals(tag, len, "pre"))
    {
        p->in_pre = false;
    }
    else if (is_block_tag(tag, len))
    {
        rstrip_output(p);
    }
    put_str(p, p->out, "</");
    put(p, p->out, tag, len);
    put_str(p, p->out, ">");
    set_last_tag(p, tag, len);
    p->last = LAST_ENDTAG;
}

/* 属性 a が b より前なら true（Python のタプルの比較。値のない属性は値のある属性より前とする） */
static bool attr_less(const norm_t *p, const norm_attr_t *a, const norm_attr_t *b)
{
    const char *text = p->attr_text.data;
    size_t common = (a->name_len < b->name_len) ? a->name_len : b->name_len;
    int cmp = memcmp(text + a->name_off, text + b->name_off, common);

    if (cmp != 0)
    {
        return (cmp < 0);
    }
    if (a->name_len != b->name_len)
    {
        return (a->name_len < b->name_len);
    }
    if (!a->has_value || !b->has_value)
    {
        return (!a->has_value && b->has_value);
    }
    common = (a->value_len < b->value_len) ? a->value_len : b->value_len;
    cmp = memcmp(text + a->value_off, text + b->value_off, common);
    if (cmp != 0)
    {
        return (cmp < 0);
    }

    return (a->value_len < b->value_len);
}

/* html.escape(v, quote=True) を出力に追加する */
static void put_escaped_attr(norm_t *p, const char *v, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        switch (v[i])
        {
        case '&':
            put_str(p, p->out, "&amp;");
            break;
        case '<':
            put_str(p, p->out, "&lt;");
            break;
        case '>':
            put_str(p, p->out, "&gt;");
            break;
        case '"':
            put_str(p, p->out, "&quot;");
            break;
        case '\'':
            put_str(p, p->out, "&#x27;");
            break;
        default:
            put(p, p->out, v + i, 1);
            break;
        }
    }
}

static void handle_starttag(norm_t *p, const char *tag, size_t len)
{
    size_t i;

    if (tag_equals(tag, len, "pre"))
    {
        p->in_pre = true;
    }
    if (is_block_tag(tag, len))
    {
        rstrip_output(p);
    }
    put_str(p, p->out, "<");
    put(p, p->out, tag, len);
    /* attrs.sort()（安定な挿入ソート） */
    for (i = 1; i < p->attr_count; i++)
    {
        norm_attr_t key = p->attrs[i];
        size_t j = i;

        while ((j > 0) && attr_less(p, &key, &p->attrs[j - 1]))
        {
            p->attrs[j] = p->attrs[j - 1];
            j--;
        }
        p->attrs[j] = key;
    }
    for (i = 0; i < p->attr_count; i++)
    {
        const norm_attr_t *attr = &p->attrs[i];

        put_str(p, p->out, " ");
        put(p, p->out, p->attr_text.data + attr->name_off, attr->name_len);
        if (attr->has_value)
        {
            put_str(p, p->out, "=\"");
            put_escaped_attr(p, p->attr_text.data + attr->value_off, attr->value_len);
            put_str(p, p->out, "\"");
        }
    }
    put_str(p, p->out, ">");
    set_last_tag(p, tag, len);
    p->last = LAST_STARTTAG;
}

static void handle_comment(norm_t *p, const char *data, size_t len)
{
    put_str(p, p->out, "<!--");
    put(p, p->out, data, len);
    put_str(p, p->out, "-->");
    p->last = LAST_COMMENT;
}

/* handle_decl と unknown_decl */
static void handle_decl(norm_t *p, const char *data, size_t len)
{
    put_str(p, p->out, "<!");
    put(p, p->out, data, len);
    put_str(p, p->out, ">");
    p->last = LAST_DECL;
}

static void handle_pi(norm_t *p, const char *data, size_t len)
{
    put_str(p, p->out, "<?");
    put(p, p->out, data, len);
    put_str(p, p->out, ">");
    p->last = LAST_PI;
}

static void put_fallback_ref(norm_t *p, const char *name, size_t len)
{
    put_str(p, p->out, "&");
    put(p, p->out, name, len);
    put_str(p, p->out, ";");
}

static void handle_entityref(norm_t *p, const char *name, size_t len)
{
    const unsigned char *utf8 = NULL;
    size_t utf8_len = 0;

    if (mnd_md_entity_lookup(name, len, &utf8, &utf8_len))
    {
        output_char(p, utf8, utf8_len);
    }
    else
    {
        put_fallback_ref(p, name, len);
    }
    p->last = LAST_REF;
}

/*
 * Python の int(s, base)（base は 10 または 16）。前後の空白、符号、16 進の `0x` の接頭辞、数字の間の
 * `_` を受け付ける。数字は ASCII だけを扱う。値が 0x10FFFF を超えるときは *too_big を true にする。
 * 読めなければ false を返す
 */
static bool python_int(const char *str, size_t len, unsigned base, bool *negative, uint32_t *value, bool *too_big)
{
    const unsigned char *s = (const unsigned char *)str;
    size_t i = 0;
    size_t end = len;
    size_t sp;
    bool underscore_ok = false;
    bool any_digit = false;

    *negative = false;
    *value = 0;
    *too_big = false;
    sp = (i < end) ? space_len_at(s, end, i) : 0;
    while (sp > 0)
    {
        i += sp;
        sp = (i < end) ? space_len_at(s, end, i) : 0;
    }
    sp = space_len_before(s, end);
    while ((sp > 0) && (end > i))
    {
        end -= sp;
        sp = space_len_before(s, end);
    }
    if ((i < end) && ((s[i] == '+') || (s[i] == '-')))
    {
        *negative = (s[i] == '-');
        i++;
    }
    if ((base == 16) && ((i + 1) < end) && (s[i] == '0') && ((s[i + 1] == 'x') || (s[i + 1] == 'X')))
    {
        i += 2;
        underscore_ok = true;
    }
    while (i < end)
    {
        unsigned char c = s[i];
        unsigned digit;

        if (c == '_')
        {
            if (!underscore_ok)
            {
                return false;
            }
            underscore_ok = false;
            i++;
            continue;
        }
        if (is_ascii_digit(c))
        {
            digit = (unsigned)(c - '0');
        }
        else if ((base == 16) && is_ascii_hex(c))
        {
            digit = (unsigned)(ascii_lower(c) - 'a' + 10);
        }
        else
        {
            return false;
        }
        if (!*too_big)
        {
            *value = (*value * base) + digit;
            if (*value > 0x10ffff)
            {
                *too_big = true;
            }
        }
        any_digit = true;
        underscore_ok = true;
        i++;
    }

    /* 末尾の `_` は不可 */
    return (any_digit && ((end == 0) || (s[end - 1] != '_')));
}

static void handle_charref(norm_t *p, const char *name, size_t len)
{
    bool negative = false;
    bool too_big = false;
    uint32_t value = 0;
    bool ok;

    if ((len > 0) && (name[0] == 'x'))
    {
        ok = python_int(name + 1, len - 1, 16, &negative, &value, &too_big);
    }
    else
    {
        ok = python_int(name, len, 10, &negative, &value, &too_big);
    }
    /* chr() は負の数と 0x10FFFF を超える数で失敗する（-0 は 0） */
    if (ok && !too_big && (!negative || (value == 0)))
    {
        mnd_buf_t *text = &p->text;

        mnd_buf_clear(text);
        put_code_point(p, text, value);
        if (p->ok)
        {
            output_char(p, (const unsigned char *)text->data, text->len);
        }
    }
    else
    {
        put_fallback_ref(p, name, len);
    }
    p->last = LAST_REF;
}

/* ---- 属性の値の参照（HTMLParser._unescape_attrvalue と html.unescape） ---- */

/* `;` のない古い形として html.entities.html5 にある名前 */
static bool is_legacy_entity(const char *name, size_t len)
{
    static const char *const legacy[] = {
        "AElig", "AMP", "Aacute", "Acirc", "Agrave", "Aring", "Atilde", "Auml", "COPY", "Ccedil", "ETH",
        "Eacute", "Ecirc", "Egrave", "Euml", "GT", "Iacute", "Icirc", "Igrave", "Iuml", "LT", "Ntilde",
        "Oacute", "Ocirc", "Ograve", "Oslash", "Otilde", "Ouml", "QUOT", "REG", "THORN", "Uacute", "Ucirc",
        "Ugrave", "Uuml", "Yacute", "aacute", "acirc", "acute", "aelig", "agrave", "amp", "aring", "atilde",
        "auml", "brvbar", "ccedil", "cedil", "cent", "copy", "curren", "deg", "divide", "eacute", "ecirc",
        "egrave", "eth", "euml", "frac12", "frac14", "frac34", "gt", "iacute", "icirc", "iexcl", "igrave",
        "iquest", "iuml", "laquo", "lt", "macr", "micro", "middot", "nbsp", "not", "ntilde", "oacute", "ocirc",
        "ograve", "ordf", "ordm", "oslash", "otilde", "ouml", "para", "plusmn", "pound", "quot", "raquo", "reg",
        "sect", "shy", "sup1", "sup2", "sup3", "szlig", "thorn", "times", "uacute", "ucirc", "ugrave", "uml",
        "uuml", "yacute", "yen", "yuml",
    };
    size_t i;

    for (i = 0; i < (sizeof(legacy) / sizeof(legacy[0])); i++)
    {
        if (tag_equals(name, len, legacy[i]))
        {
            return true;
        }
    }

    return false;
}

/* html.unescape の数値文字参照の置き換え（html/__init__.py の _replace_charref） */
static void put_numeric_unescape(norm_t *p, mnd_buf_t *buf, uint32_t num, bool too_big)
{
    static const uint16_t c1_table[32] = {
        0x20ac, 0x81, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160,
        0x2039, 0x0152, 0x8d, 0x017d, 0x8f, 0x90, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
        0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x9d, 0x017e, 0x0178,
    };

    if (!too_big && (num == 0))
    {
        put_code_point(p, buf, 0xfffd);
    }
    else if (!too_big && (num == 0x0d))
    {
        put_code_point(p, buf, 0x0d);
    }
    else if (!too_big && (num >= 0x80) && (num <= 0x9f))
    {
        put_code_point(p, buf, c1_table[num - 0x80]);
    }
    else if (too_big || ((num >= 0xd800) && (num <= 0xdfff)))
    {
        put_code_point(p, buf, 0xfffd);
    }
    else if (((num >= 0x01) && (num <= 0x08)) || (num == 0x0b) || ((num >= 0x0e) && (num <= 0x1f))
             || ((num >= 0x7f) && (num <= 0x9f)) || ((num >= 0xfdd0) && (num <= 0xfdef))
             || ((num & 0xfffe) == 0xfffe))
    {
        /* _invalid_codepoints は空の文字列になる */
    }
    else
    {
        put_code_point(p, buf, num);
    }
}

/*
 * 属性の値 v[0..len) の参照を、正規表現 `&(#[0-9]+|#[xX][0-9a-fA-F]+|[a-zA-Z][a-zA-Z0-9]*)[;=]?` と
 * _replace_attr_charref の規則で置き換え、attr_text に追加する
 */
static void put_unescaped_attr(norm_t *p, const char *v, size_t len)
{
    const unsigned char *s = (const unsigned char *)v;
    size_t i = 0;

    while (i < len)
    {
        size_t body_start;
        size_t body_end;
        size_t ref_end;
        bool numeric = false;
        bool hex = false;

        if (s[i] != '&')
        {
            put(p, &p->attr_text, s + i, 1);
            i++;
            continue;
        }
        body_start = i + 1;
        body_end = body_start;
        if (((i + 2) < len) && (s[i + 1] == '#') && is_ascii_digit(s[i + 2]))
        {
            numeric = true;
            body_end = i + 2;
            while ((body_end < len) && is_ascii_digit(s[body_end]))
            {
                body_end++;
            }
        }
        else if (((i + 3) < len) && (s[i + 1] == '#') && ((s[i + 2] == 'x') || (s[i + 2] == 'X'))
                 && is_ascii_hex(s[i + 3]))
        {
            numeric = true;
            hex = true;
            body_end = i + 3;
            while ((body_end < len) && is_ascii_hex(s[body_end]))
            {
                body_end++;
            }
        }
        else if (((i + 1) < len) && is_ascii_alpha(s[i + 1]))
        {
            body_end = i + 2;
            while ((body_end < len) && is_ascii_alnum(s[body_end]))
            {
                body_end++;
            }
        }
        else
        {
            put(p, &p->attr_text, s + i, 1);
            i++;
            continue;
        }
        ref_end = body_end;
        if ((ref_end < len) && ((s[ref_end] == ';') || (s[ref_end] == '=')))
        {
            ref_end++;
        }
        if (numeric)
        {
            /* unescape(ref): `;` は参照の一部として消え、`=` は残る */
            size_t digits = hex ? (body_start + 2) : (body_start + 1);
            uint32_t num = 0;
            bool too_big = false;
            size_t k;

            for (k = digits; k < body_end; k++)
            {
                unsigned d = is_ascii_digit(s[k]) ? (unsigned)(s[k] - '0')
                                                  : (unsigned)(ascii_lower(s[k]) - 'a' + 10);

                if (!too_big)
                {
                    num = (num * (hex ? 16u : 10u)) + d;
                    too_big = (num > 0x10ffff);
                }
            }
            put_numeric_unescape(p, &p->attr_text, num, too_big);
            if ((ref_end > body_end) && (s[body_end] == '='))
            {
                put(p, &p->attr_text, "=", 1);
            }
        }
        else
        {
            const unsigned char *utf8 = NULL;
            size_t utf8_len = 0;
            bool semicolon = ((ref_end > body_end) && (s[body_end] == ';'));
            bool equals = ((ref_end > body_end) && (s[body_end] == '='));
            const char *name = v + body_start;
            size_t name_len = body_end - body_start;

            if (!equals && (semicolon || is_legacy_entity(name, name_len))
                && mnd_md_entity_lookup(name, name_len, &utf8, &utf8_len))
            {
                put(p, &p->attr_text, utf8, utf8_len);
            }
            else
            {
                put(p, &p->attr_text, s + i, ref_end - i);
            }
        }
        i = ref_end;
    }
}

/* ---- HTMLParser の字句の認識 ---- */

/*
 * 位置 r から、属性の値の指示 `[\t\n\r\f ]*=[\t\n\r\f ]*('[^']*'|"[^"]*"|(?!['"])[^>\t\n\r\f ]*)` を
 * 後戻りを含めて照合する。一致すれば値の範囲（引用符を含む）と一致の終わりを書いて true を返す
 */
static bool match_attr_value(const char *raw, size_t n, size_t r, size_t *value_start, size_t *value_end,
                             size_t *match_end)
{
    size_t q = r;
    size_t after_eq;
    size_t m;
    size_t pos;

    while ((q < n) && is_tag_space((unsigned char)raw[q]))
    {
        q++;
    }
    if ((q >= n) || (raw[q] != '='))
    {
        return false;
    }
    after_eq = q + 1;
    m = after_eq;
    while ((m < n) && is_tag_space((unsigned char)raw[m]))
    {
        m++;
    }
    /* `=` の後の空白は、長い方から試す。空白の位置では引用符のない空の値が一致する */
    pos = m;
    for (;;)
    {
        if ((pos < n) && ((raw[pos] == '\'') || (raw[pos] == '"')))
        {
            const char *close = memchr(raw + pos + 1, raw[pos], n - pos - 1);

            if (close != NULL)
            {
                *value_start = pos;
                *value_end = (size_t)(close - raw) + 1;
                *match_end = *value_end;

                return true;
            }
        }
        else
        {
            size_t e = pos;

            while ((e < n) && (raw[e] != '>') && !is_tag_space((unsigned char)raw[e]))
            {
                e++;
            }
            *value_start = pos;
            *value_end = e;
            *match_end = e;

            return true;
        }
        if (pos == after_eq)
        {
            return false;
        }
        pos--;
    }
}

/*
 * 位置 i（`[a-zA-Z]` の上）から属性名を照合する（`(?<=['"\t\n\r\f /])[^\t\n\r\f />][^\t\n\r\f /=>]*`）。
 * 一致すれば名前の終わりを返し、一致しなければ i を返す
 */
static size_t match_attr_name(const char *raw, size_t n, size_t i)
{
    size_t e;

    if ((i == 0) || (i >= n) || !is_attr_lookbehind((unsigned char)raw[i - 1])
        || is_tag_space_or_slash((unsigned char)raw[i]) || (raw[i] == '>'))
    {
        return i;
    }
    e = i + 1;
    while ((e < n) && !is_tag_space_or_slash((unsigned char)raw[e]) && (raw[e] != '=') && (raw[e] != '>'))
    {
        e++;
    }

    return e;
}

/* タグ名 `[a-zA-Z][^\t\n\r\f />]*` の終わり（start は英字の上） */
static size_t tag_name_end(const char *raw, size_t n, size_t start)
{
    size_t e = start + 1;

    while ((e < n) && !is_tag_space_or_slash((unsigned char)raw[e]) && (raw[e] != '>'))
    {
        e++;
    }

    return e;
}

/* 正規表現 locatetagend を start（英字の上）から照合し、一致の終わりを返す */
static size_t locate_tag_end(const char *raw, size_t n, size_t start)
{
    size_t pos = tag_name_end(raw, n, start);

    while ((pos < n) && is_tag_space_or_slash((unsigned char)raw[pos]))
    {
        pos++;
    }
    for (;;)
    {
        size_t name_end = match_attr_name(raw, n, pos);
        size_t value_start;
        size_t value_end;
        size_t match_end;

        if (name_end == pos)
        {
            break;
        }
        pos = name_end;
        if (match_attr_value(raw, n, pos, &value_start, &value_end, &match_end))
        {
            pos = match_end;
        }
        while ((pos < n) && is_tag_space_or_slash((unsigned char)raw[pos]))
        {
            pos++;
        }
    }
    if ((pos < n) && (raw[pos] == '>'))
    {
        pos++;
    }

    return pos;
}

/* `(?:[\t\n\r\f ]|/(?!>))*` の終わり */
static size_t skip_space_or_lone_slash(const char *raw, size_t n, size_t pos)
{
    while (pos < n)
    {
        if (is_tag_space((unsigned char)raw[pos]))
        {
            pos++;
        }
        else if ((raw[pos] == '/') && (((pos + 1) >= n) || (raw[pos + 1] != '>')))
        {
            pos++;
        }
        else
        {
            break;
        }
    }

    return pos;
}

/* 解析中のタグ名を小文字にして p->tag に入れる */
static void set_tag(norm_t *p, const char *raw, size_t start, size_t end)
{
    size_t i;

    mnd_buf_clear(&p->tag);
    for (i = start; i < end; i++)
    {
        unsigned char c = ascii_lower((unsigned char)raw[i]);

        put(p, &p->tag, &c, 1);
    }
}

static bool add_attr(norm_t *p, const char *raw, size_t name_start, size_t name_end, bool has_value,
                     size_t value_start, size_t value_end)
{
    norm_attr_t *attr;
    size_t i;

    if (p->attr_count == p->attr_cap)
    {
        size_t cap = (p->attr_cap == 0) ? 8 : (p->attr_cap * 2);
        norm_attr_t *grown = realloc(p->attrs, cap * sizeof(*grown));

        if (grown == NULL)
        {
            p->ok = false;

            return false;
        }
        p->attrs = grown;
        p->attr_cap = cap;
    }
    attr = &p->attrs[p->attr_count];
    attr->name_off = p->attr_text.len;
    for (i = name_start; i < name_end; i++)
    {
        unsigned char c = ascii_lower((unsigned char)raw[i]);

        put(p, &p->attr_text, &c, 1);
    }
    attr->name_len = name_end - name_start;
    attr->has_value = has_value;
    attr->value_off = p->attr_text.len;
    if (has_value)
    {
        size_t vs = value_start;
        size_t ve = value_end;

        if (((ve - vs) >= 2) && ((raw[vs] == '\'') || (raw[vs] == '"')) && (raw[ve - 1] == raw[vs]))
        {
            vs++;
            ve--;
        }
        put_unescaped_attr(p, raw + vs, ve - vs);
    }
    attr->value_len = p->attr_text.len - attr->value_off;
    p->attr_count++;

    return p->ok;
}

static void set_cdata_mode(norm_t *p, bool escapable)
{
    p->cdata = true;
    p->escapable = escapable;
    p->plaintext = tag_equals(p->tag.data, p->tag.len, "plaintext");
    mnd_buf_clear(&p->cdata_elem);
    put(p, &p->cdata_elem, p->tag.data, p->tag.len);
}

static void clear_cdata_mode(norm_t *p)
{
    p->cdata = false;
    p->escapable = true;
    p->plaintext = false;
}

/* parse_starttag。完全なタグでなければ false を返す */
static bool parse_starttag(norm_t *p, const char *raw, size_t n, size_t i, size_t *next)
{
    size_t endpos = locate_tag_end(raw, n, i + 1);
    size_t name_end;
    size_t k;
    size_t a;
    size_t b;

    if (raw[endpos - 1] != '>')
    {
        return false;
    }
    *next = endpos;
    name_end = tag_name_end(raw, n, i + 1);
    set_tag(p, raw, i + 1, name_end);
    k = skip_space_or_lone_slash(raw, n, name_end);
    p->attr_count = 0;
    mnd_buf_clear(&p->attr_text);
    while (k < endpos)
    {
        size_t attr_name_end = match_attr_name(raw, n, k);
        size_t value_start = 0;
        size_t value_end = 0;
        size_t match_end = attr_name_end;
        bool has_value;

        if (attr_name_end == k)
        {
            break;
        }
        has_value = match_attr_value(raw, n, attr_name_end, &value_start, &value_end, &match_end);
        if (!add_attr(p, raw, k, attr_name_end, has_value, value_start, value_end))
        {
            return true;
        }
        k = skip_space_or_lone_slash(raw, n, has_value ? match_end : attr_name_end);
    }
    /* end = rawdata[k:endpos].strip() */
    a = (k < endpos) ? k : endpos;
    b = endpos;
    while ((a < b) && (space_len_at((const unsigned char *)raw, b, a) > 0))
    {
        a += space_len_at((const unsigned char *)raw, b, a);
    }
    while ((b > a) && (space_len_before((const unsigned char *)raw, b) > 0))
    {
        b -= space_len_before((const unsigned char *)raw, b);
    }
    if (((b - a) == 1) && (raw[a] == '>'))
    {
        handle_starttag(p, p->tag.data, p->tag.len);
        if (tag_equals(p->tag.data, p->tag.len, "script") || tag_equals(p->tag.data, p->tag.len, "style")
            || tag_equals(p->tag.data, p->tag.len, "xmp") || tag_equals(p->tag.data, p->tag.len, "iframe")
            || tag_equals(p->tag.data, p->tag.len, "noembed") || tag_equals(p->tag.data, p->tag.len, "noframes")
            || tag_equals(p->tag.data, p->tag.len, "plaintext"))
        {
            set_cdata_mode(p, false);
        }
        else if (tag_equals(p->tag.data, p->tag.len, "textarea") || tag_equals(p->tag.data, p->tag.len, "title"))
        {
            set_cdata_mode(p, true);
        }
    }
    else if (((b - a) == 2) && (raw[a] == '/') && (raw[a + 1] == '>'))
    {
        /* MyHTMLParser.handle_startendtag */
        handle_starttag(p, p->tag.data, p->tag.len);
        set_last_tag(p, p->tag.data, p->tag.len);
        p->last = LAST_ENDTAG;
    }
    else
    {
        handle_data(p, raw + i, endpos - i);
    }

    return true;
}

/* parse_bogus_comment */
static bool parse_bogus_comment(norm_t *p, const char *raw, size_t n, size_t i, size_t *next)
{
    const char *gt = memchr(raw + i + 2, '>', n - i - 2);

    if (gt == NULL)
    {
        return false;
    }
    handle_comment(p, raw + i + 2, (size_t)(gt - raw) - i - 2);
    *next = (size_t)(gt - raw) + 1;

    return true;
}

/* parse_endtag */
static bool parse_endtag(norm_t *p, const char *raw, size_t n, size_t i, size_t *next)
{
    size_t j;

    if (memchr(raw + i + 2, '>', n - i - 2) == NULL)
    {
        return false;
    }
    if (!is_ascii_alpha((unsigned char)raw[i + 2]))
    {
        if (raw[i + 2] == '>')
        {
            *next = i + 3;

            return true;
        }

        return parse_bogus_comment(p, raw, n, i, next);
    }
    j = locate_tag_end(raw, n, i + 2);
    if (raw[j - 1] != '>')
    {
        return false;
    }
    set_tag(p, raw, i + 2, tag_name_end(raw, n, i + 2));
    handle_endtag(p, p->tag.data, p->tag.len);
    clear_cdata_mode(p);
    *next = j;

    return true;
}

/* parse_comment */
static bool parse_comment(norm_t *p, const char *raw, size_t n, size_t i, size_t *next)
{
    size_t s = i + 4;
    size_t k;

    if ((s < n) && (raw[s] == '>'))
    {
        handle_comment(p, raw + s, 0);
        *next = s + 1;

        return true;
    }
    if (((s + 1) < n) && (raw[s] == '-') && (raw[s + 1] == '>'))
    {
        handle_comment(p, raw + s, 0);
        *next = s + 2;

        return true;
    }
    for (k = s; (k + 2) < n; k++)
    {
        if ((raw[k] == '-') && (raw[k + 1] == '-'))
        {
            if (raw[k + 2] == '>')
            {
                handle_comment(p, raw + s, k - s);
                *next = k + 3;

                return true;
            }
            if (((k + 3) < n) && (raw[k + 2] == '!') && (raw[k + 3] == '>'))
            {
                handle_comment(p, raw + s, k - s);
                *next = k + 4;

                return true;
            }
        }
    }

    return false;
}

/* parse_pi */
static bool parse_pi(norm_t *p, const char *raw, size_t n, size_t i, size_t *next)
{
    const char *gt = memchr(raw + i + 2, '>', n - i - 2);

    if (gt == NULL)
    {
        return false;
    }
    handle_pi(p, raw + i + 2, (size_t)(gt - raw) - i - 2);
    *next = (size_t)(gt - raw) + 1;

    return true;
}

static bool starts_with(const char *raw, size_t n, size_t i, const char *prefix)
{
    size_t len = strlen(prefix);

    return (((n - i) >= len) && (memcmp(raw + i, prefix, len) == 0));
}

/* rawdata[i:i+9].lower() == '<!doctype' */
static bool starts_with_doctype(const char *raw, size_t n, size_t i)
{
    static const char doctype[] = "<!doctype";
    size_t k;

    if ((n - i) < 9)
    {
        return false;
    }
    for (k = 0; k < 9; k++)
    {
        if (ascii_lower((unsigned char)raw[i + k]) != (unsigned char)doctype[k])
        {
            return false;
        }
    }

    return true;
}

/* parse_html_declaration（`<!--` は呼び出し側が先に扱う） */
static bool parse_html_declaration(norm_t *p, const char *raw, size_t n, size_t i, size_t *next)
{
    if (starts_with(raw, n, i, "<![CDATA["))
    {
        size_t k;

        for (k = i + 9; (k + 2) < n; k++)
        {
            if ((raw[k] == ']') && (raw[k + 1] == ']') && (raw[k + 2] == '>'))
            {
                handle_decl(p, raw + i + 3, k - i - 3);
                *next = k + 3;

                return true;
            }
        }

        return false;
    }
    if (starts_with_doctype(raw, n, i))
    {
        const char *gt = memchr(raw + i + 9, '>', n - i - 9);

        if (gt == NULL)
        {
            return false;
        }
        handle_decl(p, raw + i + 2, (size_t)(gt - raw) - i - 2);
        *next = (size_t)(gt - raw) + 1;

        return true;
    }

    return parse_bogus_comment(p, raw, n, i, next);
}

/* `<` の構文が入力の終わりまでに閉じなかったときの goahead(1) の扱い */
static void handle_unclosed_markup(norm_t *p, const char *raw, size_t n, size_t i)
{
    if (((i + 1) < n) && is_ascii_alpha((unsigned char)raw[i + 1]))
    {
        /* 閉じない開始タグは捨てられる */
    }
    else if (starts_with(raw, n, i, "</"))
    {
        if ((i + 2) == n)
        {
            handle_data(p, "</", 2);
        }
        else if (is_ascii_alpha((unsigned char)raw[i + 2]))
        {
            /* 閉じない終了タグは捨てられる */
        }
        else
        {
            handle_comment(p, raw + i + 2, n - i - 2);
        }
    }
    else if (starts_with(raw, n, i, "<!--"))
    {
        size_t j = n;
        size_t rest = n - i - 4;

        if ((rest >= 3) && (memcmp(raw + n - 3, "--!", 3) == 0))
        {
            j -= 3;
        }
        else if ((rest >= 2) && (memcmp(raw + n - 2, "--", 2) == 0))
        {
            j -= 2;
        }
        else if ((rest >= 1) && (raw[n - 1] == '-'))
        {
            j -= 1;
        }
        handle_comment(p, raw + i + 4, j - i - 4);
    }
    else if (starts_with(raw, n, i, "<![CDATA["))
    {
        handle_decl(p, raw + i + 3, n - i - 3);
    }
    else if (starts_with_doctype(raw, n, i))
    {
        handle_decl(p, raw + i + 2, n - i - 2);
    }
    else if (starts_with(raw, n, i, "<!"))
    {
        handle_comment(p, raw + i + 2, n - i - 2);
    }
    else
    {
        /* `<?` */
        handle_pi(p, raw + i + 2, n - i - 2);
    }
}

/*
 * self.interesting.search(rawdata, i)。見つかれば位置を *j に書いて true を返す。
 * 通常は `[&<]`。cdata のときは `</elem(?=[\t\n\r\f />])`（ASCII の大文字小文字を区別しない）で、
 * エスケープできる要素では `&` も含む。plaintext では `\Z`（入力の終わり）
 */
static bool find_interesting(const norm_t *p, const char *raw, size_t n, size_t i, size_t *j)
{
    size_t k;

    if (!p->cdata)
    {
        for (k = i; k < n; k++)
        {
            if ((raw[k] == '&') || (raw[k] == '<'))
            {
                *j = k;

                return true;
            }
        }

        return false;
    }
    if (p->plaintext)
    {
        *j = n;

        return true;
    }
    for (k = i; k < n; k++)
    {
        if (p->escapable && (raw[k] == '&'))
        {
            *j = k;

            return true;
        }
        if ((raw[k] == '<') && ((k + 2 + p->cdata_elem.len) < n) && (raw[k + 1] == '/'))
        {
            size_t e;
            bool match = true;

            for (e = 0; e < p->cdata_elem.len; e++)
            {
                if (ascii_lower((unsigned char)raw[k + 2 + e]) != (unsigned char)p->cdata_elem.data[e])
                {
                    match = false;
                    break;
                }
            }
            if (match && (is_tag_space_or_slash((unsigned char)raw[k + 2 + p->cdata_elem.len])
                          || (raw[k + 2 + p->cdata_elem.len] == '>')))
            {
                *j = k;

                return true;
            }
        }
    }

    return false;
}

/* HTMLParser.goahead。読み終えた分を rawdata から除く */
static void goahead(norm_t *p, bool end)
{
    const char *raw = p->raw.data;
    size_t n = p->raw.len;
    size_t i = 0;

    while (p->ok && (i < n))
    {
        size_t j;
        size_t k = 0;

        if (!find_interesting(p, raw, n, i, &j))
        {
            if (p->cdata)
            {
                break;
            }
            j = n;
        }
        if (i < j)
        {
            handle_data(p, raw + i, j - i);
        }
        i = j;
        if (i == n)
        {
            break;
        }
        if (raw[i] == '<')
        {
            bool complete;

            if (((i + 1) < n) && is_ascii_alpha((unsigned char)raw[i + 1]))
            {
                complete = parse_starttag(p, raw, n, i, &k);
            }
            else if (starts_with(raw, n, i, "</"))
            {
                complete = parse_endtag(p, raw, n, i, &k);
            }
            else if (starts_with(raw, n, i, "<!--"))
            {
                complete = parse_comment(p, raw, n, i, &k);
            }
            else if (starts_with(raw, n, i, "<?"))
            {
                complete = parse_pi(p, raw, n, i, &k);
            }
            else if (starts_with(raw, n, i, "<!"))
            {
                complete = parse_html_declaration(p, raw, n, i, &k);
            }
            else if (((i + 1) < n) || end)
            {
                handle_data(p, "<", 1);
                complete = true;
                k = i + 1;
            }
            else
            {
                break;
            }
            if (!complete)
            {
                if (!end)
                {
                    break;
                }
                handle_unclosed_markup(p, raw, n, i);
                k = n;
            }
            i = k;
        }
        else if (starts_with(raw, n, i, "&#"))
        {
            size_t q = i + 2;
            bool matched = false;

            /* charref: `&#(?:[0-9]+|[xX][0-9a-fA-F]+)[^0-9a-fA-F]` */
            if ((q < n) && is_ascii_digit((unsigned char)raw[q]))
            {
                while ((q < n) && is_ascii_digit((unsigned char)raw[q]))
                {
                    q++;
                }
                matched = ((q < n) && !is_ascii_hex((unsigned char)raw[q]));
            }
            else if (((q + 1) < n) && ((raw[q] == 'x') || (raw[q] == 'X')) && is_ascii_hex((unsigned char)raw[q + 1]))
            {
                q++;
                while ((q < n) && is_ascii_hex((unsigned char)raw[q]))
                {
                    q++;
                }
                matched = ((q < n) && !is_ascii_hex((unsigned char)raw[q]));
            }
            if (matched)
            {
                handle_charref(p, raw + i + 2, q - i - 2);
                i = (raw[q] == ';') ? (q + 1) : q;
                continue;
            }
            /* incomplete_charref: `&#(?:[0-9]|[xX][0-9a-fA-F])` */
            if ((((i + 2) < n) && is_ascii_digit((unsigned char)raw[i + 2]))
                || (((i + 3) < n) && ((raw[i + 2] == 'x') || (raw[i + 2] == 'X'))
                    && is_ascii_hex((unsigned char)raw[i + 3])))
            {
                if (end)
                {
                    handle_charref(p, raw + i + 2, n - i - 2);
                    i = n;
                }
                break;
            }
            else if (code_point_count(raw + i, ((n - i) < 16) ? (n - i) : 16) > 3)
            {
                /* Python の `i + 3 < n` はコードポイントで数える */
                handle_data(p, "&#", 2);
                i += 2;
            }
            else
            {
                break;
            }
        }
        else
        {
            /* entityref: `&([a-zA-Z][-.a-zA-Z0-9]*)[^a-zA-Z0-9]`（後戻りを含む） */
            size_t name_start = i + 1;
            size_t q;
            size_t name_end = 0;
            bool matched = false;

            if ((name_start < n) && is_ascii_alpha((unsigned char)raw[name_start]))
            {
                q = name_start + 1;
                while ((q < n) && (is_ascii_alnum((unsigned char)raw[q]) || (raw[q] == '-') || (raw[q] == '.')))
                {
                    q++;
                }
                if (q < n)
                {
                    name_end = q;
                    matched = true;
                }
                else
                {
                    size_t l;

                    for (l = q - 1; l > name_start; l--)
                    {
                        if ((raw[l] == '-') || (raw[l] == '.'))
                        {
                            name_end = l;
                            matched = true;
                            break;
                        }
                    }
                }
            }
            if (matched)
            {
                handle_entityref(p, raw + name_start, name_end - name_start);
                i = (raw[name_end] == ';') ? (name_end + 1) : name_end;
                continue;
            }
            /* incomplete: `&[a-zA-Z#]` */
            if ((name_start < n) && is_ascii_alpha((unsigned char)raw[name_start]))
            {
                if (end)
                {
                    handle_entityref(p, raw + name_start, n - name_start);
                    i = n;
                }
                break;
            }
            else if ((i + 1) < n)
            {
                handle_data(p, "&", 1);
                i++;
            }
            else
            {
                break;
            }
        }
    }
    if (p->ok && end && (i < n))
    {
        handle_data(p, raw + i, n - i);
        i = n;
    }
    /* self.rawdata = rawdata[i:] */
    if (i > 0)
    {
        memmove(p->raw.data, p->raw.data + i, n - i);
        p->raw.len = n - i;
        p->raw.data[p->raw.len] = '\0';
    }
}

/* HTMLParser.feed */
static void feed(norm_t *p, const char *data, size_t len)
{
    size_t before;

    p->pending_len += code_point_count(data, len);
    if (p->pending_len < p->threshold)
    {
        put(p, &p->pending, data, len);

        return;
    }
    put(p, &p->raw, p->pending.data, p->pending.len);
    put(p, &p->raw, data, len);
    mnd_buf_clear(&p->pending);
    p->pending_len = 0;
    if (!p->ok)
    {
        return;
    }
    before = p->raw.len;
    goahead(p, false);
    if (p->raw.len < before)
    {
        p->threshold = 1;
    }
    else
    {
        p->threshold = code_point_count(p->raw.data, p->raw.len);
    }
}

/* HTMLParser.close */
static void close_parser(norm_t *p)
{
    put(p, &p->raw, p->pending.data, p->pending.len);
    mnd_buf_clear(&p->pending);
    p->pending_len = 0;
    if (p->ok)
    {
        goahead(p, true);
    }
}

bool mnd_md_html_normalize(const char *html, size_t len, mnd_buf_t *out)
{
    norm_t p;
    size_t pos = 0;
    bool ok;

    memset(&p, 0, sizeof(p));
    p.out = out;
    p.base = out->len;
    mnd_buf_init(&p.raw);
    mnd_buf_init(&p.pending);
    mnd_buf_init(&p.last_tag);
    mnd_buf_init(&p.cdata_elem);
    mnd_buf_init(&p.tag);
    mnd_buf_init(&p.text);
    mnd_buf_init(&p.attr_text);
    p.threshold = 1;
    p.last = LAST_STARTTAG;
    p.escapable = true;
    p.ok = true;
    /* 出力が空でも data を持たせ、rstrip が扱えるようにする */
    put(&p, out, "", 0);
    if (!mnd_buf_reserve(out, 1))
    {
        p.ok = false;
    }

    /* 塊に分ける: `(<!\[CDATA\[.*?\]\]>|<[^>]*>|[^<]+)`。どれにも一致しない `<` は捨てられる */
    while (p.ok && (pos < len))
    {
        if (html[pos] == '<')
        {
            const char *gt;
            size_t end = 0;

            if (starts_with(html, len, pos, "<![CDATA["))
            {
                size_t k;

                for (k = pos + 9; ((k + 2) < len) && (html[k] != '\n'); k++)
                {
                    if ((html[k] == ']') && (html[k + 1] == ']') && (html[k + 2] == '>'))
                    {
                        end = k + 3;
                        break;
                    }
                }
            }
            if (end == 0)
            {
                gt = memchr(html + pos + 1, '>', len - pos - 1);
                end = (gt != NULL) ? ((size_t)(gt - html) + 1) : 0;
            }
            if ((end != 0) && starts_with(html, end, pos, "<![CDATA"))
            {
                /* `<![CDATA` で始まる塊は、パーサに渡さずそのまま出力に足す */
                put(&p, out, html + pos, end - pos);
                pos = end;
            }
            else if (end != 0)
            {
                feed(&p, html + pos, end - pos);
                pos = end;
            }
            else
            {
                pos++;
            }
        }
        else
        {
            const char *lt = memchr(html + pos, '<', len - pos);
            size_t end = (lt != NULL) ? (size_t)(lt - html) : len;

            feed(&p, html + pos, end - pos);
            pos = end;
        }
    }
    if (p.ok)
    {
        close_parser(&p);
    }
    ok = p.ok;
    mnd_buf_free(&p.raw);
    mnd_buf_free(&p.pending);
    mnd_buf_free(&p.last_tag);
    mnd_buf_free(&p.cdata_elem);
    mnd_buf_free(&p.tag);
    mnd_buf_free(&p.text);
    mnd_buf_free(&p.attr_text);
    free(p.attrs);

    return ok;
}
