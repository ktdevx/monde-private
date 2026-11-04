#include <naos/naos.h>

#include "foundation/buf.h"
#include "po/po_internal.h"
#include "po/po_write.h"

#include <string.h>

NAOS_TEST(write_options_init_is_wrap_and_lf)
{
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;

    NAOS_CHECK_FALSE(options.no_wrap);
    NAOS_CHECK_INT_EQ(MND_EOL_LF, options.eol);
}

/*
 * Wrap（文字列の行）。期待値は GNU gettext 0.21 の msgcat の出力に合わせてある。
 * テスト用: NUL 終端の s を改行 "\n" で整形し、成功したかを返す
 */
static bool wrap_ok(const char *prefix, const char *keyword, const char *s, bool wrap,
                    mnd_buf_t *out)
{
    return mnd_po_wrap_string(prefix, keyword, s, strlen(s), wrap, "\n", out);
}

/* テスト用: c を count 回並べた文字列を out に加える */
static void append_repeat(mnd_buf_t *out, const char *c, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, c));
    }
}

NAOS_TEST(write_wrap_79_columns_fits_on_keyword_line)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* msgid "<71 文字>" で 79 桁 */
    append_repeat(&s, "abcd ", 14);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "e"));
    NAOS_REQUIRE_UINT_EQ(s.len, (size_t)71);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_80_columns_uses_empty_keyword_line)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* msgid "<72 文字>" で 80 桁。続きの行は "<72 文字>" で 74 桁に収まる */
    append_repeat(&s, "abcd ", 14);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "ef"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_continuation_line_79_and_80_columns)
{
    mnd_buf_t out = MND_BUF_INIT;
    const char *a70 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    char s[128];
    char expected[256];

    /* "<a70> bbbbb " は引用符を含めて 79 桁で 1 行に収まる */
    strcpy(s, a70);
    strcat(s, " bbbbb cccccccccc");
    strcpy(expected, "msgid \"\"\n\"");
    strcat(expected, a70);
    strcat(expected, " bbbbb \"\n\"cccccccccc\"\n");
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", s, true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), expected);

    /* "<a70> bbbbbb " は 80 桁になるので、bbbbbb の前で区切る */
    mnd_buf_clear(&out);
    strcpy(s, a70);
    strcat(s, " bbbbbb cccccccccc");
    strcpy(expected, "msgid \"\"\n\"");
    strcat(expected, a70);
    strcat(expected, " \"\n\"bbbbbb cccccccccc\"\n");
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", s, true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), expected);
    mnd_buf_free(&out);
}

NAOS_TEST(write_wrap_counts_trailing_spaces)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* 行末の空白も桁に数える: "word " × 15 で 75 文字、引用符を含めて 77 桁 */
    append_repeat(&s, "word ", 20);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    append_repeat(&expected, "word ", 15);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\""));
    append_repeat(&expected, "word ", 5);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_trailing_newline_only_stays_on_one_line)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_CHECK_TRUE(wrap_ok("", "msgstr", "abc\n", true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "msgstr \"abc\\n\"\n");
    mnd_buf_free(&out);
}

NAOS_TEST(write_wrap_inner_newline_splits_after_it)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_CHECK_TRUE(wrap_ok("", "msgid", "abc\ndef", true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "msgid \"\"\n\"abc\\n\"\n\"def\"\n");

    /* 末尾の空の区分は捨てる。空の区分の間の \n はそれぞれ 1 行にする */
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", "abc\n\n", true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "msgid \"\"\n\"abc\\n\"\n\"\\n\"\n");
    mnd_buf_free(&out);
}

NAOS_TEST(write_wrap_empty_string)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_CHECK_TRUE(wrap_ok("", "msgstr", "", true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "msgstr \"\"\n");
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(wrap_ok("#~ ", "msgstr[1]", "", false, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "#~ msgstr[1] \"\"\n");
    mnd_buf_free(&out);
}

NAOS_TEST(write_wrap_long_word_without_break_opportunity)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* 分割の機会のない 90 文字の語は、W を超えてもそのまま 1 行にする */
    append_repeat(&s, "aaaa ", 5);
    append_repeat(&s, "b", 90);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, " cc"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    append_repeat(&expected, "aaaa ", 5);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\""));
    append_repeat(&expected, "b", 90);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, " \"\n\"cc\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* 最初の分割の機会までで W を超える区分 */
    mnd_buf_clear(&s);
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    append_repeat(&s, "a", 100);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "\nb"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    append_repeat(&expected, "a", 100);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\\n\"\n\"b\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_fullwidth_counts_two_columns)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* msgid "a" + あ × 35 で 6 + 1 + 71 + 1 = 79 桁 */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "a"));
    append_repeat(&s, "\xE3\x81\x82", 35);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* あ × 36 は 80 桁。続きの行には あ × 38（引用符を含めて 78 桁）まで入る */
    mnd_buf_clear(&s);
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    append_repeat(&s, "\xE3\x81\x82", 40);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    append_repeat(&expected, "\xE3\x81\x82", 38);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\""));
    append_repeat(&expected, "\xE3\x81\x82", 2);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_counts_prefix_columns)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* #~ msgid "<68 文字>" で 79 桁 */
    append_repeat(&s, "o ", 34);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#~ msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#~ ", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* #~ msgid "<69 文字>" で 80 桁 */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "p"));
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#~ msgid \"\"\n#~ \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#~ ", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* #~| msgid "<67 文字>" で 79 桁 */
    mnd_buf_clear(&s);
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    append_repeat(&s, "v ", 33);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "v"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#~| msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#~| ", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_prefix_on_continuation_lines)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* 続きの行: #| "<74 文字>" で 79 桁 */
    append_repeat(&s, "z ", 40);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#| msgid \"\"\n#| \""));
    append_repeat(&expected, "z ", 37);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n#| \""));
    append_repeat(&expected, "z ", 3);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#| ", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* 続きの行: #~| "<73 文字>" で 79 桁 */
    mnd_buf_clear(&s);
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    append_repeat(&s, "w ", 40);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#~| msgid \"\"\n#~| \""));
    append_repeat(&expected, "w ", 36);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n#~| \""));
    append_repeat(&expected, "w ", 4);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#~| ", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_escapes_count_and_are_not_split)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* \" は 2 桁に数える */
    append_repeat(&s, "say \"hi\" ", 9);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    append_repeat(&expected, "say \\\"hi\\\" ", 7);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\""));
    append_repeat(&expected, "say \\\"hi\\\" ", 2);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* タブの後ろで区切れるが、\t の内部では区切らない */
    mnd_buf_clear(&s);
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    append_repeat(&s, "x \t y ", 14);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \"\"\n\""));
    append_repeat(&expected, "x \\t y ", 11);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\""));
    append_repeat(&expected, "x \\t y ", 3);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_disabled_splits_only_after_newline)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /* 区分が 1 つなら、桁を問わず 1 行 */
    append_repeat(&s, "word ", 20);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), false, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* 途中の \n の直後でだけ区切る */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "\n"));
    append_repeat(&s, "next ", 20);
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#~ msgstr \"\"\n#~ \""));
    append_repeat(&expected, "word ", 20);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\\n\"\n#~ \""));
    append_repeat(&expected, "next ", 20);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#~ ", "msgstr", mnd_buf_cstr(&s), false, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_appends_with_given_eol)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "pre\r\n"));
    NAOS_CHECK_TRUE(mnd_po_wrap_string("", "msgstr[0]", "a\nb", 3, true, "\r\n", &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "pre\r\nmsgstr[0] \"\"\r\n\"a\\n\"\r\n\"b\"\r\n");
    mnd_buf_free(&out);
}

NAOS_SUITE(write,
    NAOS_ADD_TEST(write_options_init_is_wrap_and_lf),
    NAOS_ADD_TEST(write_wrap_79_columns_fits_on_keyword_line),
    NAOS_ADD_TEST(write_wrap_80_columns_uses_empty_keyword_line),
    NAOS_ADD_TEST(write_wrap_continuation_line_79_and_80_columns),
    NAOS_ADD_TEST(write_wrap_counts_trailing_spaces),
    NAOS_ADD_TEST(write_wrap_trailing_newline_only_stays_on_one_line),
    NAOS_ADD_TEST(write_wrap_inner_newline_splits_after_it),
    NAOS_ADD_TEST(write_wrap_empty_string),
    NAOS_ADD_TEST(write_wrap_long_word_without_break_opportunity),
    NAOS_ADD_TEST(write_wrap_fullwidth_counts_two_columns),
    NAOS_ADD_TEST(write_wrap_counts_prefix_columns),
    NAOS_ADD_TEST(write_wrap_prefix_on_continuation_lines),
    NAOS_ADD_TEST(write_wrap_escapes_count_and_are_not_split),
    NAOS_ADD_TEST(write_wrap_disabled_splits_only_after_newline),
    NAOS_ADD_TEST(write_wrap_appends_with_given_eol)
)
