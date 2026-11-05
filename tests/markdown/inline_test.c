/* インラインの種類と範囲（5.x） */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* メモリ不足のテストで、失敗させる位置を変える回数の上限（成功するまでの確保の回数より十分に大きい） */
#define OOM_MAX_STEPS 2000

/* 病的な入力の繰り返しの回数 */
#define PATHOLOGICAL_REPEAT 10000

/* 期待するインラインのノード（種類と範囲） */
typedef struct expected_node
{
    mnd_md_kind_t kind;
    size_t start;
    size_t end;
} expected_node_t;

/* 解析が成功し、診断が増えず、tree_check を満たすことを確かめる。失敗なら NULL */
static mnd_md_doc_t *parse_len_ok(const char *data, size_t len)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    bool ok = false;

    ok = mnd_md_parse(data, len, &options, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        return NULL;
    }
    NAOS_CHECK_NOT_NULL(doc);
    if (doc != NULL)
    {
        NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    }

    return doc;
}

static mnd_md_doc_t *parse_ok(const char *data)
{
    return parse_len_ok(data, strlen(data));
}

/* 前順で最初の kind のノード。なければ NULL */
static const mnd_md_node_t *find_kind(const mnd_md_doc_t *doc, mnd_md_kind_t kind)
{
    const mnd_md_node_t *root = mnd_md_doc_root(doc);
    const mnd_md_node_t *node = NULL;

    for (node = root; node != NULL; node = mnd_md_node_next(node, root))
    {
        if (node->kind == kind)
        {
            return node;
        }
    }

    return NULL;
}

/* parent の子の数 */
static size_t child_count(const mnd_md_node_t *parent)
{
    const mnd_md_node_t *child = NULL;
    size_t count = 0;

    for (child = parent->first_child; child != NULL; child = child->next)
    {
        count++;
    }

    return count;
}

/* parent の index 番目（0 起点）の子。なければ NULL */
static const mnd_md_node_t *child_at(const mnd_md_node_t *parent, size_t index)
{
    const mnd_md_node_t *child = parent->first_child;
    size_t i = 0;

    for (i = 0; (i < index) && (child != NULL); i++)
    {
        child = child->next;
    }

    return child;
}

/* leaf の子の種類と範囲が expected[0..count) と一致することを確かめる */
static void check_children(const mnd_md_node_t *leaf, const expected_node_t *expected, size_t count)
{
    const mnd_md_node_t *child = NULL;
    size_t i = 0;

    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_CHECK_UINT_EQ(count, child_count(leaf));
    for (child = leaf->first_child; (child != NULL) && (i < count); child = child->next, i++)
    {
        NAOS_CHECK_INT_EQ((int)expected[i].kind, (int)child->kind);
        NAOS_CHECK_UINT_EQ(expected[i].start, child->span.start);
        NAOS_CHECK_UINT_EQ(expected[i].end, child->span.end);
        NAOS_CHECK_TRUE(child->first_child == NULL);
        NAOS_CHECK_TRUE(child->lines == NULL);
    }
}

/* data の最初の段落の子が expected と一致することを確かめる */
static void check_paragraph(const char *data, const expected_node_t *expected, size_t count)
{
    mnd_md_doc_t *doc = parse_ok(data);

    NAOS_REQUIRE_NOT_NULL(doc);
    check_children(find_kind(doc, MND_MD_PARAGRAPH), expected, count);
    mnd_md_doc_free(doc);
}

/* leaf の行に含まれる span の論理的な文字列が expected と一致することを確かめる */
static void check_logical(const mnd_md_doc_t *doc, const mnd_md_node_t *leaf, mnd_span_t span,
                          const char *expected)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_md_logical_text(doc, leaf, span, &buf));
    NAOS_CHECK_MEM_EQ(buf.data, buf.len, expected, strlen(expected));
    mnd_buf_free(&buf);
}

/*
 * 5.1: テキストだけの段落は 1 つのテキストのノードになる。対にならない `*` と `_`、`[`、`]`、`!` もテキストで、
 * 隣のテキストと 1 つのノードにまとまる（`[` と `]` は 4.3 まで）
 */
NAOS_TEST(inline_text)
{
    static const expected_node_t one[] = { { MND_MD_TEXT, 0, 5 } };
    static const expected_node_t plain[] = { { MND_MD_TEXT, 0, 13 } };

    check_paragraph("hello\n", one, 1);
    check_paragraph("*a b_ [c]! _d\n", plain, 1);
}

/* 5.1: ソフト改行（前の行の末尾の空白 1 つと行末記号を範囲とする） */
NAOS_TEST(inline_soft_break)
{
    static const expected_node_t plain[] = {
        { MND_MD_TEXT, 0, 3 }, { MND_MD_SOFT_BREAK, 3, 4 }, { MND_MD_TEXT, 4, 7 }
    };
    static const expected_node_t space[] = {
        { MND_MD_TEXT, 0, 3 }, { MND_MD_SOFT_BREAK, 3, 5 }, { MND_MD_TEXT, 5, 8 }
    };
    static const expected_node_t crlf[] = {
        { MND_MD_TEXT, 0, 3 }, { MND_MD_SOFT_BREAK, 3, 5 }, { MND_MD_TEXT, 5, 8 }
    };

    check_paragraph("foo\nbar\n", plain, 3);
    check_paragraph("foo \nbar\n", space, 3);
    check_paragraph("foo\r\nbar\r\n", crlf, 3);
}

/* 5.1: ハード改行（2 つ以上の空白、バックスラッシュ） */
NAOS_TEST(inline_hard_break)
{
    static const expected_node_t spaces[] = {
        { MND_MD_TEXT, 0, 3 }, { MND_MD_HARD_BREAK, 3, 6 }, { MND_MD_TEXT, 6, 9 }
    };
    static const expected_node_t spaces_crlf[] = {
        { MND_MD_TEXT, 0, 3 }, { MND_MD_HARD_BREAK, 3, 8 }, { MND_MD_TEXT, 8, 11 }
    };
    static const expected_node_t backslash[] = {
        { MND_MD_TEXT, 0, 3 }, { MND_MD_HARD_BREAK, 3, 5 }, { MND_MD_TEXT, 5, 8 }
    };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    check_paragraph("foo  \nbar\n", spaces, 3);
    check_paragraph("foo   \r\nbar\n", spaces_crlf, 3);
    check_paragraph("foo\\\nbar\n", backslash, 3);

    doc = parse_ok("a  \nb\\\nc\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(5, child_count(para));
    NAOS_CHECK_FALSE(child_at(para, 1)->as.hard_break.backslash);
    NAOS_CHECK_TRUE(child_at(para, 3)->as.hard_break.backslash);
    NAOS_CHECK_UINT_EQ(1, child_at(para, 1)->start_line);
    NAOS_CHECK_UINT_EQ(1, child_at(para, 1)->end_line);
    NAOS_CHECK_UINT_EQ(2, child_at(para, 2)->start_line);
    NAOS_CHECK_UINT_EQ(3, child_at(para, 4)->start_line);
    mnd_md_doc_free(doc);
}

/* 5.1: 最後の行の末尾の空白はどのノードにも含めない。最後の行の末尾のバックスラッシュはテキスト */
NAOS_TEST(inline_final_spaces_belong_to_no_node)
{
    static const expected_node_t one_line[] = { { MND_MD_TEXT, 0, 3 } };
    static const expected_node_t two_lines[] = {
        { MND_MD_TEXT, 0, 1 }, { MND_MD_SOFT_BREAK, 1, 2 }, { MND_MD_TEXT, 2, 3 }
    };
    static const expected_node_t backslash[] = { { MND_MD_TEXT, 0, 4 } };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    check_paragraph("foo  \n", one_line, 1);
    check_paragraph("foo \t \n", one_line, 1);
    check_paragraph("a\nb   \n", two_lines, 3);
    check_paragraph("foo\\\n", backslash, 1);

    doc = parse_ok("foo  ");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_CHECK_UINT_EQ(5, para->span.end);
    check_children(para, one_line, 1);
    mnd_md_doc_free(doc);
}

/* 5.8: バックスラッシュエスケープ */
NAOS_TEST(inline_escape)
{
    static const expected_node_t escaped[] = {
        { MND_MD_TEXT, 0, 1 }, { MND_MD_ESCAPE, 1, 3 }, { MND_MD_TEXT, 3, 4 }
    };
    static const expected_node_t not_punct[] = { { MND_MD_TEXT, 0, 4 } };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    check_paragraph("a\\*b\n", escaped, 3);
    check_paragraph("\\a\\b\n", not_punct, 1);

    doc = parse_ok("\\*\\\\\\`\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(3, child_count(para));
    NAOS_CHECK_INT_EQ('*', child_at(para, 0)->as.escape.ch);
    NAOS_CHECK_INT_EQ('\\', child_at(para, 1)->as.escape.ch);
    NAOS_CHECK_INT_EQ('`', child_at(para, 2)->as.escape.ch);
    NAOS_CHECK_UINT_EQ(4, child_at(para, 2)->span.start);
    mnd_md_doc_free(doc);
}

/* char_ref のノードが numeric と text（UTF-8）を持つことを確かめる */
static void check_char_ref(const mnd_md_node_t *node, bool numeric, const char *text)
{
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_CHAR_REF, (int)node->kind);
    NAOS_CHECK_TRUE(node->as.char_ref.numeric == numeric);
    NAOS_CHECK_MEM_EQ(node->as.char_ref.text, node->as.char_ref.len, text, strlen(text));
}

/* 5.8: 実体参照と数値文字参照（&#0; は U+FFFD、&ngE; は 2 つのコードポイント） */
NAOS_TEST(inline_char_refs)
{
    static const char data[] = "&amp; &#35; &#0; &ngE; &#X22; &#xD800;\n";
    static const expected_node_t refs[] = {
        { MND_MD_CHAR_REF, 0, 5 }, { MND_MD_TEXT, 5, 6 },
        { MND_MD_CHAR_REF, 6, 11 }, { MND_MD_TEXT, 11, 12 },
        { MND_MD_CHAR_REF, 12, 16 }, { MND_MD_TEXT, 16, 17 },
        { MND_MD_CHAR_REF, 17, 22 }, { MND_MD_TEXT, 22, 23 },
        { MND_MD_CHAR_REF, 23, 29 }, { MND_MD_TEXT, 29, 30 },
        { MND_MD_CHAR_REF, 30, 38 }
    };
    static const expected_node_t invalid[] = { { MND_MD_TEXT, 0, 37 } };
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    check_children(para, refs, sizeof(refs) / sizeof(refs[0]));
    check_char_ref(child_at(para, 0), false, "&");
    check_char_ref(child_at(para, 2), true, "#");
    check_char_ref(child_at(para, 4), true, "\xEF\xBF\xBD");
    check_char_ref(child_at(para, 6), false, "\xE2\x89\xA7\xCC\xB8");
    check_char_ref(child_at(para, 8), true, "\"");
    check_char_ref(child_at(para, 10), true, "\xEF\xBF\xBD");
    mnd_md_doc_free(doc);

    /* 存在しない名前、`;` のない参照、桁数の多すぎる数値、空の数値はテキスト */
    check_paragraph("&nosuch; &amp &#12345678; &#; &#x; &x\n", invalid, 1);
}

/* 5.6: コードスパンの内容の範囲とバッククォートの数 */
NAOS_TEST(inline_code_span)
{
    static const expected_node_t single[] = {
        { MND_MD_TEXT, 0, 2 }, { MND_MD_CODE_SPAN, 2, 5 }, { MND_MD_TEXT, 5, 7 }
    };
    static const expected_node_t unclosed[] = { { MND_MD_TEXT, 0, 4 } };
    static const expected_node_t backslash[] = { { MND_MD_CODE_SPAN, 0, 4 }, { MND_MD_TEXT, 4, 6 } };
    static const expected_node_t escaped_open[] = {
        { MND_MD_ESCAPE, 0, 2 }, { MND_MD_CODE_SPAN, 2, 5 }
    };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *code = NULL;

    check_paragraph("a `b` c\n", single, 3);
    doc = parse_ok("a `b` c\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    code = find_kind(doc, MND_MD_CODE_SPAN);
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_CHECK_UINT_EQ(1, code->as.code_span.ticks);
    NAOS_CHECK_UINT_EQ(3, code->as.code_span.content.start);
    NAOS_CHECK_UINT_EQ(4, code->as.code_span.content.end);
    mnd_md_doc_free(doc);

    /* 2 つのバッククォートの中の 1 つのバッククォートは内容になる */
    doc = parse_ok("``a`b``\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    code = find_kind(doc, MND_MD_CODE_SPAN);
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_CHECK_UINT_EQ(0, code->span.start);
    NAOS_CHECK_UINT_EQ(7, code->span.end);
    NAOS_CHECK_UINT_EQ(2, code->as.code_span.ticks);
    NAOS_CHECK_UINT_EQ(2, code->as.code_span.content.start);
    NAOS_CHECK_UINT_EQ(5, code->as.code_span.content.end);
    mnd_md_doc_free(doc);

    /* 長さの合う閉じがなければ、バッククォートの並びはテキスト */
    check_paragraph("``a`\n", unclosed, 1);
    /* コードスパンの中ではバックスラッシュはエスケープにならない */
    check_paragraph("`a\\`b`\n", backslash, 2);
    /* エスケープしたバッククォートは開きにならない */
    check_paragraph("\\``a`\n", escaped_open, 2);
}

/* 5.10: 引用の中で行をまたぐコードスパンの範囲（途中のプレフィックスと行末記号を含む）と論理的な文字列 */
NAOS_TEST(inline_code_span_across_lines_in_block_quote)
{
    static const char data[] = "> a `b\n> c` d\n";
    static const expected_node_t children[] = {
        { MND_MD_TEXT, 2, 4 }, { MND_MD_CODE_SPAN, 4, 11 }, { MND_MD_TEXT, 11, 13 }
    };
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    check_children(para, children, 3);
    code = child_at(para, 1);
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_CHECK_UINT_EQ(5, code->as.code_span.content.start);
    NAOS_CHECK_UINT_EQ(10, code->as.code_span.content.end);
    NAOS_CHECK_UINT_EQ(1, code->start_line);
    NAOS_CHECK_UINT_EQ(2, code->end_line);
    check_logical(doc, para, code->as.code_span.content, "b\nc");
    check_logical(doc, para, code->span, "`b\nc`");
    mnd_md_doc_free(doc);
}

/* 5.7: URI とメールアドレスの自動リンク */
NAOS_TEST(inline_autolinks)
{
    static const char data[] = "<http://a.b/c?d> <foo@bar.example>\n";
    static const expected_node_t links[] = {
        { MND_MD_AUTOLINK, 0, 16 }, { MND_MD_TEXT, 16, 17 }, { MND_MD_AUTOLINK, 17, 34 }
    };
    static const expected_node_t not_links[] = { { MND_MD_TEXT, 0, 30 } };
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *link = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    check_children(para, links, 3);
    link = child_at(para, 0);
    NAOS_REQUIRE_NOT_NULL(link);
    NAOS_CHECK_FALSE(link->as.autolink.email);
    NAOS_CHECK_UINT_EQ(1, link->as.autolink.dest.start);
    NAOS_CHECK_UINT_EQ(15, link->as.autolink.dest.end);
    link = child_at(para, 2);
    NAOS_REQUIRE_NOT_NULL(link);
    NAOS_CHECK_TRUE(link->as.autolink.email);
    NAOS_CHECK_UINT_EQ(18, link->as.autolink.dest.start);
    NAOS_CHECK_UINT_EQ(33, link->as.autolink.dest.end);
    mnd_md_doc_free(doc);

    /* 空白を含む URI、1 文字のスキーム、`\` を含むメールアドレスは自動リンクではない */
    check_paragraph("<http://a b> <m:abc> <a\\b@c.d>\n", not_links, 1);
}

/* 5.1: 生の HTML（開始タグ、終了タグ、コメント、処理命令、宣言、CDATA） */
NAOS_TEST(inline_raw_html)
{
    static const char data[] = "a <b c=\"d\">x</b> <!-- c --> <?p?> <!X> <![CDATA[]]>\n";
    static const expected_node_t children[] = {
        { MND_MD_TEXT, 0, 2 }, { MND_MD_HTML_INLINE, 2, 11 }, { MND_MD_TEXT, 11, 12 },
        { MND_MD_HTML_INLINE, 12, 16 }, { MND_MD_TEXT, 16, 17 }, { MND_MD_HTML_INLINE, 17, 27 },
        { MND_MD_TEXT, 27, 28 }, { MND_MD_HTML_INLINE, 28, 33 }, { MND_MD_TEXT, 33, 34 },
        { MND_MD_HTML_INLINE, 34, 38 }, { MND_MD_TEXT, 38, 39 }, { MND_MD_HTML_INLINE, 39, 51 }
    };
    static const expected_node_t not_html[] = { { MND_MD_TEXT, 0, 20 } };

    check_paragraph(data, children, sizeof(children) / sizeof(children[0]));
    /* 閉じのないコメント、処理命令、属性の値はテキスト */
    check_paragraph("x <!-- a <? b <c d=\"\n", not_html, 1);
}

/* 5.10: 引用の中で行をまたぐ生の HTML（途中のプレフィックスを範囲に含む） */
NAOS_TEST(inline_raw_html_across_lines_in_block_quote)
{
    static const char data[] = "> x <a\n> href=\"y\">\n";
    static const expected_node_t children[] = { { MND_MD_TEXT, 2, 4 }, { MND_MD_HTML_INLINE, 4, 18 } };
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    check_children(para, children, 2);
    NAOS_REQUIRE_NOT_NULL(child_at(para, 1));
    check_logical(doc, para, child_at(para, 1)->span, "<a\nhref=\"y\">");
    mnd_md_doc_free(doc);
}

/* 5.1: 見出しの内容（ATX は閉じの並びを除く。Setext は行をまたぎ、最後の行の末尾の空白を除く） */
NAOS_TEST(inline_headings)
{
    static const expected_node_t atx[] = { { MND_MD_TEXT, 2, 4 }, { MND_MD_CODE_SPAN, 4, 7 } };
    static const expected_node_t setext[] = {
        { MND_MD_TEXT, 0, 1 }, { MND_MD_SOFT_BREAK, 1, 2 }, { MND_MD_TEXT, 2, 3 }
    };
    mnd_md_doc_t *doc = NULL;

    doc = parse_ok("# a `b` #\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    check_children(find_kind(doc, MND_MD_HEADING), atx, 2);
    mnd_md_doc_free(doc);

    doc = parse_ok("a\nb  \n==\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    check_children(find_kind(doc, MND_MD_HEADING), setext, 3);
    mnd_md_doc_free(doc);

    doc = parse_ok("#\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    check_children(find_kind(doc, MND_MD_HEADING), NULL, 0);
    mnd_md_doc_free(doc);
}

/* 5.11: コードブロック、HTML ブロック、リンク参照定義の内容はインラインとして解析しない */
NAOS_TEST(inline_not_parsed_in_other_leaves)
{
    static const char *const inputs[] = {
        "    *a* `b`\n",
        "```\n*a* `b`\n```\n",
        "<div>\n*a* `b` &amp;\n</div>\n",
        "[x]: /u \"*a* `b` &amp;\"\n"
    };
    static const mnd_md_kind_t kinds[] = {
        MND_MD_CODE_BLOCK, MND_MD_CODE_BLOCK, MND_MD_HTML_BLOCK, MND_MD_LINK_REF_DEF
    };
    size_t i = 0;

    for (i = 0; i < (sizeof(inputs) / sizeof(inputs[0])); i++)
    {
        mnd_md_doc_t *doc = parse_ok(inputs[i]);
        const mnd_md_node_t *root = NULL;

        NAOS_REQUIRE_NOT_NULL(doc);
        root = mnd_md_doc_root(doc);
        NAOS_REQUIRE_UINT_EQ(1, child_count(root));
        NAOS_CHECK_INT_EQ((int)kinds[i], (int)root->first_child->kind);
        NAOS_CHECK_NULL(root->first_child->first_child);
        mnd_md_doc_free(doc);
    }
}

/* 5.1: リスト項目の中の段落、遅延継続行、NUL を含む段落 */
NAOS_TEST(inline_in_containers_and_nul)
{
    static const expected_node_t lazy[] = {
        { MND_MD_TEXT, 2, 3 }, { MND_MD_SOFT_BREAK, 3, 4 }, { MND_MD_CODE_SPAN, 4, 7 }
    };
    static const char nul_data[] = "a\0b `\0`\n";
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    check_paragraph("- a\n`b`\n", lazy, 3);

    doc = parse_len_ok(nul_data, sizeof(nul_data) - 1);
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(2, child_count(para));
    NAOS_CHECK_UINT_EQ(4, child_at(para, 0)->span.end);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_SPAN, (int)child_at(para, 1)->kind);
    NAOS_CHECK_UINT_EQ(7, child_at(para, 1)->span.end);
    mnd_md_doc_free(doc);
}

/* 開始位置を指定した文書でも、インラインの範囲は内容全体の先頭を基準にする（1.3） */
NAOS_TEST(inline_ranges_with_body_start)
{
    static const char data[] = "xx\n`a` b\n";
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *code = NULL;

    options.body_start = 3;
    NAOS_REQUIRE_TRUE(mnd_md_parse(data, sizeof(data) - 1, &options, &doc, &diags));
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    code = find_kind(doc, MND_MD_CODE_SPAN);
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_CHECK_UINT_EQ(3, code->span.start);
    NAOS_CHECK_UINT_EQ(6, code->span.end);
    NAOS_CHECK_UINT_EQ(2, code->start_line);
    mnd_md_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* 8.2: インラインの段階のメモリ不足で、false・*out が NULL・診断・確保と解放の回数が等しいこと */
NAOS_TEST(inline_out_of_memory)
{
    static const char data[] =
        "> a `b\n> c` <d\n> e=\"f\"> &amp; \\* <http://x> \n> g  \nh\\\ni\n\n# j `k`\n\n***l** m* _n_ *o\n";
    size_t fail_at = 0;
    size_t failures = 0;
    bool succeeded = false;

    for (fail_at = 1; (fail_at <= OOM_MAX_STEPS) && !succeeded; fail_at++)
    {
        mnd_md_test_alloc_t ta;
        mnd_md_options_t options = MND_MD_OPTIONS_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        mnd_md_doc_t *doc = (mnd_md_doc_t *)&options;

        mnd_md_test_alloc_init(&ta, fail_at);
        options.allocator = &ta.allocator;
        if (mnd_md_parse(data, sizeof(data) - 1, &options, &doc, &diags))
        {
            succeeded = true;
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_REQUIRE_NOT_NULL(doc);
            NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
            NAOS_CHECK_NOT_NULL(find_kind(doc, MND_MD_HTML_INLINE));
            NAOS_CHECK_NOT_NULL(find_kind(doc, MND_MD_STRONG));
            mnd_md_doc_free(doc);
        }
        else
        {
            failures++;
            NAOS_CHECK_NULL(doc);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
            NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
        }
        NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(&ta));
        mnd_diag_list_free(&diags);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(failures, 0);
}

/* unit を count 回繰り返した文字列を確保する（呼び出し側が free する）。失敗したら NULL */
static char *repeat(const char *unit, size_t count, size_t *len)
{
    size_t unit_len = strlen(unit);
    char *text = (char *)malloc((unit_len * count) + 1);
    size_t i = 0;

    if (text == NULL)
    {
        return NULL;
    }
    for (i = 0; i < count; i++)
    {
        memcpy(text + (i * unit_len), unit, unit_len);
    }
    text[unit_len * count] = '\0';
    *len = unit_len * count;

    return text;
}

/*
 * 8.3: 閉じのないコメント・処理命令・CDATA・宣言・バッククォートの繰り返しで、解析が終わり tree_check を
 * 満たすこと（二乗の時間がかかると、このテストが目に見えて遅くなる）
 */
NAOS_TEST(inline_pathological_unclosed)
{
    static const char *const units[] = {
        "x <!-- ", "x <? ", "x <![CDATA[ ", "x <!A ", "`a ", "a``` ", "x <a b=\"", "x <a b='"
    };
    size_t i = 0;

    for (i = 0; i < (sizeof(units) / sizeof(units[0])); i++)
    {
        size_t len = 0;
        char *text = repeat(units[i], PATHOLOGICAL_REPEAT, &len);
        mnd_md_doc_t *doc = NULL;

        NAOS_REQUIRE_NOT_NULL(text);
        doc = parse_len_ok(text, len);
        NAOS_CHECK_NOT_NULL(doc);
        mnd_md_doc_free(doc);
        free(text);
    }
}

/* 強調のノードの種類・範囲・区切り記号の文字・開きと閉じの区切り記号の範囲を確かめる */
static void check_emph(const mnd_md_node_t *node, mnd_md_kind_t kind, char delim, size_t open_start,
                       size_t close_end)
{
    size_t width = (kind == MND_MD_STRONG) ? 2 : 1;

    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_CHECK_INT_EQ((int)kind, (int)node->kind);
    NAOS_CHECK_INT_EQ(delim, node->as.emph.delim);
    NAOS_CHECK_UINT_EQ(open_start, node->span.start);
    NAOS_CHECK_UINT_EQ(close_end, node->span.end);
    NAOS_CHECK_UINT_EQ(open_start, node->as.emph.open.start);
    NAOS_CHECK_UINT_EQ(open_start + width, node->as.emph.open.end);
    NAOS_CHECK_UINT_EQ(close_end - width, node->as.emph.close.start);
    NAOS_CHECK_UINT_EQ(close_end, node->as.emph.close.end);
    NAOS_CHECK_TRUE(node->lines == NULL);
}

/* 5.1、5.9: `*a*` と `__a__` の強調と強い強調 */
NAOS_TEST(inline_emphasis_simple)
{
    static const expected_node_t inner_a[] = { { MND_MD_TEXT, 1, 2 } };
    static const expected_node_t inner_b[] = { { MND_MD_TEXT, 2, 3 } };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    doc = parse_ok("*a*\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    check_emph(child_at(para, 0), MND_MD_EMPH, '*', 0, 3);
    check_children(child_at(para, 0), inner_a, 1);
    NAOS_CHECK_UINT_EQ(1, child_at(para, 0)->start_line);
    NAOS_CHECK_UINT_EQ(1, child_at(para, 0)->end_line);
    mnd_md_doc_free(doc);

    doc = parse_ok("__a__\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    check_emph(child_at(para, 0), MND_MD_STRONG, '_', 0, 5);
    check_children(child_at(para, 0), inner_b, 1);
    mnd_md_doc_free(doc);
}

/* 5.9: `***a***` は強調の中の強い強調（内側が開きの後ろの 2 つと閉じの前の 2 つを使う） */
NAOS_TEST(inline_emphasis_triple)
{
    static const expected_node_t inner[] = { { MND_MD_TEXT, 3, 4 } };
    mnd_md_doc_t *doc = parse_ok("***a***\n");
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *em = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    em = child_at(para, 0);
    check_emph(em, MND_MD_EMPH, '*', 0, 7);
    NAOS_REQUIRE_UINT_EQ(1, child_count(em));
    check_emph(child_at(em, 0), MND_MD_STRONG, '*', 1, 6);
    check_children(child_at(em, 0), inner, 1);
    mnd_md_doc_free(doc);
}

/* 5.9: `*a **b** c*` は強調の中に強い強調とテキストを持つ */
NAOS_TEST(inline_emphasis_nested)
{
    static const expected_node_t inner[] = { { MND_MD_TEXT, 5, 6 } };
    mnd_md_doc_t *doc = parse_ok("*a **b** c*\n");
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *em = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    em = child_at(para, 0);
    check_emph(em, MND_MD_EMPH, '*', 0, 11);
    NAOS_REQUIRE_UINT_EQ(3, child_count(em));
    NAOS_CHECK_INT_EQ((int)MND_MD_TEXT, (int)child_at(em, 0)->kind);
    NAOS_CHECK_UINT_EQ(1, child_at(em, 0)->span.start);
    NAOS_CHECK_UINT_EQ(3, child_at(em, 0)->span.end);
    check_emph(child_at(em, 1), MND_MD_STRONG, '*', 3, 8);
    check_children(child_at(em, 1), inner, 1);
    NAOS_CHECK_INT_EQ((int)MND_MD_TEXT, (int)child_at(em, 2)->kind);
    NAOS_CHECK_UINT_EQ(8, child_at(em, 2)->span.start);
    NAOS_CHECK_UINT_EQ(10, child_at(em, 2)->span.end);
    mnd_md_doc_free(doc);
}

/*
 * 5.9: 強調にならない例。`_` の単語の中（`_a_b`）、句読点の隣接（`a*"b"*`）、Unicode の句読点（S の `£`）、
 * Unicode の空白（Zs の U+00A0 とタブ）の隣。区切り記号は隣のテキストと 1 つのテキストになる
 */
NAOS_TEST(inline_emphasis_not_formed)
{
    static const expected_node_t intraword[] = { { MND_MD_TEXT, 0, 4 } };
    static const expected_node_t punct[] = { { MND_MD_TEXT, 0, 6 } };
    static const expected_node_t pound[] = { { MND_MD_TEXT, 0, 7 } };
    static const expected_node_t nbsp[] = { { MND_MD_TEXT, 0, 7 } };
    static const expected_node_t tab[] = { { MND_MD_TEXT, 0, 6 } };
    static const expected_node_t unclosed[] = { { MND_MD_TEXT, 0, 6 } };

    check_paragraph("_a_b\n", intraword, 1);
    check_paragraph("a*\"b\"*\n", punct, 1);
    check_paragraph("*\xC2\xA3*bra\n", pound, 1);
    check_paragraph("*\xC2\xA0" "a\xC2\xA0*\n", nbsp, 1);
    check_paragraph("a *\tb*\n", tab, 1);
    check_paragraph("**a b_\n", unclosed, 1);
}

/* 5.9: NUL は U+FFFD（句読点）として隣接を判定する（`a*\0b*` は句読点の前の `*` が開きにならない） */
NAOS_TEST(inline_emphasis_nul_is_punctuation)
{
    static const char data[] = "a*\0b*\n";
    static const expected_node_t plain[] = { { MND_MD_TEXT, 0, 5 } };
    mnd_md_doc_t *doc = parse_len_ok(data, sizeof(data) - 1);

    NAOS_REQUIRE_NOT_NULL(doc);
    check_children(find_kind(doc, MND_MD_PARAGRAPH), plain, 1);
    mnd_md_doc_free(doc);
}

/* 5.9: 句読点の隣でも、開きの前が空白か句読点なら強調になる（`*"b"*`）。`_` の単語の境目（`a _b_ c`） */
NAOS_TEST(inline_emphasis_punctuation)
{
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    doc = parse_ok("*\"b\"*\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    check_emph(child_at(para, 0), MND_MD_EMPH, '*', 0, 5);
    mnd_md_doc_free(doc);

    doc = parse_ok("a _b_ c\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(3, child_count(para));
    NAOS_CHECK_UINT_EQ(2, child_at(para, 0)->span.end);
    check_emph(child_at(para, 1), MND_MD_EMPH, '_', 2, 5);
    NAOS_CHECK_UINT_EQ(5, child_at(para, 2)->span.start);
    mnd_md_doc_free(doc);
}

/*
 * 5.9: 残った区切り記号はテキスト。開きは後ろから、閉じは前から使う（`**a*`、`*a**`）。
 * 3 の倍数の規則（`*a**b*` は強調の中に `**` が残る）
 */
NAOS_TEST(inline_emphasis_leftover_and_rule_of_three)
{
    static const expected_node_t merged[] = { { MND_MD_TEXT, 1, 5 } };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;

    doc = parse_ok("**a*\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(2, child_count(para));
    NAOS_CHECK_INT_EQ((int)MND_MD_TEXT, (int)child_at(para, 0)->kind);
    NAOS_CHECK_UINT_EQ(0, child_at(para, 0)->span.start);
    NAOS_CHECK_UINT_EQ(1, child_at(para, 0)->span.end);
    check_emph(child_at(para, 1), MND_MD_EMPH, '*', 1, 4);
    mnd_md_doc_free(doc);

    doc = parse_ok("*a**\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(2, child_count(para));
    check_emph(child_at(para, 0), MND_MD_EMPH, '*', 0, 3);
    NAOS_CHECK_INT_EQ((int)MND_MD_TEXT, (int)child_at(para, 1)->kind);
    NAOS_CHECK_UINT_EQ(3, child_at(para, 1)->span.start);
    NAOS_CHECK_UINT_EQ(4, child_at(para, 1)->span.end);
    mnd_md_doc_free(doc);

    doc = parse_ok("*a**b*\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    check_emph(child_at(para, 0), MND_MD_EMPH, '*', 0, 6);
    check_children(child_at(para, 0), merged, 1);
    mnd_md_doc_free(doc);
}

/* 5.9、5.10: 引用の中で行をまたぐ強調は、途中のプレフィックスと行末記号を範囲に含む。見出しの中の強調 */
NAOS_TEST(inline_emphasis_across_lines_and_in_heading)
{
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *em = NULL;

    doc = parse_ok("> *a\n> b*\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_PARAGRAPH);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, child_count(para));
    em = child_at(para, 0);
    check_emph(em, MND_MD_EMPH, '*', 2, 9);
    NAOS_CHECK_UINT_EQ(1, em->start_line);
    NAOS_CHECK_UINT_EQ(2, em->end_line);
    NAOS_REQUIRE_UINT_EQ(3, child_count(em));
    NAOS_CHECK_INT_EQ((int)MND_MD_SOFT_BREAK, (int)child_at(em, 1)->kind);
    mnd_md_doc_free(doc);

    doc = parse_ok("# x __y__\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    para = find_kind(doc, MND_MD_HEADING);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(2, child_count(para));
    check_emph(child_at(para, 1), MND_MD_STRONG, '_', 4, 9);
    mnd_md_doc_free(doc);
}

/* 8.3: 対にならない区切り記号の繰り返しと、対になる区切り記号の繰り返しで、解析が終わり tree_check を満たすこと */
NAOS_TEST(inline_pathological_emphasis)
{
    static const char *const units[] = { "*a", "a**b", "_a", "*a_", "**a* ", "a***", "*_" };
    size_t i = 0;

    for (i = 0; i < (sizeof(units) / sizeof(units[0])); i++)
    {
        size_t len = 0;
        char *text = repeat(units[i], PATHOLOGICAL_REPEAT, &len);
        mnd_md_doc_t *doc = NULL;

        NAOS_REQUIRE_NOT_NULL(text);
        doc = parse_len_ok(text, len);
        NAOS_CHECK_NOT_NULL(doc);
        mnd_md_doc_free(doc);
        free(text);
    }
}

NAOS_SUITE(inline,
    NAOS_ADD_TEST(inline_text),
    NAOS_ADD_TEST(inline_soft_break),
    NAOS_ADD_TEST(inline_hard_break),
    NAOS_ADD_TEST(inline_final_spaces_belong_to_no_node),
    NAOS_ADD_TEST(inline_escape),
    NAOS_ADD_TEST(inline_char_refs),
    NAOS_ADD_TEST(inline_code_span),
    NAOS_ADD_TEST(inline_code_span_across_lines_in_block_quote),
    NAOS_ADD_TEST(inline_autolinks),
    NAOS_ADD_TEST(inline_raw_html),
    NAOS_ADD_TEST(inline_raw_html_across_lines_in_block_quote),
    NAOS_ADD_TEST(inline_headings),
    NAOS_ADD_TEST(inline_not_parsed_in_other_leaves),
    NAOS_ADD_TEST(inline_in_containers_and_nul),
    NAOS_ADD_TEST(inline_ranges_with_body_start),
    NAOS_ADD_TEST(inline_out_of_memory),
    NAOS_ADD_TEST(inline_pathological_unclosed),
    NAOS_ADD_TEST(inline_emphasis_simple),
    NAOS_ADD_TEST(inline_emphasis_triple),
    NAOS_ADD_TEST(inline_emphasis_nested),
    NAOS_ADD_TEST(inline_emphasis_not_formed),
    NAOS_ADD_TEST(inline_emphasis_nul_is_punctuation),
    NAOS_ADD_TEST(inline_emphasis_punctuation),
    NAOS_ADD_TEST(inline_emphasis_leftover_and_rule_of_three),
    NAOS_ADD_TEST(inline_emphasis_across_lines_and_in_heading),
    NAOS_ADD_TEST(inline_pathological_emphasis)
)
