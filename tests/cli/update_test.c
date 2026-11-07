/* スイート update: 引き継ぎ、--no-fuzzy-matching、ヘッダの保持、同じパスへの上書き、エラー（2.5、2.7、2.8、6.x、8.3） */

#include <naos/naos.h>

#include "cli/cli.h"
#include "foundation/buf.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 既存の翻訳ファイルのヘッダ（翻訳者が設定した値を持つ） */
#define UPDATE_OLD_HEADER \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Project-Id-Version: PACKAGE VERSION\\n\"\n" \
    "\"Report-Msgid-Bugs-To: \\n\"\n" \
    "\"POT-Creation-Date: 2026-01-01 10:00+0900\\n\"\n" \
    "\"PO-Revision-Date: 2026-01-15 09:30+0900\\n\"\n" \
    "\"Last-Translator: Taro Yamada <taro@example.com>\\n\"\n" \
    "\"Language-Team: Japanese\\n\"\n" \
    "\"Language: ja\\n\"\n" \
    "\"MIME-Version: 1.0\\n\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n" \
    "\"Content-Transfer-Encoding: 8bit\\n\"\n"

/* 新しいテンプレート: 見出しは同じ、本文は句読点だけ変わり、段落を 1 つ追加し、1 つ削除した */
#define UPDATE_TEMPLATE \
    "#, fuzzy\n" \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Project-Id-Version: PACKAGE VERSION\\n\"\n" \
    "\"Report-Msgid-Bugs-To: \\n\"\n" \
    "\"POT-Creation-Date: 2026-02-01 10:00+0900\\n\"\n" \
    "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n" \
    "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n" \
    "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n" \
    "\"Language: \\n\"\n" \
    "\"MIME-Version: 1.0\\n\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n" \
    "\"Content-Transfer-Encoding: 8bit\\n\"\n" \
    "\n" \
    "#. heading (level 1)\n" \
    "#: doc.md:1\n" \
    "msgid \"Heading\"\n" \
    "msgstr \"\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:3\n" \
    "msgid \"This is the body text of the document!\"\n" \
    "msgstr \"\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:5\n" \
    "msgid \"Install it with make.\"\n" \
    "msgstr \"\"\n"

/* 既存の翻訳ファイル */
#define UPDATE_OLD \
    UPDATE_OLD_HEADER \
    "\n" \
    "#. heading (level 1)\n" \
    "#: doc.md:1\n" \
    "msgid \"Heading\"\n" \
    "msgstr \"見出し\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:3\n" \
    "msgid \"This is the body text of the document.\"\n" \
    "msgstr \"これは文書の本文です。\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:5\n" \
    "msgid \"Removed paragraph.\"\n" \
    "msgstr \"削除された段落。\"\n"

/* マージ後のヘッダ（既存のヘッダを引き継ぎ、POT-Creation-Date だけテンプレートの値になる） */
#define UPDATE_MERGED_HEADER \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Project-Id-Version: PACKAGE VERSION\\n\"\n" \
    "\"Report-Msgid-Bugs-To: \\n\"\n" \
    "\"POT-Creation-Date: 2026-02-01 10:00+0900\\n\"\n" \
    "\"PO-Revision-Date: 2026-01-15 09:30+0900\\n\"\n" \
    "\"Last-Translator: Taro Yamada <taro@example.com>\\n\"\n" \
    "\"Language-Team: Japanese\\n\"\n" \
    "\"Language: ja\\n\"\n" \
    "\"MIME-Version: 1.0\\n\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n" \
    "\"Content-Transfer-Encoding: 8bit\\n\"\n"

/* msgmerge --previous（gettext 0.21）で old.po と new.pot をマージした結果 */
#define UPDATE_MERGED \
    UPDATE_MERGED_HEADER \
    "\n" \
    "#. heading (level 1)\n" \
    "#: doc.md:1\n" \
    "msgid \"Heading\"\n" \
    "msgstr \"見出し\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:3\n" \
    "#, fuzzy\n" \
    "#| msgid \"This is the body text of the document.\"\n" \
    "msgid \"This is the body text of the document!\"\n" \
    "msgstr \"これは文書の本文です。\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:5\n" \
    "msgid \"Install it with make.\"\n" \
    "msgstr \"\"\n" \
    "\n" \
    "#~ msgid \"Removed paragraph.\"\n" \
    "#~ msgstr \"削除された段落。\"\n"

/* msgmerge --previous --no-fuzzy-matching の結果 */
#define UPDATE_MERGED_NO_FUZZY \
    UPDATE_MERGED_HEADER \
    "\n" \
    "#. heading (level 1)\n" \
    "#: doc.md:1\n" \
    "msgid \"Heading\"\n" \
    "msgstr \"見出し\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:3\n" \
    "msgid \"This is the body text of the document!\"\n" \
    "msgstr \"\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:5\n" \
    "msgid \"Install it with make.\"\n" \
    "msgstr \"\"\n" \
    "\n" \
    "#~ msgid \"This is the body text of the document.\"\n" \
    "#~ msgstr \"これは文書の本文です。\"\n" \
    "\n" \
    "#~ msgid \"Removed paragraph.\"\n" \
    "#~ msgstr \"削除された段落。\"\n"

/* 構文エラーを含む PO（6 行目） */
#define UPDATE_SYNTAX_ERROR \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n" \
    "\n" \
    "msgid \"Body.\"\n" \
    "bogus line\n"

/* ---- 補助 ---- */

#define UPDATE_MAX_ARGS 16
#define UPDATE_MAX_LEN 1024

typedef struct update_argv
{
    char storage[UPDATE_MAX_ARGS][UPDATE_MAX_LEN];
    char *argv[UPDATE_MAX_ARGS + 1];
    int argc;
} update_argv_t;

/* "monde update" に NULL で終わる args を続けて test_cli_run で実行する（name はテストごとに一意） */
static void run_update(const char *name, const char *stdin_bytes, size_t stdin_len, const char *const *args,
                       test_cli_result_t *result)
{
    static update_argv_t copy;
    size_t i;

    test_env_set(NULL, 0);

    strcpy(copy.storage[0], "monde");
    strcpy(copy.storage[1], "update");
    copy.argc = 2;
    for (i = 0; args[i] != NULL; i++)
    {
        NAOS_REQUIRE_TRUE(copy.argc < UPDATE_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(args[i]) < UPDATE_MAX_LEN);
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

/* 作業ディレクトリに name の内容 text（strlen の長さ）を書き、そのパスを path に入れる */
static void write_input(const char *name, const char *text, mnd_buf_t *path)
{
    NAOS_REQUIRE_TRUE(test_write_file(name, text, strlen(text)));
    NAOS_REQUIRE_TRUE(test_work_path(name, path));
}

/* buf の内容が needle を含む */
static bool buf_contains(const mnd_buf_t *buf, const char *needle)
{
    return (strstr(mnd_buf_cstr(buf), needle) != NULL);
}

/* buf のすべての \n の直前が \r である（\n が 1 つ以上ある） */
static bool all_lines_crlf(const mnd_buf_t *buf)
{
    const char *data = mnd_buf_cstr(buf);
    size_t newlines = 0;
    size_t i;

    for (i = 0; i < buf->len; i++)
    {
        if (data[i] == '\n')
        {
            if ((i == 0) || (data[i - 1] != '\r'))
            {
                return false;
            }
            newlines++;
        }
    }
    return (newlines > 0);
}

/* LF の text を CRLF に変えて out に入れる */
static void to_crlf(const char *text, mnd_buf_t *out)
{
    const char *p;

    mnd_buf_clear(out);
    for (p = text; *p != '\0'; p++)
    {
        if (*p == '\n')
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append(out, "\r\n", 2));
        }
        else
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append(out, p, 1));
        }
    }
}

/*
 * <name>.pot に template_text、<name>.po に old_text を書き、-o <name>.po（既存の翻訳ファイルを上書き）で
 * update を実行する。終了コード 1、標準出力は空、<name>.po がバイト単位で変わらないことを確かめ、
 * err をパスの組とともに呼び出し側に返す
 */
static void run_failing_overwrite(const char *name, const char *template_text, const char *old_text,
                                  mnd_buf_t *template_path, mnd_buf_t *old_path, test_cli_result_t *result)
{
    char template_name[128];
    char old_name[128];
    const char *args[5];
    mnd_buf_t after = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(strlen(name) + 4 < sizeof(template_name));
    strcpy(template_name, name);
    strcat(template_name, ".pot");
    strcpy(old_name, name);
    strcat(old_name, ".po");

    write_input(template_name, template_text, template_path);
    write_input(old_name, old_text, old_path);

    args[0] = mnd_buf_cstr(template_path);
    args[1] = mnd_buf_cstr(old_path);
    args[2] = "-o";
    args[3] = mnd_buf_cstr(old_path);
    args[4] = NULL;
    run_update(name, NULL, 0, args, result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result->exit_code);
    NAOS_CHECK_UINT_EQ(0, result->out.len);
    NAOS_CHECK_FALSE(buf_contains(&result->err, "update:"));
    NAOS_REQUIRE_TRUE(test_read_file(old_name, &after));
    NAOS_CHECK_MEM_EQ(old_text, strlen(old_text), mnd_buf_cstr(&after), after.len);

    mnd_buf_free(&after);
}

/* ---- テスト ---- */

/*
 * 完全一致の訳を引き継ぎ、似た msgid は fuzzy と前回の msgid 付きで引き継ぐ。新規は空の msgstr、
 * 削除されたエントリは廃止エントリになる。出力は msgmerge --previous と同じで、件数を 1 行で表示する（6.1、6.2、8.3）
 */
NAOS_TEST(update_merges_with_fuzzy_matching)
{
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    const char *args[3];
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("update_merges_with_fuzzy_matching.pot", UPDATE_TEMPLATE, &template_path);
    write_input("update_merges_with_fuzzy_matching.po", UPDATE_OLD, &old_path);

    args[0] = mnd_buf_cstr(&template_path);
    args[1] = mnd_buf_cstr(&old_path);
    args[2] = NULL;
    run_update("update_merges_with_fuzzy_matching", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(UPDATE_MERGED, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("update: 1 matched, 1 fuzzy, 1 added, 1 obsoleted\n", mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* --no-fuzzy-matching では fuzzy にせず、完全一致だけを引き継ぐ（6.3、8.3） */
NAOS_TEST(update_no_fuzzy_matching)
{
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    const char *args[4];
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("update_no_fuzzy_matching.pot", UPDATE_TEMPLATE, &template_path);
    write_input("update_no_fuzzy_matching.po", UPDATE_OLD, &old_path);

    args[0] = "--no-fuzzy-matching";
    args[1] = mnd_buf_cstr(&template_path);
    args[2] = mnd_buf_cstr(&old_path);
    args[3] = NULL;
    run_update("update_no_fuzzy_matching", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(UPDATE_MERGED_NO_FUZZY, mnd_buf_cstr(&result.out));
    NAOS_CHECK_FALSE(buf_contains(&result.out, "fuzzy"));
    NAOS_CHECK_STR_EQ("update: 1 matched, 0 fuzzy, 2 added, 2 obsoleted\n", mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* 既存の翻訳ファイルのヘッダ（Language、PO-Revision-Date、Last-Translator）を書き換えずに引き継ぐ（6.4） */
NAOS_TEST(update_keeps_existing_header)
{
    const char *argv_args[3];
    mnd_buf_t old_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    /* テンプレートは標準入力から読む（`-` が 1 つなら使える） */
    write_input("update_keeps_existing_header.po", UPDATE_OLD, &old_path);
    argv_args[0] = "-";
    argv_args[1] = mnd_buf_cstr(&old_path);
    argv_args[2] = NULL;
    run_update("update_keeps_existing_header", UPDATE_TEMPLATE, strlen(UPDATE_TEMPLATE), argv_args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"Language: ja\\n\"\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"PO-Revision-Date: 2026-01-15 09:30+0900\\n\"\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"Last-Translator: Taro Yamada <taro@example.com>\\n\"\n"));
    NAOS_CHECK_FALSE(buf_contains(&result.out, "YEAR-MO-DA"));
    /* ヘッダの前に fuzzy は付かない（テンプレートのヘッダの fuzzy を引き継がない） */
    NAOS_CHECK_TRUE(strncmp(mnd_buf_cstr(&result.out), "msgid \"\"\n", 9) == 0);

    test_cli_result_free(&result);
    mnd_buf_free(&old_path);
}

/* 2 つの入力に `-` を指定すると、出力を作らずに使い方のエラーで 2 を返す（2.5） */
NAOS_TEST(update_two_stdin_inputs_is_usage_error)
{
    const char *args[5];
    mnd_buf_t output_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_remove_file("update_two_stdin_inputs_is_usage_error.po"));
    NAOS_REQUIRE_TRUE(test_work_path("update_two_stdin_inputs_is_usage_error.po", &output_path));

    args[0] = "-";
    args[1] = "-";
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&output_path);
    args[4] = NULL;
    run_update("update_two_stdin_inputs_is_usage_error", UPDATE_TEMPLATE, strlen(UPDATE_TEMPLATE), args,
               &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("update_two_stdin_inputs_is_usage_error.po"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("monde: error: standard input ('-') can be used only once, but 2 inputs are '-'\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&output_path);
}

/* 出力先 = 既存の翻訳ファイルのパス: 両方を読み終えてから書くため、訳を失わずに上書きする（2.7、6.7） */
NAOS_TEST(update_overwrites_existing_po_in_place)
{
    const char *args[5];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("update_overwrites_existing_po_in_place.pot", UPDATE_TEMPLATE, &template_path);
    write_input("update_overwrites_existing_po_in_place.po", UPDATE_OLD, &old_path);

    args[0] = mnd_buf_cstr(&template_path);
    args[1] = mnd_buf_cstr(&old_path);
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&old_path);
    args[4] = NULL;
    run_update("update_overwrites_existing_po_in_place", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("update: 1 matched, 1 fuzzy, 1 added, 1 obsoleted\n", mnd_buf_cstr(&result.err));
    NAOS_REQUIRE_TRUE(test_read_file("update_overwrites_existing_po_in_place.po", &written));
    NAOS_CHECK_STR_EQ(UPDATE_MERGED, mnd_buf_cstr(&written));

    test_cli_result_free(&result);
    mnd_buf_free(&written);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* 既存の翻訳ファイルが不正なら、上書きの指定があってもそのファイルを変えずに 1 で終わる（2.8、6.6、6.7） */
NAOS_TEST(update_invalid_existing_po_keeps_file)
{
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    mnd_buf_t expected_prefix = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_failing_overwrite("update_invalid_existing_po_keeps_file", UPDATE_TEMPLATE, UPDATE_SYNTAX_ERROR,
                          &template_path, &old_path, &result);
    /* エラーは既存の翻訳ファイルのパスと行で示す */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected_prefix, mnd_buf_cstr(&old_path))
                      && mnd_buf_append_str(&expected_prefix, ":6: error: "));
    NAOS_REQUIRE_UINT_GE(result.err.len, expected_prefix.len);
    NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&expected_prefix), expected_prefix.len, mnd_buf_cstr(&result.err),
                      expected_prefix.len);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&template_path, ":"));
    NAOS_CHECK_FALSE(buf_contains(&result.err, mnd_buf_cstr(&template_path)));

    test_cli_result_free(&result);
    mnd_buf_free(&expected_prefix);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* テンプレートが不正なら、既存の翻訳ファイルを変えずに 1 で終わり、エラーはテンプレートのパスで示す（2.8、6.6、6.7） */
NAOS_TEST(update_invalid_template_keeps_file)
{
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    mnd_buf_t expected_prefix = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_failing_overwrite("update_invalid_template_keeps_file", UPDATE_SYNTAX_ERROR, UPDATE_OLD,
                          &template_path, &old_path, &result);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected_prefix, mnd_buf_cstr(&template_path))
                      && mnd_buf_append_str(&expected_prefix, ":6: error: "));
    NAOS_REQUIRE_UINT_GE(result.err.len, expected_prefix.len);
    NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&expected_prefix), expected_prefix.len, mnd_buf_cstr(&result.err),
                      expected_prefix.len);
    /* テンプレートのパスは既存の翻訳ファイルのパスを接頭辞に含むため、":" まで比べる */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&old_path, ":"));
    NAOS_CHECK_FALSE(buf_contains(&result.err, mnd_buf_cstr(&old_path)));

    test_cli_result_free(&result);
    mnd_buf_free(&expected_prefix);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/*
 * 両方のファイルにエラーがあれば、報告されたすべてのエラーをそれぞれのファイルのパスで示す（6.6）。
 * テンプレートは重複、既存の翻訳ファイルは構文エラー
 */
NAOS_TEST(update_errors_in_both_files_identify_paths)
{
    static const char template_text[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"A\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"A\"\n"
        "msgstr \"\"\n";
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_failing_overwrite("update_errors_in_both_files_identify_paths", template_text, UPDATE_SYNTAX_ERROR,
                          &template_path, &old_path, &result);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&template_path))
                      && mnd_buf_append_str(&expected,
                                            ":8: error: duplicate message definition (first defined at line 5)\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.err, mnd_buf_cstr(&expected)));
    mnd_buf_clear(&expected);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&old_path))
                      && mnd_buf_append_str(&expected, ":6: error: "));
    NAOS_CHECK_TRUE(buf_contains(&result.err, mnd_buf_cstr(&expected)));

    test_cli_result_free(&result);
    mnd_buf_free(&expected);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* 存在しないテンプレート: パスを示して 1 で終わり、既存の翻訳ファイルは変わらない（2.8、2.9） */
NAOS_TEST(update_missing_template_keeps_file)
{
    const char *args[5];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    mnd_buf_t after = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_remove_file("update_missing_template_keeps_file.pot"));
    NAOS_REQUIRE_TRUE(test_work_path("update_missing_template_keeps_file.pot", &template_path));
    write_input("update_missing_template_keeps_file.po", UPDATE_OLD, &old_path);

    args[0] = mnd_buf_cstr(&template_path);
    args[1] = mnd_buf_cstr(&old_path);
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&old_path);
    args[4] = NULL;
    run_update("update_missing_template_keeps_file", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.err, mnd_buf_cstr(&template_path)));
    NAOS_REQUIRE_TRUE(test_read_file("update_missing_template_keeps_file.po", &after));
    NAOS_CHECK_STR_EQ(UPDATE_OLD, mnd_buf_cstr(&after));

    test_cli_result_free(&result);
    mnd_buf_free(&after);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* 出力の改行コードは既存の翻訳ファイル（CRLF）に合わせ、BOM は付けない（3.3、3.7） */
NAOS_TEST(update_uses_existing_po_eol_without_bom)
{
    const char *args[3];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    mnd_buf_t old_text = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    /* テンプレートは LF、既存の翻訳ファイルは BOM 付きの CRLF */
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&old_text, "\xEF\xBB\xBF"));
    to_crlf(UPDATE_OLD, &expected);
    NAOS_REQUIRE_TRUE(mnd_buf_append(&old_text, mnd_buf_cstr(&expected), expected.len));
    write_input("update_uses_existing_po_eol_without_bom.pot", UPDATE_TEMPLATE, &template_path);
    write_input("update_uses_existing_po_eol_without_bom.po", mnd_buf_cstr(&old_text), &old_path);

    args[0] = mnd_buf_cstr(&template_path);
    args[1] = mnd_buf_cstr(&old_path);
    args[2] = NULL;
    run_update("update_uses_existing_po_eol_without_bom", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(all_lines_crlf(&result.out));
    to_crlf(UPDATE_MERGED, &expected);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&expected), mnd_buf_cstr(&result.out));

    test_cli_result_free(&result);
    mnd_buf_free(&expected);
    mnd_buf_free(&old_text);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* 長い msgid: 既定では折り返し、--no-wrap では折り返さない（6.5） */
NAOS_TEST(update_no_wrap)
{
    static const char template_text[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"This paragraph is intentionally long so that the default writer has to wrap it across lines.\"\n"
        "msgstr \"\"\n";
    static const char old_text[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Language: ja\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n";
    const char *args[4];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t old_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("update_no_wrap.pot", template_text, &template_path);
    write_input("update_no_wrap.po", old_text, &old_path);

    args[0] = mnd_buf_cstr(&template_path);
    args[1] = mnd_buf_cstr(&old_path);
    args[2] = NULL;
    run_update("update_no_wrap_default", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"\"\n\"This paragraph is intentionally long "));
    NAOS_CHECK_STR_EQ("update: 0 matched, 0 fuzzy, 1 added, 0 obsoleted\n", mnd_buf_cstr(&result.err));

    args[0] = "--no-wrap";
    args[1] = mnd_buf_cstr(&template_path);
    args[2] = mnd_buf_cstr(&old_path);
    args[3] = NULL;
    run_update("update_no_wrap_enabled", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out,
        "msgid \"This paragraph is intentionally long so that the default writer has to wrap it across lines.\"\n"));

    test_cli_result_free(&result);
    mnd_buf_free(&old_path);
    mnd_buf_free(&template_path);
}

/* --quiet なら件数を表示しない（8.7） */
NAOS_TEST(update_quiet_suppresses_stats)
{
    const char *args[4];
    mnd_buf_t old_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("update_quiet_suppresses_stats.po", UPDATE_OLD, &old_path);
    args[0] = "-q";
    args[1] = "-";
    args[2] = mnd_buf_cstr(&old_path);
    args[3] = NULL;
    run_update("update_quiet_suppresses_stats", UPDATE_TEMPLATE, strlen(UPDATE_TEMPLATE), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.err.len);
    NAOS_CHECK_STR_EQ(UPDATE_MERGED, mnd_buf_cstr(&result.out));

    test_cli_result_free(&result);
    mnd_buf_free(&old_path);
}

NAOS_SUITE(update,
    NAOS_ADD_TEST(update_merges_with_fuzzy_matching),
    NAOS_ADD_TEST(update_no_fuzzy_matching),
    NAOS_ADD_TEST(update_keeps_existing_header),
    NAOS_ADD_TEST(update_two_stdin_inputs_is_usage_error),
    NAOS_ADD_TEST(update_overwrites_existing_po_in_place),
    NAOS_ADD_TEST(update_invalid_existing_po_keeps_file),
    NAOS_ADD_TEST(update_invalid_template_keeps_file),
    NAOS_ADD_TEST(update_errors_in_both_files_identify_paths),
    NAOS_ADD_TEST(update_missing_template_keeps_file),
    NAOS_ADD_TEST(update_uses_existing_po_eol_without_bom),
    NAOS_ADD_TEST(update_no_wrap),
    NAOS_ADD_TEST(update_quiet_suppresses_stats)
)
