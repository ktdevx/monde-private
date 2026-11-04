#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
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

NAOS_TEST(write_wrap_single_segment_without_break_opportunity_stays_on_keyword_line)
{
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    /*
     * msgcat 0.21 と同じく、区分が 1 つで分割の機会がなければ、W を超えてもキーワードの行に書く
     * （"" の行にするのは、実際に区切るときか、途中に \n があるときだけ）
     */
    append_repeat(&s, "x", 100);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgid \""));
    append_repeat(&expected, "x", 100);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("", "msgid", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* 末尾の \n の直前でも区切らないので、末尾だけに \n がある区分も同じ */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&s, "\n"));
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#~ msgstr \""));
    append_repeat(&expected, "x", 100);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\\n\"\n"));
    NAOS_CHECK_TRUE(wrap_ok("#~ ", "msgstr", mnd_buf_cstr(&s), true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
}

NAOS_TEST(write_wrap_break_opportunities_come_from_escaped_text)
{
    mnd_buf_t out = MND_BUF_INIT;

    /*
     * msgcat 0.21 と同じく、分割の機会はエスケープ後の文字列で求める。タブの後ろ（エスケープ前の BA）
     * ではなく、"\t" の "\"（PR）の前で区切れるかで決まる: ":" の後ろでは区切れ、英字の後ろでは区切れない
     */
    NAOS_CHECK_TRUE(wrap_ok("", "msgid",
                            "Columns:\tname\tsize\tdate\tpermissions\towner\tgroup\tmodified\tcreated"
                            "\taccessed\tchanged\tinode",
                            true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"\"\n"
                      "\"Columns:\"\n"
                      "\"\\tname\\tsize\\tdate\\tpermissions\\towner\\tgroup\\tmodified\\tcreated"
                      "\\taccessed\\tchanged\\tinode\"\n");

    /* 空白の後ろでも、\n のエスケープの直前では区切らない */
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(wrap_ok("", "msgid",
                            "abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd "
                            "abcdef \nnext line",
                            true, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"\"\n"
                      "\"abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd abcd \"\n"
                      "\"abcdef \\n\"\n"
                      "\"next line\"\n");
    mnd_buf_free(&out);
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

/*
 * Writer（カタログの書き出し）。期待値は GNU gettext 0.21 の msgcat の出力に合わせてある。
 * テスト用: 末尾にエントリを加え、msgctxt（NULL ならなし）、msgid、msgstr（NULL なら 0 個）を設定する
 */
static mnd_po_entry_t *add_msg(mnd_po_catalog_t *catalog, const char *msgctxt, const char *msgid,
                               const char *msgstr)
{
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);

    if (entry == NULL)
    {
        return NULL;
    }

    if (msgctxt != NULL)
    {
        entry->has_msgctxt = true;
        if (!mnd_buf_append_str(&entry->msgctxt, msgctxt))
        {
            return NULL;
        }
    }

    if (!mnd_buf_append_str(&entry->msgid, msgid))
    {
        return NULL;
    }

    if ((msgstr != NULL) && !mnd_po_strings_push(&entry->msgstr, msgstr, strlen(msgstr)))
    {
        return NULL;
    }

    return entry;
}

static bool push_str(mnd_po_strings_t *list, const char *str)
{
    return mnd_po_strings_push(list, str, strlen(str));
}

/* テスト用: options（NULL 可）で書き出し、成功して診断がないことを確かめる */
static void write_ok(const mnd_po_catalog_t *catalog, const mnd_po_write_options_t *options,
                     mnd_buf_t *out)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_TRUE(mnd_po_write(catalog, options, out, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), (size_t)0);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_entry_line_order_and_comments)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 呼び出し側が並べた順（msgid の順ではない）で書く（1.8、5.1） */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "second", "zweite"));
    entry = add_msg(&catalog, "c", "x", "y");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, "ys"));
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "tc"));
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, ""));
    NAOS_REQUIRE_TRUE(push_str(&entry->extracted_comments, "ex"));
    NAOS_REQUIRE_TRUE(push_str(&entry->extracted_comments, ""));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "a.c", 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 0));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    entry->has_prev_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgctxt, "pc"));
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, "old"));
    entry->has_prev_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid_plural, "olds"));
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "xs"));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "first", ""));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"second\"\n"
                      "msgstr \"zweite\"\n"
                      "\n"
                      "# tc\n"
                      "#\n"
                      "#. ex\n"
                      "#.\n"
                      "#: a.c:1 b.c\n"
                      "#, c-format\n"
                      "#| msgctxt \"pc\"\n"
                      "#| msgid \"old\"\n"
                      "#| msgid_plural \"olds\"\n"
                      "msgctxt \"c\"\n"
                      "msgid \"x\"\n"
                      "msgid_plural \"xs\"\n"
                      "msgstr[0] \"y\"\n"
                      "msgstr[1] \"ys\"\n"
                      "\n"
                      "msgid \"first\"\n"
                      "msgstr \"\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_escapes_strings)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;

    /* 3.4: エスケープして書く */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, "q\"\\", "t\tb\x07", "\r"));
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgctxt \"q\\\"\\\\\"\n"
                      "msgid \"t\\tb\\a\"\n"
                      "msgstr \"\\r\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_appends_to_existing_output)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;

    /* 空のカタログでは何も書かずに成功する */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep\n"));
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep\n");

    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "a", "b"));
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep\nmsgid \"a\"\nmsgstr \"b\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_fuzzy_flag_comes_first)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    entry = add_msg(&catalog, NULL, "a", "b");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "range: 0..10"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "no-c-format"));
    entry = add_msg(&catalog, NULL, "c", "d");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry = add_msg(&catalog, NULL, "e", "f");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "python-format"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "#, fuzzy, c-format, range: 0..10, no-c-format\n"
                      "msgid \"a\"\n"
                      "msgstr \"b\"\n"
                      "\n"
                      "#, fuzzy\n"
                      "msgid \"c\"\n"
                      "msgstr \"d\"\n"
                      "\n"
                      "#, python-format\n"
                      "msgid \"e\"\n"
                      "msgstr \"f\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_obsolete_entry_prefixes)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.3: コメント・参照・フラグはそのまま、文字列の行には #~ と #~| を付ける */
    entry = add_msg(&catalog, "oc", "o", "p");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, "ps"));
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "tc"));
    NAOS_REQUIRE_TRUE(push_str(&entry->extracted_comments, "ex"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "o.c", 3));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry->has_prev_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgctxt, "pc"));
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, "oldo"));
    entry->has_prev_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid_plural, "olds"));
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "os"));
    entry = add_msg(&catalog, NULL, "a\nb", "c");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "# tc\n"
                      "#. ex\n"
                      "#: o.c:3\n"
                      "#, fuzzy\n"
                      "#~| msgctxt \"pc\"\n"
                      "#~| msgid \"oldo\"\n"
                      "#~| msgid_plural \"olds\"\n"
                      "#~ msgctxt \"oc\"\n"
                      "#~ msgid \"o\"\n"
                      "#~ msgid_plural \"os\"\n"
                      "#~ msgstr[0] \"p\"\n"
                      "#~ msgstr[1] \"ps\"\n"
                      "\n"
                      "#~ msgid \"\"\n"
                      "#~ \"a\\n\"\n"
                      "#~ \"b\"\n"
                      "#~ msgstr \"c\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_crlf_and_default_eol)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t lf = MND_BUF_INIT;
    mnd_po_entry_t *entry;
    char a70[71];

    memset(a70, 'a', 70);
    a70[70] = '\0';
    entry = add_msg(&catalog, NULL, "x\ny", "z");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "c"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, a70, 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 2));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "w", ""));

    options.eol = MND_EOL_CRLF;
    write_ok(&catalog, &options, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "# c\r\n"
                      "#: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:1\r\n"
                      "#: b.c:2\r\n"
                      "#, fuzzy\r\n"
                      "msgid \"\"\r\n"
                      "\"x\\n\"\r\n"
                      "\"y\"\r\n"
                      "msgstr \"z\"\r\n"
                      "\r\n"
                      "msgid \"w\"\r\n"
                      "msgstr \"\"\r\n");

    /* 指定がなければ（NULL、MND_EOL_LF、MND_EOL_NONE）LF（5.4） */
    mnd_buf_clear(&out);
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "# c\n"
                      "#: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:1\n"
                      "#: b.c:2\n"
                      "#, fuzzy\n"
                      "msgid \"\"\n"
                      "\"x\\n\"\n"
                      "\"y\"\n"
                      "msgstr \"z\"\n"
                      "\n"
                      "msgid \"w\"\n"
                      "msgstr \"\"\n");
    options.eol = MND_EOL_NONE;
    NAOS_REQUIRE_TRUE(mnd_buf_append(&lf, out.data, out.len));
    mnd_buf_clear(&out);
    write_ok(&catalog, &options, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&lf));
    mnd_buf_free(&out);
    mnd_buf_free(&lf);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_duplicate_leaves_output_unchanged)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;
    const mnd_diag_t *diag;

    /* msgctxt なしと空の msgctxt は別のキー。廃止エントリは重複にならない */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "a", "1"));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, "", "a", "2"));
    entry = add_msg(&catalog, NULL, "a", "3");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep\n"));
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_UINT_GT(out.len, (size_t)5);

    /* 重複があれば、out を変えずに false を返す（5.5） */
    mnd_buf_clear(&out);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep\n"));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "a", "4"));
    NAOS_CHECK_FALSE(mnd_po_write(&catalog, NULL, &out, &diags));
    NAOS_CHECK_UINT_EQ(out.len, (size_t)5);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep\n");
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), (size_t)1);
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, diag->severity);
    NAOS_CHECK_STR_EQ(diag->message, "duplicate message definition: msgctxt (none), msgid \"a\"");
    NAOS_CHECK_NULL(diag->path);
    NAOS_CHECK_UINT_EQ(diag->line, (size_t)0);
    mnd_diag_list_free(&diags);

    /* 空の出力でも変わらない */
    mnd_buf_free(&out);
    NAOS_CHECK_FALSE(mnd_po_write(&catalog, NULL, &out, &diags));
    NAOS_CHECK_UINT_EQ(out.len, (size_t)0);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "");
    mnd_diag_list_free(&diags);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_duplicate_reports_each_pair_with_shortened_values)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    char long_id[51];

    /* 値は先頭 40 バイトまでで、長ければ ... を付ける。制御文字は ? にする */
    memset(long_id, 'x', 50);
    long_id[50] = '\0';
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, "ctx", long_id, ""));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "l1\nl2", ""));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, "ctx", long_id, ""));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "l1\nl2", ""));
    NAOS_CHECK_FALSE(mnd_po_write(&catalog, NULL, &out, &diags));
    NAOS_CHECK_UINT_EQ(out.len, (size_t)0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), (size_t)2);
    NAOS_CHECK_STR_EQ(mnd_diag_list_at(&diags, 0)->message,
                      "duplicate message definition: msgctxt \"ctx\", "
                      "msgid \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...\"");
    NAOS_CHECK_STR_EQ(mnd_diag_list_at(&diags, 1)->message,
                      "duplicate message definition: msgctxt (none), msgid \"l1?l2\"");
    mnd_diag_list_free(&diags);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

/* テスト用: c を count 回並べた NUL 終端の文字列を buf（count + 1 バイト以上）に作る */
static const char *repeat_char(char *buf, char c, size_t count)
{
    memset(buf, c, count);
    buf[count] = '\0';

    return buf;
}

NAOS_TEST(write_refs_79_byte_boundary)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    mnd_po_entry_t *entry;
    char name[128];

    /* "#: <66 文字>.c:1 b.c:1" はちょうど 79 バイトで 1 行（6.7） */
    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    repeat_char(name, 'a', 66);
    strcat(name, ".c");
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, name, 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 1));

    /* 80 バイトになるなら、次の参照を新しい行に書く */
    entry = add_msg(&catalog, NULL, "b", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    repeat_char(name, 'a', 67);
    strcat(name, ".c");
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, name, 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 1));

    /* 1 つで 79 バイトを超える参照は、その 1 つだけで 1 行にする */
    entry = add_msg(&catalog, NULL, "c", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "d.c", 0));
    repeat_char(name, 'c', 90);
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, name, 5));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "e.c", 12));

    write_ok(&catalog, NULL, &out);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, repeat_char(name, 'a', 66)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ".c:1 b.c:1\nmsgid \"a\"\nmsgstr \"\"\n\n#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, repeat_char(name, 'a', 67)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ".c:1\n#: b.c:1\nmsgid \"b\"\nmsgstr \"\"\n\n"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: d.c\n#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, repeat_char(name, 'c', 90)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":5\n#: e.c:12\nmsgid \"c\"\nmsgstr \"\"\n"));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_refs_count_bytes_and_isolate_names_with_spaces)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t name = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    mnd_po_entry_t *entry;
    char a[64];

    /* 桁ではなくバイト数で数える: "#: " + あ × 22 + ".c:1 b.c:1" は 79 バイト（57 桁） */
    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    append_repeat(&name, "\xE3\x81\x82", 22);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&name, ".c"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, mnd_buf_cstr(&name), 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 1));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&name)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1 b.c:1\nmsgid \"a\"\nmsgstr \"\"\n\n"));

    /* あ × 23 では 82 バイト（59 桁）になるので、新しい行にする */
    entry = add_msg(&catalog, NULL, "b", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    mnd_buf_clear(&name);
    append_repeat(&name, "\xE3\x81\x82", 23);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&name, ".c"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, mnd_buf_cstr(&name), 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 1));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&name)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1\n#: b.c:1\nmsgid \"b\"\nmsgstr \"\"\n\n"));

    /*
     * 空白・タブを含むファイル名は U+2068 と U+2069 で囲み、その 6 バイトも数える:
     * "#: <60 文字>.c:1 ⁨x y.c⁩" は 79 バイト
     */
    entry = add_msg(&catalog, NULL, "c", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    repeat_char(a, 'a', 60);
    strcat(a, ".c");
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, a, 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "x y.c", 0));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "t\tb.c", 7));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, a));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1 \xE2\x81\xA8x y.c\xE2\x81\xA9\n"
                                                    "#: \xE2\x81\xA8t\tb.c\xE2\x81\xA9:7\n"
                                                    "msgid \"c\"\nmsgstr \"\"\n\n"));

    /* <61 文字> では 80 バイトになる */
    entry = add_msg(&catalog, NULL, "d", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    repeat_char(a, 'a', 61);
    strcat(a, ".c");
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, a, 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "x y.c", 0));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, a));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1\n#: \xE2\x81\xA8x y.c\xE2\x81\xA9\n"
                                                    "msgid \"d\"\nmsgstr \"\"\n"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&out);
    mnd_buf_free(&name);
    mnd_buf_free(&expected);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_refs_with_empty_file_name)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /*
     * 行番号のある空のファイル名は msgcat 0.21 と同じく ":5" と書く。行番号もない空のファイル名は、
     * 読み直しても参照が消えないように U+2068 と U+2069 だけを書く（リーダーは空のファイル名に戻す）
     */
    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "", 5));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "", 0));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 0));
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "#: :5 \xE2\x81\xA8\xE2\x81\xA9 b.c\nmsgid \"a\"\nmsgstr \"\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_no_wrap_option_still_wraps_refs)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    mnd_po_entry_t *entry;
    char name[128];
    size_t i;

    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    repeat_char(name, 'a', 67);
    strcat(name, ".c");
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, name, 1));
    for (i = 1; i <= 20; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", i));
    }

    /* msgcat 0.21 の --no-wrap と同じく、折り返しなしでも参照は 79 バイトで折り返す */
    options.no_wrap = true;
    write_ok(&catalog, &options, &out);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, name));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1\n"
                                                    "#: b.c:1 b.c:2 b.c:3 b.c:4 b.c:5 b.c:6 b.c:7"
                                                    " b.c:8 b.c:9 b.c:10 b.c:11 b.c:12\n"
                                                    "#: b.c:13 b.c:14 b.c:15 b.c:16 b.c:17 b.c:18"
                                                    " b.c:19 b.c:20\nmsgid \"a\"\nmsgstr \"\"\n"));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_no_wrap_flag_and_option)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_buf_t s = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    mnd_buf_t wrapped = MND_BUF_INIT;
    mnd_buf_t name = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 100 文字の文字列。折り返すと 15 語と 5 語の 2 行になる */
    append_repeat(&s, "word ", 20);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&wrapped, "\"\"\n\""));
    append_repeat(&wrapped, "word ", 15);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&wrapped, "\"\n\""));
    append_repeat(&wrapped, "word ", 5);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&wrapped, "\"\n"));
    append_repeat(&name, "n", 70);

    /* 1 つ目: no-wrap フラグ。文字列は折り返さないが、参照は折り返す（6.6、6.7） */
    entry = add_msg(&catalog, NULL, mnd_buf_cstr(&s), mnd_buf_cstr(&s));
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "no-wrap"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, mnd_buf_cstr(&name), 1));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "b.c", 2));
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, mnd_buf_cstr(&s)));
    /* 2 つ目: フラグなし */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, "c", mnd_buf_cstr(&s), mnd_buf_cstr(&s)));

    write_ok(&catalog, NULL, &out);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&name)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1\n#: b.c:2\n#, no-wrap\n#| msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\nmsgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\nmsgstr \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\nmsgctxt \"c\"\nmsgid "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&wrapped)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "msgstr "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&wrapped)));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));

    /* no_wrap オプションでは、どのエントリの文字列も折り返さない（6.5）。参照は折り返す */
    mnd_buf_clear(&out);
    mnd_buf_clear(&expected);
    options.no_wrap = true;
    write_ok(&catalog, &options, &out);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "#: "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&name)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, ":1\n#: b.c:2\n#, no-wrap\n#| msgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\nmsgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\nmsgstr \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n\nmsgctxt \"c\"\nmsgid \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\nmsgstr \""));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&s)));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, "\"\n"));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), mnd_buf_cstr(&expected));
    mnd_buf_free(&s);
    mnd_buf_free(&out);
    mnd_buf_free(&expected);
    mnd_buf_free(&wrapped);
    mnd_buf_free(&name);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_zero_msgstr_entries)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /*
     * 単数形: msgstr ""。複数形: msgstr[0] "" と msgstr[1] ""。
     * msgstr が 0 個の複数形の廃止エントリ gone は、最初の msgstr が空でないという条件を満たさないので、
     * エントリごと書かないことを確かめる（5.8。廃止エントリの msgstr[n] の書き方は
     * write_obsolete_entry_prefixes で訳のあるエントリで確かめる）
     */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "one", NULL));
    entry = add_msg(&catalog, NULL, "file", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "files"));
    entry = add_msg(&catalog, NULL, "gone", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "gones"));
    /* 単数形で 2 個以上あれば 1 つ目だけを書く */
    entry = add_msg(&catalog, NULL, "two", "first");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, "second"));
    /* 複数形で 3 個なら 3 個とも書く */
    entry = add_msg(&catalog, NULL, "day", "d0");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "days"));
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, "d1"));
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, "d2"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"one\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "msgid \"file\"\n"
                      "msgid_plural \"files\"\n"
                      "msgstr[0] \"\"\n"
                      "msgstr[1] \"\"\n"
                      "\n"
                      "msgid \"two\"\n"
                      "msgstr \"first\"\n"
                      "\n"
                      "msgid \"day\"\n"
                      "msgid_plural \"days\"\n"
                      "msgstr[0] \"d0\"\n"
                      "msgstr[1] \"d1\"\n"
                      "msgstr[2] \"d2\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

/* テスト用: エントリのフラグが names（count 個）と同じ順で同じ内容であることを確かめる */
static void check_flags(const mnd_po_entry_t *entry, const char *const *names, size_t count)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(entry->flags.count, count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->flags.items[i]), names[i]);
    }
}

NAOS_TEST(write_fuzzy_skipped_when_singular_msgstr_empty)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.7: fuzzy だけで msgstr が空ならフラグの行を書かない（fuzzy が重複していても同じ） */
    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry = add_msg(&catalog, NULL, "b", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"a\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "msgid \"b\"\n"
                      "msgstr \"\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_fuzzy_skipped_keeps_other_flags)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.7: fuzzy を飛ばし、ほかのフラグはエントリの順に書く */
    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    entry = add_msg(&catalog, NULL, "b", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "python-format"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "range: 0..10"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "#, c-format\n"
                      "msgid \"a\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "#, python-format, range: 0..10\n"
                      "msgid \"b\"\n"
                      "msgstr \"\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_fuzzy_skipped_when_no_msgstr)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.7: msgstr が 0 個（単数形と複数形）なら fuzzy を書かない */
    entry = add_msg(&catalog, NULL, "one", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry = add_msg(&catalog, NULL, "file", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "files"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"one\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "#, c-format\n"
                      "msgid \"file\"\n"
                      "msgid_plural \"files\"\n"
                      "msgstr[0] \"\"\n"
                      "msgstr[1] \"\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_fuzzy_plural_depends_on_first_msgstr)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.7: 複数形は msgstr[0] だけで決める。msgstr[0] が空なら書かない */
    entry = add_msg(&catalog, NULL, "x", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "xs"));
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, "ys"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    /* msgstr[1] だけが空なら書く */
    entry = add_msg(&catalog, NULL, "z", "w");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "zs"));
    NAOS_REQUIRE_TRUE(push_str(&entry->msgstr, ""));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"x\"\n"
                      "msgid_plural \"xs\"\n"
                      "msgstr[0] \"\"\n"
                      "msgstr[1] \"ys\"\n"
                      "\n"
                      "#, fuzzy\n"
                      "msgid \"z\"\n"
                      "msgid_plural \"zs\"\n"
                      "msgstr[0] \"w\"\n"
                      "msgstr[1] \"\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_fuzzy_skipped_for_obsolete_and_header)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /*
     * 5.7: ヘッダにも同じ規則を当てはめる。最初の msgstr が空の廃止エントリ o と p は、fuzzy を
     * 飛ばすのではなくエントリごと書かない（5.8）。訳のある fuzzy の廃止エントリ q は #, fuzzy 付きで書く
     */
    entry = add_msg(&catalog, NULL, "", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "h"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry = add_msg(&catalog, NULL, "o", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry = add_msg(&catalog, NULL, "p", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    entry = add_msg(&catalog, NULL, "q", "r");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "# h\n"
                      "msgid \"\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "#, fuzzy\n"
                      "#~ msgid \"q\"\n"
                      "#~ msgstr \"r\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_fuzzy_skip_leaves_entry_flags_unchanged)
{
    static const char *const only_fuzzy[] = {"fuzzy"};
    static const char *const mixed[] = {"c-format", "fuzzy", "fuzzy", "no-wrap"};
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.7: 書き出しはエントリの flags を変更しない（書かなかった fuzzy も残る） */
    entry = add_msg(&catalog, NULL, "a", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    entry = add_msg(&catalog, NULL, "b", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "no-wrap"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"a\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "#, c-format, no-wrap\n"
                      "msgid \"b\"\n"
                      "msgstr \"\"\n");
    NAOS_REQUIRE_UINT_EQ(catalog.count, (size_t)2);
    check_flags(&catalog.entries[0], only_fuzzy, 1);
    check_flags(&catalog.entries[1], mixed, 4);
    NAOS_CHECK_INT_EQ((int)mnd_po_entry_state(&catalog.entries[0]), (int)MND_PO_STATE_UNTRANSLATED);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

/* テスト用: 単数形の廃止エントリ msgid（msgstr が NULL なら 0 個）を加える */
static mnd_po_entry_t *add_obsolete(mnd_po_catalog_t *catalog, const char *msgid,
                                    const char *msgstr)
{
    mnd_po_entry_t *entry = add_msg(catalog, NULL, msgid, msgstr);

    if (entry != NULL)
    {
        entry->obsolete = true;
    }

    return entry;
}

/* テスト用: 複数形の廃止エントリ msgid を加え、msgstr[0] と msgstr[1] を s0 と s1 にする */
static mnd_po_entry_t *add_obsolete_plural(mnd_po_catalog_t *catalog, const char *msgid,
                                           const char *s0, const char *s1)
{
    mnd_po_entry_t *entry = add_obsolete(catalog, msgid, s0);

    if ((entry == NULL) || !mnd_buf_append_str(&entry->msgid_plural, "pl")
        || !push_str(&entry->msgstr, s1))
    {
        return NULL;
    }

    entry->has_msgid_plural = true;

    return entry;
}

NAOS_TEST(write_obsolete_without_first_msgstr_is_skipped)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.8: 単数形の空、msgstr が 0 個（単数形と複数形）は書かない */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "keep1", "K1"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "empty", ""));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "none", NULL));
    entry = add_obsolete(&catalog, "noneplural", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid_plural, "pl"));
    /* コメント・参照・フラグ・前回の msgctxt と msgid と msgid_plural・msgctxt を持つものも 1 行も書かない */
    entry = add_msg(&catalog, "ctx", "full", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "tc"));
    NAOS_REQUIRE_TRUE(push_str(&entry->extracted_comments, "ex"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, "f.c", 1));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    entry->has_prev_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgctxt, "pc"));
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, "pm"));
    entry->has_prev_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid_plural, "pp"));
    /* 複数形ですべて空、msgstr[0] だけ空のものは書かない */
    NAOS_REQUIRE_NOT_NULL(add_obsolete_plural(&catalog, "allempty", "", ""));
    NAOS_REQUIRE_NOT_NULL(add_obsolete_plural(&catalog, "first", "", "S1"));
    /* 複数形で msgstr[1] だけ空のものと、訳のあるものは書く */
    NAOS_REQUIRE_NOT_NULL(add_obsolete_plural(&catalog, "second", "S0", ""));
    NAOS_REQUIRE_NOT_NULL(add_obsolete_plural(&catalog, "both", "B0", "B1"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "single", "T"));

    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"keep1\"\n"
                      "msgstr \"K1\"\n"
                      "\n"
                      "#~ msgid \"second\"\n"
                      "#~ msgid_plural \"pl\"\n"
                      "#~ msgstr[0] \"S0\"\n"
                      "#~ msgstr[1] \"\"\n"
                      "\n"
                      "#~ msgid \"both\"\n"
                      "#~ msgid_plural \"pl\"\n"
                      "#~ msgstr[0] \"B0\"\n"
                      "#~ msgstr[1] \"B1\"\n"
                      "\n"
                      "#~ msgid \"single\"\n"
                      "#~ msgstr \"T\"\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

/* テスト用: catalog を書き出した結果が expected であることを確かめる */
static void check_written(const mnd_po_catalog_t *catalog, const char *expected)
{
    mnd_buf_t out = MND_BUF_INIT;

    write_ok(catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), expected);
    mnd_buf_free(&out);
}

NAOS_TEST(write_skipped_obsolete_separators)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_po_entry_t *entry;

    /* 5.1・5.8: 空行は実際に書いたエントリの間にだけ置く。先頭・途中で連続・末尾で連続に飛ばす */
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s1", ""));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "a", "A"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s2", NULL));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s3", ""));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "b", "B"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s4", ""));
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "c", "C"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s5", ""));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s6", NULL));
    check_written(&catalog,
                  "msgid \"a\"\n"
                  "msgstr \"A\"\n"
                  "\n"
                  "msgid \"b\"\n"
                  "msgstr \"B\"\n"
                  "\n"
                  "msgid \"c\"\n"
                  "msgstr \"C\"\n");
    mnd_po_catalog_free(&catalog);

    /* 書くエントリが 1 つだけなら空行を書かない */
    mnd_po_catalog_init(&catalog);
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s1", ""));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "w", "W"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s2", ""));
    check_written(&catalog,
                  "#~ msgid \"w\"\n"
                  "#~ msgstr \"W\"\n");
    mnd_po_catalog_free(&catalog);

    /* すべて飛ばすなら空のカタログと同じく何も書かずに成功する（既存の出力はそのまま） */
    mnd_po_catalog_init(&catalog);
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "s1", ""));
    entry = add_obsolete(&catalog, "s2", NULL);
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "tc"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete_plural(&catalog, "s3", "", "X"));
    check_written(&catalog, "");
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep\n"));
    write_ok(&catalog, NULL, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep\n");
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(write_skipped_obsolete_leaves_catalog_unchanged)
{
    static const char *const flags[] = {"fuzzy", "c-format"};
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;

    /* 5.8: 書かなかった廃止エントリもカタログから消さず、内容を変えない */
    NAOS_REQUIRE_NOT_NULL(add_msg(&catalog, NULL, "a", "A"));
    entry = add_obsolete(&catalog, "s", "");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(push_str(&entry->translator_comments, "tc"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry->flags, "c-format"));
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, "pm"));
    NAOS_REQUIRE_NOT_NULL(add_obsolete(&catalog, "n", NULL));

    check_written(&catalog, "msgid \"a\"\nmsgstr \"A\"\n");
    NAOS_REQUIRE_UINT_EQ(catalog.count, (size_t)3);
    entry = &catalog.entries[1];
    NAOS_CHECK_TRUE(entry->obsolete);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->msgid), "s");
    NAOS_REQUIRE_UINT_EQ(entry->msgstr.count, (size_t)1);
    NAOS_CHECK_UINT_EQ(entry->msgstr.items[0].len, (size_t)0);
    NAOS_REQUIRE_UINT_EQ(entry->translator_comments.count, (size_t)1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[0]), "tc");
    check_flags(entry, flags, 2);
    NAOS_CHECK_TRUE(entry->has_prev_msgid);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->prev_msgid), "pm");
    NAOS_CHECK_TRUE(catalog.entries[2].obsolete);
    NAOS_CHECK_UINT_EQ(catalog.entries[2].msgstr.count, (size_t)0);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[2].msgid), "n");

    /* 2 回目の書き出しも同じ結果になる */
    check_written(&catalog, "msgid \"a\"\nmsgstr \"A\"\n");
    mnd_po_catalog_free(&catalog);
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
    NAOS_ADD_TEST(write_wrap_single_segment_without_break_opportunity_stays_on_keyword_line),
    NAOS_ADD_TEST(write_wrap_break_opportunities_come_from_escaped_text),
    NAOS_ADD_TEST(write_wrap_fullwidth_counts_two_columns),
    NAOS_ADD_TEST(write_wrap_counts_prefix_columns),
    NAOS_ADD_TEST(write_wrap_prefix_on_continuation_lines),
    NAOS_ADD_TEST(write_wrap_escapes_count_and_are_not_split),
    NAOS_ADD_TEST(write_wrap_disabled_splits_only_after_newline),
    NAOS_ADD_TEST(write_wrap_appends_with_given_eol),
    NAOS_ADD_TEST(write_entry_line_order_and_comments),
    NAOS_ADD_TEST(write_escapes_strings),
    NAOS_ADD_TEST(write_appends_to_existing_output),
    NAOS_ADD_TEST(write_fuzzy_flag_comes_first),
    NAOS_ADD_TEST(write_obsolete_entry_prefixes),
    NAOS_ADD_TEST(write_crlf_and_default_eol),
    NAOS_ADD_TEST(write_duplicate_leaves_output_unchanged),
    NAOS_ADD_TEST(write_duplicate_reports_each_pair_with_shortened_values),
    NAOS_ADD_TEST(write_refs_79_byte_boundary),
    NAOS_ADD_TEST(write_refs_count_bytes_and_isolate_names_with_spaces),
    NAOS_ADD_TEST(write_refs_with_empty_file_name),
    NAOS_ADD_TEST(write_no_wrap_option_still_wraps_refs),
    NAOS_ADD_TEST(write_no_wrap_flag_and_option),
    NAOS_ADD_TEST(write_zero_msgstr_entries),
    NAOS_ADD_TEST(write_fuzzy_skipped_when_singular_msgstr_empty),
    NAOS_ADD_TEST(write_fuzzy_skipped_keeps_other_flags),
    NAOS_ADD_TEST(write_fuzzy_skipped_when_no_msgstr),
    NAOS_ADD_TEST(write_fuzzy_plural_depends_on_first_msgstr),
    NAOS_ADD_TEST(write_fuzzy_skipped_for_obsolete_and_header),
    NAOS_ADD_TEST(write_fuzzy_skip_leaves_entry_flags_unchanged),
    NAOS_ADD_TEST(write_obsolete_without_first_msgstr_is_skipped),
    NAOS_ADD_TEST(write_skipped_obsolete_separators),
    NAOS_ADD_TEST(write_skipped_obsolete_leaves_catalog_unchanged)
)
