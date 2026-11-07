/* スイート front_matter: フロントマターの検出（1.1〜1.7、6.6、7.1） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "foundation/source.h"
#include "yaml/yaml_front_matter.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * 結果に検出前の値が残らないことを確かめるため、*out を意味のない値で埋める
 */
static void fill_garbage(mnd_yaml_front_matter_t *out)
{
    out->present = true;
    out->span.start = 77;
    out->span.end = 88;
    out->content.start = 55;
    out->content.end = 66;
    out->body_start = 99;
}

/*
 * text を検出し、診断が追加されずに true を返すことを確かめる
 */
static bool detect_ok(const char *text, mnd_yaml_front_matter_t *out)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;
    size_t count;

    fill_garbage(out);
    ok = mnd_yaml_front_matter_detect(text, strlen(text), NULL, out, &diags);
    count = mnd_diag_list_count(&diags);
    mnd_diag_list_free(&diags);

    return ok && (count == 0);
}

/*
 * フロントマターがない結果 {false, {0,0}, {0,0}, 0} であることを確かめる
 */
static void check_absent(const mnd_yaml_front_matter_t *fm)
{
    NAOS_CHECK_FALSE(fm->present);
    NAOS_CHECK_UINT_EQ(0, fm->span.start);
    NAOS_CHECK_UINT_EQ(0, fm->span.end);
    NAOS_CHECK_UINT_EQ(0, fm->content.start);
    NAOS_CHECK_UINT_EQ(0, fm->content.end);
    NAOS_CHECK_UINT_EQ(0, fm->body_start);
}

/*
 * 検出したフロントマターの範囲と本文の開始位置を確かめる
 */
static void check_present(const mnd_yaml_front_matter_t *fm, size_t span_end,
                          size_t content_start, size_t content_end)
{
    NAOS_CHECK_TRUE(fm->present);
    NAOS_CHECK_UINT_EQ(0, fm->span.start);
    NAOS_CHECK_UINT_EQ(span_end, fm->span.end);
    NAOS_CHECK_UINT_EQ(content_start, fm->content.start);
    NAOS_CHECK_UINT_EQ(content_end, fm->content.end);
    NAOS_CHECK_UINT_EQ(span_end, fm->body_start);
}

/* 基本の文書: 全体・内容の範囲と本文の開始位置（1.1、1.2） */
NAOS_TEST(front_matter_basic_ranges_and_body_start)
{
    const char *text = "---\ntitle: a\n---\n# body\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, 17, 4, 13);
    NAOS_CHECK_MEM_EQ(text + fm.content.start, fm.content.end - fm.content.start, "title: a\n", 9);
    NAOS_CHECK_MEM_EQ(text + fm.body_start, strlen(text) - fm.body_start, "# body\n", 7);
}

/* `...` の行で閉じる（1.1） */
NAOS_TEST(front_matter_dot_closing_line)
{
    const char *text = "---\na: 1\n...\nbody\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, 13, 4, 9);
}

/* 最初に現れた閉じの行で閉じる（1.1） */
NAOS_TEST(front_matter_first_closing_line_wins)
{
    const char *text = "---\na: 1\n---\nb\n---\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, 13, 4, 9);
}

/* 開きと閉じの行の後ろの空白・タブは許される（1.1） */
NAOS_TEST(front_matter_trailing_whitespace_after_markers)
{
    const char *text = "--- \t\na: 1\n---  \t \nbody";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, 19, 6, 11);
}

/* 1 行目が `--- a` の文書はフロントマターなし（1.3） */
NAOS_TEST(front_matter_first_line_with_extra_content)
{
    const char *text = "--- a\nb: 1\n---\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_absent(&fm);
}

/* 1 行目が `# x` の文書はフロントマターなし（1.3） */
NAOS_TEST(front_matter_first_line_heading)
{
    const char *text = "# x\n---\na: 1\n---\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_absent(&fm);
}

/* 開きの行の前に空白がある、または `-` が 4 つの行はフロントマターではない（1.3） */
NAOS_TEST(front_matter_first_line_not_exact_marker)
{
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(" ---\na\n---\n", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("----\na\n---\n", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("...\na\n...\n", &fm));
    check_absent(&fm);
}

/* 空の文書はフロントマターなし。len == 0 なら data は NULL 可（1.3） */
NAOS_TEST(front_matter_empty_document)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;

    fill_garbage(&fm);
    NAOS_CHECK_TRUE(mnd_yaml_front_matter_detect(NULL, 0, NULL, &fm, &diags));
    check_absent(&fm);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

/* 閉じの行がない文書はフロントマターなし（1.4） */
NAOS_TEST(front_matter_no_closing_line)
{
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok("---\na: 1\nb: 2\n", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("---\n", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("---", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("---\na\n --- \n----\n---x\n", &fm));
    check_absent(&fm);
}

/* 内容が空のフロントマター（1.5） */
NAOS_TEST(front_matter_empty_content)
{
    const char *text = "---\n---\nbody\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, 8, 4, 4);
}

/* 閉じの行が最後で行末記号がなければ、本文の開始位置は文書の長さ（1.6） */
NAOS_TEST(front_matter_closing_line_without_eol)
{
    const char *text = "---\na: 1\n---";
    const char *text_ws = "---\na: 1\n... \t";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, strlen(text), 4, 9);
    NAOS_CHECK_UINT_EQ(strlen(text), fm.body_start);

    NAOS_REQUIRE_TRUE(detect_ok(text_ws, &fm));
    check_present(&fm, strlen(text_ws), 4, 9);
}

/* CRLF の文書: 行末記号の CRLF を含めて範囲を返す（1.2、1.7） */
NAOS_TEST(front_matter_crlf_document)
{
    const char *text = "---\r\na: 1\r\nb: 2\r\n---\r\nbody\r\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, 22, 5, 17);
    NAOS_CHECK_MEM_EQ(text + fm.content.start, fm.content.end - fm.content.start,
                      "a: 1\r\nb: 2\r\n", 12);
}

/* LF と CRLF が混在してもよい（1.7） */
NAOS_TEST(front_matter_mixed_line_endings)
{
    const char *text = "---\r\na: 1\n---\r\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, strlen(text), 5, 10);
}

/* `---` の後ろが単独の CR の行は閉じの行にならない（1.7） */
NAOS_TEST(front_matter_lone_cr_is_not_closing)
{
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok("---\na: 1\n---\rb\n", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("---\na: 1\n---\r", &fm));
    check_absent(&fm);
    NAOS_REQUIRE_TRUE(detect_ok("---\ra: 1\n---\n", &fm));
    check_absent(&fm);
}

/* 単独の CR の行を飛ばして、後の閉じの行で閉じる（1.1、1.7） */
NAOS_TEST(front_matter_lone_cr_line_skipped_then_closed)
{
    const char *text = "---\na: 1\n---\rb\n...\n";
    mnd_yaml_front_matter_t fm;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    check_present(&fm, strlen(text), 4, 15);
}

/* 検出の前後で内容のバイト列が変わらない（1.7） */
NAOS_TEST(front_matter_content_unchanged)
{
    char buf[] = "---\r\ntitle: \"x\"\n---\nbody\n";
    char copy[sizeof(buf)];
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;

    memcpy(copy, buf, sizeof(buf));
    NAOS_CHECK_TRUE(mnd_yaml_front_matter_detect(buf, sizeof(buf) - 1, NULL, &fm, &diags));
    NAOS_CHECK_TRUE(fm.present);
    NAOS_CHECK_MEM_EQ(copy, sizeof(copy), buf, sizeof(buf));
    mnd_diag_list_free(&diags);
}

/* 不正な UTF-8: 行番号付きのエラーを追加し、結果をフロントマターなしにして false を返す（6.6） */
NAOS_TEST(front_matter_invalid_utf8_error_with_line)
{
    const char *text = "---\r\na: 1\nb: \xff\n---\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;
    const mnd_diag_t *diag;

    fill_garbage(&fm);
    NAOS_CHECK_FALSE(mnd_yaml_front_matter_detect(text, strlen(text), "doc.md", &fm, &diags));
    check_absent(&fm);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, diag->severity);
    NAOS_CHECK_STR_EQ("invalid UTF-8 sequence", diag->message);
    NAOS_REQUIRE_TRUE(diag->path != NULL);
    NAOS_CHECK_STR_EQ("doc.md", diag->path);
    NAOS_CHECK_UINT_EQ(3, diag->line);
    mnd_diag_list_free(&diags);
}

/* 不正な UTF-8 はフロントマターの外（本文）にあってもエラーになる（6.6） */
NAOS_TEST(front_matter_invalid_utf8_in_body)
{
    const char *text = "---\na: 1\n---\nok\n\xc3\x28\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;

    fill_garbage(&fm);
    NAOS_CHECK_FALSE(mnd_yaml_front_matter_detect(text, strlen(text), NULL, &fm, &diags));
    check_absent(&fm);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    NAOS_CHECK_STR_EQ("invalid UTF-8 sequence", mnd_diag_list_at(&diags, 0)->message);
    NAOS_CHECK_NULL(mnd_diag_list_at(&diags, 0)->path);
    NAOS_CHECK_UINT_EQ(5, mnd_diag_list_at(&diags, 0)->line);
    mnd_diag_list_free(&diags);
}

/* 内容の最初の行は文書全体の 2 行目（7.1） */
NAOS_TEST(front_matter_content_first_line_is_line_two)
{
    const char *text = "---\r\na: 1\nb: 2\n---\nbody\n";
    mnd_yaml_front_matter_t fm;
    size_t line = 0;

    NAOS_REQUIRE_TRUE(detect_ok(text, &fm));
    NAOS_REQUIRE_TRUE(fm.present);
    NAOS_REQUIRE_TRUE(mnd_source_line_of(text, strlen(text), fm.content.start, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    NAOS_REQUIRE_TRUE(mnd_source_line_of(text, strlen(text), fm.body_start, &line));
    NAOS_CHECK_UINT_EQ(5, line);
}

NAOS_SUITE(front_matter,
    NAOS_ADD_TEST(front_matter_basic_ranges_and_body_start),
    NAOS_ADD_TEST(front_matter_dot_closing_line),
    NAOS_ADD_TEST(front_matter_first_closing_line_wins),
    NAOS_ADD_TEST(front_matter_trailing_whitespace_after_markers),
    NAOS_ADD_TEST(front_matter_first_line_with_extra_content),
    NAOS_ADD_TEST(front_matter_first_line_heading),
    NAOS_ADD_TEST(front_matter_first_line_not_exact_marker),
    NAOS_ADD_TEST(front_matter_empty_document),
    NAOS_ADD_TEST(front_matter_no_closing_line),
    NAOS_ADD_TEST(front_matter_empty_content),
    NAOS_ADD_TEST(front_matter_closing_line_without_eol),
    NAOS_ADD_TEST(front_matter_crlf_document),
    NAOS_ADD_TEST(front_matter_mixed_line_endings),
    NAOS_ADD_TEST(front_matter_lone_cr_is_not_closing),
    NAOS_ADD_TEST(front_matter_lone_cr_line_skipped_then_closed),
    NAOS_ADD_TEST(front_matter_content_unchanged),
    NAOS_ADD_TEST(front_matter_invalid_utf8_error_with_line),
    NAOS_ADD_TEST(front_matter_invalid_utf8_in_body),
    NAOS_ADD_TEST(front_matter_content_first_line_is_line_two)
)
