#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "foundation/diag.h"
#include "test_decl.h"

/* サブコマンドの後の短いオプション（4.1〜4.7）と、短いオプションの中の h（8.2） */

#define SHORT_MAX_ARGS 8
#define SHORT_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct short_run
{
    char storage[SHORT_MAX_ARGS][SHORT_MAX_LEN];
    char *argv[SHORT_MAX_ARGS + 1];
    mnd_args_result_t result;
    mnd_diag_list_t diags;
    mnd_args_status_t status;
} short_run_t;

/* tokens の argc 個を引数列として test_tool_program で解析する */
static void run_parse(short_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    mnd_diag_list_init(&run->diags);

    NAOS_REQUIRE_TRUE(argc <= SHORT_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < SHORT_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = mnd_args_parse(&test_tool_program, argc, run->argv, &run->result, &run->diags);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(short_run_t *run)
{
    mnd_args_result_free(&run->result);
    mnd_diag_list_free(&run->diags);
}

/* 診断なしで MND_ARGS_OK になり、build が選択されていることを確かめる */
static void check_ok(const short_run_t *run)
{
    NAOS_CHECK_INT_EQ(MND_ARGS_OK, run->status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run->diags));
}

/* 使い方のエラーで、期待するメッセージの診断がちょうど 1 件だけ追加されたことを確かめる */
static void check_usage_error(const short_run_t *run, const char *expected)
{
    const mnd_diag_t *d;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&run->diags));
    d = mnd_diag_list_at(&run->diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_STR_EQ(expected, d->message);
}

/* 位置引数がちょうど 1 個で、argv[index] を指していることを確かめる */
static void check_single_positional(const short_run_t *run, int index)
{
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&run->result));
    NAOS_CHECK_PTR_EQ(run->argv[index], mnd_args_positional_at(&run->result, 0));
}

/* 次のトークンを値として消費する（4.1） */

NAOS_TEST(value_takes_next_token)
{
    static const char *const tokens[] = {"tool", "build", "-o", "y", "x"};
    short_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_value(&run.result, "output"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run.result, "output"));
    check_single_positional(&run, 4);
    run_free(&run);
}

NAOS_TEST(value_takes_next_token_starting_with_dash)
{
    static const char *const tokens[] = {"tool", "build", "-o", "-y", "x"};
    short_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_value(&run.result, "output"));
    NAOS_CHECK_STR_EQ("-y", mnd_args_value(&run.result, "output"));
    check_single_positional(&run, 4);
    run_free(&run);
}

/* 値として消費した -h はヘルプの要求にならない（4.1、8.2） */
NAOS_TEST(value_consumes_dash_h)
{
    static const char *const tokens[] = {"tool", "build", "-o", "-h", "x"};
    short_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_value(&run.result, "output"));
    NAOS_CHECK_STR_EQ("-h", mnd_args_value(&run.result, "output"));
    check_single_positional(&run, 4);
    run_free(&run);
}

/* 連結された値（4.2） */

NAOS_TEST(attached_value)
{
    static const char *const tokens[] = {"tool", "build", "-oy", "x"};
    short_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[2] + 2, mnd_args_value(&run.result, "output"));
    NAOS_CHECK_STR_EQ("y", mnd_args_value(&run.result, "output"));
    check_single_positional(&run, 3);
    run_free(&run);
}

NAOS_TEST(attached_value_keeps_h_and_dashes)
{
    static const char *const tokens[] = {"tool", "build", "-oh-v", "x"};
    short_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("h-v", mnd_args_value(&run.result, "output"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "verbose"));
    check_single_positional(&run, 3);
    run_free(&run);
}

/* フラグのまとめ書き（4.3） */

NAOS_TEST(bundled_flags_are_set)
{
    static const char *const tokens[] = {"tool", "build", "-vq", "-o", "y", "x"};
    short_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run);
    NAOS_CHECK_TRUE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run.result, "verbose"));
    NAOS_CHECK_TRUE(mnd_args_flag(&run.result, "quiet"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run.result, "quiet"));
    NAOS_CHECK_PTR_EQ(run.argv[4], mnd_args_value(&run.result, "output"));
    check_single_positional(&run, 5);
    run_free(&run);
}

/* まとめ書きの途中の値付きオプション（4.4） */

NAOS_TEST(bundle_value_takes_next_token)
{
    static const char *const tokens[] = {"tool", "build", "-vo", "y", "x"};
    short_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_TRUE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_value(&run.result, "output"));
    check_single_positional(&run, 4);
    run_free(&run);
}

NAOS_TEST(bundle_value_takes_rest)
{
    static const char *const tokens[] = {"tool", "build", "-voy", "x"};
    short_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run);
    NAOS_CHECK_TRUE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_PTR_EQ(run.argv[2] + 3, mnd_args_value(&run.result, "output"));
    NAOS_CHECK_STR_EQ("y", mnd_args_value(&run.result, "output"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "quiet"));
    check_single_positional(&run, 3);
    run_free(&run);
}

NAOS_TEST(bundle_value_rest_is_not_parsed_as_flags)
{
    static const char *const tokens[] = {"tool", "build", "-voq", "x"};
    short_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("q", mnd_args_value(&run.result, "output"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "quiet"));
    run_free(&run);
}

/* 短い形のリスト値（4.1、4.2、5.2） */

NAOS_TEST(list_value_by_short_name)
{
    static const char *const tokens[] = {"tool", "build", "-o", "y", "-ka,b", "-k", "c", "x"};
    short_run_t run;

    run_parse(&run, 8, tokens);
    check_ok(&run);
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_list_count(&run.result, "keys"));
    NAOS_CHECK_STR_EQ("a", mnd_args_list_at(&run.result, "keys", 0));
    NAOS_CHECK_STR_EQ("b", mnd_args_list_at(&run.result, "keys", 1));
    NAOS_CHECK_STR_EQ("c", mnd_args_list_at(&run.result, "keys", 2));
    check_single_positional(&run, 7);
    run_free(&run);
}

NAOS_TEST(list_empty_item_names_short_option)
{
    static const char *const separate[] = {"tool", "build", "-k", "a,,b"};
    static const char *const attached[] = {"tool", "build", "-vka,,b"};
    short_run_t run;

    run_parse(&run, 4, separate);
    check_usage_error(&run, "tool build: option '-k' has an empty item in 'a,,b'");
    run_free(&run);

    run_parse(&run, 3, attached);
    check_usage_error(&run, "tool build: option '-k' has an empty item in 'a,,b'");
    run_free(&run);
}

/* 未知の文字（4.5） */

NAOS_TEST(unknown_character_in_bundle)
{
    static const char *const tokens[] = {"tool", "build", "-vx", "-o", "y", "x"};
    short_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, "tool build: unknown option '-x'");
    run_free(&run);
}

NAOS_TEST(unknown_single_character)
{
    static const char *const tokens[] = {"tool", "build", "-o", "y", "x", "-z"};
    short_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, "tool build: unknown option '-z'");
    run_free(&run);
}

NAOS_TEST(short_names_are_case_sensitive)
{
    static const char *const upper_h[] = {"tool", "build", "-H"};
    static const char *const upper_v[] = {"tool", "build", "-o", "y", "-V", "x"};
    short_run_t run;

    run_parse(&run, 3, upper_h);
    check_usage_error(&run, "tool build: unknown option '-H'");
    run_free(&run);

    run_parse(&run, 6, upper_v);
    check_usage_error(&run, "tool build: unknown option '-V'");
    run_free(&run);
}

/* ASCII 以外のバイトはトークン全体で示す */
NAOS_TEST(non_ascii_byte_names_whole_token)
{
    static const char *const tokens[] = {"tool", "build", "-v\xC3\xA9", "-o", "y", "x"};
    short_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, "tool build: unknown option '-v\xC3\xA9'");
    run_free(&run);
}

/* 末尾の値の欠落（4.6） */

NAOS_TEST(trailing_value_option_requires_value)
{
    static const char *const single[] = {"tool", "build", "x", "-o"};
    static const char *const bundled[] = {"tool", "build", "x", "-vo"};
    static const char *const list[] = {"tool", "build", "x", "-o", "y", "-k"};
    short_run_t run;

    run_parse(&run, 4, single);
    check_usage_error(&run, "tool build: option '-o' requires a value");
    run_free(&run);

    run_parse(&run, 4, bundled);
    check_usage_error(&run, "tool build: option '-o' requires a value");
    run_free(&run);

    run_parse(&run, 6, list);
    check_usage_error(&run, "tool build: option '-k' requires a value");
    run_free(&run);
}

/* "-" 単独は位置引数（4.7） */

NAOS_TEST(single_dash_is_positional)
{
    static const char *const tokens[] = {"tool", "build", "-o", "y", "-"};
    short_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    check_single_positional(&run, 4);
    NAOS_CHECK_STR_EQ("-", mnd_args_positional_at(&run.result, 0));
    run_free(&run);
}

/* 短いオプションの中の h（8.2） */

NAOS_TEST(h_in_bundle_requests_help)
{
    static const char *const tokens[] = {"tool", "build", "-vh"};
    short_run_t run;

    run_parse(&run, 3, tokens);
    NAOS_CHECK_INT_EQ(MND_ARGS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run.diags));
    run_free(&run);
}

NAOS_TEST(single_h_requests_help)
{
    static const char *const tokens[] = {"tool", "build", "x", "-h"};
    short_run_t run;

    run_parse(&run, 4, tokens);
    NAOS_CHECK_INT_EQ(MND_ARGS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run.diags));
    run_free(&run);
}

NAOS_SUITE(short,
    NAOS_ADD_TEST(value_takes_next_token),
    NAOS_ADD_TEST(value_takes_next_token_starting_with_dash),
    NAOS_ADD_TEST(value_consumes_dash_h),
    NAOS_ADD_TEST(attached_value),
    NAOS_ADD_TEST(attached_value_keeps_h_and_dashes),
    NAOS_ADD_TEST(bundled_flags_are_set),
    NAOS_ADD_TEST(bundle_value_takes_next_token),
    NAOS_ADD_TEST(bundle_value_takes_rest),
    NAOS_ADD_TEST(bundle_value_rest_is_not_parsed_as_flags),
    NAOS_ADD_TEST(list_value_by_short_name),
    NAOS_ADD_TEST(list_empty_item_names_short_option),
    NAOS_ADD_TEST(unknown_character_in_bundle),
    NAOS_ADD_TEST(unknown_single_character),
    NAOS_ADD_TEST(short_names_are_case_sensitive),
    NAOS_ADD_TEST(non_ascii_byte_names_whole_token),
    NAOS_ADD_TEST(trailing_value_option_requires_value),
    NAOS_ADD_TEST(single_dash_is_positional),
    NAOS_ADD_TEST(h_in_bundle_requests_help),
    NAOS_ADD_TEST(single_h_requests_help)
)
