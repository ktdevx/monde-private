/* spec_reader: spec.txt からの例の取り出し */

#include <naos/naos.h>

#include "spec_reader.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 例の開始と終了の行（32 個のバッククォート） */
#define FENCE "````````````````````````````````"

/* spec.txt（タグ 0.31.2）の大きさと例の数 */
#define SPEC_FILE_SIZE 205025u
#define SPEC_EXAMPLE_COUNT 652u

/* NUL 終端の text を取り出し、成功することを確かめる */
static void parse_str(const char *text, mnd_md_spec_t *spec)
{
    NAOS_REQUIRE_TRUE(mnd_md_spec_parse(text, strlen(text), spec));
}

/* 実際の spec.txt を読み込み、成功することを確かめる */
static void load_spec_file(mnd_md_spec_t *spec)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = mnd_md_spec_load(MND_MD_SPEC_FILE, spec, &diags);

    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
}

NAOS_TEST(spec_reader_spec_file_path_is_defined)
{
    const char *path = MND_MD_SPEC_FILE;
    const char *suffix = "tests/data/commonmark/spec.txt";
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);

    NAOS_REQUIRE_TRUE(path_len >= suffix_len);
    NAOS_CHECK_STR_EQ(path + (path_len - suffix_len), suffix);
}

NAOS_TEST(spec_reader_spec_file_size_and_no_cr)
{
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;
    bool ok = mnd_file_read(MND_MD_SPEC_FILE, &content, &had_bom, &diags);

    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(SPEC_FILE_SIZE, content.len);
    NAOS_CHECK_NULL(memchr(mnd_buf_cstr(&content), '\r', content.len));
    mnd_buf_free(&content);
}

NAOS_TEST(spec_reader_spec_file_has_652_examples)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    size_t i;

    load_spec_file(&spec);
    NAOS_CHECK_UINT_EQ(SPEC_EXAMPLE_COUNT, spec.count);
    for (i = 0; i < spec.count; i++)
    {
        NAOS_CHECK_UINT_EQ(i + 1, spec.examples[i].number);
    }
    NAOS_REQUIRE_UINT_GT(spec.count, 0);
    NAOS_CHECK_UINT_EQ(SPEC_EXAMPLE_COUNT, spec.examples[spec.count - 1].number);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_spec_file_first_example_has_tabs)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    const mnd_md_spec_example_t *ex;

    load_spec_file(&spec);
    NAOS_REQUIRE_UINT_GT(spec.count, 0);
    ex = &spec.examples[0];
    NAOS_CHECK_UINT_EQ(1, ex->number);
    NAOS_CHECK_NOT_NULL(memchr(ex->markdown.data, '\t', ex->markdown.len));
    NAOS_CHECK_STR_EQ("\tfoo\tbaz\t\tbim\n", ex->markdown.data);
    NAOS_CHECK_STR_EQ("<pre><code>foo\tbaz\t\tbim\n</code></pre>\n", ex->html.data);
    NAOS_CHECK_STR_EQ("Tabs", ex->section.data);
    NAOS_CHECK_UINT_EQ(355, ex->start_line);
    NAOS_CHECK_UINT_EQ(360, ex->end_line);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_spec_file_last_example)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    const mnd_md_spec_example_t *ex;

    load_spec_file(&spec);
    NAOS_REQUIRE_UINT_EQ(SPEC_EXAMPLE_COUNT, spec.count);
    ex = &spec.examples[spec.count - 1];
    NAOS_CHECK_STR_EQ("Multiple     spaces\n", ex->markdown.data);
    NAOS_CHECK_STR_EQ("<p>Multiple     spaces</p>\n", ex->html.data);
    NAOS_CHECK_STR_EQ("Textual content", ex->section.data);
    NAOS_CHECK_UINT_EQ(9411, ex->start_line);
    NAOS_CHECK_UINT_EQ(9415, ex->end_line);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_spec_file_every_section_is_non_empty)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    size_t i;

    load_spec_file(&spec);
    NAOS_REQUIRE_UINT_GT(spec.count, 0);
    for (i = 0; i < spec.count; i++)
    {
        const mnd_md_spec_example_t *ex = &spec.examples[i];

        NAOS_CHECK_UINT_GT(ex->section.len, 0);
        NAOS_CHECK_UINT_GT(ex->markdown.len + ex->html.len, 0);
        /* `→` は残らない（すべてタブに置き換えられる） */
        NAOS_CHECK_NULL(strstr(ex->markdown.data, "\xE2\x86\x92"));
        NAOS_CHECK_NULL(strstr(ex->html.data, "\xE2\x86\x92"));
    }
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_parse_basic_example)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    const mnd_md_spec_example_t *ex;

    parse_str("# Intro\n"
              "\n"
              "## Sec\n"
              "\n"
              FENCE " example\n"
              "a\xE2\x86\x92" "b\n"
              "\n"
              ".\n"
              "<p>a\xE2\x86\x92" "b</p>\n"
              FENCE "\n",
              &spec);
    NAOS_REQUIRE_UINT_EQ(1, spec.count);
    ex = &spec.examples[0];
    NAOS_CHECK_UINT_EQ(1, ex->number);
    NAOS_CHECK_STR_EQ("Sec", ex->section.data);
    NAOS_CHECK_STR_EQ("a\tb\n\n", ex->markdown.data);
    NAOS_CHECK_UINT_EQ(5, ex->markdown.len);
    NAOS_CHECK_STR_EQ("<p>a\tb</p>\n", ex->html.data);
    NAOS_CHECK_UINT_EQ(5, ex->start_line);
    NAOS_CHECK_UINT_EQ(10, ex->end_line);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_parse_numbers_and_sections)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    /* 見出しは行頭の `#+ ` のみ。`#+ ` はすべて取り除かれる（re.sub と同じ）。例の中の見出しは無視 */
    parse_str("## A ## b\n"
              FENCE " example\n"
              "# not a header\n"
              ".\n"
              "<h1>not a header</h1>\n"
              FENCE "\n"
              "#NoSpace\n"
              "  # indented\n"
              FENCE " example\n"
              ".\n"
              FENCE "\n"
              "### Next\t \n"
              "  " FENCE " example  \n"
              "x\n"
              " . \n"
              FENCE,
              &spec);
    NAOS_REQUIRE_UINT_EQ(3, spec.count);

    NAOS_CHECK_UINT_EQ(1, spec.examples[0].number);
    NAOS_CHECK_STR_EQ("A b", spec.examples[0].section.data);
    NAOS_CHECK_STR_EQ("# not a header\n", spec.examples[0].markdown.data);
    NAOS_CHECK_STR_EQ("<h1>not a header</h1>\n", spec.examples[0].html.data);

    /* Markdown と HTML が空の例。開始の行は 0 のまま */
    NAOS_CHECK_UINT_EQ(2, spec.examples[1].number);
    NAOS_CHECK_STR_EQ("A b", spec.examples[1].section.data);
    NAOS_CHECK_STR_EQ("", spec.examples[1].markdown.data);
    NAOS_CHECK_UINT_EQ(0, spec.examples[1].markdown.len);
    NAOS_CHECK_STR_EQ("", spec.examples[1].html.data);
    NAOS_CHECK_UINT_EQ(0, spec.examples[1].start_line);
    NAOS_CHECK_UINT_EQ(11, spec.examples[1].end_line);

    /* 前後の空白を除いて比べる。最後の行に改行がなくても閉じる */
    NAOS_CHECK_UINT_EQ(3, spec.examples[2].number);
    NAOS_CHECK_STR_EQ("Next", spec.examples[2].section.data);
    NAOS_CHECK_STR_EQ("x\n", spec.examples[2].markdown.data);
    NAOS_CHECK_STR_EQ("", spec.examples[2].html.data);
    NAOS_CHECK_UINT_EQ(13, spec.examples[2].start_line);
    NAOS_CHECK_UINT_EQ(16, spec.examples[2].end_line);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_parse_keeps_raw_lines)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    /*
     * 行の内容は空白も含めてそのまま。閉じていない例は数えない。
     * spec_tests.py と同じく、例の外の `.` の行でも HTML の状態に入り、次の閉じの行で例になる
     */
    parse_str(".\n"
              FENCE "\n"
              FENCE " example\n"
              "  two  \n"
              "\xE2\x86\x92\xE2\x86\x92\n"
              ".\n"
              "  <p>two</p>  \n"
              FENCE "\n"
              FENCE " example\n"
              "unterminated\n",
              &spec);
    NAOS_REQUIRE_UINT_EQ(2, spec.count);
    NAOS_CHECK_STR_EQ("", spec.examples[0].markdown.data);
    NAOS_CHECK_STR_EQ("", spec.examples[0].html.data);
    NAOS_CHECK_UINT_EQ(0, spec.examples[0].start_line);
    NAOS_CHECK_UINT_EQ(2, spec.examples[0].end_line);
    NAOS_CHECK_UINT_EQ(2, spec.examples[1].number);
    NAOS_CHECK_STR_EQ("", spec.examples[1].section.data);
    NAOS_CHECK_STR_EQ("  two  \n\t\t\n", spec.examples[1].markdown.data);
    NAOS_CHECK_STR_EQ("  <p>two</p>  \n", spec.examples[1].html.data);
    NAOS_CHECK_UINT_EQ(3, spec.examples[1].start_line);
    NAOS_CHECK_UINT_EQ(8, spec.examples[1].end_line);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_parse_empty_and_reuse)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    parse_str(FENCE " example\nx\n.\ny\n" FENCE "\n", &spec);
    NAOS_CHECK_UINT_EQ(1, spec.count);

    /* 既存の例は解放され、置き換えられる */
    NAOS_REQUIRE_TRUE(mnd_md_spec_parse(NULL, 0, &spec));
    NAOS_CHECK_UINT_EQ(0, spec.count);

    mnd_md_spec_free(&spec);
    NAOS_CHECK_NULL(spec.examples);
    NAOS_CHECK_UINT_EQ(0, spec.count);
    mnd_md_spec_free(&spec);
}

NAOS_TEST(spec_reader_load_missing_file_reports_error)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_FALSE(mnd_md_spec_load(MND_MD_SPEC_FILE ".missing", &spec, &diags));
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));
    NAOS_CHECK_UINT_EQ(0, spec.count);
    mnd_diag_list_free(&diags);
    mnd_md_spec_free(&spec);
}

NAOS_SUITE(spec_reader,
    NAOS_ADD_TEST(spec_reader_spec_file_path_is_defined),
    NAOS_ADD_TEST(spec_reader_spec_file_size_and_no_cr),
    NAOS_ADD_TEST(spec_reader_spec_file_has_652_examples),
    NAOS_ADD_TEST(spec_reader_spec_file_first_example_has_tabs),
    NAOS_ADD_TEST(spec_reader_spec_file_last_example),
    NAOS_ADD_TEST(spec_reader_spec_file_every_section_is_non_empty),
    NAOS_ADD_TEST(spec_reader_parse_basic_example),
    NAOS_ADD_TEST(spec_reader_parse_numbers_and_sections),
    NAOS_ADD_TEST(spec_reader_parse_keeps_raw_lines),
    NAOS_ADD_TEST(spec_reader_parse_empty_and_reuse),
    NAOS_ADD_TEST(spec_reader_load_missing_file_reports_error)
)
