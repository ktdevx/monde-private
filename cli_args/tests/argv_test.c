#include <naos/naos.h>

#include "clopts/argv.h"
#include "foundation/diag.h"

#ifdef _WIN32
#include <wchar.h>
#endif

NAOS_TEST(argv_free_on_init_is_safe_twice)
{
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;

    mnd_args_argv_free(&args);
    mnd_args_argv_free(&args);
    NAOS_CHECK_INT_EQ(0, args.argc);
    NAOS_CHECK_NULL(args.argv);
    NAOS_CHECK_FALSE(args.owned);
}

#ifndef _WIN32

NAOS_TEST(argv_get_returns_given_argv_as_is)
{
    char arg0[] = "tool";
    char arg1[] = "\xE6\x97\xA5\xE6\x9C\xAC";
    char *argv[] = {arg0, arg1, NULL};
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(mnd_args_argv_get(2, argv, &args, &diags));
    NAOS_CHECK_INT_EQ(2, args.argc);
    NAOS_CHECK_PTR_EQ(argv, args.argv);
    NAOS_CHECK_FALSE(args.owned);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    /* 所有しないので、解放しても呼び出し側の配列は変わらない */
    mnd_args_argv_free(&args);
    NAOS_CHECK_INT_EQ(0, args.argc);
    NAOS_CHECK_NULL(args.argv);
    NAOS_CHECK_FALSE(args.owned);
    NAOS_CHECK_PTR_EQ(arg0, argv[0]);
    NAOS_CHECK_PTR_EQ(arg1, argv[1]);
    NAOS_CHECK_STR_EQ("tool", argv[0]);

    mnd_args_argv_free(&args);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(argv,
    NAOS_ADD_TEST(argv_free_on_init_is_safe_twice),
    NAOS_ADD_TEST(argv_get_returns_given_argv_as_is)
)

#else

/* 「日本」（U+65E5 U+672C）の UTF-16 と UTF-8。どちらも NUL 終端を含む */
static const wchar_t argv_nihon_utf16[] = {0x65E5, 0x672C, 0x0000};
static const unsigned char argv_nihon_utf8[] = {0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC, 0x00};

/* 対になっていない上位サロゲートを含む UTF-16 */
static const wchar_t argv_lone_surrogate[] = {0x0061, 0xD800, 0x0062, 0x0000};

static const wchar_t argv_tool_utf16[] = {0x0074, 0x006F, 0x006F, 0x006C, 0x0000};

NAOS_TEST(argv_from_wide_converts_to_utf8)
{
    wchar_t *const wargv[] = {(wchar_t *)argv_tool_utf16, (wchar_t *)argv_nihon_utf16, NULL};
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(mnd_args_argv_from_wide(2, wargv, &args, &diags));
    NAOS_CHECK_INT_EQ(2, args.argc);
    NAOS_REQUIRE_NOT_NULL(args.argv);
    NAOS_CHECK_TRUE(args.owned);
    NAOS_CHECK_STR_EQ("tool", args.argv[0]);
    NAOS_CHECK_MEM_EQ(args.argv[1], sizeof argv_nihon_utf8, argv_nihon_utf8,
                      sizeof argv_nihon_utf8);
    NAOS_CHECK_NULL(args.argv[2]);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_args_argv_free(&args);
    NAOS_CHECK_INT_EQ(0, args.argc);
    NAOS_CHECK_NULL(args.argv);
    NAOS_CHECK_FALSE(args.owned);

    /* 解放済みに対しても安全 */
    mnd_args_argv_free(&args);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(argv_from_wide_reports_index_of_invalid_utf16)
{
    wchar_t *const wargv[] = {(wchar_t *)argv_tool_utf16, (wchar_t *)argv_lone_surrogate, NULL};
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;

    NAOS_CHECK_FALSE(mnd_args_argv_from_wide(2, wargv, &args, &diags));
    NAOS_CHECK_INT_EQ(0, args.argc);
    NAOS_CHECK_NULL(args.argv);
    NAOS_CHECK_FALSE(args.owned);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_STR_EQ("command-line argument 1 is not valid UTF-16", d->message);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);

    mnd_args_argv_free(&args);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(argv_get_reads_process_command_line)
{
    char *dummy[] = {NULL};
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    int i;

    /* Windows では main の引数を使わず、プロセスのコマンドラインから取得する */
    NAOS_REQUIRE_TRUE(mnd_args_argv_get(0, dummy, &args, &diags));
    NAOS_CHECK_INT_GE(args.argc, 1);
    NAOS_REQUIRE_NOT_NULL(args.argv);
    NAOS_CHECK_TRUE(args.owned);
    for (i = 0; i < args.argc; i++)
    {
        NAOS_CHECK_NOT_NULL(args.argv[i]);
    }
    NAOS_CHECK_NULL(args.argv[args.argc]);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_args_argv_free(&args);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(argv,
    NAOS_ADD_TEST(argv_free_on_init_is_safe_twice),
    NAOS_ADD_TEST(argv_from_wide_converts_to_utf8),
    NAOS_ADD_TEST(argv_from_wide_reports_index_of_invalid_utf16),
    NAOS_ADD_TEST(argv_get_reads_process_command_line)
)

#endif
