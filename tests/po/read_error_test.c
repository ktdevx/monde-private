#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_read.h"

#include <string.h>

#define READ_ERROR_TEST_PATH "dir/err.po"

static bool read_str(const char *content, mnd_po_catalog_t *catalog, mnd_diag_list_t *diags)
{
    return mnd_po_read(content, strlen(content), READ_ERROR_TEST_PATH, catalog, diags);
}

/* テスト用: index 番目の診断の重大度、メッセージ、ファイル名、行番号を確かめる */
static void check_diag(const mnd_diag_list_t *diags, size_t index, mnd_severity_t severity,
                       const char *message, size_t line)
{
    const mnd_diag_t *diag;

    NAOS_REQUIRE_TRUE(index < mnd_diag_list_count(diags));
    diag = mnd_diag_list_at(diags, index);
    NAOS_CHECK_INT_EQ(diag->severity, severity);
    NAOS_CHECK_STR_EQ(diag->message, message);
    NAOS_REQUIRE_NOT_NULL(diag->path);
    NAOS_CHECK_STR_EQ(diag->path, READ_ERROR_TEST_PATH);
    NAOS_CHECK_UINT_EQ(diag->line, line);
}

/* テスト用: 読み込みが失敗し、カタログが空で、エラーが 1 件だけある */
static void check_one_error(const char *content, const char *message, size_t line)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 1);
    check_diag(&diags, 0, MND_SEVERITY_ERROR, message, line);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_syntax_messages_and_lines)
{
    /* 4.4: 文字列リテラル、キーワード、欠落 */
    check_one_error("msgid \"a\"\nmsgstr \"abc\n", "unterminated string", 2);
    check_one_error("msgid \"a\"\nmsgstr \"\" \"b\"\n", "unexpected text after string", 2);
    check_one_error("msgid \"a\"\nmsgstr\n", "missing string after 'msgstr'", 2);
    check_one_error("# c\nmsgid \"a\"\nmsgstr \"\"\nmsgtxt \"x\"\n", "unknown keyword 'msgtxt'", 4);
    check_one_error("msgctxt \"c\"\nmsgstr \"a\"\n", "missing msgid", 2);
    check_one_error("msgstr \"a\"\n", "missing msgid", 1);
    check_one_error("msgid \"a\"\n\nmsgid \"b\"\nmsgstr \"\"\n", "missing msgstr", 2);
    check_one_error("msgid \"a\"\nmsgid \"b\"\nmsgstr \"\"\n", "missing msgstr", 2);
    check_one_error("msgid \"a\"\nmsgstr \"\"\n\nmsgid \"b\"\n", "missing msgstr", 4);
    /* 4.5: 単数形と複数形の不一致、添字の順序 */
    check_one_error("msgid \"a\"\nmsgid_plural \"b\"\nmsgstr \"\"\n",
                    "msgstr form does not match msgid_plural", 3);
    check_one_error("msgid \"a\"\nmsgstr[0] \"\"\n", "msgstr form does not match msgid_plural", 2);
    check_one_error("msgid \"a\"\nmsgid_plural \"b\"\nmsgstr[1] \"\"\n",
                    "msgstr index out of order: expected [0]", 3);
    check_one_error("msgid \"a\"\nmsgid_plural \"b\"\nmsgstr[0] \"\"\nmsgstr[2] \"\"\n",
                    "msgstr index out of order: expected [1]", 4);
    /* 4.6: 廃止の行と通常の行の混在 */
    check_one_error("msgid \"a\"\n#~ msgstr \"\"\n", "obsolete and active lines mixed in one entry",
                    2);
    check_one_error("#~ msgid \"a\"\n#~ msgstr \"\"\n\"b\"\n",
                    "obsolete and active lines mixed in one entry", 3);
    /* NUL と不正な UTF-8（4.8） */
    check_one_error("msgid \"a\"\nmsgstr \"\\0\"\n", "string contains a NUL byte", 2);
    check_one_error("msgid \"a\"\nmsgstr \"\"\n\n# \xFF\n", "invalid UTF-8 sequence", 4);
}

NAOS_TEST(read_error_one_error_per_entry_in_three_entries)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content =
        "msgid \"one\"\n"           /* 1 */
        "msgstr \"1\" x\n"          /* 2: 余分な文字 */
        "\n"                        /* 3 */
        "msgid \"two\"\n"           /* 4 */
        "msgfoo \"2\"\n"            /* 5: 未知のキーワード */
        "msgstr \"2\"\n"            /* 6 */
        "\n"                        /* 7 */
        "msgid \"three\"\n"         /* 8 */
        "msgstr[0] \"3\"\n"         /* 9: 単数形と複数形の不一致 */
        "\n"                        /* 10 */
        "msgid \"four\"\n"          /* 11: 誤りのないエントリ */
        "msgstr \"4\"\n";           /* 12 */

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 3);
    check_diag(&diags, 0, MND_SEVERITY_ERROR, "unexpected text after string", 2);
    check_diag(&diags, 1, MND_SEVERITY_ERROR, "unknown keyword 'msgfoo'", 5);
    check_diag(&diags, 2, MND_SEVERITY_ERROR, "msgstr form does not match msgid_plural", 9);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_later_errors_in_same_entry_are_not_reported)
{
    check_one_error("msgid \"a\" x\n"
                    "msgfoo \"b\"\n"
                    "\"unterminated\n"
                    "msgstr[3] \"\"\n"
                    "#~ msgstr \"\"\n"
                    "msgstr \"\" y\n"
                    "\n"
                    "msgid \"ok\"\n"
                    "msgstr \"\"\n",
                    "unexpected text after string", 1);
}

NAOS_TEST(read_error_recovers_at_comment_msgctxt_and_msgid)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content =
        "msgid \"a\"\n"             /* 1 */
        "msgstr \"\" x\n"           /* 2: エラー */
        "\"c\" y\n"                 /* 3: 読み飛ばす */
        "msgstr[1] \"\"\n"          /* 4: 読み飛ばす */
        "# comment\n"               /* 5: 再開 */
        "msgid \"b\" x\n"           /* 6: エラー */
        "msgstr \"\"\n"             /* 7: 読み飛ばす */
        "msgctxt \"k\"\n"           /* 8: 再開 */
        "msgstr \"s\"\n"            /* 9: msgid の欠落 */
        "msgid_plural \"p\"\n"      /* 10: 読み飛ばす */
        "msgid \"d\"\n"             /* 11: 再開 */
        "msgstr[0] \"\"\n"          /* 12: 不一致 */
        "msgstr \"\" z\n"           /* 13: 読み飛ばす */
        "#~ msgid \"e\"\n"          /* 14: 再開（廃止エントリ） */
        "msgstr \"\"\n"             /* 15: 混在 */
        "#|\n"                      /* 16: 残りが空の #| は再開の行ではない */
        "msgstr \"\" w\n"           /* 17: 読み飛ばす */
        "#| msgid \"p\"\n"          /* 18: 再開 */
        "msgid \"f\"\n"             /* 19 */
        "msgstr \"\" v\n";          /* 20: エラー */

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 6);
    check_diag(&diags, 0, MND_SEVERITY_ERROR, "unexpected text after string", 2);
    check_diag(&diags, 1, MND_SEVERITY_ERROR, "unexpected text after string", 6);
    check_diag(&diags, 2, MND_SEVERITY_ERROR, "missing msgid", 9);
    check_diag(&diags, 3, MND_SEVERITY_ERROR, "msgstr form does not match msgid_plural", 12);
    check_diag(&diags, 4, MND_SEVERITY_ERROR, "obsolete and active lines mixed in one entry", 15);
    check_diag(&diags, 5, MND_SEVERITY_ERROR, "unexpected text after string", 20);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_recovery_line_may_itself_be_an_error)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char content[] =
        "msgid \"a\" x\n"           /* 1: エラー */
        "msgid \"b\n"               /* 2: 再開の行で、閉じていない文字列 */
        "msgstr \"\"\n"             /* 3: 読み飛ばす */
        "# c\0d\n"                  /* 4: 再開の行で、NUL */
        "msgid \"e\"\n"             /* 5: 再開 */
        "msgstr \"\"\n";            /* 6 */

    NAOS_CHECK_FALSE(mnd_po_read(content, sizeof(content) - 1, READ_ERROR_TEST_PATH, &catalog,
                                 &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 3);
    check_diag(&diags, 0, MND_SEVERITY_ERROR, "unexpected text after string", 1);
    check_diag(&diags, 1, MND_SEVERITY_ERROR, "unterminated string", 2);
    check_diag(&diags, 2, MND_SEVERITY_ERROR, "string contains a NUL byte", 4);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

/* テスト用: 読み込みが失敗し、カタログが空で、2 件のエラー first と second がそれぞれの行にある */
static void check_two_errors(const char *content, const char *first, size_t first_line,
                             const char *second, size_t second_line)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 2);
    check_diag(&diags, 0, MND_SEVERITY_ERROR, first, first_line);
    check_diag(&diags, 1, MND_SEVERITY_ERROR, second, second_line);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_line_starting_next_entry_is_not_skipped)
{
    /* 誤りを見つけた msgid の行から次のエントリを始め、その誤りも報告する */
    check_two_errors("msgid \"a\"\nmsgid \"b\"\nmsgfoo \"x\"\nmsgstr \"\"\n", "missing msgstr", 2,
                     "unknown keyword 'msgfoo'", 3);
    check_two_errors("msgid \"a\"\nmsgid \"b\"\nmsgstr[0] \"\"\n", "missing msgstr", 2,
                     "msgstr form does not match msgid_plural", 3);
    check_two_errors("msgid \"a\"\nmsgid \"b\"\nmsgstr \"x\" junk\n", "missing msgstr", 2,
                     "unexpected text after string", 3);
    /* msgctxt の行は次のエントリに残る（残らなければ 3 行目は読み飛ばされる） */
    check_two_errors("msgid \"a\"\nmsgctxt \"k\"\nmsgstr \"x\"\n", "missing msgstr", 2,
                     "missing msgid", 3);
    check_two_errors("msgid \"a\"\nmsgctxt \"k\"\nmsgid \"b\"\nmsgstr \"x\" junk\n",
                     "missing msgstr", 2, "unexpected text after string", 4);
    /* コメントの行、前回の文字列の行、廃止エントリの msgid の行から再開する */
    check_two_errors("msgid \"a\"\n# c\nmsgid \"b\"\nmsgstr[0] \"\"\n", "missing msgstr", 2,
                     "msgstr form does not match msgid_plural", 4);
    check_two_errors("msgid \"a\"\n#| msgid \"p\"\nmsgid \"b\"\nmsgstr \"\" x\n", "missing msgstr",
                     2, "unexpected text after string", 4);
    check_two_errors("msgid \"a\"\n#~ msgid \"b\"\nmsgstr \"\"\n",
                     "obsolete and active lines mixed in one entry", 2,
                     "obsolete and active lines mixed in one entry", 3);
    /* 次のエントリが正しければ、エラーは 1 件だけ */
    check_one_error("msgid \"a\"\nmsgid \"b\"\nmsgstr \"\"\n", "missing msgstr", 2);
    check_one_error("#| msgid \"p\"\n#~ msgid \"a\"\n#~ msgstr \"\"\n",
                    "obsolete and active lines mixed in one entry", 2);
}

NAOS_TEST(read_error_duplicates_report_both_lines)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content =
        "# first\n"                 /* 1 */
        "msgid \"a\"\n"             /* 2 */
        "msgstr \"\"\n"             /* 3 */
        "\n"                        /* 4 */
        "msgctxt \"k\"\n"           /* 5: msgctxt が違うので重複ではない */
        "msgid \"a\"\n"             /* 6 */
        "msgstr \"\"\n"             /* 7 */
        "\n"                        /* 8 */
        "#~ msgid \"a\"\n"          /* 9: 廃止エントリは対象外 */
        "#~ msgstr \"\"\n"          /* 10 */
        "\n"                        /* 11 */
        "msgid \"a\"\n"             /* 12: 1 行目のエントリと重複 */
        "msgstr \"x\"\n"            /* 13 */
        "\n"                        /* 14 */
        "msgctxt \"k\"\n"           /* 15: 5 行目のエントリと重複 */
        "msgid \"a\"\n"             /* 16 */
        "msgstr \"\"\n"             /* 17 */
        "\n"                        /* 18 */
        "msgid \"a\"\n"             /* 19: 1 行目のエントリと重複 */
        "msgstr \"\"\n";            /* 20 */

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 3);
    check_diag(&diags, 0, MND_SEVERITY_ERROR,
               "duplicate message definition (first defined at line 1)", 12);
    check_diag(&diags, 1, MND_SEVERITY_ERROR,
               "duplicate message definition (first defined at line 5)", 15);
    check_diag(&diags, 2, MND_SEVERITY_ERROR,
               "duplicate message definition (first defined at line 1)", 19);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_post_checks_skipped_after_syntax_error)
{
    check_one_error("msgid \"a\"\nmsgstr \"\"\n\nmsgid \"a\"\nmsgstr \"\"\n\nmsgfoo \"x\"\n",
                    "unknown keyword 'msgfoo'", 7);
}

/* テスト用: Content-Type のヘッダの後ろに通常のエントリを 1 つ置いた内容を読む */
static bool read_with_content_type(const char *content_type, mnd_diag_list_t *diags)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t content = MND_BUF_INIT;
    bool ok;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&content, "# header\n"
                                                   "msgid \"\"\n"
                                                   "msgstr \"\"\n"
                                                   "\"Project-Id-Version: x\\n\"\n"
                                                   "\"Content-Type: ")
                      && mnd_buf_append_str(&content, content_type)
                      && mnd_buf_append_str(&content, "\\n\"\n"
                                                      "\n"
                                                      "msgid \"a\"\n"
                                                      "msgstr \"b\"\n"));
    ok = mnd_po_read(content.data, content.len, READ_ERROR_TEST_PATH, &catalog, diags);
    if (ok)
    {
        NAOS_CHECK_UINT_EQ(catalog.count, 2);
    }
    else
    {
        NAOS_CHECK_UINT_EQ(catalog.count, 0);
    }

    mnd_po_catalog_free(&catalog);
    mnd_buf_free(&content);

    return ok;
}

NAOS_TEST(read_error_charset_other_than_utf8)
{
    static const char *const accepted[] = {
        "text/plain; charset=UTF-8",
        "text/plain; charset=utf-8",
        "text/plain; charset=CHARSET",
        "text/plain; charset=charset",
        "text/plain; charset= UTF-8 ; format=flowed",
        "text/plain"
    };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    size_t i;

    for (i = 0; i < (sizeof(accepted) / sizeof(accepted[0])); i++)
    {
        NAOS_CHECK_TRUE(read_with_content_type(accepted[i], &diags));
        NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
        mnd_diag_list_free(&diags);
    }

    NAOS_CHECK_FALSE(read_with_content_type("text/plain; charset=ISO-8859-1", &diags));
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 1);
    check_diag(&diags, 0, MND_SEVERITY_ERROR,
               "unsupported charset 'ISO-8859-1'; only UTF-8 is supported", 1);
    mnd_diag_list_free(&diags);

    /* 値は ';' までとし、40 バイトまでに切り詰め、制御文字を '?' に置き換える */
    NAOS_CHECK_FALSE(read_with_content_type(
        "text/plain; charset=a\\tbcdefghijklmnopqrstuvwxyz0123456789ABCDEFG; x=y", &diags));
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 1);
    check_diag(&diags, 0, MND_SEVERITY_ERROR,
               "unsupported charset 'a?bcdefghijklmnopqrstuvwxyz0123456789ABC'; "
               "only UTF-8 is supported",
               1);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_newline_mismatch_is_warning)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content =
        "msgid \"\"\n"                       /* 1: ヘッダは対象外 */
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"\\nbegin\"\n"               /* 4: 先頭の不一致 */
        "msgstr \"begin\"\n"
        "\n"
        "msgid \"end\"\n"                    /* 7: 末尾の不一致 */
        "msgstr \"end\\n\"\n"
        "\n"
        "msgid \"\\nboth\\n\"\n"             /* 10: 一致 */
        "msgstr \"\\nboth\\n\"\n"
        "\n"
        "msgid \"one\\n\"\n"                 /* 13: 複数形の msgstr[1] だけ不一致 */
        "msgid_plural \"many\\n\"\n"
        "msgstr[0] \"o\\n\"\n"
        "msgstr[1] \"\\nm\"\n"
        "\n"
        "#, fuzzy\n"                         /* 18: fuzzy は対象外 */
        "msgid \"fuzzy\"\n"
        "msgstr \"\\nfuzzy\\n\"\n"
        "\n"
        "msgid \"untranslated\\n\"\n"        /* 22: 未翻訳は対象外 */
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"obsolete\"\n"            /* 25: 廃止エントリは対象外 */
        "#~ msgstr \"obsolete\\n\"\n";

    NAOS_CHECK_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 8);
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&diags));
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 4);
    check_diag(&diags, 0, MND_SEVERITY_WARNING, "msgid and msgstr do not both begin with '\\n'",
               4);
    check_diag(&diags, 1, MND_SEVERITY_WARNING, "msgid and msgstr do not both end with '\\n'", 7);
    check_diag(&diags, 2, MND_SEVERITY_WARNING,
               "msgid and msgstr[1] do not both begin with '\\n'", 13);
    check_diag(&diags, 3, MND_SEVERITY_WARNING, "msgid and msgstr[1] do not both end with '\\n'",
               13);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_error_warning_kept_when_other_check_fails)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content =
        "msgid \"a\"\n"
        "msgstr \"a\\n\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"\"\n";

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 2);
    check_diag(&diags, 0, MND_SEVERITY_ERROR,
               "duplicate message definition (first defined at line 1)", 4);
    check_diag(&diags, 1, MND_SEVERITY_WARNING, "msgid and msgstr do not both end with '\\n'", 1);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(read_error,
    NAOS_ADD_TEST(read_error_syntax_messages_and_lines),
    NAOS_ADD_TEST(read_error_one_error_per_entry_in_three_entries),
    NAOS_ADD_TEST(read_error_later_errors_in_same_entry_are_not_reported),
    NAOS_ADD_TEST(read_error_recovers_at_comment_msgctxt_and_msgid),
    NAOS_ADD_TEST(read_error_recovery_line_may_itself_be_an_error),
    NAOS_ADD_TEST(read_error_line_starting_next_entry_is_not_skipped),
    NAOS_ADD_TEST(read_error_duplicates_report_both_lines),
    NAOS_ADD_TEST(read_error_post_checks_skipped_after_syntax_error),
    NAOS_ADD_TEST(read_error_charset_other_than_utf8),
    NAOS_ADD_TEST(read_error_newline_mismatch_is_warning),
    NAOS_ADD_TEST(read_error_warning_kept_when_other_check_fails)
)
