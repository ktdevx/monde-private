/* Html: HTML の字句と、HTML ブロックの開始・終了条件 */

#include "markdown/md.h"
#include "markdown/md_html.h"

#include <string.h>

/* 種類 1 のタグ名（開始条件と終了条件） */
static const char *const raw_tag_names[] =
{
    "pre",
    "script",
    "style",
    "textarea"
};

/* 種類 6 のタグ名（spec 0.31.2 の一覧） */
static const char *const block_tag_names[] =
{
    "address",
    "article",
    "aside",
    "base",
    "basefont",
    "blockquote",
    "body",
    "caption",
    "center",
    "col",
    "colgroup",
    "dd",
    "details",
    "dialog",
    "dir",
    "div",
    "dl",
    "dt",
    "fieldset",
    "figcaption",
    "figure",
    "footer",
    "form",
    "frame",
    "frameset",
    "h1",
    "h2",
    "h3",
    "h4",
    "h5",
    "h6",
    "head",
    "header",
    "hr",
    "html",
    "iframe",
    "legend",
    "li",
    "link",
    "main",
    "menu",
    "menuitem",
    "nav",
    "noframes",
    "ol",
    "optgroup",
    "option",
    "p",
    "param",
    "search",
    "section",
    "summary",
    "table",
    "tbody",
    "td",
    "tfoot",
    "th",
    "thead",
    "title",
    "tr",
    "track",
    "ul"
};

static bool is_ascii_letter(char c)
{
    return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
}

static bool is_ascii_digit(char c)
{
    return ((c >= '0') && (c <= '9'));
}

static bool is_space_or_tab(char c)
{
    return ((c == ' ') || (c == '\t'));
}

static char ascii_lower(char c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return (char)(c - 'A' + 'a');
    }

    return c;
}

/* data[pos..end) が s で始まるなら真（大文字小文字を区別する） */
static bool starts_with(const char *data, size_t pos, size_t end, const char *s)
{
    size_t len = strlen(s);

    return ((pos <= end) && ((end - pos) >= len) && (memcmp(data + pos, s, len) == 0));
}

/* data[pos..pos+len) が小文字の name と ASCII の大文字小文字を区別せずに等しいなら真 */
static bool equals_ignore_case(const char *data, size_t pos, size_t len, const char *name)
{
    size_t i;

    if (strlen(name) != len)
    {
        return false;
    }

    for (i = 0; i < len; i++)
    {
        if (ascii_lower(data[pos + i]) != name[i])
        {
            return false;
        }
    }

    return true;
}

/* data[pos..pos+len) が names のどれかと大文字小文字を区別せずに等しいなら真 */
static bool in_names(const char *data, size_t pos, size_t len, const char *const *names, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (equals_ignore_case(data, pos, len, names[i]))
        {
            return true;
        }
    }

    return false;
}

/* data[pos..end) から s を探し、見つかればその直後の位置を、なければ 0 を返す（pos より後ろは 0 にならない） */
static size_t find_after(const char *data, size_t pos, size_t end, const char *s)
{
    size_t len = strlen(s);
    size_t i = pos;

    while ((i <= end) && ((end - i) >= len))
    {
        if (memcmp(data + i, s, len) == 0)
        {
            return i + len;
        }

        i++;
    }

    return 0;
}

/*
 * 空白（spaces, tabs, and up to one line ending）を飛ばした位置。行末記号（LF、CRLF、CR）は
 * 1 つまで。2 つ目の行末記号の手前で止まる
 */
static size_t skip_whitespace(const char *data, size_t pos, size_t end)
{
    size_t i = pos;
    bool line_ending = false;

    while (i < end)
    {
        if (is_space_or_tab(data[i]))
        {
            i++;
        }
        else if (((data[i] == '\n') || (data[i] == '\r')) && !line_ending)
        {
            line_ending = true;

            if ((data[i] == '\r') && ((i + 1) < end) && (data[i + 1] == '\n'))
            {
                i++;
            }

            i++;
        }
        else
        {
            break;
        }
    }

    return i;
}

/* タグ名（ASCII の英字と、英字・数字・`-`）の終わりの位置。タグ名でなければ pos */
static size_t scan_tag_name(const char *data, size_t pos, size_t end)
{
    size_t i = pos;

    if ((i >= end) || !is_ascii_letter(data[i]))
    {
        return pos;
    }

    i++;

    while ((i < end) && (is_ascii_letter(data[i]) || is_ascii_digit(data[i]) || (data[i] == '-')))
    {
        i++;
    }

    return i;
}

/* 属性の名前（`[A-Za-z_:][A-Za-z0-9_.:-]*`）の終わりの位置。名前でなければ pos */
static size_t scan_attribute_name(const char *data, size_t pos, size_t end)
{
    size_t i = pos;

    if ((i >= end) || !(is_ascii_letter(data[i]) || (data[i] == '_') || (data[i] == ':')))
    {
        return pos;
    }

    i++;

    while ((i < end) && (is_ascii_letter(data[i]) || is_ascii_digit(data[i]) || (data[i] == '_')
                         || (data[i] == '.') || (data[i] == ':') || (data[i] == '-')))
    {
        i++;
    }

    return i;
}

/* 引用符のない属性の値に使えない文字 */
static bool is_unquoted_value_excluded(char c)
{
    return (is_space_or_tab(c) || (c == '\n') || (c == '\r') || (c == '"') || (c == '\'')
            || (c == '=') || (c == '<') || (c == '>') || (c == '`'));
}

/* 属性の値（引用符なし、`'`、`"`）の終わりの位置。値でなければ pos */
static size_t scan_attribute_value(const char *data, size_t pos, size_t end)
{
    size_t i = pos;

    if (i >= end)
    {
        return pos;
    }

    if ((data[i] == '"') || (data[i] == '\''))
    {
        char quote = data[i];

        i++;
        while ((i < end) && (data[i] != quote))
        {
            i++;
        }

        if (i >= end)
        {
            return pos;
        }

        return i + 1;
    }

    while ((i < end) && !is_unquoted_value_excluded(data[i]))
    {
        i++;
    }

    return i;
}

/*
 * 属性（空白、名前、省略できる値の指定）の終わりの位置。属性でなければ pos。
 * 値の指定（空白、`=`、空白、値）は、`=` がなければ省略として名前の直後で終わる。
 * `=` があって値がなければ属性ではない（そのあとに続けられる字句がないため、タグにもならない）
 */
static size_t scan_attribute(const char *data, size_t pos, size_t end)
{
    size_t name_start = skip_whitespace(data, pos, end);
    size_t name_end;
    size_t eq;
    size_t value_start;
    size_t value_end;

    if (name_start == pos)
    {
        return pos;
    }

    name_end = scan_attribute_name(data, name_start, end);
    if (name_end == name_start)
    {
        return pos;
    }

    eq = skip_whitespace(data, name_end, end);
    if ((eq >= end) || (data[eq] != '='))
    {
        return name_end;
    }

    value_start = skip_whitespace(data, eq + 1, end);
    value_end = scan_attribute_value(data, value_start, end);
    if (value_end == value_start)
    {
        return pos;
    }

    return value_end;
}

size_t mnd_md_html_open_tag(const char *data, size_t pos, size_t end)
{
    size_t i;
    size_t next;

    if ((pos >= end) || (data[pos] != '<'))
    {
        return 0;
    }

    i = scan_tag_name(data, pos + 1, end);
    if (i == (pos + 1))
    {
        return 0;
    }

    for (;;)
    {
        next = scan_attribute(data, i, end);
        if (next == i)
        {
            break;
        }

        i = next;
    }

    i = skip_whitespace(data, i, end);
    if ((i < end) && (data[i] == '/'))
    {
        i++;
    }

    if ((i >= end) || (data[i] != '>'))
    {
        return 0;
    }

    return (i + 1) - pos;
}

size_t mnd_md_html_closing_tag(const char *data, size_t pos, size_t end)
{
    size_t i;

    if (!starts_with(data, pos, end, "</"))
    {
        return 0;
    }

    i = scan_tag_name(data, pos + 2, end);
    if (i == (pos + 2))
    {
        return 0;
    }

    i = skip_whitespace(data, i, end);
    if ((i >= end) || (data[i] != '>'))
    {
        return 0;
    }

    return (i + 1) - pos;
}

size_t mnd_md_html_comment(const char *data, size_t pos, size_t end)
{
    size_t after;

    if (!starts_with(data, pos, end, "<!--"))
    {
        return 0;
    }

    if (starts_with(data, pos + 4, end, ">"))
    {
        return 5;
    }

    if (starts_with(data, pos + 4, end, "->"))
    {
        return 6;
    }

    after = find_after(data, pos + 4, end, "-->");
    if (after == 0)
    {
        return 0;
    }

    return after - pos;
}

size_t mnd_md_html_processing_instruction(const char *data, size_t pos, size_t end)
{
    size_t after;

    if (!starts_with(data, pos, end, "<?"))
    {
        return 0;
    }

    after = find_after(data, pos + 2, end, "?>");
    if (after == 0)
    {
        return 0;
    }

    return after - pos;
}

size_t mnd_md_html_declaration(const char *data, size_t pos, size_t end)
{
    size_t after;

    if (!starts_with(data, pos, end, "<!") || ((pos + 2) >= end) || !is_ascii_letter(data[pos + 2]))
    {
        return 0;
    }

    after = find_after(data, pos + 3, end, ">");
    if (after == 0)
    {
        return 0;
    }

    return after - pos;
}

size_t mnd_md_html_cdata(const char *data, size_t pos, size_t end)
{
    size_t after;

    if (!starts_with(data, pos, end, "<![CDATA["))
    {
        return 0;
    }

    after = find_after(data, pos + 9, end, "]]>");
    if (after == 0)
    {
        return 0;
    }

    return after - pos;
}

size_t mnd_md_html_tag(const char *data, size_t pos, size_t end)
{
    size_t len;

    if ((pos >= end) || (data[pos] != '<'))
    {
        return 0;
    }

    len = mnd_md_html_open_tag(data, pos, end);
    if (len == 0)
    {
        len = mnd_md_html_closing_tag(data, pos, end);
    }

    if (len == 0)
    {
        len = mnd_md_html_comment(data, pos, end);
    }

    if (len == 0)
    {
        len = mnd_md_html_processing_instruction(data, pos, end);
    }

    if (len == 0)
    {
        len = mnd_md_html_cdata(data, pos, end);
    }

    if (len == 0)
    {
        len = mnd_md_html_declaration(data, pos, end);
    }

    return len;
}

/* `<` の直後の英字と数字の並び（種類 1 と 6 のタグ名の候補）の終わりの位置 */
static size_t scan_alnum(const char *data, size_t pos, size_t end)
{
    size_t i = pos;

    while ((i < end) && (is_ascii_letter(data[i]) || is_ascii_digit(data[i])))
    {
        i++;
    }

    return i;
}

/* 種類 1: `<pre`、`<script`、`<style`、`<textarea` の後ろに空白、タブ、`>`、または行末 */
static bool is_start_condition_1(const char *data, size_t pos, size_t end)
{
    size_t name_end = scan_alnum(data, pos + 1, end);

    if (!in_names(data, pos + 1, name_end - (pos + 1), raw_tag_names,
                  sizeof(raw_tag_names) / sizeof(raw_tag_names[0])))
    {
        return false;
    }

    return ((name_end >= end) || is_space_or_tab(data[name_end]) || (data[name_end] == '>'));
}

/* 種類 6: `<` か `</` と一覧のタグ名の後ろに空白、タブ、行末、`>`、または `/>` */
static bool is_start_condition_6(const char *data, size_t pos, size_t end)
{
    size_t name_start = pos + 1;
    size_t name_end;

    if ((name_start < end) && (data[name_start] == '/'))
    {
        name_start++;
    }

    name_end = scan_alnum(data, name_start, end);
    if (!in_names(data, name_start, name_end - name_start, block_tag_names,
                  sizeof(block_tag_names) / sizeof(block_tag_names[0])))
    {
        return false;
    }

    return ((name_end >= end) || is_space_or_tab(data[name_end]) || (data[name_end] == '>')
            || starts_with(data, name_end, end, "/>"));
}

/* data[pos..end) が空白とタブだけ（空を含む）なら真 */
static bool rest_is_blank(const char *data, size_t pos, size_t end)
{
    size_t i;

    for (i = pos; i < end; i++)
    {
        if (!is_space_or_tab(data[i]))
        {
            return false;
        }
    }

    return true;
}

/*
 * 種類 7: 完全な開始タグ（種類 1 のタグ名を除く）か終了タグの後ろに空白とタブだけ。
 * end は行末なので、タグの中の空白に行末記号は現れない
 */
static bool is_start_condition_7(const char *data, size_t pos, size_t end)
{
    size_t len = mnd_md_html_open_tag(data, pos, end);

    if (len != 0)
    {
        size_t name_end = scan_tag_name(data, pos + 1, end);

        if (in_names(data, pos + 1, name_end - (pos + 1), raw_tag_names,
                     sizeof(raw_tag_names) / sizeof(raw_tag_names[0])))
        {
            return false;
        }
    }
    else
    {
        len = mnd_md_html_closing_tag(data, pos, end);
    }

    if (len == 0)
    {
        return false;
    }

    return rest_is_blank(data, pos + len, end);
}

bool mnd_md_html_block_start(const char *data, size_t pos, size_t end, mnd_md_html_start_t *out)
{
    int type = 0;

    if ((pos < end) && (data[pos] == '<'))
    {
        if (is_start_condition_1(data, pos, end))
        {
            type = 1;
        }
        else if (starts_with(data, pos, end, "<!--"))
        {
            type = 2;
        }
        else if (starts_with(data, pos, end, "<?"))
        {
            type = 3;
        }
        else if (starts_with(data, pos, end, "<!") && ((pos + 2) < end) && is_ascii_letter(data[pos + 2]))
        {
            type = 4;
        }
        else if (starts_with(data, pos, end, "<![CDATA["))
        {
            type = 5;
        }
        else if (is_start_condition_6(data, pos, end))
        {
            type = 6;
        }
        else if (is_start_condition_7(data, pos, end))
        {
            type = 7;
        }
    }

    out->type = type;
    out->interrupts_paragraph = ((type != 0) && (type != 7));

    return (type != 0);
}

/* 行に `</pre>`、`</script>`、`</style>`、`</textarea>` のどれか（大文字小文字を区別しない）があれば真 */
static bool contains_raw_end_tag(const char *data, size_t pos, size_t end)
{
    size_t i;

    for (i = pos; i < end; i++)
    {
        if (starts_with(data, i, end, "</"))
        {
            size_t name_end = scan_alnum(data, i + 2, end);

            if ((name_end < end) && (data[name_end] == '>')
                && in_names(data, i + 2, name_end - (i + 2), raw_tag_names,
                            sizeof(raw_tag_names) / sizeof(raw_tag_names[0])))
            {
                return true;
            }
        }
    }

    return false;
}

bool mnd_md_html_block_end(const char *data, size_t pos, size_t end, int type)
{
    bool result = false;

    if (pos > end)
    {
        return false;
    }

    switch (type)
    {
    case 1:
        result = contains_raw_end_tag(data, pos, end);
        break;
    case 2:
        result = (find_after(data, pos, end, "-->") != 0);
        break;
    case 3:
        result = (find_after(data, pos, end, "?>") != 0);
        break;
    case 4:
        result = (find_after(data, pos, end, ">") != 0);
        break;
    case 5:
        result = (find_after(data, pos, end, "]]>") != 0);
        break;
    default:
        result = false;
        break;
    }

    return result;
}
