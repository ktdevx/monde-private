/* スイート translate: 置き換え、--use-fuzzy、--front-matter-keys、警告、エラー、廃止・msgctxt の無視、BOM、件数（7.x、8.4〜8.6） */

#include <naos/naos.h>

#include "cli/cli.h"
#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "foundation/buf.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 翻訳ファイルのヘッダ */
#define TRANSLATE_PO_HEADER \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Language: ja\\n\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"

/* 原文: 見出し 1 つと段落 3 つ */
#define TRANSLATE_SOURCE \
    "# Heading\n" \
    "\n" \
    "First paragraph.\n" \
    "\n" \
    "Second paragraph.\n" \
    "\n" \
    "Third paragraph.\n"

/* 見出しと 1 つ目の段落は翻訳済み、2 つ目は fuzzy、3 つ目は未翻訳 */
#define TRANSLATE_PO \
    TRANSLATE_PO_HEADER \
    "\n" \
    "msgid \"Heading\"\n" \
    "msgstr \"見出し\"\n" \
    "\n" \
    "msgid \"First paragraph.\"\n" \
    "msgstr \"最初の段落。\"\n" \
    "\n" \
    "#, fuzzy\n" \
    "msgid \"Second paragraph.\"\n" \
    "msgstr \"二番目の段落。\"\n" \
    "\n" \
    "msgid \"Third paragraph.\"\n" \
    "msgstr \"\"\n"

/* 既定の出力: fuzzy と未翻訳は原文のまま */
#define TRANSLATE_EXPECTED \
    "# 見出し\n" \
    "\n" \
    "最初の段落。\n" \
    "\n" \
    "Second paragraph.\n" \
    "\n" \
    "Third paragraph.\n"

/* --use-fuzzy の出力: fuzzy の訳も採用する */
#define TRANSLATE_EXPECTED_FUZZY \
    "# 見出し\n" \
    "\n" \
    "最初の段落。\n" \
    "\n" \
    "二番目の段落。\n" \
    "\n" \
    "Third paragraph.\n"

/* ---- 補助 ---- */

#define TRANSLATE_MAX_ARGS 16
#define TRANSLATE_MAX_LEN 1024

typedef struct translate_argv
{
    char storage[TRANSLATE_MAX_ARGS][TRANSLATE_MAX_LEN];
    char *argv[TRANSLATE_MAX_ARGS + 1];
    int argc;
} translate_argv_t;

/* "monde <command>" に NULL で終わる args を続けて test_cli_run で実行する（name はテストごとに一意） */
static void run_command(const char *command, const char *name, const char *stdin_bytes, size_t stdin_len,
                        const char *const *args, test_cli_result_t *result)
{
    static translate_argv_t copy;
    size_t i;

    test_env_set(NULL, 0);

    strcpy(copy.storage[0], "monde");
    strcpy(copy.storage[1], command);
    copy.argc = 2;
    for (i = 0; args[i] != NULL; i++)
    {
        NAOS_REQUIRE_TRUE(copy.argc < TRANSLATE_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(args[i]) < TRANSLATE_MAX_LEN);
        strcpy(copy.storage[copy.argc], args[i]);
        copy.argc++;
    }
    for (i = 0; i < (size_t)copy.argc; i++)
    {
        copy.argv[i] = copy.storage[i];
    }
    copy.argv[copy.argc] = NULL;

    NAOS_REQUIRE_TRUE(test_cli_run(name, stdin_bytes, stdin_len, copy.argc, copy.argv, result));
}

/* 作業ディレクトリに name の内容 bytes（len バイト）を書き、そのパスを path に入れる */
static void write_input_bytes(const char *name, const char *bytes, size_t len, mnd_buf_t *path)
{
    NAOS_REQUIRE_TRUE(test_write_file(name, bytes, len));
    NAOS_REQUIRE_TRUE(test_work_path(name, path));
}

/* 作業ディレクトリに name の内容 text（strlen の長さ）を書き、そのパスを path に入れる */
static void write_input(const char *name, const char *text, mnd_buf_t *path)
{
    write_input_bytes(name, text, strlen(text), path);
}

/* buf の内容が needle を含む */
static bool buf_contains(const mnd_buf_t *buf, const char *needle)
{
    return (strstr(mnd_buf_cstr(buf), needle) != NULL);
}

/*
 * <name>.md に source、<name>.po に po を書き、extra（NULL で終わる。NULL 可）の後に 2 つのパスを続けて
 * translate を実行する。パスは md_path / po_path に返す
 */
static void run_translate(const char *name, const char *source, size_t source_len, const char *po,
                          const char *const *extra, mnd_buf_t *md_path, mnd_buf_t *po_path,
                          test_cli_result_t *result)
{
    char md_name[128];
    char po_name[128];
    const char *args[TRANSLATE_MAX_ARGS];
    size_t n = 0;
    size_t i;

    NAOS_REQUIRE_TRUE(strlen(name) + 4 < sizeof(md_name));
    strcpy(md_name, name);
    strcat(md_name, ".md");
    strcpy(po_name, name);
    strcat(po_name, ".po");
    write_input_bytes(md_name, source, source_len, md_path);
    write_input(po_name, po, po_path);

    if (extra != NULL)
    {
        for (i = 0; extra[i] != NULL; i++)
        {
            NAOS_REQUIRE_TRUE(n + 3 < TRANSLATE_MAX_ARGS);
            args[n] = extra[i];
            n++;
        }
    }
    args[n] = mnd_buf_cstr(md_path);
    args[n + 1] = mnd_buf_cstr(po_path);
    args[n + 2] = NULL;
    run_command("translate", name, NULL, 0, args, result);
}

/* ---- テスト ---- */

/* 雛形: translate のハンドラがハンドラの型で宣言されている */
NAOS_TEST(translate_scaffold_handler_is_declared)
{
    mnd_cli_handler_fn handler = mnd_cli_cmd_translate;

    NAOS_CHECK_TRUE(handler != NULL);
}

/*
 * 翻訳済みのエントリだけを置き換え、未翻訳と fuzzy は原文のまま出力する。
 * 件数と翻訳率（翻訳済み ÷ 合計、fuzzy は数えない）を 1 行で表示する（7.1、7.2、8.4、8.5）
 */
NAOS_TEST(translate_replaces_translated_entries)
{
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_replaces_translated_entries", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE),
                  TRANSLATE_PO, NULL, &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(TRANSLATE_EXPECTED, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("translate: 2 translated, 1 fuzzy, 1 untranslated, 0 missing, 4 total, 50.0% translated\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* --use-fuzzy で fuzzy の訳を採用する。fuzzy は翻訳率に数えない（7.3、8.5） */
NAOS_TEST(translate_use_fuzzy_adopts_fuzzy)
{
    static const char *const extra[] = { "--use-fuzzy", NULL };
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_use_fuzzy_adopts_fuzzy", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE), TRANSLATE_PO,
                  extra, &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(TRANSLATE_EXPECTED_FUZZY, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("translate: 2 translated, 1 fuzzy, 1 untranslated, 0 missing, 4 total, 50.0% translated\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* --front-matter-keys で指定したフロントマターの値も置き換え、指定しなければ置き換えない（7.4） */
NAOS_TEST(translate_front_matter_keys)
{
    static const char source[] =
        "---\n"
        "title: Hello\n"
        "author: Someone\n"
        "---\n"
        "\n"
        "Body.\n";
    static const char po[] =
        TRANSLATE_PO_HEADER
        "\n"
        "msgid \"Hello\"\n"
        "msgstr \"こんにちは\"\n"
        "\n"
        "msgid \"Someone\"\n"
        "msgstr \"誰か\"\n"
        "\n"
        "msgid \"Body.\"\n"
        "msgstr \"本文。\"\n";
    static const char *const extra[] = { "--front-matter-keys", "title", NULL };
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_front_matter_keys", source, strlen(source), po, extra, &md_path, &po_path,
                  &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ("---\ntitle: こんにちは\nauthor: Someone\n---\n\n本文。\n", mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("translate: 2 translated, 0 fuzzy, 0 untranslated, 0 missing, 2 total, 100.0% translated\n",
                      mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);

    /* キーを指定しなければフロントマターは原文のまま */
    run_translate("translate_front_matter_keys_none", source, strlen(source), po, NULL, &md_path, &po_path,
                  &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ("---\ntitle: Hello\nauthor: Someone\n---\n\n本文。\n", mnd_buf_cstr(&result.out));

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/*
 * 同じキーパスで extract した出力をそのまま渡すと、原文とバイト単位で同じ出力になる。
 * extract の msgid はすべて見つかり、カタログにない件数は 0（7.5、7.6、8.4）
 */
NAOS_TEST(translate_extract_output_yields_identical_source)
{
    static const char source[] =
        "---\r\n"
        "title: Hello\r\n"
        "---\r\n"
        "\r\n"
        "# Heading\r\n"
        "\r\n"
        "Some *emphasis* and `code`\r\n"
        "on two lines.\r\n"
        "\r\n"
        "- item one\r\n"
        "- item two\r\n"
        "\r\n"
        "> quoted\r\n"
        "\r\n"
        "Some *emphasis* and `code`\r\n"
        "on two lines.\r\n";
    const char *args[6];
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t pot_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("translate_extract_output_yields_identical_source.md", source, &md_path);
    NAOS_REQUIRE_TRUE(test_work_path("translate_extract_output_yields_identical_source.pot", &pot_path));

    args[0] = "--front-matter-keys";
    args[1] = "title";
    args[2] = mnd_buf_cstr(&md_path);
    args[3] = "-o";
    args[4] = mnd_buf_cstr(&pot_path);
    args[5] = NULL;
    run_command("extract", "translate_extract_output_yields_identical_source_extract", NULL, 0, args, &result);
    NAOS_REQUIRE_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    test_cli_result_free(&result);

    args[0] = "--front-matter-keys";
    args[1] = "title";
    args[2] = mnd_buf_cstr(&md_path);
    args[3] = mnd_buf_cstr(&pot_path);
    args[4] = NULL;
    run_command("translate", "translate_extract_output_yields_identical_source", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_MEM_EQ(source, strlen(source), mnd_buf_cstr(&result.out), result.out.len);
    NAOS_CHECK_STR_EQ("translate: 0 translated, 0 fuzzy, 7 untranslated, 0 missing, 7 total, 0.0% translated\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&pot_path);
    mnd_buf_free(&md_path);
}

/* 空のカタログ（ヘッダのみ）では原文とバイト単位で同じ出力になり、すべてカタログにない（7.6、8.4） */
NAOS_TEST(translate_empty_catalog_yields_identical_source)
{
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_empty_catalog_yields_identical_source", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE),
                  TRANSLATE_PO_HEADER, NULL, &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(TRANSLATE_SOURCE, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("translate: 0 translated, 0 fuzzy, 0 untranslated, 4 missing, 4 total, 0.0% translated\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 構造を変え得る訳文は警告を原文のパスと行で表示し、訳文どおりに出力して 0 で終わる（7.7） */
NAOS_TEST(translate_structure_changing_translation_warns_and_succeeds)
{
    static const char source[] = "Intro\n\nPara\n";
    static const char po[] =
        TRANSLATE_PO_HEADER
        "\n"
        "msgid \"Para\"\n"
        "msgstr \"a\\n\\nb\"\n";
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_structure_changing_translation_warns_and_succeeds", source, strlen(source), po,
                  NULL, &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ("Intro\n\na\n\nb\n", mnd_buf_cstr(&result.out));
    /* 件数はハンドラが、診断はハンドラの後に Run が表示する */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected,
                                         "translate: 1 translated, 0 fuzzy, 0 untranslated, 1 missing, "
                                         "2 total, 50.0% translated\n")
                      && mnd_buf_append_str(&expected, mnd_buf_cstr(&md_path))
                      && mnd_buf_append_str(&expected,
                                            ":3: warning: translation contains a blank line (msgid at line 3)\n"));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&expected), mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&expected);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 翻訳ファイルが PO として読めなければ、エラーを翻訳ファイルのパスと行で示し、出力を作らずに 1 で終わる（7.8） */
NAOS_TEST(translate_invalid_po_is_error)
{
    static const char po[] =
        TRANSLATE_PO_HEADER
        "\n"
        "msgid \"Heading\"\n"
        "bogus line\n";
    const char *extra[3];
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_remove_file("translate_invalid_po_is_error.out.md"));
    NAOS_REQUIRE_TRUE(test_work_path("translate_invalid_po_is_error.out.md", &output_path));
    extra[0] = "-o";
    extra[1] = mnd_buf_cstr(&output_path);
    extra[2] = NULL;
    run_translate("translate_invalid_po_is_error", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE), po, extra,
                  &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("translate_invalid_po_is_error.out.md"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&po_path))
                      && mnd_buf_append_str(&expected, ":7: error: "));
    NAOS_REQUIRE_UINT_GE(result.err.len, expected.len);
    NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&expected), expected.len, mnd_buf_cstr(&result.err), expected.len);
    NAOS_CHECK_FALSE(buf_contains(&result.err, "translate:"));

    test_cli_result_free(&result);
    mnd_buf_free(&expected);
    mnd_buf_free(&output_path);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 訳文が正しい UTF-8 でなければ（エスケープで作ったバイト）、出力を作らずに 1 で終わる（7.8） */
NAOS_TEST(translate_invalid_utf8_translation_is_error)
{
    static const char po[] =
        TRANSLATE_PO_HEADER
        "\n"
        "msgid \"Heading\"\n"
        "msgstr \"Bon\\377jour\"\n";
    const char *extra[3];
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_remove_file("translate_invalid_utf8_translation_is_error.out.md"));
    NAOS_REQUIRE_TRUE(test_work_path("translate_invalid_utf8_translation_is_error.out.md", &output_path));
    extra[0] = "-o";
    extra[1] = mnd_buf_cstr(&output_path);
    extra[2] = NULL;
    run_translate("translate_invalid_utf8_translation_is_error", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE), po,
                  extra, &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("translate_invalid_utf8_translation_is_error.out.md"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&po_path))
                      && mnd_buf_append_str(&expected, ":7: error: "));
    NAOS_CHECK_TRUE(buf_contains(&result.err, mnd_buf_cstr(&expected)));
    NAOS_CHECK_TRUE(buf_contains(&result.err, "UTF-8"));
    NAOS_CHECK_FALSE(buf_contains(&result.err, "translate:"));
    test_cli_result_free(&result);

    /* ファイルのバイトそのものが不正な UTF-8 でも同じ */
    {
        static const char raw_po[] =
            TRANSLATE_PO_HEADER
            "\n"
            "msgid \"Heading\"\n"
            "msgstr \"Bon\xFFjour\"\n";

        NAOS_REQUIRE_TRUE(test_remove_file("translate_invalid_utf8_translation_is_error.out.md"));
        run_translate("translate_invalid_utf8_translation_is_error_raw", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE),
                      raw_po, extra, &md_path, &po_path, &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
        NAOS_CHECK_FALSE(test_file_exists("translate_invalid_utf8_translation_is_error.out.md"));
        NAOS_CHECK_UINT_EQ(0, result.out.len);
        NAOS_CHECK_TRUE(buf_contains(&result.err, mnd_buf_cstr(&po_path)));
        NAOS_CHECK_TRUE(buf_contains(&result.err, "UTF-8"));
    }

    test_cli_result_free(&result);
    mnd_buf_free(&expected);
    mnd_buf_free(&output_path);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 廃止エントリと msgctxt 付きのエントリは置き換えに使わず、カタログにないものとして数える（7.9） */
NAOS_TEST(translate_ignores_obsolete_and_msgctxt_entries)
{
    static const char po[] =
        TRANSLATE_PO_HEADER
        "\n"
        "msgctxt \"menu\"\n"
        "msgid \"Heading\"\n"
        "msgstr \"見出し\"\n"
        "\n"
        "msgid \"First paragraph.\"\n"
        "msgstr \"最初の段落。\"\n"
        "\n"
        "#~ msgid \"Second paragraph.\"\n"
        "#~ msgstr \"二番目の段落。\"\n";
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_ignores_obsolete_and_msgctxt_entries", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE),
                  po, NULL, &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ("# Heading\n\n最初の段落。\n\nSecond paragraph.\n\nThird paragraph.\n",
                      mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("translate: 1 translated, 0 fuzzy, 0 untranslated, 3 missing, 4 total, 25.0% translated\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 2 つの入力に `-` を指定すると、出力を作らずに使い方のエラーで 2 を返す（2.5） */
NAOS_TEST(translate_two_stdin_inputs_is_usage_error)
{
    const char *args[5];
    mnd_buf_t output_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_remove_file("translate_two_stdin_inputs_is_usage_error.out.md"));
    NAOS_REQUIRE_TRUE(test_work_path("translate_two_stdin_inputs_is_usage_error.out.md", &output_path));

    args[0] = "-";
    args[1] = "-";
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&output_path);
    args[4] = NULL;
    run_command("translate", "translate_two_stdin_inputs_is_usage_error", TRANSLATE_SOURCE,
                strlen(TRANSLATE_SOURCE), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("translate_two_stdin_inputs_is_usage_error.out.md"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("monde: error: standard input ('-') can be used only once, but 2 inputs are '-'\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&output_path);
}

/* BOM 付きの CRLF の原文: 出力にも BOM を付け、訳文の行の境界も CRLF にする（3.6、7.1） */
NAOS_TEST(translate_bom_source_yields_bom_output)
{
    static const char source[] = "\xEF\xBB\xBF" "Intro\r\n\r\nPara\r\n";
    static const char po[] =
        TRANSLATE_PO_HEADER
        "\n"
        "msgid \"Para\"\n"
        "msgstr \"first\\nsecond\"\n";
    static const char expected[] = "\xEF\xBB\xBF" "Intro\r\n\r\nfirst\r\nsecond\r\n";
    const char *extra[3];
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_work_path("translate_bom_source_yields_bom_output.out.md", &output_path));
    extra[0] = "-o";
    extra[1] = mnd_buf_cstr(&output_path);
    extra[2] = NULL;
    run_translate("translate_bom_source_yields_bom_output", source, strlen(source), po, extra, &md_path,
                  &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_REQUIRE_TRUE(test_read_file("translate_bom_source_yields_bom_output.out.md", &written));
    NAOS_CHECK_MEM_EQ(expected, strlen(expected), mnd_buf_cstr(&written), written.len);
    test_cli_result_free(&result);

    /* BOM のない原文の出力には BOM を付けない */
    run_translate("translate_bom_source_yields_bom_output_none", source + 3, strlen(source + 3), po, NULL,
                  &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_MEM_EQ(expected + 3, strlen(expected + 3), mnd_buf_cstr(&result.out), result.out.len);

    test_cli_result_free(&result);
    mnd_buf_free(&written);
    mnd_buf_free(&output_path);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 翻訳セグメントがない文書では翻訳率を出さず、対象がないことを示す（8.6） */
NAOS_TEST(translate_no_segments_reports_no_messages)
{
    static const char source[] = "```\ncode only\n```\n";
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_translate("translate_no_segments_reports_no_messages", source, strlen(source), TRANSLATE_PO, NULL,
                  &md_path, &po_path, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(source, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("translate: no translatable messages\n", mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&md_path);
}

/* 原文を標準入力から読める（`-` が 1 つなら使える）。-q で件数を出さない（8.7） */
NAOS_TEST(translate_stdin_source_and_quiet)
{
    const char *args[4];
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("translate_stdin_source_and_quiet.po", TRANSLATE_PO, &po_path);
    args[0] = "-q";
    args[1] = "-";
    args[2] = mnd_buf_cstr(&po_path);
    args[3] = NULL;
    run_command("translate", "translate_stdin_source_and_quiet", TRANSLATE_SOURCE, strlen(TRANSLATE_SOURCE),
                args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(TRANSLATE_EXPECTED, mnd_buf_cstr(&result.out));
    NAOS_CHECK_UINT_EQ(0, result.err.len);

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
}

NAOS_SUITE(translate,
    NAOS_ADD_TEST(translate_scaffold_handler_is_declared),
    NAOS_ADD_TEST(translate_replaces_translated_entries),
    NAOS_ADD_TEST(translate_use_fuzzy_adopts_fuzzy),
    NAOS_ADD_TEST(translate_front_matter_keys),
    NAOS_ADD_TEST(translate_extract_output_yields_identical_source),
    NAOS_ADD_TEST(translate_empty_catalog_yields_identical_source),
    NAOS_ADD_TEST(translate_structure_changing_translation_warns_and_succeeds),
    NAOS_ADD_TEST(translate_invalid_po_is_error),
    NAOS_ADD_TEST(translate_invalid_utf8_translation_is_error),
    NAOS_ADD_TEST(translate_ignores_obsolete_and_msgctxt_entries),
    NAOS_ADD_TEST(translate_two_stdin_inputs_is_usage_error),
    NAOS_ADD_TEST(translate_bom_source_yields_bom_output),
    NAOS_ADD_TEST(translate_no_segments_reports_no_messages),
    NAOS_ADD_TEST(translate_stdin_source_and_quiet)
)
