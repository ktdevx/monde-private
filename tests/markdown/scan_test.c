/* Scan: 1 行の中のブロックの開始の判定とタブの桁 */

#include <naos/naos.h>

#include "markdown/md.h"
#include "markdown/md_scan.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* タブの桁 */

NAOS_TEST(next_column_counts_tabs_to_multiples_of_four)
{
    NAOS_CHECK_UINT_EQ(1, mnd_md_scan_next_column(0, 'a'));
    NAOS_CHECK_UINT_EQ(6, mnd_md_scan_next_column(5, ' '));
    NAOS_CHECK_UINT_EQ(4, mnd_md_scan_next_column(0, '\t'));
    NAOS_CHECK_UINT_EQ(4, mnd_md_scan_next_column(1, '\t'));
    NAOS_CHECK_UINT_EQ(4, mnd_md_scan_next_column(3, '\t'));
    NAOS_CHECK_UINT_EQ(8, mnd_md_scan_next_column(4, '\t'));
    NAOS_CHECK_UINT_EQ(1, mnd_md_scan_next_column(0, '\0'));
}

/* text の cur から columns 桁を取り除き、取り除いた桁数と cur を確かめる */
static void check_skip(const char *text, mnd_md_scan_pos_t cur, size_t columns, size_t removed,
                       size_t pos, size_t column, size_t pad)
{
    NAOS_CHECK_UINT_EQ(removed, mnd_md_scan_skip_columns(text, strlen(text), &cur, columns));
    NAOS_CHECK_UINT_EQ(pos, cur.pos);
    NAOS_CHECK_UINT_EQ(column, cur.column);
    NAOS_CHECK_UINT_EQ(pad, cur.pad);
}

NAOS_TEST(skip_columns_whole_tab_has_no_pad)
{
    const mnd_md_scan_pos_t start = { 0, 0, 0 };

    check_skip("\tfoo", start, 4, 4, 1, 4, 0);
    check_skip("    foo", start, 4, 4, 4, 4, 0);
    check_skip("     foo", start, 4, 4, 4, 4, 0);
    check_skip("  \tfoo", start, 4, 4, 3, 4, 0);
    check_skip("\tfoo", start, 0, 0, 0, 0, 0);
}

NAOS_TEST(skip_columns_part_of_tab_gives_pad)
{
    const mnd_md_scan_pos_t start = { 0, 0, 0 };
    const mnd_md_scan_pos_t after_dash = { 1, 1, 0 };

    /* タブの 2 桁だけを使う: タブを越えて進み、残りの 2 桁が pad */
    check_skip("\tfoo", start, 2, 2, 1, 2, 2);
    check_skip("\tfoo", start, 1, 1, 1, 1, 3);
    check_skip("\tfoo", start, 3, 3, 1, 3, 1);
    /* 空白のあとのタブ（桁 1 から 4）の 1 桁だけを使う */
    check_skip(" \tfoo", start, 2, 2, 2, 2, 2);
    /* `-\tfoo`: 記号の後ろのタブは桁 1 から 4 の 3 桁。3 桁を使えば pad はない */
    check_skip("-\tfoo", after_dash, 3, 3, 2, 4, 0);
    /* `-\t\tfoo`: 記号の後ろの 1 桁だけを使うと、最初のタブの残り 2 桁が pad */
    check_skip("-\t\tfoo", after_dash, 1, 1, 2, 2, 2);
}

NAOS_TEST(skip_columns_uses_pending_pad_first)
{
    /* `-\t\tfoo` の記号の後ろで 1 桁を使った状態（pad 2）から、インデントコードの 4 桁を除く */
    const mnd_md_scan_pos_t after_padding = { 2, 2, 2 };
    const mnd_md_scan_pos_t pad3 = { 1, 1, 3 };

    check_skip("-\t\tfoo", after_padding, 4, 4, 3, 6, 2);
    check_skip("-\t\tfoo", after_padding, 2, 2, 2, 4, 0);
    check_skip("-\t\tfoo", after_padding, 1, 1, 2, 3, 1);
    check_skip("\tfoo", pad3, 1, 1, 1, 2, 2);
    check_skip("\tfoo", pad3, 5, 3, 1, 4, 0);
}

NAOS_TEST(skip_columns_stops_at_non_space_and_end)
{
    const mnd_md_scan_pos_t start = { 0, 0, 0 };

    check_skip("  foo", start, 4, 2, 2, 2, 0);
    check_skip("   ", start, 4, 3, 3, 3, 0);
    check_skip("", start, 4, 0, 0, 0, 0);
    /* NUL は空白ではない */
    check_skip(" \0 x", start, 4, 1, 1, 1, 0);
}

NAOS_TEST(skip_columns_reads_only_up_to_end)
{
    mnd_md_scan_pos_t cur = { 0, 0, 0 };

    /* end の後ろの空白は使わない */
    NAOS_CHECK_UINT_EQ(2, mnd_md_scan_skip_columns("    ", 2, &cur, 4));
    NAOS_CHECK_UINT_EQ(2, cur.pos);
}

NAOS_TEST(indent_counts_columns_and_blank)
{
    const mnd_md_scan_pos_t start = { 0, 0, 0 };
    const mnd_md_scan_pos_t after_padding = { 2, 2, 2 };
    mnd_md_scan_indent_t ind;

    mnd_md_scan_indent("   # foo", 8, &start, &ind);
    NAOS_CHECK_UINT_EQ(3, ind.pos);
    NAOS_CHECK_UINT_EQ(3, ind.column);
    NAOS_CHECK_UINT_EQ(3, ind.indent);
    NAOS_CHECK_FALSE(ind.blank);

    mnd_md_scan_indent("\t foo", 5, &start, &ind);
    NAOS_CHECK_UINT_EQ(2, ind.pos);
    NAOS_CHECK_UINT_EQ(5, ind.column);
    NAOS_CHECK_UINT_EQ(5, ind.indent);
    NAOS_CHECK_FALSE(ind.blank);

    mnd_md_scan_indent("  \t ", 4, &start, &ind);
    NAOS_CHECK_UINT_EQ(4, ind.pos);
    NAOS_CHECK_UINT_EQ(5, ind.column);
    NAOS_CHECK_TRUE(ind.blank);

    mnd_md_scan_indent("", 0, &start, &ind);
    NAOS_CHECK_UINT_EQ(0, ind.pos);
    NAOS_CHECK_UINT_EQ(0, ind.indent);
    NAOS_CHECK_TRUE(ind.blank);

    /* pad を含む: `-\t\tfoo` の記号の後ろで 1 桁を使った位置からは 6 桁（インデントコード） */
    mnd_md_scan_indent("-\t\tfoo", 6, &after_padding, &ind);
    NAOS_CHECK_UINT_EQ(3, ind.pos);
    NAOS_CHECK_UINT_EQ(8, ind.column);
    NAOS_CHECK_UINT_EQ(6, ind.indent);
    NAOS_CHECK_FALSE(ind.blank);

    /* NUL は空白でない文字 */
    mnd_md_scan_indent(" \0", 2, &start, &ind);
    NAOS_CHECK_UINT_EQ(1, ind.pos);
    NAOS_CHECK_FALSE(ind.blank);
}

/* テーマ区切り */

static bool thematic(const char *text, char *ch)
{
    return mnd_md_scan_thematic_break(text, 0, strlen(text), ch);
}

NAOS_TEST(thematic_break_positive)
{
    char ch = 0;

    NAOS_CHECK_TRUE(thematic("***", &ch));
    NAOS_CHECK_INT_EQ('*', ch);
    NAOS_CHECK_TRUE(thematic("* * *", &ch));
    NAOS_CHECK_INT_EQ('*', ch);
    NAOS_CHECK_TRUE(thematic("---", &ch));
    NAOS_CHECK_INT_EQ('-', ch);
    NAOS_CHECK_TRUE(thematic("___", &ch));
    NAOS_CHECK_INT_EQ('_', ch);
    NAOS_CHECK_TRUE(thematic("_ _ _", &ch));
    NAOS_CHECK_INT_EQ('_', ch);
    NAOS_CHECK_TRUE(thematic("_____________________________________", NULL));
    NAOS_CHECK_TRUE(thematic("-  -  -", NULL));
    NAOS_CHECK_TRUE(thematic("**  * ** * ** * **", NULL));
    NAOS_CHECK_TRUE(thematic("- - - -    ", NULL));
    NAOS_CHECK_TRUE(thematic("*\t*\t*\t", NULL));
    /* 行の途中の位置から */
    NAOS_CHECK_TRUE(mnd_md_scan_thematic_break("> ***", 2, 5, &ch));
    NAOS_CHECK_INT_EQ('*', ch);
}

NAOS_TEST(thematic_break_negative)
{
    NAOS_CHECK_FALSE(thematic("+++", NULL));
    NAOS_CHECK_FALSE(thematic("===", NULL));
    NAOS_CHECK_FALSE(thematic("--", NULL));
    NAOS_CHECK_FALSE(thematic("**", NULL));
    NAOS_CHECK_FALSE(thematic("__", NULL));
    NAOS_CHECK_FALSE(thematic("", NULL));
    NAOS_CHECK_FALSE(thematic("_ _ _ _ a", NULL));
    NAOS_CHECK_FALSE(thematic("a------", NULL));
    NAOS_CHECK_FALSE(thematic("---a---", NULL));
    NAOS_CHECK_FALSE(thematic("*-*", NULL));
    NAOS_CHECK_FALSE(mnd_md_scan_thematic_break("**\0*", 0, 4, NULL));
    /* end の後ろは読まない */
    NAOS_CHECK_FALSE(mnd_md_scan_thematic_break("***", 0, 2, NULL));
}

/* ATX 見出し */

/* text が level の ATX 見出しで、内容が content であることを確かめる */
static void check_atx(const char *text, int level, const char *content)
{
    mnd_md_heading_t h;
    size_t len = strlen(text);

    memset(&h, 0xAB, sizeof(h));
    NAOS_REQUIRE_TRUE(mnd_md_scan_atx_heading(text, 0, len, &h));
    NAOS_CHECK_INT_EQ(level, h.level);
    NAOS_CHECK_FALSE(h.setext);
    NAOS_REQUIRE_TRUE(h.content.start <= h.content.end);
    NAOS_REQUIRE_TRUE(h.content.end <= len);
    NAOS_CHECK_UINT_EQ(strlen(content), h.content.end - h.content.start);
    NAOS_CHECK_MEM_EQ(content, strlen(content), text + h.content.start, h.content.end - h.content.start);
}

static bool atx(const char *text)
{
    mnd_md_heading_t h;

    return mnd_md_scan_atx_heading(text, 0, strlen(text), &h);
}

NAOS_TEST(atx_heading_levels)
{
    check_atx("# foo", 1, "foo");
    check_atx("## foo", 2, "foo");
    check_atx("### foo", 3, "foo");
    check_atx("#### foo", 4, "foo");
    check_atx("##### foo", 5, "foo");
    check_atx("###### foo", 6, "foo");
    check_atx("#\tfoo", 1, "foo");
    check_atx("# foo *bar* \\*baz\\*", 1, "foo *bar* \\*baz\\*");
    check_atx("#                  foo                     ", 1, "foo");
}

NAOS_TEST(atx_heading_negative)
{
    NAOS_CHECK_FALSE(atx("####### foo"));
    NAOS_CHECK_FALSE(atx("#######"));
    NAOS_CHECK_FALSE(atx("#5 bolt"));
    NAOS_CHECK_FALSE(atx("#hashtag"));
    NAOS_CHECK_FALSE(atx("\\## foo"));
    NAOS_CHECK_FALSE(atx("foo"));
    NAOS_CHECK_FALSE(atx(""));
    NAOS_CHECK_FALSE(mnd_md_scan_atx_heading("#\0", 0, 2, &(mnd_md_heading_t){ 0 }));
}

NAOS_TEST(atx_heading_closing_sequence)
{
    check_atx("## foo ##", 2, "foo");
    check_atx("###   bar    ###", 3, "bar");
    check_atx("# foo ##################################", 1, "foo");
    check_atx("##### foo ##", 5, "foo");
    check_atx("### foo ###     ", 3, "foo");
    check_atx("# foo #", 1, "foo");
    check_atx("# foo\t#\t", 1, "foo");
    /* 閉じの並びの後ろに他の文字があれば閉じではない */
    check_atx("### foo ### b", 3, "foo ### b");
    /* 閉じの並びの前には空白かタブが必要 */
    check_atx("# foo#", 1, "foo#");
    /* エスケープされた # は内容に残る */
    check_atx("### foo \\###", 3, "foo \\###");
    check_atx("## foo #\\##", 2, "foo #\\##");
    check_atx("# foo \\#", 1, "foo \\#");
}

NAOS_TEST(atx_heading_empty_content)
{
    mnd_md_heading_t h;

    check_atx("#", 1, "");
    check_atx("## ", 2, "");
    check_atx("### ###", 3, "");
    check_atx("# #", 1, "");

    /* 空の内容は、開きの並びの直後の位置の空の範囲 */
    NAOS_REQUIRE_TRUE(mnd_md_scan_atx_heading("## ", 0, 3, &h));
    NAOS_CHECK_UINT_EQ(2, h.content.start);
    NAOS_CHECK_UINT_EQ(2, h.content.end);
    NAOS_REQUIRE_TRUE(mnd_md_scan_atx_heading("### ###", 0, 7, &h));
    NAOS_CHECK_UINT_EQ(3, h.content.start);
    NAOS_CHECK_UINT_EQ(3, h.content.end);
}

NAOS_TEST(atx_heading_offsets_and_end)
{
    mnd_md_heading_t h;

    /* 行の途中の位置からで、範囲は data の先頭を基準にする */
    NAOS_REQUIRE_TRUE(mnd_md_scan_atx_heading("> ## foo ##\n", 2, 11, &h));
    NAOS_CHECK_INT_EQ(2, h.level);
    NAOS_CHECK_UINT_EQ(5, h.content.start);
    NAOS_CHECK_UINT_EQ(8, h.content.end);

    /* end の後ろは読まない（"# foo #" の end を 5 にすると内容は "foo"） */
    NAOS_REQUIRE_TRUE(mnd_md_scan_atx_heading("# foo #", 0, 5, &h));
    NAOS_CHECK_UINT_EQ(2, h.content.start);
    NAOS_CHECK_UINT_EQ(5, h.content.end);

    /* NUL は内容の文字 */
    NAOS_REQUIRE_TRUE(mnd_md_scan_atx_heading("# a\0b #", 0, 7, &h));
    NAOS_CHECK_UINT_EQ(2, h.content.start);
    NAOS_CHECK_UINT_EQ(5, h.content.end);
}

/* Setext の下線 */

static int setext(const char *text)
{
    return mnd_md_scan_setext_underline(text, 0, strlen(text));
}

NAOS_TEST(setext_underline)
{
    NAOS_CHECK_INT_EQ(1, setext("="));
    NAOS_CHECK_INT_EQ(1, setext("========="));
    NAOS_CHECK_INT_EQ(1, setext("===   "));
    NAOS_CHECK_INT_EQ(1, setext("===\t"));
    NAOS_CHECK_INT_EQ(2, setext("-"));
    NAOS_CHECK_INT_EQ(2, setext("---"));
    NAOS_CHECK_INT_EQ(2, setext("---------------------------------------- "));

    NAOS_CHECK_INT_EQ(0, setext(""));
    NAOS_CHECK_INT_EQ(0, setext("= ="));
    NAOS_CHECK_INT_EQ(0, setext("--- -"));
    NAOS_CHECK_INT_EQ(0, setext("==a"));
    NAOS_CHECK_INT_EQ(0, setext("-="));
    NAOS_CHECK_INT_EQ(0, setext("***"));
    NAOS_CHECK_INT_EQ(0, mnd_md_scan_setext_underline("==\0", 0, 3));
}

/* フェンス */

/* text が開きフェンスで、文字・長さ・情報文字列が期待どおりであることを確かめる（info が NULL なら情報文字列なし） */
static void check_open_fence(const char *text, char fence_char, size_t fence_len, const char *info)
{
    mnd_md_code_block_t cb;
    size_t len = strlen(text);

    memset(&cb, 0xAB, sizeof(cb));
    NAOS_REQUIRE_TRUE(mnd_md_scan_open_fence(text, 0, len, &cb));
    NAOS_CHECK_TRUE(cb.fenced);
    NAOS_CHECK_INT_EQ(fence_char, cb.fence_char);
    NAOS_CHECK_UINT_EQ(fence_len, cb.fence_len);
    NAOS_CHECK_UINT_EQ(0, cb.fence_indent);
    NAOS_CHECK_FALSE(cb.closed);
    NAOS_REQUIRE_TRUE(cb.info.start <= cb.info.end);
    NAOS_REQUIRE_TRUE(cb.info.end <= len);
    if (info == NULL)
    {
        NAOS_CHECK_FALSE(cb.has_info);
        NAOS_CHECK_UINT_EQ(cb.info.start, cb.info.end);
    }
    else
    {
        NAOS_CHECK_TRUE(cb.has_info);
        NAOS_CHECK_UINT_EQ(strlen(info), cb.info.end - cb.info.start);
        NAOS_CHECK_MEM_EQ(info, strlen(info), text + cb.info.start, cb.info.end - cb.info.start);
    }
}

static bool open_fence(const char *text)
{
    mnd_md_code_block_t cb;

    return mnd_md_scan_open_fence(text, 0, strlen(text), &cb);
}

NAOS_TEST(open_fence_positive)
{
    check_open_fence("```", '`', 3, NULL);
    check_open_fence("~~~", '~', 3, NULL);
    check_open_fence("````", '`', 4, NULL);
    check_open_fence("~~~~~~", '~', 6, NULL);
    check_open_fence("```   ", '`', 3, NULL);
    check_open_fence("``` ruby", '`', 3, "ruby");
    check_open_fence("```ruby", '`', 3, "ruby");
    check_open_fence("~~~ ruby", '~', 3, "ruby");
    check_open_fence("```   aa   ", '`', 3, "aa");
    check_open_fence("~~~~    ruby startline=3 $%@#$", '~', 4, "ruby startline=3 $%@#$");
    check_open_fence("````;", '`', 4, ";");
    check_open_fence("```\tc\t", '`', 3, "c");
    /* チルダのフェンスの情報文字列はバッククォートを含んでよい */
    check_open_fence("~~~ aa ``` ~~~", '~', 3, "aa ``` ~~~");
}

NAOS_TEST(open_fence_negative)
{
    NAOS_CHECK_FALSE(open_fence("``"));
    NAOS_CHECK_FALSE(open_fence("~~"));
    NAOS_CHECK_FALSE(open_fence("`~~"));
    NAOS_CHECK_FALSE(open_fence(""));
    /* バッククォートのフェンスの情報文字列はバッククォートを含まない */
    NAOS_CHECK_FALSE(open_fence("``` ```"));
    NAOS_CHECK_FALSE(open_fence("``` aa `b"));
    NAOS_CHECK_FALSE(open_fence("```a`"));
    /* end の後ろは読まない */
    NAOS_CHECK_FALSE(mnd_md_scan_open_fence("```", 0, 2, &(mnd_md_code_block_t){ 0 }));
}

NAOS_TEST(open_fence_offsets)
{
    mnd_md_code_block_t cb;

    NAOS_REQUIRE_TRUE(mnd_md_scan_open_fence("> ``` c` x", 2, 5, &cb));
    NAOS_CHECK_FALSE(cb.has_info);
    NAOS_REQUIRE_TRUE(mnd_md_scan_open_fence("> ~~~ c\0d \n", 2, 10, &cb));
    NAOS_CHECK_TRUE(cb.has_info);
    NAOS_CHECK_UINT_EQ(6, cb.info.start);
    NAOS_CHECK_UINT_EQ(9, cb.info.end);
}

static bool close_fence(const char *text, char fence_char, size_t fence_len)
{
    return mnd_md_scan_close_fence(text, 0, strlen(text), fence_char, fence_len);
}

NAOS_TEST(close_fence)
{
    NAOS_CHECK_TRUE(close_fence("```", '`', 3));
    NAOS_CHECK_TRUE(close_fence("`````", '`', 3));
    NAOS_CHECK_TRUE(close_fence("```   ", '`', 3));
    NAOS_CHECK_TRUE(close_fence("```\t", '`', 3));
    NAOS_CHECK_TRUE(close_fence("~~~~", '~', 4));
    NAOS_CHECK_TRUE(close_fence("~~~~~~~", '~', 4));

    NAOS_CHECK_FALSE(close_fence("``", '`', 3));
    NAOS_CHECK_FALSE(close_fence("~~~", '`', 3));
    NAOS_CHECK_FALSE(close_fence("```", '~', 3));
    NAOS_CHECK_FALSE(close_fence("~~~", '~', 4));
    NAOS_CHECK_FALSE(close_fence("``` aaa", '`', 3));
    NAOS_CHECK_FALSE(close_fence("~~~ ~~", '~', 3));
    NAOS_CHECK_FALSE(close_fence("", '`', 3));
    NAOS_CHECK_FALSE(mnd_md_scan_close_fence("```\0", 0, 4, '`', 3));
    NAOS_CHECK_FALSE(mnd_md_scan_close_fence("```", 0, 2, '`', 3));
}

/* リスト記号 */

/* text がリスト記号で始まり、属性と範囲が期待どおりであることを確かめる */
static void check_marker(const char *text, bool ordered, char marker, unsigned long start,
                         size_t span_end, bool rest_blank)
{
    mnd_md_scan_list_marker_t m;

    memset(&m, 0xAB, sizeof(m));
    NAOS_REQUIRE_TRUE(mnd_md_scan_list_marker(text, 0, strlen(text), &m));
    NAOS_CHECK_INT_EQ(ordered, m.ordered);
    NAOS_CHECK_INT_EQ(marker, m.marker);
    NAOS_CHECK_UINT_EQ(start, m.start);
    NAOS_CHECK_UINT_EQ(0, m.span.start);
    NAOS_CHECK_UINT_EQ(span_end, m.span.end);
    NAOS_CHECK_INT_EQ(rest_blank, m.rest_blank);
}

static bool marker(const char *text)
{
    mnd_md_scan_list_marker_t m;

    return mnd_md_scan_list_marker(text, 0, strlen(text), &m);
}

NAOS_TEST(list_marker_bullets)
{
    check_marker("- foo", false, '-', 0, 1, false);
    check_marker("+ foo", false, '+', 0, 1, false);
    check_marker("* foo", false, '*', 0, 1, false);
    check_marker("-\tfoo", false, '-', 0, 1, false);
    check_marker("-", false, '-', 0, 1, true);
    check_marker("-   ", false, '-', 0, 1, true);
    check_marker("* * *", false, '*', 0, 1, false);
}

NAOS_TEST(list_marker_ordered)
{
    check_marker("1. foo", true, '.', 1, 2, false);
    check_marker("1) foo", true, ')', 1, 2, false);
    check_marker("0. ok", true, '.', 0, 2, false);
    check_marker("003. ok", true, '.', 3, 4, false);
    check_marker("123456789. ok", true, '.', 123456789UL, 10, false);
    check_marker("999999999)", true, ')', 999999999UL, 10, true);
    check_marker("10)\t", true, ')', 10, 3, true);
}

NAOS_TEST(list_marker_negative)
{
    /* 10 桁の番号 */
    NAOS_CHECK_FALSE(marker("1234567890. not ok"));
    NAOS_CHECK_FALSE(marker("0000000000. not ok"));
    NAOS_CHECK_FALSE(marker("-1. not ok"));
    NAOS_CHECK_FALSE(marker("-foo"));
    NAOS_CHECK_FALSE(marker("1.foo"));
    NAOS_CHECK_FALSE(marker("1"));
    NAOS_CHECK_FALSE(marker("1:"));
    NAOS_CHECK_FALSE(marker("a."));
    NAOS_CHECK_FALSE(marker("=="));
    NAOS_CHECK_FALSE(marker(""));
    NAOS_CHECK_FALSE(mnd_md_scan_list_marker("-\0", 0, 2, &(mnd_md_scan_list_marker_t){ 0 }));
}

NAOS_TEST(list_marker_offsets_and_end)
{
    mnd_md_scan_list_marker_t m;

    NAOS_REQUIRE_TRUE(mnd_md_scan_list_marker(">  12) x", 3, 8, &m));
    NAOS_CHECK_UINT_EQ(3, m.span.start);
    NAOS_CHECK_UINT_EQ(6, m.span.end);
    NAOS_CHECK_UINT_EQ(12, m.start);
    /* end で終わる記号は行末に続く（end の後ろは読まない） */
    NAOS_REQUIRE_TRUE(mnd_md_scan_list_marker("1.x", 0, 2, &m));
    NAOS_CHECK_TRUE(m.rest_blank);
    NAOS_CHECK_FALSE(mnd_md_scan_list_marker("12.", 0, 2, &m));
}

static size_t padding(const char *text, size_t pos, size_t column)
{
    return mnd_md_scan_list_padding(text, pos, strlen(text), column);
}

NAOS_TEST(list_padding)
{
    NAOS_CHECK_UINT_EQ(1, padding("- foo", 1, 1));
    NAOS_CHECK_UINT_EQ(2, padding("-  foo", 1, 1));
    NAOS_CHECK_UINT_EQ(4, padding("-    foo", 1, 1));
    /* 5 桁以上なら 1（内容はインデントコード） */
    NAOS_CHECK_UINT_EQ(1, padding("-     foo", 1, 1));
    /* 空の開始 */
    NAOS_CHECK_UINT_EQ(1, padding("-", 1, 1));
    NAOS_CHECK_UINT_EQ(1, padding("-   ", 1, 1));
    /* タブの桁 */
    NAOS_CHECK_UINT_EQ(3, padding("-\tfoo", 1, 1));
    NAOS_CHECK_UINT_EQ(1, padding("-\t\tfoo", 1, 1));
    NAOS_CHECK_UINT_EQ(2, padding("1.  foo", 2, 2));
    NAOS_CHECK_UINT_EQ(2, padding("1.\tfoo", 2, 2));
    /* 行の途中（桁 2 の記号）では、タブの桁は基準の桁から数える */
    NAOS_CHECK_UINT_EQ(1, padding("> -\tfoo", 3, 3));
}

NAOS_TEST(list_padding_with_skip_columns_gives_pad)
{
    /* `-\t\tfoo`: 記号の後ろの 1 桁を除くと pad が 2 になり、残りのインデントは 6 桁 */
    mnd_md_scan_pos_t cur = { 1, 1, 0 };
    mnd_md_scan_indent_t ind;
    size_t n = padding("-\t\tfoo", 1, 1);

    NAOS_CHECK_UINT_EQ(n, mnd_md_scan_skip_columns("-\t\tfoo", 6, &cur, n));
    NAOS_CHECK_UINT_EQ(2, cur.pos);
    NAOS_CHECK_UINT_EQ(2, cur.pad);
    mnd_md_scan_indent("-\t\tfoo", 6, &cur, &ind);
    NAOS_CHECK_UINT_EQ(6, ind.indent);
}

NAOS_SUITE(scan,
    NAOS_ADD_TEST(next_column_counts_tabs_to_multiples_of_four),
    NAOS_ADD_TEST(skip_columns_whole_tab_has_no_pad),
    NAOS_ADD_TEST(skip_columns_part_of_tab_gives_pad),
    NAOS_ADD_TEST(skip_columns_uses_pending_pad_first),
    NAOS_ADD_TEST(skip_columns_stops_at_non_space_and_end),
    NAOS_ADD_TEST(skip_columns_reads_only_up_to_end),
    NAOS_ADD_TEST(indent_counts_columns_and_blank),
    NAOS_ADD_TEST(thematic_break_positive),
    NAOS_ADD_TEST(thematic_break_negative),
    NAOS_ADD_TEST(atx_heading_levels),
    NAOS_ADD_TEST(atx_heading_negative),
    NAOS_ADD_TEST(atx_heading_closing_sequence),
    NAOS_ADD_TEST(atx_heading_empty_content),
    NAOS_ADD_TEST(atx_heading_offsets_and_end),
    NAOS_ADD_TEST(setext_underline),
    NAOS_ADD_TEST(open_fence_positive),
    NAOS_ADD_TEST(open_fence_negative),
    NAOS_ADD_TEST(open_fence_offsets),
    NAOS_ADD_TEST(close_fence),
    NAOS_ADD_TEST(list_marker_bullets),
    NAOS_ADD_TEST(list_marker_ordered),
    NAOS_ADD_TEST(list_marker_negative),
    NAOS_ADD_TEST(list_marker_offsets_and_end),
    NAOS_ADD_TEST(list_padding),
    NAOS_ADD_TEST(list_padding_with_skip_columns_gives_pad)
)
