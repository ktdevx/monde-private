/*
 * インストールした CLOpts の利用側プログラム。プロセスの引数を取得して解析し、ヘルプを生成する。
 * package_test.cmake が "--name world --verbose input.txt" を渡して実行し、終了コード 0 を期待する
 */
#include "clopts/argv.h"
#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"

#include <stdio.h>
#include <string.h>

static const clopts_option_t root_options[] = {
    { .long_name = "name", .short_name = 'n', .kind = CLOPTS_VALUE, .value_name = "NAME",
      .help = "Name to greet" },
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_FLAG, .help = "Print more" },
};

static const clopts_positional_t root_positionals[] = {
    { .name = "INPUT", .help = "Input file" },
};

static const clopts_command_t root_command = {
    .description = "Package consumer of CLOpts.",
    .options = root_options,
    .option_count = sizeof(root_options) / sizeof(root_options[0]),
    .positionals = root_positionals,
    .positional_count = 1,
    .min_positionals = 1,
    .max_positionals = 1,
};

static const clopts_program_t program = {
    .name = "consumer",
    .version = "1.0.0",
    .root = &root_command,
};

/* errors の各メッセージを標準エラー出力に書く */
static void print_errors(const clopts_errors_t *errors)
{
    size_t i;
    for (i = 0; i < clopts_errors_count(errors); ++i)
    {
        fprintf(stderr, "error: %s\n", clopts_errors_at(errors, i));
    }
}

/* 失敗の内容を書いて false を返す */
static bool fail(const char *what)
{
    fprintf(stderr, "consumer: %s\n", what);
    return false;
}

/* 解析結果とヘルプを確かめる */
static bool check(const clopts_result_t *result)
{
    const char *name = clopts_value(result, "name");
    char *help;
    bool ok;

    if ((name == NULL) || (strcmp(name, "world") != 0))
    {
        return fail("--name was not parsed as 'world'");
    }
    if (!clopts_flag(result, "verbose"))
    {
        return fail("--verbose was not parsed");
    }
    if ((clopts_positional_count(result) != 1) || (strcmp(clopts_positional_at(result, 0), "input.txt") != 0))
    {
        return fail("the positional argument was not parsed as 'input.txt'");
    }

    help = clopts_format_help(result);
    if (help == NULL)
    {
        return fail("clopts_format_help returned NULL");
    }
    ok = (strstr(help, "--name") != NULL) && (strstr(help, "INPUT") != NULL);
    fputs(help, stdout);
    clopts_text_free(help);
    if (!ok)
    {
        return fail("the help does not mention --name and INPUT");
    }
    return true;
}

int main(int argc, char **argv)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    clopts_argv_t args = CLOPTS_ARGV_INIT;
    clopts_result_t result = CLOPTS_RESULT_INIT;
    bool ok = false;

    if (!clopts_argv_get(argc, argv, &args, &errors))
    {
        print_errors(&errors);
        clopts_errors_free(&errors);
        return 1;
    }

    if (clopts_parse(&program, args.argc, args.argv, &result, &errors) == CLOPTS_OK)
    {
        ok = check(&result);
    }
    else
    {
        print_errors(&errors);
    }

    clopts_result_free(&result);
    clopts_argv_free(&args);
    clopts_errors_free(&errors);
    return ok ? 0 : 1;
}
