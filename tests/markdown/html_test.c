/* Html: HTML の字句と HTML ブロックの条件 */

#include <naos/naos.h>

#include "markdown/md_html.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 字句の関数の型 */
typedef size_t (*lex_fn)(const char *data, size_t pos, size_t end);

/* text の先頭から fn で認識した長さ */
static size_t lex(lex_fn fn, const char *text)
{
    return fn(text, 0, strlen(text));
}

/* text の先頭から行の残り全体で判定した開始条件の種類 */
static int start_type(const char *text)
{
    mnd_md_html_start_t out;

    out.type = -1;
    out.interrupts_paragraph = true;
    if (!mnd_md_html_block_start(text, 0, strlen(text), &out))
    {
        NAOS_CHECK_INT_EQ(0, out.type);
        NAOS_CHECK_FALSE(out.interrupts_paragraph);
    }

    return out.type;
}

/* 開始タグ */

NAOS_TEST(open_tag_simple_and_empty_elements)
{
    NAOS_CHECK_UINT_EQ(3, lex(mnd_md_html_open_tag, "<a><bab><c2c>"));
    NAOS_CHECK_UINT_EQ(5, lex(mnd_md_html_open_tag, "<bab><c2c>"));
    NAOS_CHECK_UINT_EQ(4, lex(mnd_md_html_open_tag, "<a/><b2/>"));
    NAOS_CHECK_UINT_EQ(5, lex(mnd_md_html_open_tag, "<b2/>"));
}

NAOS_TEST(open_tag_custom_name_with_hyphen)
{
    const char *text = "<responsive-image src=\"foo.jpg\" />";

    NAOS_CHECK_UINT_EQ(strlen(text), lex(mnd_md_html_open_tag, text));
}

NAOS_TEST(open_tag_whitespace_may_include_one_line_ending)
{
    NAOS_CHECK_UINT_EQ(6, lex(mnd_md_html_open_tag, "<a  /><b2\ndata=\"foo\" >"));
    NAOS_CHECK_UINT_EQ(16, lex(mnd_md_html_open_tag, "<b2\ndata=\"foo\" >"));
    NAOS_CHECK_UINT_EQ(17, lex(mnd_md_html_open_tag, "<b2\r\ndata=\"foo\" >"));
    NAOS_CHECK_UINT_EQ(16, lex(mnd_md_html_open_tag, "<b2\rdata=\"foo\" >"));
    NAOS_CHECK_UINT_EQ(8, lex(mnd_md_html_open_tag, "<a \n \t/>"));
    NAOS_CHECK_UINT_EQ(9, lex(mnd_md_html_open_tag, "<a x\n=\n1>"));
    /* 空白の中の行末記号は 1 つまで */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<b2\n\ndata=\"foo\" >"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<b2\n \ndata=\"foo\" >"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a\n\n>"));
}

NAOS_TEST(open_tag_attributes_and_quoted_values)
{
    const char *text = "<a foo=\"bar\" bam = 'baz <em>\"</em>'\n_boolean zoop:33=zoop:33 />";

    NAOS_CHECK_UINT_EQ(strlen(text), lex(mnd_md_html_open_tag, text));
    NAOS_CHECK_UINT_EQ(13, lex(mnd_md_html_open_tag, "<a href=\"hi\">x"));
    NAOS_CHECK_UINT_EQ(13, lex(mnd_md_html_open_tag, "<a href='hi'>x"));
    NAOS_CHECK_UINT_EQ(11, lex(mnd_md_html_open_tag, "<a href=hi>x"));
    NAOS_CHECK_UINT_EQ(11, lex(mnd_md_html_open_tag, "<a href=\"\">x"));
    NAOS_CHECK_UINT_EQ(17, lex(mnd_md_html_open_tag, "<a href=\"&ouml;\">"));
    NAOS_CHECK_UINT_EQ(13, lex(mnd_md_html_open_tag, "<a href=\"\\*\">"));
    /* 引用符の中には行末記号も `>` も含められる */
    NAOS_CHECK_UINT_EQ(13, lex(mnd_md_html_open_tag, "<a t=\"x\n>\ny\">"));
    /* 属性の名前の先頭の `_` と `:`、名前の中の `.` `:` `-` */
    NAOS_CHECK_UINT_EQ(17, lex(mnd_md_html_open_tag, "<a _x :y z.w:-v1>"));
}

NAOS_TEST(open_tag_negative)
{
    /* タグ名の誤り */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<33>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<__>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "< a>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<\nfoo>"));
    /* 属性の名前の誤り */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a h*#ref=\"hi\">"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<foo bar=baz\nbim!bop />"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a 1x>"));
    /* 属性の値の誤り（引用符が閉じない、引用符のない値の中の引用符、空の値） */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=\"hi'> <a href=hi'>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=hi'>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=\"\\\"\">"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=`x`>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=a=b>"));
    /* 属性の前の空白がない */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href='bar'title=title>"));
    /* `/` と `>` の間の空白 */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<bar/ >"));
    /* 閉じない */
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "<a href=\"x\""));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_open_tag, "</a>"));
}

NAOS_TEST(open_tag_stops_at_end)
{
    /* end より後ろの `>` は使わない */
    NAOS_CHECK_UINT_EQ(0, mnd_md_html_open_tag("<a>", 0, 2));
    NAOS_CHECK_UINT_EQ(3, mnd_md_html_open_tag("x<a>", 1, 4));
}

NAOS_TEST(lexemes_do_not_stop_at_nul)
{
    const char quoted[] = "<a t=\"\0\">";
    const char comment[] = "<!-- \0 -->";
    const char bare[] = "<a\0>";

    NAOS_CHECK_UINT_EQ(sizeof(quoted) - 1, mnd_md_html_open_tag(quoted, 0, sizeof(quoted) - 1));
    NAOS_CHECK_UINT_EQ(sizeof(comment) - 1, mnd_md_html_comment(comment, 0, sizeof(comment) - 1));
    /* NUL は空白でもタグ名の文字でもない */
    NAOS_CHECK_UINT_EQ(0, mnd_md_html_open_tag(bare, 0, sizeof(bare) - 1));
}

/* 終了タグ */

NAOS_TEST(closing_tag_positive_and_negative)
{
    NAOS_CHECK_UINT_EQ(4, lex(mnd_md_html_closing_tag, "</a></foo >"));
    NAOS_CHECK_UINT_EQ(7, lex(mnd_md_html_closing_tag, "</foo >"));
    NAOS_CHECK_UINT_EQ(7, lex(mnd_md_html_closing_tag, "</foo\n>"));
    NAOS_CHECK_UINT_EQ(9, lex(mnd_md_html_closing_tag, "</x-1 \r\n>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_closing_tag, "</a href=\"foo\">"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_closing_tag, "</a/>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_closing_tag, "</ a>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_closing_tag, "</1>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_closing_tag, "</foo\n\n>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_closing_tag, "<a>"));
}

/* コメント */

NAOS_TEST(comment_positive_and_negative)
{
    const char *hyphens = "<!-- this is a --\ncomment - with hyphens -->";

    NAOS_CHECK_UINT_EQ(12, lex(mnd_md_html_comment, "<!-- foo -->"));
    NAOS_CHECK_UINT_EQ(strlen(hyphens), lex(mnd_md_html_comment, hyphens));
    NAOS_CHECK_UINT_EQ(5, lex(mnd_md_html_comment, "<!--> foo -->"));
    NAOS_CHECK_UINT_EQ(6, lex(mnd_md_html_comment, "<!---> foo -->"));
    NAOS_CHECK_UINT_EQ(7, lex(mnd_md_html_comment, "<!---->"));
    NAOS_CHECK_UINT_EQ(11, lex(mnd_md_html_comment, "<!-- a --->"));
    NAOS_CHECK_UINT_EQ(10, lex(mnd_md_html_comment, "<!-- a --> -->"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_comment, "<!-- foo"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_comment, "<!-- foo --"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_comment, "<!-"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_comment, "<!- foo -->"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_comment, "<!--"));
}

/* 処理命令 */

NAOS_TEST(processing_instruction_positive_and_negative)
{
    NAOS_CHECK_UINT_EQ(17, lex(mnd_md_html_processing_instruction, "<?php echo $a; ?>"));
    NAOS_CHECK_UINT_EQ(4, lex(mnd_md_html_processing_instruction, "<?\?>"));
    NAOS_CHECK_UINT_EQ(7, lex(mnd_md_html_processing_instruction, "<?a\nb?>?>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_processing_instruction, "<?>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_processing_instruction, "<?php echo"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_processing_instruction, "<!php ?>"));
}

/* 宣言 */

NAOS_TEST(declaration_positive_and_negative)
{
    NAOS_CHECK_UINT_EQ(19, lex(mnd_md_html_declaration, "<!ELEMENT br EMPTY>"));
    NAOS_CHECK_UINT_EQ(15, lex(mnd_md_html_declaration, "<!DOCTYPE html>"));
    NAOS_CHECK_UINT_EQ(4, lex(mnd_md_html_declaration, "<!x>"));
    NAOS_CHECK_UINT_EQ(7, lex(mnd_md_html_declaration, "<!a\nb\n>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_declaration, "<!1 x>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_declaration, "<! x>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_declaration, "<!-- x -->"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_declaration, "<!ELEMENT br"));
}

/* CDATA */

NAOS_TEST(cdata_positive_and_negative)
{
    NAOS_CHECK_UINT_EQ(15, lex(mnd_md_html_cdata, "<![CDATA[>&<]]>"));
    NAOS_CHECK_UINT_EQ(12, lex(mnd_md_html_cdata, "<![CDATA[]]>"));
    NAOS_CHECK_UINT_EQ(15, lex(mnd_md_html_cdata, "<![CDATA[a\n]]]>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_cdata, "<![CDATA[>&<]]"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_cdata, "<![cdata[x]]>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_cdata, "<![CDATA"));
}

/* HTML タグ（6 種類のどれか） */

NAOS_TEST(tag_recognizes_each_lexeme)
{
    NAOS_CHECK_UINT_EQ(14, lex(mnd_md_html_tag, "<a href=\"x\" />y"));
    NAOS_CHECK_UINT_EQ(4, lex(mnd_md_html_tag, "</a>y"));
    NAOS_CHECK_UINT_EQ(5, lex(mnd_md_html_tag, "<!-->y"));
    NAOS_CHECK_UINT_EQ(12, lex(mnd_md_html_tag, "<!-- foo -->y"));
    NAOS_CHECK_UINT_EQ(17, lex(mnd_md_html_tag, "<?php echo $a; ?>y"));
    NAOS_CHECK_UINT_EQ(19, lex(mnd_md_html_tag, "<!ELEMENT br EMPTY>y"));
    NAOS_CHECK_UINT_EQ(15, lex(mnd_md_html_tag, "<![CDATA[>&<]]>y"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_tag, "<33> <__>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_tag, "<a h*#ref=\"hi\">"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_tag, "</a href=\"foo\">"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_tag, "<!- x ->"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_tag, "a<b>"));
    NAOS_CHECK_UINT_EQ(0, lex(mnd_md_html_tag, ""));
    NAOS_CHECK_UINT_EQ(3, mnd_md_html_tag("x <b> y", 2, 7));
}

/* HTML ブロックの開始条件 */

NAOS_TEST(block_start_condition_1)
{
    NAOS_CHECK_INT_EQ(1, start_type("<pre language=\"haskell\"><code>"));
    NAOS_CHECK_INT_EQ(1, start_type("<script type=\"text/javascript\">"));
    NAOS_CHECK_INT_EQ(1, start_type("<style"));
    NAOS_CHECK_INT_EQ(1, start_type("<textarea>"));
    NAOS_CHECK_INT_EQ(1, start_type("<PRE>"));
    NAOS_CHECK_INT_EQ(1, start_type("<sCrIpT\tx"));
    /* 名前の後ろが空白、タブ、`>`、行末でなければ種類 1 ではない */
    NAOS_CHECK_INT_EQ(0, start_type("<prex"));
    NAOS_CHECK_INT_EQ(0, start_type("<pre/>"));
    /* 終了タグは種類 1 ではない（完全な終了タグなので種類 7） */
    NAOS_CHECK_INT_EQ(7, start_type("</pre>"));
}

NAOS_TEST(block_start_condition_2_to_5)
{
    NAOS_CHECK_INT_EQ(2, start_type("<!-- foo"));
    NAOS_CHECK_INT_EQ(2, start_type("<!-->"));
    NAOS_CHECK_INT_EQ(2, start_type("<!--"));
    NAOS_CHECK_INT_EQ(3, start_type("<?php"));
    NAOS_CHECK_INT_EQ(3, start_type("<?"));
    NAOS_CHECK_INT_EQ(4, start_type("<!DOCTYPE html>"));
    NAOS_CHECK_INT_EQ(4, start_type("<!x"));
    NAOS_CHECK_INT_EQ(5, start_type("<![CDATA["));
    NAOS_CHECK_INT_EQ(0, start_type("<!-x"));
    NAOS_CHECK_INT_EQ(0, start_type("<!1"));
    NAOS_CHECK_INT_EQ(0, start_type("<![CDATA"));
}

NAOS_TEST(block_start_condition_6)
{
    NAOS_CHECK_INT_EQ(6, start_type("<table><tr><td>"));
    NAOS_CHECK_INT_EQ(6, start_type("<div"));
    NAOS_CHECK_INT_EQ(6, start_type("</div>"));
    NAOS_CHECK_INT_EQ(6, start_type("<DIV CLASS=\"foo\">"));
    NAOS_CHECK_INT_EQ(6, start_type("<div id=\"foo\"\tclass=\"bar\""));
    NAOS_CHECK_INT_EQ(6, start_type("<hr/>"));
    NAOS_CHECK_INT_EQ(6, start_type("<h1>"));
    NAOS_CHECK_INT_EQ(6, start_type("<search>"));
    NAOS_CHECK_INT_EQ(6, start_type("<menuitem>"));
    NAOS_CHECK_INT_EQ(6, start_type("<td x *"));
    /* 一覧の名前の途中まで、一覧にない名前、`/` の後ろが `>` でない */
    NAOS_CHECK_INT_EQ(0, start_type("<divx"));
    NAOS_CHECK_INT_EQ(0, start_type("<div/x"));
    NAOS_CHECK_INT_EQ(0, start_type("<h7 *"));
    /* source は 0.31.2 の一覧にない（完全なタグなら種類 7） */
    NAOS_CHECK_INT_EQ(7, start_type("<source>"));
    NAOS_CHECK_INT_EQ(0, start_type("<source x"));
}

NAOS_TEST(block_start_condition_7)
{
    NAOS_CHECK_INT_EQ(7, start_type("<a href=\"foo\">"));
    NAOS_CHECK_INT_EQ(7, start_type("<Warning>"));
    NAOS_CHECK_INT_EQ(7, start_type("</ins> \t"));
    NAOS_CHECK_INT_EQ(7, start_type("<i class=\"foo\">"));
    NAOS_CHECK_INT_EQ(7, start_type("<del>"));
    /* タグの後ろに空白とタブ以外がある */
    NAOS_CHECK_INT_EQ(0, start_type("<del>*foo*</del>"));
    NAOS_CHECK_INT_EQ(0, start_type("<a href=\"foo\">x"));
    /* 完全なタグではない（行の中で閉じない） */
    NAOS_CHECK_INT_EQ(0, start_type("<a href=\"foo"));
    NAOS_CHECK_INT_EQ(0, start_type("<33>"));
    NAOS_CHECK_INT_EQ(0, start_type("<a h*#ref=\"hi\">"));
    NAOS_CHECK_INT_EQ(0, start_type("<!-x>"));
    NAOS_CHECK_INT_EQ(0, start_type("foo"));
    NAOS_CHECK_INT_EQ(0, start_type(""));
}

NAOS_TEST(block_start_interrupts_paragraph_except_type_7)
{
    const char *texts[] = { "<pre>", "<!-- x", "<?x", "<!X", "<![CDATA[", "<div>", "<del>" };
    size_t i;

    for (i = 0; (i < (sizeof(texts) / sizeof(texts[0]))); i++)
    {
        mnd_md_html_start_t out;

        NAOS_CHECK_TRUE(mnd_md_html_block_start(texts[i], 0, strlen(texts[i]), &out));
        NAOS_CHECK_INT_EQ((int)(i + 1), out.type);
        NAOS_CHECK_TRUE(out.interrupts_paragraph == (out.type != 7));
    }
}

NAOS_TEST(block_start_reads_only_to_end)
{
    mnd_md_html_start_t out;

    /* 行末より後ろは読まない: `<a` だけでは完全なタグではない */
    NAOS_CHECK_FALSE(mnd_md_html_block_start("<a>", 0, 2, &out));
    NAOS_CHECK_INT_EQ(0, out.type);
    /* `<pre` で行が終わる */
    NAOS_CHECK_TRUE(mnd_md_html_block_start("<prex", 0, 4, &out));
    NAOS_CHECK_INT_EQ(1, out.type);
    /* pos から判定する */
    NAOS_CHECK_TRUE(mnd_md_html_block_start("   <div>", 3, 8, &out));
    NAOS_CHECK_INT_EQ(6, out.type);
}

/* HTML ブロックの終了条件 */

/* text 全体を 1 行として、種類 type の終了条件を判定する */
static bool ends(const char *text, int type)
{
    return mnd_md_html_block_end(text, 0, strlen(text), type);
}

NAOS_TEST(block_end_conditions)
{
    NAOS_CHECK_TRUE(ends("</pre>", 1));
    NAOS_CHECK_TRUE(ends("okay </SCRIPT> more", 1));
    NAOS_CHECK_TRUE(ends("x</style>", 1));
    NAOS_CHECK_TRUE(ends("</TextArea>", 1));
    /* 開始タグと一致しなくてよい。`>` が直後に必要 */
    NAOS_CHECK_FALSE(ends("</pre >", 1));
    NAOS_CHECK_FALSE(ends("</div>", 1));
    NAOS_CHECK_FALSE(ends("<pre>", 1));
    NAOS_CHECK_FALSE(ends("</pre", 1));

    NAOS_CHECK_TRUE(ends("foo -->", 2));
    NAOS_CHECK_TRUE(ends("<!-->", 2));
    NAOS_CHECK_FALSE(ends("foo --", 2));
    NAOS_CHECK_FALSE(ends("foo ->", 2));

    NAOS_CHECK_TRUE(ends("echo ?>", 3));
    NAOS_CHECK_FALSE(ends("echo ? >", 3));

    NAOS_CHECK_TRUE(ends("html>", 4));
    NAOS_CHECK_FALSE(ends("html", 4));

    NAOS_CHECK_TRUE(ends("x ]]> y", 5));
    NAOS_CHECK_FALSE(ends("x ]] >", 5));

    /* 種類 6、7 は空行で終わる（呼び出し側が判定する） */
    NAOS_CHECK_FALSE(ends("</div>", 6));
    NAOS_CHECK_FALSE(ends("", 7));
    NAOS_CHECK_FALSE(ends("-->", 0));
}

NAOS_TEST(block_end_reads_only_to_end)
{
    NAOS_CHECK_FALSE(mnd_md_html_block_end("x -->", 0, 4, 2));
    NAOS_CHECK_FALSE(mnd_md_html_block_end("</pre>", 0, 5, 1));
    NAOS_CHECK_FALSE(mnd_md_html_block_end("-->x", 1, 4, 2));
    NAOS_CHECK_TRUE(mnd_md_html_block_end("a-->", 1, 4, 2));
}

NAOS_SUITE(html,
    NAOS_ADD_TEST(open_tag_simple_and_empty_elements),
    NAOS_ADD_TEST(open_tag_custom_name_with_hyphen),
    NAOS_ADD_TEST(open_tag_whitespace_may_include_one_line_ending),
    NAOS_ADD_TEST(open_tag_attributes_and_quoted_values),
    NAOS_ADD_TEST(open_tag_negative),
    NAOS_ADD_TEST(open_tag_stops_at_end),
    NAOS_ADD_TEST(lexemes_do_not_stop_at_nul),
    NAOS_ADD_TEST(closing_tag_positive_and_negative),
    NAOS_ADD_TEST(comment_positive_and_negative),
    NAOS_ADD_TEST(processing_instruction_positive_and_negative),
    NAOS_ADD_TEST(declaration_positive_and_negative),
    NAOS_ADD_TEST(cdata_positive_and_negative),
    NAOS_ADD_TEST(tag_recognizes_each_lexeme),
    NAOS_ADD_TEST(block_start_condition_1),
    NAOS_ADD_TEST(block_start_condition_2_to_5),
    NAOS_ADD_TEST(block_start_condition_6),
    NAOS_ADD_TEST(block_start_condition_7),
    NAOS_ADD_TEST(block_start_interrupts_paragraph_except_type_7),
    NAOS_ADD_TEST(block_start_reads_only_to_end),
    NAOS_ADD_TEST(block_end_conditions),
    NAOS_ADD_TEST(block_end_reads_only_to_end)
)
