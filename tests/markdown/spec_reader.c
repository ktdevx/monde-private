/*
 * spec_reader: spec.txt から例を取り出す。
 * 規則は commonmark-spec（タグ 0.31.2）の test/spec_tests.py の get_tests と同じ:
 * - 行は LF で区切り、前後の空白を除いた行を比べる
 * - 「32 個のバッククォート + " example"」で Markdown の状態に入る（どの状態でも）
 * - HTML の状態で「32 個のバッククォート」なら例を 1 つ閉じ、番号を 1 から順に振る
 * - 「.」で HTML の状態に入る（どの状態でも）
 * - それ以外の行は、Markdown／HTML の状態ならその内容に、通常の状態で行頭が `#+ ` なら
 *   その行から `#+ ` をすべて取り除き前後の空白を除いたものを節の名前にする
 * - Markdown と HTML の `→`（U+2192）をタブに置き換える
 */

#include "spec_reader.h"

#include "foundation/file_io.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 例の開始と終了の行のバッククォートの数 */
#define MND_MD_SPEC_FENCE_LEN 32u

/* `→`（U+2192）の UTF-8 */
static const char mnd_md_spec_arrow[3] = { '\xE2', '\x86', '\x92' };

/* spec_tests.py の state */
typedef enum mnd_md_spec_state
{
    MND_MD_SPEC_STATE_TEXT,
    MND_MD_SPEC_STATE_MARKDOWN,
    MND_MD_SPEC_STATE_HTML
} mnd_md_spec_state_t;

/* Python の str.strip() が除く ASCII の空白か */
static bool mnd_md_spec_is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\v') || (c == '\f')
        || (c == '\x1c') || (c == '\x1d') || (c == '\x1e') || (c == '\x1f');
}

/* Python の str.strip() が除く U+0085 と U+00A0 以上の空白の UTF-8 の長さ（空白でなければ 0）*/
static size_t mnd_md_spec_unicode_space_len(const char *s, size_t len)
{
    static const char *const spaces[] = {
        "\xC2\x85", "\xC2\xA0", "\xE1\x9A\x80",
        "\xE2\x80\x80", "\xE2\x80\x81", "\xE2\x80\x82", "\xE2\x80\x83", "\xE2\x80\x84",
        "\xE2\x80\x85", "\xE2\x80\x86", "\xE2\x80\x87", "\xE2\x80\x88", "\xE2\x80\x89",
        "\xE2\x80\x8A", "\xE2\x80\xA8", "\xE2\x80\xA9", "\xE2\x80\xAF", "\xE2\x81\x9F",
        "\xE3\x80\x80"
    };
    size_t i;

    for (i = 0; i < (sizeof(spaces) / sizeof(spaces[0])); i++)
    {
        size_t n = strlen(spaces[i]);

        if ((n <= len) && (memcmp(s, spaces[i], n) == 0))
        {
            return n;
        }
    }

    return 0;
}

/* 末尾が Python の str.strip() の Unicode の空白なら、その UTF-8 の長さ（でなければ 0） */
static size_t mnd_md_spec_unicode_space_len_back(const char *s, size_t len)
{
    size_t n;

    for (n = 2; n <= 3; n++)
    {
        if ((n <= len) && (mnd_md_spec_unicode_space_len(s + (len - n), n) == n))
        {
            return n;
        }
    }

    return 0;
}

/* s[0..*len) の前後の空白を除く（Python の str.strip()）。先頭の位置を返し、*len を更新する */
static const char *mnd_md_spec_strip(const char *s, size_t *len)
{
    size_t n = *len;
    bool changed = true;

    while (changed)
    {
        size_t u;

        changed = false;
        if ((n > 0) && mnd_md_spec_is_space(s[0]))
        {
            s++;
            n--;
            changed = true;
        }
        else
        {
            u = mnd_md_spec_unicode_space_len(s, n);
            if (u > 0)
            {
                s += u;
                n -= u;
                changed = true;
            }
        }
    }

    changed = true;
    while (changed)
    {
        size_t u;

        changed = false;
        if ((n > 0) && mnd_md_spec_is_space(s[n - 1]))
        {
            n--;
            changed = true;
        }
        else
        {
            u = mnd_md_spec_unicode_space_len_back(s, n);
            if (u > 0)
            {
                n -= u;
                changed = true;
            }
        }
    }

    *len = n;

    return s;
}

/* s[0..len) が 32 個のバッククォートで始まるか */
static bool mnd_md_spec_starts_with_fence(const char *s, size_t len)
{
    size_t i;

    if (len < MND_MD_SPEC_FENCE_LEN)
    {
        return false;
    }

    for (i = 0; i < MND_MD_SPEC_FENCE_LEN; i++)
    {
        if (s[i] != '`')
        {
            return false;
        }
    }

    return true;
}

/* 前後の空白を除いた行が「32 個のバッククォート + " example"」か */
static bool mnd_md_spec_is_example_start(const char *s, size_t len)
{
    static const char suffix[] = " example";
    size_t suffix_len = sizeof(suffix) - 1;

    return (len == (MND_MD_SPEC_FENCE_LEN + suffix_len))
        && mnd_md_spec_starts_with_fence(s, len)
        && (memcmp(s + MND_MD_SPEC_FENCE_LEN, suffix, suffix_len) == 0);
}

/* 前後の空白を除いた行が「32 個のバッククォート」か */
static bool mnd_md_spec_is_example_end(const char *s, size_t len)
{
    return (len == MND_MD_SPEC_FENCE_LEN) && mnd_md_spec_starts_with_fence(s, len);
}

/* 行が `#+ ` で始まるか（re.match('#+ ', line)） */
static bool mnd_md_spec_is_header(const char *line, size_t len)
{
    size_t i = 0;

    while ((i < len) && (line[i] == '#'))
    {
        i++;
    }

    return (i > 0) && (i < len) && (line[i] == ' ');
}

/* 空の内容を NUL 終端の文字列として確保する（data を NULL にしない） */
static bool mnd_md_spec_buf_prepare(mnd_buf_t *buf)
{
    mnd_buf_init(buf);

    return mnd_buf_reserve(buf, 0);
}

/* line[0..len) を `→` をタブに置き換えて buf に追加する */
static bool mnd_md_spec_append_tabbed(mnd_buf_t *buf, const char *line, size_t len)
{
    size_t start = 0;
    size_t i = 0;

    while (i < len)
    {
        if (((len - i) >= sizeof(mnd_md_spec_arrow))
            && (memcmp(line + i, mnd_md_spec_arrow, sizeof(mnd_md_spec_arrow)) == 0))
        {
            if (!mnd_buf_append(buf, line + start, i - start) || !mnd_buf_append_byte(buf, '\t'))
            {
                return false;
            }
            i += sizeof(mnd_md_spec_arrow);
            start = i;
        }
        else
        {
            i++;
        }
    }

    return mnd_buf_append(buf, line + start, len - start);
}

/* 見出しの行 line[0..len) から `#+ ` をすべて取り除き（re.sub）、前後の空白を除いて section に入れる */
static bool mnd_md_spec_set_section(mnd_buf_t *section, const char *line, size_t len)
{
    mnd_buf_t removed = MND_BUF_INIT;
    const char *stripped;
    size_t stripped_len;
    size_t i = 0;
    bool ok = true;

    while (ok && (i < len))
    {
        if (line[i] == '#')
        {
            size_t j = i;

            while ((j < len) && (line[j] == '#'))
            {
                j++;
            }
            if ((j < len) && (line[j] == ' '))
            {
                i = j + 1;
            }
            else
            {
                ok = mnd_buf_append(&removed, line + i, j - i);
                i = j;
            }
        }
        else
        {
            ok = mnd_buf_append_byte(&removed, (unsigned char)line[i]);
            i++;
        }
    }

    if (ok)
    {
        stripped_len = removed.len;
        stripped = mnd_md_spec_strip(mnd_buf_cstr(&removed), &stripped_len);
        mnd_buf_clear(section);
        ok = mnd_buf_append(section, stripped, stripped_len);
    }
    mnd_buf_free(&removed);

    return ok;
}

/* 例 1 つ分の文字列を解放する */
static void mnd_md_spec_example_free(mnd_md_spec_example_t *ex)
{
    mnd_buf_free(&ex->section);
    mnd_buf_free(&ex->markdown);
    mnd_buf_free(&ex->html);
}

/*
 * 例を末尾に追加する。成功すると markdown と html の所有権は spec に移り、
 * 呼び出し側の markdown と html は空の新しいバッファになる
 */
static bool mnd_md_spec_push(mnd_md_spec_t *spec, const mnd_buf_t *section,
                             mnd_buf_t *markdown, mnd_buf_t *html,
                             size_t start_line, size_t end_line)
{
    mnd_md_spec_example_t *ex;
    mnd_buf_t section_copy;
    mnd_buf_t next_markdown;
    mnd_buf_t next_html;

    if (spec->count == spec->cap)
    {
        size_t new_cap = (spec->cap != 0) ? spec->cap : 16u;
        mnd_md_spec_example_t *items;

        if (spec->cap != 0)
        {
            if (spec->cap > ((SIZE_MAX / sizeof(*items)) / 2u))
            {
                return false;
            }
            new_cap = spec->cap * 2u;
        }

        items = (mnd_md_spec_example_t *)realloc(spec->examples, new_cap * sizeof(*items));
        if (items == NULL)
        {
            return false;
        }
        spec->examples = items;
        spec->cap = new_cap;
    }

    if (!mnd_md_spec_buf_prepare(&section_copy))
    {
        return false;
    }
    if (!mnd_buf_append(&section_copy, section->data, section->len)
        || !mnd_md_spec_buf_prepare(&next_markdown))
    {
        mnd_buf_free(&section_copy);

        return false;
    }
    if (!mnd_md_spec_buf_prepare(&next_html))
    {
        mnd_buf_free(&section_copy);
        mnd_buf_free(&next_markdown);

        return false;
    }

    ex = &spec->examples[spec->count];
    ex->number = spec->count + 1;
    ex->start_line = start_line;
    ex->end_line = end_line;
    ex->section = section_copy;
    ex->markdown = *markdown;
    ex->html = *html;
    spec->count++;

    *markdown = next_markdown;
    *html = next_html;

    return true;
}

void mnd_md_spec_init(mnd_md_spec_t *spec)
{
    spec->examples = NULL;
    spec->count = 0;
    spec->cap = 0;
}

bool mnd_md_spec_parse(const char *text, size_t len, mnd_md_spec_t *spec)
{
    mnd_md_spec_state_t state = MND_MD_SPEC_STATE_TEXT;
    mnd_buf_t section = MND_BUF_INIT;
    mnd_buf_t markdown = MND_BUF_INIT;
    mnd_buf_t html = MND_BUF_INIT;
    size_t line_number = 0;
    size_t start_line = 0;
    size_t pos = 0;
    bool ok;

    mnd_md_spec_free(spec);

    ok = mnd_md_spec_buf_prepare(&section)
        && mnd_md_spec_buf_prepare(&markdown)
        && mnd_md_spec_buf_prepare(&html);

    while (ok && (pos < len))
    {
        const char *line = text + pos;
        const char *nl = (const char *)memchr(line, '\n', len - pos);
        size_t line_len = (nl != NULL) ? ((size_t)(nl - line) + 1u) : (len - pos);
        size_t l_len = line_len;
        const char *l = mnd_md_spec_strip(line, &l_len);

        pos += line_len;
        line_number++;

        if (mnd_md_spec_is_example_start(l, l_len))
        {
            state = MND_MD_SPEC_STATE_MARKDOWN;
        }
        else if ((state == MND_MD_SPEC_STATE_HTML) && mnd_md_spec_is_example_end(l, l_len))
        {
            state = MND_MD_SPEC_STATE_TEXT;
            ok = mnd_md_spec_push(spec, &section, &markdown, &html, start_line, line_number);
            start_line = 0;
        }
        else if ((l_len == 1) && (l[0] == '.'))
        {
            state = MND_MD_SPEC_STATE_HTML;
        }
        else if (state == MND_MD_SPEC_STATE_MARKDOWN)
        {
            if (start_line == 0)
            {
                start_line = line_number - 1;
            }
            ok = mnd_md_spec_append_tabbed(&markdown, line, line_len);
        }
        else if (state == MND_MD_SPEC_STATE_HTML)
        {
            ok = mnd_md_spec_append_tabbed(&html, line, line_len);
        }
        else if (mnd_md_spec_is_header(line, line_len))
        {
            ok = mnd_md_spec_set_section(&section, line, line_len);
        }
    }

    mnd_buf_free(&section);
    mnd_buf_free(&markdown);
    mnd_buf_free(&html);

    if (!ok)
    {
        mnd_md_spec_free(spec);
    }

    return ok;
}

bool mnd_md_spec_load(const char *path, mnd_md_spec_t *spec, mnd_diag_list_t *diags)
{
    mnd_buf_t content = MND_BUF_INIT;
    bool had_bom = false;
    bool ok;

    mnd_md_spec_free(spec);

    if (!mnd_file_read(path, &content, &had_bom, diags))
    {
        return false;
    }

    ok = mnd_md_spec_parse(content.data, content.len, spec);
    if (!ok)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "out of memory while reading spec examples");
    }
    mnd_buf_free(&content);

    return ok;
}

void mnd_md_spec_free(mnd_md_spec_t *spec)
{
    size_t i;

    for (i = 0; i < spec->count; i++)
    {
        mnd_md_spec_example_free(&spec->examples[i]);
    }
    free(spec->examples);
    mnd_md_spec_init(spec);
}
