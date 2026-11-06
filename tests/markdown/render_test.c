/* html_render: commonmark.js と同じ規則の HTML 出力 */

#include <naos/naos.h>

#include "html_render.h"
#include "tree_check.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_dialect.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 入力の Markdown と、期待する HTML（spec の例は番号を付ける。0 は spec の例でないもの） */
typedef struct render_case
{
    int example;
    const char *markdown;
    const char *html;
} render_case_t;

/* data[0..len) を dialect（NULL 可）で解析して HTML にし、expected と完全に一致することを確かめる */
static void check_render_len(const char *data, size_t len, const mnd_md_dialect_t *dialect, int example,
                             const char *expected)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    mnd_buf_t out = MND_BUF_INIT;
    bool ok = false;

    options.dialect = dialect;
    ok = mnd_md_parse(data, len, &options, &doc, &diags);
    mnd_diag_list_free(&diags);
    NAOS_CHECK_TRUE(ok);
    if (!ok)
    {
        NAOS_NOTE("example %d: parse failed", example);
        return;
    }

    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    ok = mnd_md_html_render(doc, &out);
    NAOS_CHECK_TRUE(ok);
    if ((out.len != strlen(expected)) || (memcmp(mnd_buf_cstr(&out), expected, out.len) != 0))
    {
        NAOS_NOTE("example %d: markdown \"%s\"", example, data);
        NAOS_NOTE("example %d: actual \"%s\"", example, mnd_buf_cstr(&out));
    }

    NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&out), out.len, expected, strlen(expected));
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

static void check_cases(const render_case_t *cases, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        check_render_len(cases[i].markdown, strlen(cases[i].markdown), NULL, cases[i].example,
                         cases[i].html);
    }
}

#define CHECK_CASES(cases) check_cases((cases), sizeof(cases) / sizeof((cases)[0]))

/* 段落（Paragraphs、Hard line breaks、Soft line breaks、Textual content、Entity references） */
static const render_case_t PARAGRAPH_CASES[] = {
    {219, "aaa\n\nbbb\n", "<p>aaa</p>\n<p>bbb</p>\n"},
    {220, "aaa\nbbb\n\nccc\nddd\n", "<p>aaa\nbbb</p>\n<p>ccc\nddd</p>\n"},
    {633, "foo  \nbaz\n", "<p>foo<br />\nbaz</p>\n"},
    {646, "### foo\\\n", "<h3>foo\\</h3>\n"},
    {26, "&#35; &#1234; &#992; &#0;\n", "<p># \323\222 \317\240 \357\277\275</p>\n"},
    {43, "***\n---\n___\n", "<hr />\n<hr />\n<hr />\n"},
};

/* 見出し（ATX headings、Setext headings） */
static const render_case_t HEADING_CASES[] = {
    {62, "# foo\n## foo\n### foo\n#### foo\n##### foo\n###### foo\n",
     "<h1>foo</h1>\n<h2>foo</h2>\n<h3>foo</h3>\n<h4>foo</h4>\n<h5>foo</h5>\n<h6>foo</h6>\n"},
    {80, "Foo *bar*\n=========\n\nFoo *bar*\n---------\n", "<h1>Foo <em>bar</em></h1>\n<h2>Foo <em>bar</em></h2>\n"},
};

/* コード（Tabs、Indented code blocks、Fenced code blocks、Code spans） */
static const render_case_t CODE_CASES[] = {
    {1, "\tfoo\tbaz\t\tbim\n", "<pre><code>foo\tbaz\t\tbim\n</code></pre>\n"},
    {107, "    a simple\n      indented code block\n", "<pre><code>a simple\n  indented code block\n</code></pre>\n"},
    {142, "```ruby\ndef foo(x)\n  return 3\nend\n```\n",
     "<pre><code class=\"language-ruby\">def foo(x)\n  return 3\nend\n</code></pre>\n"},
    {143, "~~~~    ruby startline=3 $%@#$\ndef foo(x)\n  return 3\nend\n~~~~~~~\n",
     "<pre><code class=\"language-ruby\">def foo(x)\n  return 3\nend\n</code></pre>\n"},
    {328, "`foo`\n", "<p><code>foo</code></p>\n"},
    {336, "``\nfoo \n``\n", "<p><code>foo </code></p>\n"},
    {348, "`foo\n", "<p>`foo</p>\n"},
    /* 内容が行の境目で始まるか終わるコードスパン（4.1 の注意）。境目の改行が空白になる */
    {0, "``\na``\n", "<p><code> a</code></p>\n"},
    {0, "`` \nfoo\n``\n", "<p><code> foo</code></p>\n"},
    /* 情報文字列のアンエスケープとエスケープ、コードの中のエスケープ */
    {0, "``` a\\_b&amp;\"\n<&>\n```\n", "<pre><code class=\"language-a_b&amp;&quot;\">&lt;&amp;&gt;\n</code></pre>\n"},
    {0, "```\n```\n", "<pre><code></code></pre>\n"},
};

/* 引用（Block quotes） */
static const render_case_t QUOTE_CASES[] = {
    {228, "> # Foo\n> bar\n> baz\n", "<blockquote>\n<h1>Foo</h1>\n<p>bar\nbaz</p>\n</blockquote>\n"},
    {229, "># Foo\n>bar\n> baz\n", "<blockquote>\n<h1>Foo</h1>\n<p>bar\nbaz</p>\n</blockquote>\n"},
    {253, "A paragraph\nwith two lines.\n\n    indented code\n\n> A block quote.\n",
     "<p>A paragraph\nwith two lines.</p>\n<pre><code>indented code\n</code></pre>\n<blockquote>\n"
     "<p>A block quote.</p>\n</blockquote>\n"},
};

/* リスト（List items、Lists） */
static const render_case_t LIST_CASES[] = {
    {301, "- foo\n- bar\n+ baz\n", "<ul>\n<li>foo</li>\n<li>bar</li>\n</ul>\n<ul>\n<li>baz</li>\n</ul>\n"},
    {306, "- foo\n\n- bar\n\n\n- baz\n",
     "<ul>\n<li>\n<p>foo</p>\n</li>\n<li>\n<p>bar</p>\n</li>\n<li>\n<p>baz</p>\n</li>\n</ul>\n"},
    {265, "123456789. ok\n", "<ol start=\"123456789\">\n<li>ok</li>\n</ol>\n"},
    {9, " - foo\n   - bar\n\t - baz\n",
     "<ul>\n<li>foo\n<ul>\n<li>bar\n<ul>\n<li>baz</li>\n</ul>\n</li>\n</ul>\n</li>\n</ul>\n"},
    {320, "* a\n  > b\n  >\n* c\n", "<ul>\n<li>a\n<blockquote>\n<p>b</p>\n</blockquote>\n</li>\n<li>c</li>\n</ul>\n"},
    {7, "-\t\tfoo\n", "<ul>\n<li>\n<pre><code>  foo\n</code></pre>\n</li>\n</ul>\n"},
    {290, "  1.  A paragraph\nwith two lines.\n\n          indented code\n\n      > A block quote.\n",
     "<ol>\n<li>\n<p>A paragraph\nwith two lines.</p>\n<pre><code>indented code\n</code></pre>\n<blockquote>\n"
     "<p>A block quote.</p>\n</blockquote>\n</li>\n</ol>\n"},
};

/* HTML（HTML blocks、Raw HTML） */
static const render_case_t HTML_CASES[] = {
    {148, "<table><tr><td>\n<pre>\n**Hello**,\n\n_world_.\n</pre>\n</td></tr></table>\n",
     "<table><tr><td>\n<pre>\n**Hello**,\n<p><em>world</em>.\n</pre></p>\n</td></tr></table>\n"},
    {190, "<table>\n\n<tr>\n\n<td>\nHi\n</td>\n\n</tr>\n\n</table>\n",
     "<table>\n<tr>\n<td>\nHi\n</td>\n</tr>\n</table>\n"},
    {613, "<a><bab><c2c>\n", "<p><a><bab><c2c></p>\n"},
    {617, "Foo <responsive-image src=\"foo.jpg\" />\n", "<p>Foo <responsive-image src=\"foo.jpg\" /></p>\n"},
};

/* リンク（Links、Link reference definitions、Autolinks、Backslash escapes） */
static const render_case_t LINK_CASES[] = {
    {483, "[link](/uri)\n", "<p><a href=\"/uri\">link</a></p>\n"},
    {486, "[link](<>)\n", "<p><a href=\"\">link</a></p>\n"},
    {502, "[link](foo\\bar)\n", "<p><a href=\"foo%5Cbar\">link</a></p>\n"},
    {505, "[link](/url \"title\")\n[link](/url 'title')\n[link](/url (title))\n",
     "<p><a href=\"/url\" title=\"title\">link</a>\n<a href=\"/url\" title=\"title\">link</a>\n"
     "<a href=\"/url\" title=\"title\">link</a></p>\n"},
    {519, "[foo *[bar [baz](/uri)](/uri)*](/uri)\n",
     "<p>[foo <em>[bar <a href=\"/uri\">baz</a>](/uri)</em>](/uri)</p>\n"},
    {32, "[foo](/f&ouml;&ouml; \"f&ouml;&ouml;\")\n",
     "<p><a href=\"/f%C3%B6%C3%B6\" title=\"f\303\266\303\266\">foo</a></p>\n"},
    {197, "[foo]: /url 'title\n\nwith blank line'\n\n[foo]\n",
     "<p>[foo]: /url 'title</p>\n<p>with blank line'</p>\n<p>[foo]</p>\n"},
    {20, "<https://example.com?find=\\*>\n",
     "<p><a href=\"https://example.com?find=%5C*\">https://example.com?find=\\*</a></p>\n"},
    {594, "<http://foo.bar.baz>\n", "<p><a href=\"http://foo.bar.baz\">http://foo.bar.baz</a></p>\n"},
    {606, "<foo\\+@bar.example.com>\n", "<p>&lt;foo+@bar.example.com&gt;</p>\n"},
    /* 参照形式（定義の中のタイトルが行をまたぐ）、メールの自動リンク、既存の %XX と % の扱い */
    {0, "[a]\n\n[a]: /u%20v%zz\n\"t\nu\"\n", "<p><a href=\"/u%20v%25zz\" title=\"t\nu\">a</a></p>\n"},
    {0, "<a@b.c>\n", "<p><a href=\"mailto:a@b.c\">a@b.c</a></p>\n"},
    {0, "[x](<a b\"c>)\n", "<p><a href=\"a%20b%22c\">x</a></p>\n"},
};

/* 画像（Images） */
static const render_case_t IMAGE_CASES[] = {
    {573, "![foo *bar*]\n\n[foo *bar*]: train.jpg \"train & tracks\"\n",
     "<p><img src=\"train.jpg\" alt=\"foo bar\" title=\"train &amp; tracks\" /></p>\n"},
    {577, "![foo *bar*][foobar]\n\n[FOOBAR]: train.jpg \"train & tracks\"\n",
     "<p><img src=\"train.jpg\" alt=\"foo bar\" title=\"train &amp; tracks\" /></p>\n"},
    {578, "![foo](train.jpg)\n", "<p><img src=\"train.jpg\" alt=\"foo\" /></p>\n"},
    {580, "![foo](<url>)\n", "<p><img src=\"url\" alt=\"foo\" /></p>\n"},
    /* alt の中のリンク、画像、コード、改行は文字列だけになる */
    {0, "![a [b](c) ![d](e) `f` g  \nh\ni](j)\n", "<p><img src=\"j\" alt=\"a b d f g\nh\ni\" /></p>\n"},
};

/* 強調（Emphasis and strong emphasis） */
static const render_case_t EMPH_CASES[] = {
    {350, "*foo bar*\n", "<p><em>foo bar</em></p>\n"},
    {351, "a * foo bar*\n", "<p>a * foo bar*</p>\n"},
    {412, "*foo**bar*\n", "<p><em>foo**bar</em></p>\n"},
    {0, "**a _b_ c**\n", "<p><strong>a <em>b</em> c</strong></p>\n"},
};

NAOS_TEST(paragraphs)
{
    CHECK_CASES(PARAGRAPH_CASES);
}

NAOS_TEST(headings)
{
    CHECK_CASES(HEADING_CASES);
}

NAOS_TEST(code)
{
    CHECK_CASES(CODE_CASES);
}

NAOS_TEST(block_quotes)
{
    CHECK_CASES(QUOTE_CASES);
}

NAOS_TEST(lists)
{
    CHECK_CASES(LIST_CASES);
}

NAOS_TEST(html)
{
    CHECK_CASES(HTML_CASES);
}

NAOS_TEST(links)
{
    CHECK_CASES(LINK_CASES);
}

NAOS_TEST(images)
{
    CHECK_CASES(IMAGE_CASES);
}

NAOS_TEST(emphasis)
{
    CHECK_CASES(EMPH_CASES);
}

/* テキスト、コード、HTML ブロック、URL の NUL は U+FFFD になる */
NAOS_TEST(nul_becomes_replacement_character)
{
    static const char para[] = "a\0b `c\0d` <x\0>\n";
    static const char code[] = "```\n\0\n```\n";
    static const char html[] = "<div>\0\n";
    static const char link[] = "[a](b\0c)\n";

    check_render_len(para, sizeof(para) - 1, NULL, 0,
                     "<p>a\357\277\275b <code>c\357\277\275d</code> &lt;x\357\277\275&gt;</p>\n");
    check_render_len(code, sizeof(code) - 1, NULL, 0, "<pre><code>\357\277\275\n</code></pre>\n");
    check_render_len(html, sizeof(html) - 1, NULL, 0, "<div>\357\277\275\n");
    check_render_len(link, sizeof(link) - 1, NULL, 0, "<p><a href=\"b%EF%BF%BDc\">a</a></p>\n");
}

/* テキストの & < > " のエスケープと、URL の ASCII 以外の文字のパーセントエンコード */
NAOS_TEST(escapes_text_and_encodes_urls)
{
    static const render_case_t cases[] = {
        {0, "a & b < c > d \" e\n", "<p>a &amp; b &lt; c &gt; d &quot; e</p>\n"},
        {0, "[\303\266](/\303\266?a=1&b=[2])\n", "<p><a href=\"/%C3%B6?a=1&amp;b=%5B2%5D\">\303\266</a></p>\n"},
    };

    CHECK_CASES(cases);
}

/* 方言の種類のノードは、子だけを出力する（5.2） */
#define KIND_MARK ((mnd_md_kind_t)(MND_MD_KIND_DIALECT + 1))

static bool mark_match_delims(const mnd_md_delim_run_t *opener, const mnd_md_delim_run_t *closer, size_t *use)
{
    if ((opener->length < 2) || (closer->length < 2))
    {
        return false;
    }

    *use = 2;

    return true;
}

NAOS_TEST(dialect_kinds_render_children_only)
{
    static const mnd_md_inline_rule_t rules[1] = {
        { KIND_MARK, '=', true, NULL, mark_match_delims }
    };
    static const mnd_md_dialect_t dialect = { "mark", NULL, 0, rules, 1, NULL };
    static const char data[] = "a ==b *c*== d\n";

    check_render_len(data, sizeof(data) - 1, &dialect, 0, "<p>a b <em>c</em> d</p>\n");
}

/* 出力は out の既存の内容の後ろに追加する。空の文書は何も出力しない */
NAOS_TEST(appends_and_empty_document)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_md_parse(NULL, 0, NULL, &doc, &diags));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "x"));
    NAOS_CHECK_TRUE(mnd_md_html_render(doc, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "x");
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* 深い入れ子でも再帰を使わずに出力できる */
NAOS_TEST(deep_nesting_without_recursion)
{
    enum { DEPTH = 10000 };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    mnd_buf_t in = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    size_t opens = 0;
    size_t i;

    for (i = 0; i < DEPTH; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&in, '>'));
    }

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&in, " x\n"));
    NAOS_REQUIRE_TRUE(mnd_md_parse(in.data, in.len, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(mnd_md_html_render(doc, &out));
    for (i = 0; (i + 12) <= out.len; i++)
    {
        if (memcmp(out.data + i, "<blockquote>", 12) == 0)
        {
            opens++;
        }
    }

    NAOS_CHECK_UINT_EQ(DEPTH, opens);
    mnd_buf_free(&in);
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(render,
    NAOS_ADD_TEST(paragraphs),
    NAOS_ADD_TEST(headings),
    NAOS_ADD_TEST(code),
    NAOS_ADD_TEST(block_quotes),
    NAOS_ADD_TEST(lists),
    NAOS_ADD_TEST(html),
    NAOS_ADD_TEST(links),
    NAOS_ADD_TEST(images),
    NAOS_ADD_TEST(emphasis),
    NAOS_ADD_TEST(nul_becomes_replacement_character),
    NAOS_ADD_TEST(escapes_text_and_encodes_urls),
    NAOS_ADD_TEST(dialect_kinds_render_children_only),
    NAOS_ADD_TEST(appends_and_empty_document),
    NAOS_ADD_TEST(deep_nesting_without_recursion)
)
