/* Commands: プログラムとサブコマンドの宣言のテーブルと、サブコマンド名からハンドラへの対応 */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>
#include <string.h>

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* 全サブコマンドに共通のオプション（2.2、8.7）。--output は省略で標準出力にするため必須にしない（2.3） */
#define OPTION_OUTPUT                                                                   \
    { .long_name = MND_CLI_OPT_OUTPUT, .short_name = 'o', .kind = CLOPTS_VALUE,         \
      .value_name = "FILE", .help = "Write the result to FILE (default: standard output)" }
#define OPTION_QUIET                                                                    \
    { .long_name = MND_CLI_OPT_QUIET, .short_name = 'q', .kind = CLOPTS_FLAG,           \
      .help = "Do not print statistics or informational messages" }
#define OPTION_NO_WRAP                                                                  \
    { .long_name = MND_CLI_OPT_NO_WRAP, .kind = CLOPTS_FLAG,                            \
      .help = "Do not wrap long lines in the output" }

/* extract: INPUT（4.3〜4.5、4.8） */
static const clopts_option_t extract_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { .long_name = MND_CLI_OPT_FRONT_MATTER_KEYS, .kind = CLOPTS_LIST, .value_name = "KEY",
      .help = "Extract front matter values at these key paths" },
    { .long_name = MND_CLI_OPT_PROJECT_ID_VERSION, .kind = CLOPTS_VALUE, .value_name = "NAME",
      .help = "Value of the Project-Id-Version header (default: PACKAGE VERSION)" },
    { .long_name = MND_CLI_OPT_MSGID_BUGS_ADDRESS, .kind = CLOPTS_VALUE, .value_name = "ADDRESS",
      .help = "Value of the Report-Msgid-Bugs-To header (default: empty)" },
    OPTION_NO_WRAP,
};

static const clopts_positional_t extract_positionals[] = {
    { .name = "INPUT", .help = "Markdown file to extract from ('-' for standard input)" },
};

/* init: TEMPLATE（5.2、5.9）。--language は環境変数から決められるため必須にしない（5.3） */
static const clopts_option_t init_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { .long_name = MND_CLI_OPT_LANGUAGE, .short_name = 'l', .kind = CLOPTS_VALUE, .value_name = "CODE",
      .help = "Language code for the Language header (default: from LC_ALL, LC_MESSAGES or LANG)" },
    OPTION_NO_WRAP,
};

static const clopts_positional_t init_positionals[] = {
    { .name = "TEMPLATE", .help = "Translation template (.pot) to start from ('-' for standard input)" },
};

/* update: TEMPLATE PO（6.3、6.5） */
static const clopts_option_t update_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { .long_name = MND_CLI_OPT_NO_FUZZY_MATCHING, .kind = CLOPTS_FLAG,
      .help = "Do not use fuzzy matching; reuse translations of exact matches only" },
    OPTION_NO_WRAP,
};

static const clopts_positional_t update_positionals[] = {
    { .name = "TEMPLATE", .help = "New translation template (.pot) ('-' for standard input)" },
    { .name = "PO", .help = "Existing translation file (.po) to merge ('-' for standard input)" },
};

/* translate: INPUT PO（7.3、7.4） */
static const clopts_option_t translate_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { .long_name = MND_CLI_OPT_FRONT_MATTER_KEYS, .kind = CLOPTS_LIST, .value_name = "KEY",
      .help = "Translate front matter values at these key paths" },
    { .long_name = MND_CLI_OPT_USE_FUZZY, .kind = CLOPTS_FLAG,
      .help = "Use non-empty translations of fuzzy entries" },
};

static const clopts_positional_t translate_positionals[] = {
    { .name = "INPUT", .help = "Markdown file to translate ('-' for standard input)" },
    { .name = "PO", .help = "Translation file (.po) with the translations ('-' for standard input)" },
};

/* 4 つのサブコマンド（1.1）。位置引数はすべて必須で個数は固定（2.1） */
static const clopts_command_t commands[] = {
    {
        .name = "extract",
        .summary = "Create a translation template (.pot) from a Markdown file",
        .description = "Extract translatable text from a Markdown file and write a translation template (.pot).",
        .options = extract_options,
        .option_count = COUNT_OF(extract_options),
        .positionals = extract_positionals,
        .positional_count = COUNT_OF(extract_positionals),
        .min_positionals = COUNT_OF(extract_positionals),
        .max_positionals = COUNT_OF(extract_positionals),
    },
    {
        .name = "init",
        .summary = "Create a new translation file (.po) from a template",
        .description = "Create a new translation file (.po) for a language from a translation template (.pot).",
        .options = init_options,
        .option_count = COUNT_OF(init_options),
        .positionals = init_positionals,
        .positional_count = COUNT_OF(init_positionals),
        .min_positionals = COUNT_OF(init_positionals),
        .max_positionals = COUNT_OF(init_positionals),
    },
    {
        .name = "update",
        .summary = "Merge a new template into an existing translation file",
        .description = "Merge a new translation template (.pot) into an existing translation file (.po), "
                       "keeping existing translations.",
        .options = update_options,
        .option_count = COUNT_OF(update_options),
        .positionals = update_positionals,
        .positional_count = COUNT_OF(update_positionals),
        .min_positionals = COUNT_OF(update_positionals),
        .max_positionals = COUNT_OF(update_positionals),
    },
    {
        .name = "translate",
        .summary = "Create a translated Markdown file from a translation file",
        .description = "Replace translatable text in a Markdown file with translations from a translation file (.po).",
        .options = translate_options,
        .option_count = COUNT_OF(translate_options),
        .positionals = translate_positionals,
        .positional_count = COUNT_OF(translate_positionals),
        .min_positionals = COUNT_OF(translate_positionals),
        .max_positionals = COUNT_OF(translate_positionals),
    },
};

/* ルートコマンド（13.1）: プログラムの説明と 4 つの子コマンドを持ち、オプションを持たない */
static const clopts_command_t root_command = {
    .description = "Translate Markdown documents through gettext .pot/.po files.",
    .commands = commands,
    .command_count = COUNT_OF(commands),
};

/* サブコマンド名とハンドラの対応 */
typedef struct handler_entry
{
    const char *name;
    mnd_cli_handler_fn handler;
} handler_entry_t;

static const handler_entry_t handlers[] = {
    { "extract", mnd_cli_cmd_extract },
    { "init", mnd_cli_cmd_init },
    { "update", mnd_cli_cmd_update },
    { "translate", mnd_cli_cmd_translate },
};

/* 宣言を複製し、version だけを実行時の値に差し替える。ルートコマンドは共有する */
clopts_program_t mnd_cli_program(const char *version)
{
    static const clopts_program_t program_template = {
        .name = "monde",
        .root = &root_command,
    };
    clopts_program_t program = program_template;

    program.version = version;
    return program;
}

mnd_cli_handler_fn mnd_cli_find_handler(const char *name)
{
    size_t i;

    if (name == NULL)
    {
        return NULL;
    }

    for (i = 0; i < COUNT_OF(handlers); i++)
    {
        if (strcmp(handlers[i].name, name) == 0)
        {
            return handlers[i].handler;
        }
    }

    return NULL;
}
