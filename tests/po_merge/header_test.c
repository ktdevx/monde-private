/* Header: ヘッダのマージと nplurals の読み取り */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>

/* catalog のヘッダを返す。なければテストを止める */
static const mnd_po_entry_t *header_of(const mnd_po_catalog_t *catalog)
{
    size_t index = 0;

    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(catalog, &index));

    return &catalog->entries[index];
}

/*
 * old_text と template_text（NULL ならテンプレートのヘッダなし）のヘッダから結果のヘッダを作り、
 * msgstr が 1 個で expected と等しいことを調べる
 */
static void check_merged_msgstr(const char *old_text, const char *template_text,
                                const char *expected)
{
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    const mnd_po_entry_t *template_header = NULL;
    mnd_po_entry_t result;

    TEST_READ_PO(old_text, &old_catalog);
    mnd_po_catalog_init(&template_catalog);
    if (template_text != NULL)
    {
        TEST_READ_PO(template_text, &template_catalog);
        template_header = header_of(&template_catalog);
    }

    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_header(&result, header_of(&old_catalog), template_header));
    NAOS_REQUIRE_UINT_EQ(1, result.msgstr.count);
    NAOS_CHECK_STR_EQ(expected, mnd_buf_cstr(&result.msgstr.items[0]));

    mnd_po_entry_free(&result);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

#define CHECK_MERGED_MSGSTR(old_text, template_text, expected) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_MERGED_MSGSTR"); \
        check_merged_msgstr((old_text), (template_text), (expected)); \
        NAOS_TRACE_POP(); \
    } while (0)

/* header の Plural-Forms から nplurals を読む（plural_forms が NULL なら Plural-Forms なし） */
static size_t nplurals_of(const char *plural_forms)
{
    mnd_po_entry_t header;
    size_t result;

    mnd_po_entry_init(&header);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&header.msgstr, "", 0));
    NAOS_REQUIRE_TRUE(mnd_po_header_set(&header, "Content-Type", "text/plain; charset=UTF-8"));
    if (plural_forms != NULL)
    {
        NAOS_REQUIRE_TRUE(mnd_po_header_set(&header, "Plural-Forms", plural_forms));
    }

    result = mnd_merge_nplurals(&header);
    mnd_po_entry_free(&header);

    return result;
}

NAOS_TEST(header_reorders_known_fields_and_unifies_spelling)
{
    /* 既知の項目は表の順と綴りに、ほかの行はその後に元の順で並ぶ */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"content-type: text/plain; charset=UTF-8\\n\"\n"
        "\"X-Generator: tool\\n\"\n"
        "\"language: ja\\n\"\n"
        "\"Plural-Forms: nplurals=1; plural=0;\\n\"\n"
        "\"LAST-TRANSLATOR: Taro <taro@example.com>\\n\"\n"
        "\"Project-Id-Version: demo 1.0\\n\"\n"
        "\"mime-version: 1.0\\n\"\n"
        "\"Content-Transfer-Encoding: 8bit\\n\"\n"
        "\"Po-Revision-Date: 2024-01-02 03:04+0900\\n\"\n",
        NULL,
        "Project-Id-Version: demo 1.0\n"
        "PO-Revision-Date: 2024-01-02 03:04+0900\n"
        "Last-Translator: Taro <taro@example.com>\n"
        "Language: ja\n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "X-Generator: tool\n"
        "Plural-Forms: nplurals=1; plural=0;\n");
}

NAOS_TEST(header_keeps_order_of_unknown_lines)
{
    /* ほかの行は、既知の項目の間にあっても元の順を保つ */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"X-B: 2\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"X-A: 1\\n\"\n"
        "\"Language-Team: Japanese\\n\"\n"
        "\"Language: ja\\n\"\n"
        "\"X-C: 3\\n\"\n",
        NULL,
        "Language-Team: Japanese\n"
        "Language: ja\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "X-B: 2\n"
        "X-A: 1\n"
        "X-C: 3\n");
}

NAOS_TEST(header_later_duplicate_field_wins)
{
    /* 同じ項目が複数の行にあれば後の行の値を 1 つだけ出す。ほかの行の重複はそのまま */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Language: ja\\n\"\n"
        "\"X-A: 1\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"LANGUAGE: fr\\n\"\n"
        "\"X-A: 2\\n\"\n",
        NULL,
        "Language: fr\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "X-A: 1\n"
        "X-A: 2\n");
}

NAOS_TEST(header_adds_newline_to_last_line)
{
    /* 最後の行が \n で終わらなければ \n を補う（既知の項目、ほかの行のどちらでも） */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Language: ja\"\n",
        NULL,
        "Language: ja\n"
        "Content-Type: text/plain; charset=UTF-8\n");
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"X-Last: value\"\n",
        NULL,
        "Content-Type: text/plain; charset=UTF-8\n"
        "X-Last: value\n");
}

NAOS_TEST(header_empty_msgstr)
{
    /* 空の msgstr は 0 行。テンプレートがなければ空のまま */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n",
        NULL,
        "");
    /* テンプレートの項目だけが加わる */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n",
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: PACKAGE VERSION\\n\"\n"
        "\"POT-Creation-Date: 2024-05-06 07:08+0000\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        "POT-Creation-Date: 2024-05-06 07:08+0000\n");
}

NAOS_TEST(header_replaces_fields_from_template)
{
    /* Report-Msgid-Bugs-To と POT-Creation-Date はテンプレートの値に置き換え、ほかはそのまま */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: demo 1.0\\n\"\n"
        "\"Report-Msgid-Bugs-To: old@example.com\\n\"\n"
        "\"POT-Creation-Date: 2020-01-01 00:00+0000\\n\"\n"
        "\"PO-Revision-Date: 2021-02-03 04:05+0900\\n\"\n"
        "\"Language: ja\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: demo 2.0\\n\"\n"
        "\"Report-Msgid-Bugs-To: new@example.com\\n\"\n"
        "\"POT-Creation-Date: 2024-05-06 07:08+0000\\n\"\n"
        "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n"
        "\"Language: \\n\"\n"
        "\"Content-Type: text/plain; charset=CHARSET\\n\"\n",
        "Project-Id-Version: demo 1.0\n"
        "Report-Msgid-Bugs-To: new@example.com\n"
        "POT-Creation-Date: 2024-05-06 07:08+0000\n"
        "PO-Revision-Date: 2021-02-03 04:05+0900\n"
        "Language: ja\n"
        "Content-Type: text/plain; charset=UTF-8\n");
}

NAOS_TEST(header_keeps_fields_missing_from_template)
{
    /* テンプレートにない項目は既存の値を保つ */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Report-Msgid-Bugs-To: old@example.com\\n\"\n"
        "\"POT-Creation-Date: 2020-01-01 00:00+0000\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"POT-Creation-Date: 2024-05-06 07:08+0000\\n\"\n"
        "\"Content-Type: text/plain; charset=CHARSET\\n\"\n",
        "Report-Msgid-Bugs-To: old@example.com\n"
        "POT-Creation-Date: 2024-05-06 07:08+0000\n"
        "Content-Type: text/plain; charset=UTF-8\n");
    /* テンプレートの名前は大文字・小文字を区別して探す */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"POT-Creation-Date: 2020-01-01 00:00+0000\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"pot-creation-date: 2024-05-06 07:08+0000\\n\"\n"
        "\"Content-Type: text/plain; charset=CHARSET\\n\"\n",
        "POT-Creation-Date: 2020-01-01 00:00+0000\n"
        "Content-Type: text/plain; charset=UTF-8\n");
}

NAOS_TEST(header_adds_fields_only_in_template)
{
    /* テンプレートにだけある項目は表の位置に加わる。末尾に \n がなければ補う */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: demo 1.0\\n\"\n"
        "\"Language: ja\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=CHARSET\\n\"\n"
        "\"Report-Msgid-Bugs-To: bugs@example.com\\n\"\n"
        "\"POT-Creation-Date: 2024-05-06 07:08+0000\"\n",
        "Project-Id-Version: demo 1.0\n"
        "Report-Msgid-Bugs-To: bugs@example.com\n"
        "POT-Creation-Date: 2024-05-06 07:08+0000\n"
        "Language: ja\n"
        "Content-Type: text/plain; charset=UTF-8\n");
}

NAOS_TEST(header_adds_empty_language_after_language_team)
{
    /* Language がなく Language-Team があれば、値が空の Language を加える */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Language-Team: Japanese <ja@example.com>\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        NULL,
        "Language-Team: Japanese <ja@example.com>\n"
        "Language: \n"
        "Content-Type: text/plain; charset=UTF-8\n");
    /* どちらもなければ加えない */
    CHECK_MERGED_MSGSTR(
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        NULL,
        "Content-Type: text/plain; charset=UTF-8\n");
}

NAOS_TEST(header_entry_elements_from_old_and_template)
{
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    const mnd_po_entry_t *old_header;
    mnd_po_entry_t result;

    TEST_READ_PO(
        "# Japanese translation.\n"
        "# Taro <taro@example.com>, 2024.\n"
        "#. old extracted\n"
        "#: old.c:1\n"
        "#, fuzzy, no-wrap\n"
        "#| msgid \"previous\"\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        &old_catalog);
    TEST_READ_PO(
        "# SOME DESCRIPTIVE TITLE.\n"
        "#. template extracted\n"
        "#: template.c:3 other.c\n"
        "#, c-format, fuzzy, x-unknown\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=CHARSET\\n\"\n",
        &template_catalog);
    old_header = header_of(&old_catalog);

    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_header(&result, old_header, header_of(&template_catalog)));

    /* msgctxt なし、空の msgid、単数形、廃止エントリでない、行番号 0 */
    NAOS_CHECK_FALSE(result.has_msgctxt);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&result.msgid));
    NAOS_CHECK_FALSE(result.has_msgid_plural);
    NAOS_CHECK_FALSE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);
    /* 翻訳者コメントは既存から */
    NAOS_REQUIRE_UINT_EQ(2, result.translator_comments.count);
    NAOS_CHECK_STR_EQ("Japanese translation.", mnd_buf_cstr(&result.translator_comments.items[0]));
    NAOS_CHECK_STR_EQ("Taro <taro@example.com>, 2024.",
                      mnd_buf_cstr(&result.translator_comments.items[1]));
    NAOS_CHECK_TRUE(result.translator_comments.items[0].data
                    != old_header->translator_comments.items[0].data);
    /* 抽出コメントと参照はテンプレートから */
    NAOS_REQUIRE_UINT_EQ(1, result.extracted_comments.count);
    NAOS_CHECK_STR_EQ("template extracted", mnd_buf_cstr(&result.extracted_comments.items[0]));
    NAOS_REQUIRE_UINT_EQ(2, result.refs.count);
    NAOS_CHECK_STR_EQ("template.c", mnd_buf_cstr(&result.refs.items[0].file));
    NAOS_CHECK_UINT_EQ(3, result.refs.items[0].line);
    NAOS_CHECK_STR_EQ("other.c", mnd_buf_cstr(&result.refs.items[1].file));
    NAOS_CHECK_UINT_EQ(0, result.refs.items[1].line);
    /* fuzzy は既存から先頭に、fuzzy 以外のフラグはテンプレートから順に。既存の no-wrap は捨てる */
    NAOS_REQUIRE_UINT_EQ(3, result.flags.count);
    NAOS_CHECK_STR_EQ("fuzzy", mnd_buf_cstr(&result.flags.items[0]));
    NAOS_CHECK_STR_EQ("c-format", mnd_buf_cstr(&result.flags.items[1]));
    NAOS_CHECK_STR_EQ("x-unknown", mnd_buf_cstr(&result.flags.items[2]));
    /* 前回の msgid は付けない */
    NAOS_CHECK_FALSE(result.has_prev_msgctxt);
    NAOS_CHECK_FALSE(result.has_prev_msgid);
    NAOS_CHECK_FALSE(result.has_prev_msgid_plural);
    /* msgstr は既存のヘッダの値 */
    NAOS_REQUIRE_UINT_EQ(1, result.msgstr.count);
    NAOS_CHECK_STR_EQ("Content-Type: text/plain; charset=UTF-8\n",
                      mnd_buf_cstr(&result.msgstr.items[0]));

    mnd_po_entry_free(&result);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(header_ignores_template_fuzzy)
{
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_po_entry_t result;

    TEST_READ_PO(
        "# Translator.\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n",
        &old_catalog);
    TEST_READ_PO(
        "#, fuzzy\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=CHARSET\\n\"\n",
        &template_catalog);

    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_header(&result, header_of(&old_catalog),
                                       header_of(&template_catalog)));
    /* 既存が fuzzy でなければ、テンプレートが fuzzy でも fuzzy にしない */
    NAOS_CHECK_UINT_EQ(0, result.flags.count);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_REQUIRE_UINT_EQ(1, result.translator_comments.count);
    NAOS_CHECK_STR_EQ("Translator.", mnd_buf_cstr(&result.translator_comments.items[0]));

    mnd_po_entry_free(&result);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(header_without_template_header)
{
    mnd_po_catalog_t old_catalog;
    mnd_po_entry_t result;

    TEST_READ_PO(
        "# Translator.\n"
        "#. extracted\n"
        "#: file.c:9\n"
        "#, fuzzy, c-format\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"POT-Creation-Date: 2020-01-01 00:00+0000\\n\"\n",
        &old_catalog);

    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_header(&result, header_of(&old_catalog), NULL));
    /* 翻訳者コメントと fuzzy は保ち、抽出コメント・参照・fuzzy 以外のフラグはなし */
    NAOS_REQUIRE_UINT_EQ(1, result.translator_comments.count);
    NAOS_CHECK_UINT_EQ(0, result.extracted_comments.count);
    NAOS_CHECK_UINT_EQ(0, result.refs.count);
    NAOS_REQUIRE_UINT_EQ(1, result.flags.count);
    NAOS_CHECK_STR_EQ("fuzzy", mnd_buf_cstr(&result.flags.items[0]));
    /* 項目は並べ替えるだけで値は保つ */
    NAOS_REQUIRE_UINT_EQ(1, result.msgstr.count);
    NAOS_CHECK_STR_EQ("POT-Creation-Date: 2020-01-01 00:00+0000\n"
                      "Content-Type: text/plain; charset=UTF-8\n",
                      mnd_buf_cstr(&result.msgstr.items[0]));

    mnd_po_entry_free(&result);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(header_result_can_be_written)
{
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t output;
    mnd_po_entry_t *result;

    TEST_READ_PO(
        "# Translator.\n"
        "#, fuzzy\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"language: ja\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Project-Id-Version: demo\\n\"\n",
        &old_catalog);
    TEST_READ_PO(
        "#. note\n"
        "#, c-format\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"POT-Creation-Date: 2024-05-06 07:08+0000\\n\"\n",
        &template_catalog);

    mnd_po_catalog_init(&output);
    result = mnd_po_catalog_append(&output);
    NAOS_REQUIRE_NOT_NULL(result);
    NAOS_REQUIRE_TRUE(mnd_merge_header(result, header_of(&old_catalog),
                                       header_of(&template_catalog)));
    TEST_CHECK_WRITTEN(&output,
                       "# Translator.\n"
                       "#. note\n"
                       "#, fuzzy, c-format\n"
                       "msgid \"\"\n"
                       "msgstr \"\"\n"
                       "\"Project-Id-Version: demo\\n\"\n"
                       "\"POT-Creation-Date: 2024-05-06 07:08+0000\\n\"\n"
                       "\"Language: ja\\n\"\n"
                       "\"Content-Type: text/plain; charset=UTF-8\\n\"\n");

    mnd_po_catalog_free(&output);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(header_nplurals)
{
    /* ヘッダがない、Plural-Forms がない */
    NAOS_CHECK_UINT_EQ(2, mnd_merge_nplurals(NULL));
    NAOS_CHECK_UINT_EQ(2, nplurals_of(NULL));
    /* 読める値 */
    NAOS_CHECK_UINT_EQ(3, nplurals_of("nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : 1);"));
    NAOS_CHECK_UINT_EQ(1, nplurals_of("nplurals = 1"));
    NAOS_CHECK_UINT_EQ(1, nplurals_of("nplurals=\t1; plural=0;"));
    NAOS_CHECK_UINT_EQ(100, nplurals_of("nplurals=100; plural=n;"));
    /* 範囲外、数字がない */
    NAOS_CHECK_UINT_EQ(2, nplurals_of("nplurals=0; plural=0;"));
    NAOS_CHECK_UINT_EQ(2, nplurals_of("nplurals=x; plural=0;"));
    NAOS_CHECK_UINT_EQ(2, nplurals_of("nplurals=101; plural=n;"));
    NAOS_CHECK_UINT_EQ(2, nplurals_of("plural=0;"));
    NAOS_CHECK_UINT_EQ(2, nplurals_of("nplurals 3"));
    /* size_t を超える桁数でも 2 */
    NAOS_CHECK_UINT_EQ(2, nplurals_of("nplurals=184467440737095516170000000001; plural=n;"));
    NAOS_CHECK_UINT_EQ(2, nplurals_of("nplurals=000000000000000000000000000000000000; plural=n;"));
    /* 先頭の 0 は値を変えない */
    NAOS_CHECK_UINT_EQ(4, nplurals_of("nplurals=00000000000000000000000000000004; plural=n;"));
}

NAOS_SUITE(header,
    NAOS_ADD_TEST(header_reorders_known_fields_and_unifies_spelling),
    NAOS_ADD_TEST(header_keeps_order_of_unknown_lines),
    NAOS_ADD_TEST(header_later_duplicate_field_wins),
    NAOS_ADD_TEST(header_adds_newline_to_last_line),
    NAOS_ADD_TEST(header_empty_msgstr),
    NAOS_ADD_TEST(header_replaces_fields_from_template),
    NAOS_ADD_TEST(header_keeps_fields_missing_from_template),
    NAOS_ADD_TEST(header_adds_fields_only_in_template),
    NAOS_ADD_TEST(header_adds_empty_language_after_language_team),
    NAOS_ADD_TEST(header_entry_elements_from_old_and_template),
    NAOS_ADD_TEST(header_ignores_template_fuzzy),
    NAOS_ADD_TEST(header_without_template_header),
    NAOS_ADD_TEST(header_result_can_be_written),
    NAOS_ADD_TEST(header_nplurals)
)
