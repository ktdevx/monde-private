/* スイート markdown: 走査の規則（Markdown）（1.1〜1.7、2.1〜2.5、2.8、3.5） */

#include <naos/naos.h>

#include "test_util.h"

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_dialect.h"
#include "markdown/md_parse.h"
#include "tu/tu.h"
#include "tu/tu_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MD MND_TU_ORIGIN_MARKDOWN
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* text を走査し、セグメントの列が expected[0..count) と同じことを検査する */
static void check_scan(const char *text, const test_segment_expect_t *expected, size_t count)
{
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, NULL, 0, doc);
    TEST_CHECK_SEGMENTS(doc, expected, count);
    mnd_tu_doc_free(doc);
}

#define CHECK_SCAN(text, expected, count) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_SCAN"); \
        check_scan((text), (expected), (count)); \
        NAOS_TRACE_POP(); \
    } while (0)

/* 空の文書の走査は成功し、セグメントを作らない */
NAOS_TEST(markdown_empty_document_has_no_segments)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;

    NAOS_CHECK_TRUE(mnd_tu_scan(NULL, 0, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc != NULL);
    NAOS_CHECK_UINT_EQ(0, mnd_tu_doc_segment_count(doc));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* 段落は 1 つのセグメントになり、複数の段落は原文の順に並ぶ（1.1、2.1） */
NAOS_TEST(markdown_paragraphs)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 11, "Hello\nworld", "paragraph" },
        { MD, 4, 15, 21, "Second", "paragraph" }
    };

    CHECK_SCAN("Hello\nworld\n\n  Second\n", expected, COUNT_OF(expected));
}

/* ATX 見出しは記号と閉じの記号を除いた内容がセグメントになる（1.2、3.5） */
NAOS_TEST(markdown_atx_headings)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 2, 7, "Title", "heading (level 1)" },
        { MD, 2, 11, 14, "Sub", "heading (level 2)" },
        { MD, 3, 25, 29, "Deep", "heading (level 6)" }
    };

    CHECK_SCAN("# Title\n## Sub ##\n###### Deep   \n", expected, COUNT_OF(expected));
}

/* setext 見出しは下線を除いた行がセグメントになり、複数行は \n で連結する（1.2、2.1、3.5） */
NAOS_TEST(markdown_setext_headings)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 5, "Title", "heading (level 1)" },
        { MD, 4, 13, 23, "Multi\nline", "heading (level 2)" }
    };

    CHECK_SCAN("Title\n=====\n\nMulti\nline  \n---\n", expected, COUNT_OF(expected));
}

/* リスト項目内の段落は記号とインデントを除き、ラベルに list item を付ける（1.1、2.1、3.5） */
NAOS_TEST(markdown_paragraph_in_list_item)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 2, 11, "one\ntwo", "list item > paragraph" },
        { MD, 3, 15, 20, "three", "list item > paragraph" }
    };

    CHECK_SCAN("- one\n  two\n1. three\n", expected, COUNT_OF(expected));
}

/* 引用内の段落は `>` と空白を除き、遅延継続行も含め、ラベルに block quote を付ける（1.1、2.1、3.5） */
NAOS_TEST(markdown_paragraph_in_block_quote)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 2, 18, "quote\nmore\nlazy", "block quote > paragraph" }
    };

    CHECK_SCAN("> quote\n>more\nlazy\n", expected, COUNT_OF(expected));
}

/* 入れ子のラベルは外側から ` > ` で連結し、リスト自体は出さない（3.5） */
NAOS_TEST(markdown_nested_labels)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 4, 5, "a", "list item > block quote > paragraph" },
        { MD, 3, 11, 12, "b", "block quote > list item > paragraph" },
        { MD, 5, 20, 21, "c", "block quote > heading (level 3)" },
        { MD, 7, 27, 28, "d", "list item > list item > paragraph" }
    };

    CHECK_SCAN("- > a\n\n> - b\n\n> ### c\n\n- - d\n", expected, COUNT_OF(expected));
}

/* コードブロック（フェンス・インデント）、HTML ブロック、リンク参照定義、区切り線はセグメントにしない（1.5） */
NAOS_TEST(markdown_excluded_blocks)
{
    static const test_segment_expect_t expected[] = {
        { MD, 15, 63, 67, "text", "paragraph" }
    };

    CHECK_SCAN("```\ncode\n```\n\n    indented\n\n<div>\nhtml\n</div>\n\n[a]: /url\n\n***\n\ntext\n",
               expected, COUNT_OF(expected));
}

/* 引用やリストの中の除外するブロックもセグメントにしない（1.5） */
NAOS_TEST(markdown_excluded_blocks_in_containers)
{
    CHECK_SCAN("> ```\n> code\n> ```\n\n- ***\n\n> [b]: /u\n", NULL, 0);
}

/* 空の見出し、閉じの記号だけの見出し、空白だけの見出しはセグメントにしない（1.6、2.8） */
NAOS_TEST(markdown_empty_heading)
{
    static const test_segment_expect_t expected[] = {
        { MD, 6, 14, 18, "text", "paragraph" }
    };

    CHECK_SCAN("#\n\n##   \n\n# #\ntext\n", expected, COUNT_OF(expected));
}

/* 画像だけの段落は `![alt](url)` の形のまま 1 つのセグメントになる（1.3、1.4） */
NAOS_TEST(markdown_image_only_paragraph)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 23, "![alt *text*](url \"tt\")", "paragraph" }
    };

    CHECK_SCAN("![alt *text*](url \"tt\")\n", expected, COUNT_OF(expected));
}

/* リンクの title、エスケープ、文字参照、インライン HTML、コードスパンは原文のまま残す（1.3、2.5） */
NAOS_TEST(markdown_inline_syntax_is_preserved)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 39, "[a](/u \"t\") \\* &amp; &#65; <b>x</b> `c`", "paragraph" }
    };

    CHECK_SCAN("[a](/u \"t\") \\* &amp; &#65; <b>x</b> `c`\n", expected, COUNT_OF(expected));
}

/* ハードブレークの末尾の 2 空白とバックスラッシュは最後の行以外では msgid に含める（2.4） */
NAOS_TEST(markdown_hard_breaks)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 8, "a  \nb\\\nc", "paragraph" }
    };

    CHECK_SCAN("a  \nb\\\nc\n", expected, COUNT_OF(expected));
}

/* 最後の行の末尾の空白とタブは範囲と msgid に含めない（2.3） */
NAOS_TEST(markdown_trailing_whitespace_of_last_line)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 4, "text", "paragraph" },
        { MD, 3, 9, 14, "a \t\nb", "paragraph" },
        { MD, 6, 18, 22, "end.", "paragraph" }
    };

    CHECK_SCAN("text \t \n\na \t\nb \t\n\nend.  ", expected, COUNT_OF(expected));
}

/* LF・CRLF・単独の CR の行境界は msgid で \n になり、CR を含めない（2.2） */
NAOS_TEST(markdown_line_endings_become_lf)
{
    static const test_segment_expect_t expected[] = {
        { MD, 1, 0, 8, "a\nb\nc\nd", "paragraph" },
        { MD, 5, 12, 16, "e\nf", "paragraph" }
    };

    CHECK_SCAN("a\r\nb\rc\nd\r\n\r\ne\r\nf\r\n", expected, COUNT_OF(expected));
}

/* テスト用の方言: `|` で始まる行が続くあいだのコンテナ */
#define KIND_BOX ((mnd_md_kind_t)MND_MD_KIND_DIALECT)

/* 行の最初の空白でない文字が `|` で、インデントが 3 桁以下か */
static bool box_marker_at(const mnd_md_line_view_t *view)
{
    return ((!view->blank) && (view->indent < 4) && (view->first_nonspace < view->end)
            && (view->data[view->first_nonspace] == '|'));
}

/* `|` とその後ろの空白 1 桁分を使う */
static void box_consume_marker(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    mnd_md_block_ctx_advance(ctx, 1);
    view = mnd_md_block_ctx_line(ctx);
    if (view.indent >= 1)
    {
        mnd_md_block_ctx_advance_columns(ctx, 1);
    }
}

static bool box_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    if (!box_marker_at(&view))
    {
        return false;
    }
    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    if (mnd_md_block_ctx_open(ctx, KIND_BOX) == NULL)
    {
        return false;
    }
    box_consume_marker(ctx);
    return true;
}

static bool box_try_continue(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    (void)node;
    if (!box_marker_at(&view))
    {
        return false;
    }
    box_consume_marker(ctx);
    return true;
}

/* kind、container、inline_content、can_interrupt_paragraph、try_start、try_continue、finalize */
static const mnd_md_block_rule_t box_rules[1] = {
    { KIND_BOX, true, false, true, box_try_start, box_try_continue, NULL }
};

static const mnd_md_dialect_t box_dialect = {
    "box", box_rules, 1, NULL, 0, NULL
};

/*
 * 方言の種類のコンテナは自身をセグメントにせず、子の段落と見出しを走査する。ラベルに方言のノードは出さない（1.7、3.5）。
 * 公開 API に方言の指定がないため、方言で解析した markdown の文書から内部の文書を組み立て、収集の関数を直接呼ぶ
 */
NAOS_TEST(markdown_dialect_container_children_are_collected)
{
    static const char data[] = "| # h\n| text\n| > q\n\nafter\n";
    static const test_segment_expect_t expected[] = {
        { MD, 1, 4, 5, "h", "heading (level 1)" },
        { MD, 2, 8, 12, "text", "paragraph" },
        { MD, 3, 17, 18, "q", "block quote > paragraph" },
        { MD, 5, 20, 25, "after", "paragraph" }
    };
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;
    const mnd_md_node_t *box = NULL;
    mnd_md_doc_t *md = NULL;

    options.dialect = &box_dialect;
    NAOS_REQUIRE_TRUE(mnd_md_parse(data, sizeof(data) - 1, &options, &md, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);

    /* 方言のコンテナが段落と見出しを子に持つこと（テストの前提）を確かめる */
    box = mnd_md_doc_root(md)->first_child;
    NAOS_REQUIRE_NOT_NULL(box);
    NAOS_REQUIRE_INT_EQ((int)KIND_BOX, (int)box->kind);
    NAOS_REQUIRE_NOT_NULL(box->first_child);
    NAOS_CHECK_INT_EQ((int)MND_MD_HEADING, (int)box->first_child->kind);

    /* mnd_tu_doc_free が md を含めて解放できるように calloc で組み立てる */
    doc = (mnd_tu_doc_t *)calloc(1, sizeof(*doc));
    if (doc == NULL)
    {
        mnd_md_doc_free(md);
        NAOS_FAIL("out of memory");
    }
    doc->data = data;
    doc->len = sizeof(data) - 1;
    doc->md = md;

    NAOS_CHECK_TRUE(mnd_tu_collect_markdown(doc));
    TEST_CHECK_SEGMENTS(doc, expected, COUNT_OF(expected));
    mnd_tu_doc_free(doc);
}

/* 内部のセグメントは作ったノードを指し、範囲はノードの行の内側にある（MarkdownSegments の事後条件） */
NAOS_TEST(markdown_segment_points_to_node)
{
    mnd_tu_doc_t *doc = NULL;
    const mnd_tu_seg_t *seg = NULL;

    TEST_SCAN("> # Head\n\npara  \n", NULL, 0, doc);
    NAOS_REQUIRE_UINT_EQ(2, mnd_tu_doc_segment_count(doc));
    seg = &doc->segs[0];
    NAOS_REQUIRE_NOT_NULL(seg->node);
    NAOS_CHECK_INT_EQ((int)MND_MD_HEADING, (int)seg->node->kind);
    NAOS_CHECK_NULL(seg->scalar);
    seg = &doc->segs[1];
    NAOS_REQUIRE_NOT_NULL(seg->node);
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)seg->node->kind);
    NAOS_CHECK_UINT_EQ(seg->node->lines[0].content.start, seg->pub.span.start);
    NAOS_CHECK_TRUE(seg->pub.span.end <= seg->node->lines[0].content.end);
    mnd_tu_doc_free(doc);
}

NAOS_SUITE(markdown,
    NAOS_ADD_TEST(markdown_empty_document_has_no_segments),
    NAOS_ADD_TEST(markdown_paragraphs),
    NAOS_ADD_TEST(markdown_atx_headings),
    NAOS_ADD_TEST(markdown_setext_headings),
    NAOS_ADD_TEST(markdown_paragraph_in_list_item),
    NAOS_ADD_TEST(markdown_paragraph_in_block_quote),
    NAOS_ADD_TEST(markdown_nested_labels),
    NAOS_ADD_TEST(markdown_excluded_blocks),
    NAOS_ADD_TEST(markdown_excluded_blocks_in_containers),
    NAOS_ADD_TEST(markdown_empty_heading),
    NAOS_ADD_TEST(markdown_image_only_paragraph),
    NAOS_ADD_TEST(markdown_inline_syntax_is_preserved),
    NAOS_ADD_TEST(markdown_hard_breaks),
    NAOS_ADD_TEST(markdown_trailing_whitespace_of_last_line),
    NAOS_ADD_TEST(markdown_line_endings_become_lf),
    NAOS_ADD_TEST(markdown_dialect_container_children_are_collected),
    NAOS_ADD_TEST(markdown_segment_points_to_node)
)
