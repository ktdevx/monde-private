/* スイート extract: ヘッダの値、--front-matter-keys、#: のファイル名、--no-wrap、改行コードと BOM、空の文書、エラー、件数（4.x） */

/* POSIX の setenv / unsetenv / tzset を C99 の厳密なモードで宣言させる（Windows は _putenv / _tzset を使う） */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <naos/naos.h>

#include "cli/cli.h"
#include "foundation/buf.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 2026-01-01 12:34:00 UTC の time_t の値（UNIX 時刻） */
#define EXTRACT_TIME ((time_t)1767225600 + (12 * 3600) + (34 * 60))

/* EXTRACT_TIME を TZ = JST-9 で表した POT-Creation-Date（実装とは別に手で求めた値） */
#define EXTRACT_DATE "2026-01-01 21:34+0900"

/* 既定の値で作ったヘッダエントリ（作成日時は EXTRACT_DATE） */
#define EXTRACT_DEFAULT_HEADER \
    "#, fuzzy\n" \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Project-Id-Version: PACKAGE VERSION\\n\"\n" \
    "\"Report-Msgid-Bugs-To: \\n\"\n" \
    "\"POT-Creation-Date: " EXTRACT_DATE "\\n\"\n" \
    "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n" \
    "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n" \
    "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n" \
    "\"Language: \\n\"\n" \
    "\"MIME-Version: 1.0\\n\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n" \
    "\"Content-Transfer-Encoding: 8bit\\n\"\n"

/* ---- 補助 ---- */

/* TZ を変えたかどうかと、変える前の値（teardown で戻す） */
static bool tz_changed = false;
static bool tz_had_value = false;
static char tz_saved[256];

/* プロセスの TZ を value にしてタイムゾーンの情報を読み直す。失敗したら false */
static bool set_tz(const char *value)
{
    if (!tz_changed)
    {
        const char *current = getenv("TZ");

        tz_had_value = (current != NULL);
        if (tz_had_value)
        {
            if (strlen(current) >= sizeof(tz_saved))
            {
                return false;
            }
            strcpy(tz_saved, current);
        }
        tz_changed = true;
    }
#if defined(_WIN32)
    {
        char assignment[64];

        if ((strlen(value) + 4) > sizeof(assignment))
        {
            return false;
        }
        strcpy(assignment, "TZ=");
        strcat(assignment, value);
        if (_putenv(assignment) != 0)
        {
            return false;
        }
        _tzset();
    }
#else
    if (setenv("TZ", value, 1) != 0)
    {
        return false;
    }
    tzset();
#endif
    return true;
}

/* TZ を変えたテストの後で元の値（なければ未設定）に戻す */
NAOS_TEARDOWN(extract_restore_tz)
{
    if (!tz_changed)
    {
        return;
    }
    tz_changed = false;
#if defined(_WIN32)
    {
        char assignment[sizeof(tz_saved) + 4];

        strcpy(assignment, "TZ=");
        if (tz_had_value)
        {
            strcat(assignment, tz_saved);
        }
        (void)_putenv(assignment);
        _tzset();
    }
#else
    if (tz_had_value)
    {
        (void)setenv("TZ", tz_saved, 1);
    }
    else
    {
        (void)unsetenv("TZ");
    }
    tzset();
#endif
}

#define EXTRACT_MAX_ARGS 16
#define EXTRACT_MAX_LEN 1024

typedef struct extract_argv
{
    char storage[EXTRACT_MAX_ARGS][EXTRACT_MAX_LEN];
    char *argv[EXTRACT_MAX_ARGS + 1];
    int argc;
} extract_argv_t;

/*
 * "monde extract" に NULL で終わる args を続けて test_cli_run で実行する（name はテストごとに一意）。
 * 時計は EXTRACT_TIME、TZ は JST-9 にする
 */
static void run_extract(const char *name, const char *stdin_bytes, size_t stdin_len, const char *const *args,
                        test_cli_result_t *result)
{
    static extract_argv_t copy;
    size_t i;

    NAOS_REQUIRE_TRUE(set_tz("JST-9"));
    test_clock_set(EXTRACT_TIME);

    strcpy(copy.storage[0], "monde");
    strcpy(copy.storage[1], "extract");
    copy.argc = 2;
    for (i = 0; args[i] != NULL; i++)
    {
        NAOS_REQUIRE_TRUE(copy.argc < EXTRACT_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(args[i]) < EXTRACT_MAX_LEN);
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

/* ---- テスト ---- */

/*
 * 標準入力から: 固定時刻の POT-Creation-Date、ヘッダの既定値（PACKAGE VERSION と空の Report-Msgid-Bugs-To）、
 * #: のファイル名が "-"、LF の入力で LF の出力、件数の表示（4.1〜4.4、4.7、3.3、8.1）
 */
NAOS_TEST(extract_stdin_header_defaults_and_fixed_time)
{
    static const char input[] = "# Heading\n\nBody text.\n";
    static const char *const args[] = { "-", NULL };
    static const char expected[] =
        EXTRACT_DEFAULT_HEADER
        "\n"
        "#. heading (level 1)\n"
        "#: -:1\n"
        "msgid \"Heading\"\n"
        "msgstr \"\"\n"
        "\n"
        "#. paragraph\n"
        "#: -:3\n"
        "msgid \"Body text.\"\n"
        "msgstr \"\"\n";
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_stdin_header_defaults_and_fixed_time", input, strlen(input), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(expected, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("extract: 2 messages\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* --project-id-version と --msgid-bugs-address の値がヘッダに書かれる（4.3、4.4） */
NAOS_TEST(extract_header_explicit_values)
{
    static const char input[] = "Body.\n";
    static const char *const args[] = {
        "--project-id-version", "Demo 1.0", "--msgid-bugs-address", "bugs@example.com", "-", NULL
    };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_header_explicit_values", input, strlen(input), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"Project-Id-Version: Demo 1.0\\n\"\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"Report-Msgid-Bugs-To: bugs@example.com\\n\"\n"));
    NAOS_CHECK_FALSE(buf_contains(&result.out, "PACKAGE VERSION"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"POT-Creation-Date: " EXTRACT_DATE "\\n\"\n"));
    NAOS_CHECK_STR_EQ("extract: 1 messages\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* --front-matter-keys の項目（コンマ区切り）が走査に渡り、フロントマターの値が含まれる（4.5） */
NAOS_TEST(extract_front_matter_keys_included)
{
    static const char text[] = "---\ntitle: Hello\nauthor: Alice\nlayout: post\n---\nBody.\n";
    const char *args[4];
    mnd_buf_t path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("extract_front_matter_keys_included.md", text, &path);
    args[0] = "--front-matter-keys";
    args[1] = "title,author";
    args[2] = mnd_buf_cstr(&path);
    args[3] = NULL;
    run_extract("extract_front_matter_keys_included", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "#. front matter: title\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"Hello\"\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "#. front matter: author\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"Alice\"\n"));
    NAOS_CHECK_FALSE(buf_contains(&result.out, "msgid \"post\"\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"Body.\"\n"));
    NAOS_CHECK_STR_EQ("extract: 3 messages\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
    mnd_buf_free(&path);
}

/* --front-matter-keys を省略するとフロントマターの値は含まれない（4.6） */
NAOS_TEST(extract_front_matter_omitted_excluded)
{
    static const char text[] = "---\ntitle: Hello\n---\nBody.\n";
    const char *args[2];
    mnd_buf_t path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("extract_front_matter_omitted_excluded.md", text, &path);
    args[0] = mnd_buf_cstr(&path);
    args[1] = NULL;
    run_extract("extract_front_matter_omitted_excluded", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_FALSE(buf_contains(&result.out, "Hello"));
    NAOS_CHECK_FALSE(buf_contains(&result.out, "front matter"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"Body.\"\n"));
    NAOS_CHECK_STR_EQ("extract: 1 messages\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
    mnd_buf_free(&path);
}

/* #: のファイル名はコマンドラインのパスのまま（"/./" も正規化しない）。-o のファイルに書く（4.7、2.2） */
NAOS_TEST(extract_reference_uses_command_line_path)
{
    static const char text[] = "Body.\n";
    const char *args[4];
    mnd_buf_t input_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected_ref = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_write_file("extract_reference_uses_command_line_path.md", text, strlen(text)));
    NAOS_REQUIRE_TRUE(test_remove_file("extract_reference_uses_command_line_path.pot"));
    NAOS_REQUIRE_TRUE(test_work_path(".", &input_path));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&input_path, "/extract_reference_uses_command_line_path.md"));
    NAOS_REQUIRE_TRUE(test_work_path("extract_reference_uses_command_line_path.pot", &output_path));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected_ref, "#: ")
                      && mnd_buf_append_str(&expected_ref, mnd_buf_cstr(&input_path))
                      && mnd_buf_append_str(&expected_ref, ":1\n"));

    args[0] = mnd_buf_cstr(&input_path);
    args[1] = "-o";
    args[2] = mnd_buf_cstr(&output_path);
    args[3] = NULL;
    run_extract("extract_reference_uses_command_line_path", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_REQUIRE_TRUE(test_read_file("extract_reference_uses_command_line_path.pot", &written));
    NAOS_CHECK_TRUE(buf_contains(&written, mnd_buf_cstr(&expected_ref)));
    NAOS_CHECK_TRUE(buf_contains(&written, "msgid \"Body.\"\n"));
    NAOS_CHECK_STR_EQ("extract: 1 messages\n", mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&written);
    mnd_buf_free(&expected_ref);
    mnd_buf_free(&output_path);
    mnd_buf_free(&input_path);
}

/* 長い段落: 既定では折り返し、--no-wrap では折り返さない（4.8） */
NAOS_TEST(extract_no_wrap)
{
    static const char input[] =
        "This paragraph is intentionally long so that the default writer has to wrap it across lines.\n";
    static const char *const wrap_args[] = { "-", NULL };
    static const char *const no_wrap_args[] = { "--no-wrap", "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_no_wrap_default", input, strlen(input), wrap_args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"\"\n\"This paragraph is intentionally long "));
    NAOS_CHECK_FALSE(buf_contains(&result.out,
        "msgid \"This paragraph is intentionally long so that the default writer has to wrap it across lines.\"\n"));

    run_extract("extract_no_wrap_enabled", input, strlen(input), no_wrap_args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out,
        "msgid \"This paragraph is intentionally long so that the default writer has to wrap it across lines.\"\n"));
    test_cli_result_free(&result);
}

/* CRLF の入力なら出力も CRLF（3.3） */
NAOS_TEST(extract_crlf_input_crlf_output)
{
    static const char input[] = "# Heading\r\n\r\nBody.\r\n";
    static const char *const args[] = { "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_crlf_input_crlf_output", input, strlen(input), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(all_lines_crlf(&result.out));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "#: -:3\r\nmsgid \"Body.\"\r\nmsgstr \"\"\r\n"));
    test_cli_result_free(&result);
}

/* BOM 付きの入力でも出力に BOM を付けない（3.7） */
NAOS_TEST(extract_bom_input_no_bom_output)
{
    static const char input[] = "\xEF\xBB\xBF" "Body.\n";
    static const char *const args[] = { "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_bom_input_no_bom_output", input, strlen(input), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_REQUIRE_UINT_GE(result.out.len, 8);
    NAOS_CHECK_MEM_EQ("#, fuzzy", 8, mnd_buf_cstr(&result.out), 8);
    NAOS_CHECK_FALSE(buf_contains(&result.out, "\xEF\xBB\xBF"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "#: -:1\nmsgid \"Body.\"\n"));
    test_cli_result_free(&result);
}

/* 翻訳単位のない文書ではヘッダだけを書き、成功して 0 件と表示する（4.10、8.1） */
NAOS_TEST(extract_no_units_header_only)
{
    static const char input[] = "\n\n";
    static const char *const args[] = { "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_no_units_header_only", input, strlen(input), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(EXTRACT_DEFAULT_HEADER, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("extract: 0 messages\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 不正なキーパスならパス付きのエラーを表示し、出力を作らずに 1 で終わる（4.9、2.8） */
NAOS_TEST(extract_invalid_key_path_fails_without_output)
{
    static const char text[] = "---\ntitle: Hello\n---\nBody.\n";
    const char *args[6];
    mnd_buf_t input_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected_error = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("extract_invalid_key_path_fails_without_output.md", text, &input_path);
    NAOS_REQUIRE_TRUE(test_remove_file("extract_invalid_key_path_fails_without_output.pot"));
    NAOS_REQUIRE_TRUE(test_work_path("extract_invalid_key_path_fails_without_output.pot", &output_path));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected_error, mnd_buf_cstr(&input_path))
                      && mnd_buf_append_str(&expected_error, ": error: invalid key path \"a..b\"\n"));

    args[0] = "--front-matter-keys";
    args[1] = "a..b";
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&output_path);
    args[4] = mnd_buf_cstr(&input_path);
    args[5] = NULL;
    run_extract("extract_invalid_key_path_fails_without_output", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("extract_invalid_key_path_fails_without_output.pot"));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&expected_error), mnd_buf_cstr(&result.err));
    NAOS_CHECK_UINT_EQ(0, result.out.len);

    test_cli_result_free(&result);
    mnd_buf_free(&expected_error);
    mnd_buf_free(&output_path);
    mnd_buf_free(&input_path);
}

/* キーパスを指定してフロントマターが不正な YAML なら、パスと行のエラーで出力を作らずに 1 で終わる（4.9、2.8） */
NAOS_TEST(extract_invalid_yaml_fails_without_output)
{
    static const char text[] = "---\ntitle: [unclosed\n---\nBody.\n";
    const char *args[6];
    mnd_buf_t input_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected_prefix = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("extract_invalid_yaml_fails_without_output.md", text, &input_path);
    NAOS_REQUIRE_TRUE(test_remove_file("extract_invalid_yaml_fails_without_output.pot"));
    NAOS_REQUIRE_TRUE(test_work_path("extract_invalid_yaml_fails_without_output.pot", &output_path));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected_prefix, mnd_buf_cstr(&input_path))
                      && mnd_buf_append_str(&expected_prefix, ":2: error: "));

    args[0] = "--front-matter-keys";
    args[1] = "title";
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&output_path);
    args[4] = mnd_buf_cstr(&input_path);
    args[5] = NULL;
    run_extract("extract_invalid_yaml_fails_without_output", NULL, 0, args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("extract_invalid_yaml_fails_without_output.pot"));
    NAOS_REQUIRE_UINT_GE(result.err.len, expected_prefix.len);
    NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&expected_prefix), expected_prefix.len, mnd_buf_cstr(&result.err),
                      expected_prefix.len);
    NAOS_CHECK_FALSE(buf_contains(&result.err, "extract:"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);

    test_cli_result_free(&result);
    mnd_buf_free(&expected_prefix);
    mnd_buf_free(&output_path);
    mnd_buf_free(&input_path);
}

/* 件数は同じ msgid をまとめたエントリの数。--quiet なら件数を表示しない（8.1、8.7） */
NAOS_TEST(extract_count_display_and_quiet)
{
    static const char input[] = "Same.\n\nSame.\n\nOther.\n";
    static const char *const args[] = { "-", NULL };
    static const char *const quiet_args[] = { "--quiet", "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_extract("extract_count_display", input, strlen(input), args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ("extract: 2 messages\n", mnd_buf_cstr(&result.err));

    run_extract("extract_count_quiet", input, strlen(input), quiet_args, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.err.len);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"Other.\"\n"));
    test_cli_result_free(&result);
}

NAOS_SUITE(extract,
    NAOS_SET_TEARDOWN(extract_restore_tz),
    NAOS_ADD_TEST(extract_stdin_header_defaults_and_fixed_time),
    NAOS_ADD_TEST(extract_header_explicit_values),
    NAOS_ADD_TEST(extract_front_matter_keys_included),
    NAOS_ADD_TEST(extract_front_matter_omitted_excluded),
    NAOS_ADD_TEST(extract_reference_uses_command_line_path),
    NAOS_ADD_TEST(extract_no_wrap),
    NAOS_ADD_TEST(extract_crlf_input_crlf_output),
    NAOS_ADD_TEST(extract_bom_input_no_bom_output),
    NAOS_ADD_TEST(extract_no_units_header_only),
    NAOS_ADD_TEST(extract_invalid_key_path_fails_without_output),
    NAOS_ADD_TEST(extract_invalid_yaml_fails_without_output),
    NAOS_ADD_TEST(extract_count_display_and_quiet)
)
