/* LineIndex: 行の分割と行番号 */

#include <naos/naos.h>

#include "foundation/source.h"
#include "markdown/md_internal.h"
#include "markdown/md_lines.h"
#include "test_alloc.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 行の分割の期待値 */
typedef struct expected_line
{
    size_t line_start;
    size_t line_end;
    size_t eol_len;
} expected_line_t;

/* data[0..len) を先頭から分割し、expected の count 個の行と一致し、そのあと行がないことを確かめる */
static void check_split(const char *data, size_t len, const expected_line_t *expected, size_t count)
{
    mnd_md_phys_line_t line;
    size_t pos = 0;
    size_t i = 0;

    for (i = 0; i < count; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_md_lines_next(data, len, &pos, &line));
        NAOS_CHECK_UINT_EQ(expected[i].line_start, line.line_start);
        NAOS_CHECK_UINT_EQ(expected[i].line_end, line.line_end);
        NAOS_CHECK_UINT_EQ(expected[i].eol_len, line.eol_len);
        NAOS_CHECK_UINT_EQ(line.line_end + line.eol_len, pos);
    }
    NAOS_CHECK_FALSE(mnd_md_lines_next(data, len, &pos, &line));
    NAOS_CHECK_UINT_EQ(len, pos);
}

/* data[0..len) のすべてのオフセット（0..len）で、表の行番号が mnd_source_line_of と一致することを確かめる */
static void check_index_matches_foundation(const char *data, size_t len)
{
    mnd_md_arena_t arena;
    mnd_md_line_index_t index;
    size_t offset = 0;
    size_t expected = 0;
    size_t actual = 0;

    mnd_md_arena_init(&arena, NULL);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_build(&index, data, len, &arena));
    NAOS_CHECK_UINT_EQ(len, index.len);
    for (offset = 0; offset <= len; offset++)
    {
        NAOS_REQUIRE_TRUE(mnd_source_line_of(data, len, offset, &expected));
        actual = 0;
        NAOS_REQUIRE_TRUE(mnd_md_line_index_line_of(&index, offset, &actual));
        NAOS_CHECK_UINT_EQ(expected, actual);
    }
    actual = 12345;
    NAOS_CHECK_FALSE(mnd_md_line_index_line_of(&index, len + 1, &actual));
    NAOS_CHECK_UINT_EQ(12345, actual);
    mnd_md_arena_free(&arena);
}

/* 行の分割 */

NAOS_TEST(lines_split_lf)
{
    static const char data[] = "a\nbc\n\nd";
    static const expected_line_t expected[] = {
        { 0, 1, 1 }, { 2, 4, 1 }, { 5, 5, 1 }, { 6, 7, 0 }
    };

    check_split(data, sizeof(data) - 1, expected, 4);
}

NAOS_TEST(lines_split_content_ending_with_eol_has_no_extra_line)
{
    static const char data[] = "a\nb\n";
    static const expected_line_t expected[] = { { 0, 1, 1 }, { 2, 3, 1 } };

    check_split(data, sizeof(data) - 1, expected, 2);
}

NAOS_TEST(lines_split_crlf)
{
    static const char data[] = "ab\r\n\r\nc\r\n";
    static const expected_line_t expected[] = { { 0, 2, 2 }, { 4, 4, 2 }, { 6, 7, 2 } };

    check_split(data, sizeof(data) - 1, expected, 3);
}

NAOS_TEST(lines_split_lone_cr)
{
    static const char data[] = "a\rb\r\rc";
    static const expected_line_t expected[] = {
        { 0, 1, 1 }, { 2, 3, 1 }, { 4, 4, 1 }, { 5, 6, 0 }
    };

    check_split(data, sizeof(data) - 1, expected, 4);
}

NAOS_TEST(lines_split_mixed_eols)
{
    /* LF CR の並びは 2 つの行末（LFCR は 1 つの行末ではない） */
    static const char data[] = "x\n\ry\r\n\nz\r";
    static const expected_line_t expected[] = {
        { 0, 1, 1 }, { 2, 2, 1 }, { 3, 4, 2 }, { 6, 6, 1 }, { 7, 8, 1 }
    };

    check_split(data, sizeof(data) - 1, expected, 5);
}

NAOS_TEST(lines_split_last_line_without_eol)
{
    static const char data[] = "abc";
    static const expected_line_t expected[] = { { 0, 3, 0 } };

    check_split(data, sizeof(data) - 1, expected, 1);
}

NAOS_TEST(lines_split_does_not_stop_at_nul)
{
    static const char data[] = "a\0b\n\0\r\nc\0";
    static const expected_line_t expected[] = { { 0, 3, 1 }, { 4, 5, 2 }, { 7, 9, 0 } };

    check_split(data, sizeof(data) - 1, expected, 3);
}

NAOS_TEST(lines_split_empty_content)
{
    mnd_md_phys_line_t line;
    size_t pos = 0;

    line.line_start = 7;
    NAOS_CHECK_FALSE(mnd_md_lines_next(NULL, 0, &pos, &line));
    NAOS_CHECK_UINT_EQ(0, pos);
    NAOS_CHECK_UINT_EQ(7, line.line_start);
}

NAOS_TEST(lines_split_starts_at_pos_and_stops_at_end)
{
    static const char data[] = "ab\ncd\r\nef";
    mnd_md_phys_line_t line;
    size_t pos = 3;

    /* 途中の位置から始める */
    NAOS_REQUIRE_TRUE(mnd_md_lines_next(data, sizeof(data) - 1, &pos, &line));
    NAOS_CHECK_UINT_EQ(3, line.line_start);
    NAOS_CHECK_UINT_EQ(5, line.line_end);
    NAOS_CHECK_UINT_EQ(2, line.eol_len);
    NAOS_CHECK_UINT_EQ(7, pos);

    /* end が CRLF の間にあれば、end の直前の CR を単独の CR として扱い、end を越えて読まない */
    pos = 3;
    NAOS_REQUIRE_TRUE(mnd_md_lines_next(data, 6, &pos, &line));
    NAOS_CHECK_UINT_EQ(3, line.line_start);
    NAOS_CHECK_UINT_EQ(5, line.line_end);
    NAOS_CHECK_UINT_EQ(1, line.eol_len);
    NAOS_CHECK_UINT_EQ(6, pos);
    NAOS_CHECK_FALSE(mnd_md_lines_next(data, 6, &pos, &line));

    /* end が行の途中にあれば、行末記号のない行として end で終える */
    pos = 0;
    NAOS_REQUIRE_TRUE(mnd_md_lines_next(data, 1, &pos, &line));
    NAOS_CHECK_UINT_EQ(0, line.line_start);
    NAOS_CHECK_UINT_EQ(1, line.line_end);
    NAOS_CHECK_UINT_EQ(0, line.eol_len);
    NAOS_CHECK_UINT_EQ(1, pos);

    /* *pos > end でも行はない */
    pos = 8;
    NAOS_CHECK_FALSE(mnd_md_lines_next(data, 6, &pos, &line));
    NAOS_CHECK_UINT_EQ(8, pos);
}

/* 行番号の表 */

NAOS_TEST(line_index_matches_foundation_for_all_offsets)
{
    static const char lf[] = "a\nbc\n\nd";
    static const char lf_end[] = "a\nb\n";
    static const char crlf[] = "ab\r\n\r\nc\r\n";
    static const char cr[] = "a\rb\r\rc";
    static const char mixed[] = "x\n\ry\r\n\nz\r\r\n\n";
    static const char nul[] = "a\0b\n\0\r\nc\0\n";
    static const char no_eol[] = "abc";
    static const char only_lf[] = "\n\n\n";

    check_index_matches_foundation(lf, sizeof(lf) - 1);
    check_index_matches_foundation(lf_end, sizeof(lf_end) - 1);
    check_index_matches_foundation(crlf, sizeof(crlf) - 1);
    check_index_matches_foundation(cr, sizeof(cr) - 1);
    check_index_matches_foundation(mixed, sizeof(mixed) - 1);
    check_index_matches_foundation(nul, sizeof(nul) - 1);
    check_index_matches_foundation(no_eol, sizeof(no_eol) - 1);
    check_index_matches_foundation(only_lf, sizeof(only_lf) - 1);
}

NAOS_TEST(line_index_crlf_offsets_follow_foundation)
{
    /* CRLF の CR と LF を指すオフセットは CR の前と同じ行、LF の直後から次の行 */
    static const char data[] = "a\r\nb";
    mnd_md_arena_t arena;
    mnd_md_line_index_t index;
    size_t line = 0;

    mnd_md_arena_init(&arena, NULL);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_build(&index, data, sizeof(data) - 1, &arena));
    NAOS_CHECK_UINT_EQ(1, index.lf_count);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_line_of(&index, 1, &line));
    NAOS_CHECK_UINT_EQ(1, line);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_line_of(&index, 2, &line));
    NAOS_CHECK_UINT_EQ(1, line);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_line_of(&index, 3, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_line_of(&index, 4, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    mnd_md_arena_free(&arena);
}

NAOS_TEST(line_index_large_document_matches_foundation)
{
    /* 専用の塊が必要な大きさの表（LF が数千個）。改行の種類を混ぜる */
    static char data[20000];
    size_t len = sizeof(data);
    size_t i = 0;

    for (i = 0; i < len; i++)
    {
        switch (i % 7)
        {
        case 2:
            data[i] = '\n';
            break;
        case 4:
            data[i] = '\r';
            break;
        case 5:
            data[i] = ((i % 3) == 0) ? '\n' : '\0';
            break;
        default:
            data[i] = 'x';
            break;
        }
    }
    check_index_matches_foundation(data, len);
}

NAOS_TEST(line_index_empty_content)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;
    mnd_md_line_index_t index;
    size_t line = 0;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_arena_init(&arena, &ta.allocator);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_build(&index, NULL, 0, &arena));
    NAOS_CHECK_NULL(index.lf);
    NAOS_CHECK_UINT_EQ(0, index.lf_count);
    NAOS_CHECK_UINT_EQ(0, index.len);
    NAOS_CHECK_UINT_EQ(0, ta.calls);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_line_of(&index, 0, &line));
    NAOS_CHECK_UINT_EQ(1, line);
    NAOS_CHECK_FALSE(mnd_md_line_index_line_of(&index, 1, &line));
    mnd_md_arena_free(&arena);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
}

NAOS_TEST(line_index_without_lf_does_not_allocate)
{
    static const char data[] = "a\rb\0c";
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;
    mnd_md_line_index_t index;

    mnd_md_test_alloc_init(&ta, 1);
    mnd_md_arena_init(&arena, &ta.allocator);
    NAOS_REQUIRE_TRUE(mnd_md_line_index_build(&index, data, sizeof(data) - 1, &arena));
    NAOS_CHECK_NULL(index.lf);
    NAOS_CHECK_UINT_EQ(0, index.lf_count);
    NAOS_CHECK_UINT_EQ(0, ta.calls);
    mnd_md_arena_free(&arena);
}

/* 小さい表と大きい表（専用の塊）の両方で、確保の失敗が返り、解放の回数が釣り合うことを確かめる */
static void check_build_oom(const char *data, size_t len)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;
    mnd_md_line_index_t index;
    size_t fail_at = 0;
    bool built = false;

    for (fail_at = 1; !built; fail_at++)
    {
        NAOS_REQUIRE_UINT_LT(fail_at, 100);
        mnd_md_test_alloc_init(&ta, fail_at);
        mnd_md_arena_init(&arena, &ta.allocator);
        built = mnd_md_line_index_build(&index, data, len, &arena);
        if (!built)
        {
            NAOS_CHECK_UINT_EQ(1, ta.failures);
            NAOS_CHECK_NULL(index.lf);
            NAOS_CHECK_UINT_EQ(0, index.lf_count);
        }
        else
        {
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_CHECK_NOT_NULL(index.lf);
        }
        mnd_md_arena_free(&arena);
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(&ta));
    }
    /* 少なくとも 1 回は失敗させた */
    NAOS_CHECK_UINT_GT(fail_at, 2);
}

NAOS_TEST(line_index_build_reports_oom_and_frees_everything)
{
    static const char small[] = "a\nb\r\nc\rd\n";
    static char big[8192];

    memset(big, '\n', sizeof(big));
    check_build_oom(small, sizeof(small) - 1);
    check_build_oom(big, sizeof(big));
}

NAOS_SUITE(lines_index,
    NAOS_ADD_TEST(lines_split_lf),
    NAOS_ADD_TEST(lines_split_content_ending_with_eol_has_no_extra_line),
    NAOS_ADD_TEST(lines_split_crlf),
    NAOS_ADD_TEST(lines_split_lone_cr),
    NAOS_ADD_TEST(lines_split_mixed_eols),
    NAOS_ADD_TEST(lines_split_last_line_without_eol),
    NAOS_ADD_TEST(lines_split_does_not_stop_at_nul),
    NAOS_ADD_TEST(lines_split_empty_content),
    NAOS_ADD_TEST(lines_split_starts_at_pos_and_stops_at_end),
    NAOS_ADD_TEST(line_index_matches_foundation_for_all_offsets),
    NAOS_ADD_TEST(line_index_crlf_offsets_follow_foundation),
    NAOS_ADD_TEST(line_index_large_document_matches_foundation),
    NAOS_ADD_TEST(line_index_empty_content),
    NAOS_ADD_TEST(line_index_without_lf_does_not_allocate),
    NAOS_ADD_TEST(line_index_build_reports_oom_and_frees_everything)
)
