/* html_normalize: normalize.py と同じ HTML の正規化 */

#include <naos/naos.h>

#include "html_normalize.h"

#include "foundation/buf.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct normalize_case
{
    const char *input;
    const char *expected;
} normalize_case_t;

/*
 * 期待値は、CommonMark 0.31.2 の test/normalize.py の normalize_html を Python 3.13.18 で実行した結果
 * （UTF-8 のバイト列を 8 進のエスケープで書く）
 */

/* normalize_html の docstring の doctest の例 */
static const normalize_case_t DOCTEST_CASES[] = {
    {"<p>a  \t b</p>", "<p>a b</p>"},
    {"<p>a  \t\nb</p>", "<p>a b</p>"},
    {"<p>a  b</p>", "<p>a b</p>"},
    {" <p>a  b</p>", "<p>a b</p>"},
    {"<p>a  b</p> ", "<p>a b</p>"},
    {"\n\t<p>\n\t\ta  b\t\t</p>\n\t", "<p>a b</p>"},
    {"<i>a  b</i> ", "<i>a b</i> "},
    {"<br />", "<br>"},
    {"<a title=\"bar\" HREF=\"foo\">x</a>", "<a href=\"foo\" title=\"bar\">x</a>"},
    {"&forall;&amp;&gt;&lt;&quot;", "\342\210\200&amp;&gt;&lt;&quot;"},
};

/* 空白、ブロックレベルのタグ、pre、br */
static const normalize_case_t WHITESPACE_CASES[] = {
    {"<p>a<br />\nb</p>", "<p>a<br>b</p>"},
    {"<p>a<br>\n\nb</p>", "<p>a<br>b</p>"},
    {"<br>\nx", "<br>x"},
    {"<pre><code>a  b\n\n  c\n</code></pre>\n", "<pre><code>a  b\n\n  c\n</code></pre>"},
    {"<pre>\n  x  </pre>  <p> y </p>", "<pre>\n  x  </pre><p>y</p>"},
    {"<p>x</p>\n<pre>a</pre>\n<p>y</p>", "<p>x</p><pre>a</pre><p>y</p>"},
    {"<ul>\n<li>\n<p>a</p>\n</li>\n<li>b</li>\n</ul>\n<blockquote>\n<p>q</p>\n</blockquote>\n",
     "<ul><li><p>a</p></li><li>b</li></ul><blockquote><p>q</p></blockquote>"},
    {"<em>a</em>\n<p>b</p>\n<h1> T </h1>", "<em>a</em><p>b</p><h1>T</h1>"},
    {"<p>a</p>  <em> b </em>  <p>c</p>", "<p>a</p><em> b </em><p>c</p>"},
    {"<p>a</p>\n\n<p>b</p>", "<p>a</p><p>b</p>"},
    {"<hr />\n<p>a</p>", "<hr><p>a</p>"},
    {"<td> a </td>", "<td>a</td>"},
    {"<DIV CLASS=\"A\">\n  <P>x</P>\n</DIV>", "<div class=\"A\"><p>x</p></div>"},
    /* Unicode の空白（U+00A0、U+3000、U+2003）もまとめ、取り除く */
    {"<p>a\302\240 \343\200\200b</p>", "<p>a b</p>"},
    {"<p>\342\200\203a\342\200\203</p>", "<p>a</p>"},
    {"<p>&nbsp;</p>", "<p></p>"},
    {"a < b", "a  b"},
};

/* 属性の並べ替えと値の書き直し */
static const normalize_case_t ATTRIBUTE_CASES[] = {
    {"<img src=\"a.png\" alt=\"b\" title='c \"d\"' />", "<img alt=\"b\" src=\"a.png\" title=\"c &quot;d&quot;\">"},
    {"<input disabled type=checkbox data-x='a&amp;b' CHECKED=\"\" />",
     "<input checked=\"\" data-x=\"a&amp;b\" disabled type=\"checkbox\">"},
    {"<a href=\"/url?a=1&amp;b=2\" title=\"&quot;t&quot; &lt;&#39;&gt;\">l</a>",
     "<a href=\"/url?a=1&amp;b=2\" title=\"&quot;t&quot; &lt;&#x27;&gt;\">l</a>"},
    {"<a href=\"x&copy y&copy=z&#x41;&#66=\">l</a>", "<a href=\"x\302\251 y&amp;copy=zAB=\">l</a>"},
    {"<a href=\"&amp\" title='&amp=x&ampx;&AMP'>t</a>", "<a href=\"&amp;\" title=\"&amp;amp=x&amp;ampx;&amp;\">t</a>"},
    {"<img src=x alt='&#0;&#128;&#xFDD0;&#xD800;&#x110000;'>",
     "<img alt=\"\357\277\275\342\202\254\357\277\275\357\277\275\" src=\"x\">"},
    {"<a  b='1'  a=\"2\"  c>t</a>", "<a a=\"2\" b=\"1\" c>t</a>"},
    {"<a b=\"2\" b=\"1\">", "<a b=\"1\" b=\"2\">"},
    {"<a title=\">\">x</a>", "<a title=\"&gt;\">x</a>"},
    {"<a b= \"c>d</a>", "<a \"c b=\"\">d</a>"},
    {"<p>`<a href=\"`\">`</p>", "<p>`<a href=\"`\">`</p>"},
    {"</> <a/> <b/>c", " <a> <b>c"},
};

/* 実体参照と数値文字参照 */
static const normalize_case_t REFERENCE_CASES[] = {
    {"<p>&ouml; &#x27; &#39; &#X41; &nosuch; &amp</p>", "<p>\303\266 ' ' &X41; &nosuch; &amp;</p>"},
    {"&#x41;&#65;&#x1F600;", "AA\360\237\230\200"},
    {"<p>&#1114112; &#x110000; &#12a; &a.b x</p>", "<p>&1114112; &x110000; &12a; &a.b x</p>;"},
    {"<p>a &b-c; d</p>", "<p>a &b-c; d</p>"},
    {"&a.b x", "&a.b; x"},
    {"&a.b", "&a;.b"},
    {"x &a-b-", "x &a-b;-"},
    {"&#x41", "A"},
    {"<p>a</p>&#65", "<p>a</p>A"},
    {"x &foo", "x &foo;"},
    {"<p>x &#12", "<p>x \014"},
    {"<p>&MadeUpEntity; &Aacute;</p>", "<p>&MadeUpEntity; \303\201</p>"},
    {"&#", "&#"},
    {"x&#x", "x&#x"},
    {"&#xZ1", "&#xZ1"},
    {"a&", "a&"},
};

/* コメント、処理命令、宣言、CDATA、生テキストの要素、閉じない構文 */
static const normalize_case_t MARKUP_CASES[] = {
    {"<p>x</p>\n<!-- c  m -->\n<?php  x ?>\n<!DOCTYPE html>\n<![CDATA[\na  b\n]]>\n<![CDATA[a  b]]>\n",
     "<p>x</p><!-- c  m --> <?php  x ?> <!DOCTYPE html> <![CDATA[\na  b\n]]> <![CDATA[a  b]]> "},
    {"<!-->x<!--->y<!--a--!>z", "<!---->x<!---->y<!--a-->z"},
    {"</ p>q", "<!-- p-->q"},
    {"<p>x</p><![CDATA[<b>]]>&amp", "<p>x</p><![CDATA[<b>]]>&amp;"},
    {"<script>a  <b>  &amp;</script>  <p>z</p>", "<script>a <b> &amp;</script><p>z</p>"},
    {"<style>p  {}</style>  <p>q</p>", "<style>p {}</style><p>q</p>"},
    {"<textarea>a &amp;  <b></textarea>", "<textarea>a &amp; <b></textarea>"},
    {"<title>a  &amp; b</title>", "<title>a &amp; b</title>"},
    {"<plaintext>a  <b>  c", "<plaintext>a <b> c"},
    {"<a title=\">x</a> y", ""},
    {"<?x", "?x"},
    {"<!--x", "!--x"},
    {"<!x", "!x"},
    {"<![CDATA[x\n", "![CDATA[x "},
    {"<!DOCTYPE x", "!DOCTYPE x"},
    {"a </", "a /"},
    {"a </b", "a /b"},
};

static void check_cases(const normalize_case_t *cases, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        mnd_buf_t out = MND_BUF_INIT;
        bool ok = mnd_md_html_normalize(cases[i].input, strlen(cases[i].input), &out);

        if (!ok || (out.len != strlen(cases[i].expected))
            || (memcmp(mnd_buf_cstr(&out), cases[i].expected, out.len) != 0))
        {
            NAOS_NOTE("case %zu: input \"%s\"", i, cases[i].input);
        }
        NAOS_CHECK_TRUE(ok);
        NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&out), out.len, cases[i].expected, strlen(cases[i].expected));
        mnd_buf_free(&out);
    }
}

#define CHECK_CASES(cases) check_cases((cases), sizeof(cases) / sizeof((cases)[0]))

NAOS_TEST(doctest_examples)
{
    CHECK_CASES(DOCTEST_CASES);
}

NAOS_TEST(whitespace_and_block_tags)
{
    CHECK_CASES(WHITESPACE_CASES);
}

NAOS_TEST(attributes)
{
    CHECK_CASES(ATTRIBUTE_CASES);
}

NAOS_TEST(references)
{
    CHECK_CASES(REFERENCE_CASES);
}

NAOS_TEST(markup_and_unclosed_constructs)
{
    CHECK_CASES(MARKUP_CASES);
}

NAOS_TEST(html5_entity_names)
{
    /*
     * normalize.py との意図した違い: 実体名は HTML5 の表で文字に戻す。
     * normalize.py（HTML4 の name2codepoint）では `&ngE;` はそのまま残る
     */
    static const normalize_case_t cases[] = {
        {"<p>&ngE; &ouml;</p>", "<p>\342\211\247\314\270 \303\266</p>"},
    };

    CHECK_CASES(cases);
}

NAOS_TEST(appends_after_existing_content)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "x "));
    NAOS_REQUIRE_TRUE(mnd_md_html_normalize("<p>a</p>\n", 9, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "x <p>a</p>");
    mnd_buf_free(&out);
}

NAOS_TEST(empty_input)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_md_html_normalize(NULL, 0, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "");
    mnd_buf_free(&out);
}

NAOS_TEST(both_sides_compare_equal)
{
    /* spec のテストでの使い方: 改行の位置だけが違う 2 つの HTML は、正規化すると等しい */
    static const char rendered[] = "<ul>\n<li>a</li>\n<li>b <em>c</em></li>\n</ul>\n";
    static const char expected[] = "<ul><li>a</li><li>b <em>c</em></li></ul>";
    mnd_buf_t a = MND_BUF_INIT;
    mnd_buf_t b = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_md_html_normalize(rendered, sizeof(rendered) - 1, &a));
    NAOS_REQUIRE_TRUE(mnd_md_html_normalize(expected, sizeof(expected) - 1, &b));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&a), mnd_buf_cstr(&b));
    mnd_buf_free(&a);
    mnd_buf_free(&b);
}

NAOS_SUITE(normalize,
    NAOS_ADD_TEST(doctest_examples),
    NAOS_ADD_TEST(whitespace_and_block_tags),
    NAOS_ADD_TEST(attributes),
    NAOS_ADD_TEST(references),
    NAOS_ADD_TEST(markup_and_unclosed_constructs),
    NAOS_ADD_TEST(html5_entity_names),
    NAOS_ADD_TEST(appends_after_existing_content),
    NAOS_ADD_TEST(empty_input),
    NAOS_ADD_TEST(both_sides_compare_equal)
)
