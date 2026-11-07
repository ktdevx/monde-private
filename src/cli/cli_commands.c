/* Commands: プログラムとサブコマンドの宣言のテーブルと、サブコマンド名からハンドラへの対応 */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>
#include <string.h>

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* 全サブコマンドに共通のオプション（2.2、8.7）。--output は省略で標準出力にするため必須にしない（2.3） */
#define OPTION_OUTPUT                          \
    { MND_CLI_OPT_OUTPUT, 'o', MND_ARGS_VALUE, "FILE", \
      "Write the result to FILE (default: standard output)", false, NULL }
#define OPTION_QUIET \
    { MND_CLI_OPT_QUIET, 'q', MND_ARGS_FLAG, NULL, "Do not print statistics or informational messages", false, NULL }
#define OPTION_NO_WRAP \
    { MND_CLI_OPT_NO_WRAP, '\0', MND_ARGS_FLAG, NULL, "Do not wrap long lines in the output", false, NULL }

/* extract: INPUT（4.3〜4.5、4.8） */
static const mnd_args_option_t extract_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { MND_CLI_OPT_FRONT_MATTER_KEYS, '\0', MND_ARGS_LIST, "KEY",
      "Extract front matter values at these key paths", false, NULL },
    { MND_CLI_OPT_PROJECT_ID_VERSION, '\0', MND_ARGS_VALUE, "NAME",
      "Value of the Project-Id-Version header (default: PACKAGE VERSION)", false, NULL },
    { MND_CLI_OPT_MSGID_BUGS_ADDRESS, '\0', MND_ARGS_VALUE, "ADDRESS",
      "Value of the Report-Msgid-Bugs-To header (default: empty)", false, NULL },
    OPTION_NO_WRAP,
};

static const mnd_args_positional_t extract_positionals[] = {
    { "INPUT", "Markdown file to extract from ('-' for standard input)" },
};

/* init: TEMPLATE（5.2、5.9）。--language は環境変数から決められるため必須にしない（5.3） */
static const mnd_args_option_t init_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { MND_CLI_OPT_LANGUAGE, 'l', MND_ARGS_VALUE, "CODE",
      "Language code for the Language header (default: from LC_ALL, LC_MESSAGES or LANG)", false, NULL },
    OPTION_NO_WRAP,
};

static const mnd_args_positional_t init_positionals[] = {
    { "TEMPLATE", "Translation template (.pot) to start from ('-' for standard input)" },
};

/* update: TEMPLATE PO（6.3、6.5） */
static const mnd_args_option_t update_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { MND_CLI_OPT_NO_FUZZY_MATCHING, '\0', MND_ARGS_FLAG, NULL,
      "Do not use fuzzy matching; reuse translations of exact matches only", false, NULL },
    OPTION_NO_WRAP,
};

static const mnd_args_positional_t update_positionals[] = {
    { "TEMPLATE", "New translation template (.pot) ('-' for standard input)" },
    { "PO", "Existing translation file (.po) to merge ('-' for standard input)" },
};

/* translate: INPUT PO（7.3、7.4） */
static const mnd_args_option_t translate_options[] = {
    OPTION_OUTPUT,
    OPTION_QUIET,
    { MND_CLI_OPT_FRONT_MATTER_KEYS, '\0', MND_ARGS_LIST, "KEY",
      "Translate front matter values at these key paths", false, NULL },
    { MND_CLI_OPT_USE_FUZZY, '\0', MND_ARGS_FLAG, NULL,
      "Use non-empty translations of fuzzy entries", false, NULL },
};

static const mnd_args_positional_t translate_positionals[] = {
    { "INPUT", "Markdown file to translate ('-' for standard input)" },
    { "PO", "Translation file (.po) with the translations ('-' for standard input)" },
};

/* 4 つのサブコマンド（1.1）。位置引数はすべて必須で個数は固定（2.1） */
static const mnd_args_command_t commands[] = {
    { "extract", "Create a translation template (.pot) from a Markdown file",
      "Extract translatable text from a Markdown file and write a translation template (.pot).",
      extract_options, COUNT_OF(extract_options), extract_positionals, COUNT_OF(extract_positionals),
      COUNT_OF(extract_positionals), COUNT_OF(extract_positionals) },
    { "init", "Create a new translation file (.po) from a template",
      "Create a new translation file (.po) for a language from a translation template (.pot).",
      init_options, COUNT_OF(init_options), init_positionals, COUNT_OF(init_positionals),
      COUNT_OF(init_positionals), COUNT_OF(init_positionals) },
    { "update", "Merge a new template into an existing translation file",
      "Merge a new translation template (.pot) into an existing translation file (.po), "
      "keeping existing translations.",
      update_options, COUNT_OF(update_options), update_positionals, COUNT_OF(update_positionals),
      COUNT_OF(update_positionals), COUNT_OF(update_positionals) },
    { "translate", "Create a translated Markdown file from a translation file",
      "Replace translatable text in a Markdown file with translations from a translation file (.po).",
      translate_options, COUNT_OF(translate_options), translate_positionals, COUNT_OF(translate_positionals),
      COUNT_OF(translate_positionals), COUNT_OF(translate_positionals) },
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

/* 宣言を複製し、version だけを実行時の値に差し替える */
mnd_args_program_t mnd_cli_program(const char *version)
{
    static const mnd_args_program_t program_template = {
        "monde", "Translate Markdown documents through gettext .pot/.po files.", NULL, commands, COUNT_OF(commands)
    };
    mnd_args_program_t program = program_template;

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
