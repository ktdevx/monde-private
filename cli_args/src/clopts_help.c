#include "clopts/help.h"
#include "clopts_buf.h"
#include "clopts_internal.h"

#include <stdlib.h>
#include <string.h>

/* HelpFormatter: 解析結果の経路が指すコマンドのヘルプ、バージョン、使い方の案内、経路の文字列 */

/* 一覧の行の字下げと、左列と説明の間の空白の幅 */
#define HELP_INDENT 2
#define HELP_GAP 2

/* ヘルプとバージョンのオプションの左列と説明 */
#define HELP_OPTION_LEFT "-h, --help"
#define HELP_OPTION_TEXT "Show this help"
#define VERSION_OPTION_LEFT "    --version"
#define VERSION_OPTION_TEXT "Show version information"
#define HELP_COMMAND_NAME "help"
#define HELP_COMMAND_TEXT "Show help for a command"

static bool has_text(const char *text)
{
    return (text != NULL) && (text[0] != '\0');
}

static bool append_spaces(clopts_buf_t *out, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (!clopts_buf_append_byte(out, ' '))
        {
            return false;
        }
    }

    return true;
}

/* 左列の後ろに空白を補い、説明の開始位置を 2 + max_width + 2 バイト目に揃える */
static bool append_pad(clopts_buf_t *out, size_t left_width, size_t max_width)
{
    return append_spaces(out, (max_width - left_width) + HELP_GAP);
}

/* 説明があれば揃えて追加し、行を終える。説明がなければ末尾に空白を出さない */
static bool finish_row(clopts_buf_t *out, size_t left_width, size_t max_width, const char *text)
{
    if (has_text(text))
    {
        if (!append_pad(out, left_width, max_width) || !clopts_buf_append_str(out, text))
        {
            return false;
        }
    }

    return clopts_buf_append_byte(out, '\n');
}

/* 左列が文字列そのものである行 */
static bool append_text_row(clopts_buf_t *out, const char *left, size_t max_width, const char *text)
{
    return append_spaces(out, HELP_INDENT) && clopts_buf_append_str(out, left)
           && finish_row(out, strlen(left), max_width, text);
}

/* 段落を始める。見出しの前に空行を置く */
static bool append_heading(clopts_buf_t *out, const char *heading)
{
    return clopts_buf_append_byte(out, '\n') && clopts_buf_append_str(out, heading)
           && clopts_buf_append_byte(out, '\n');
}

/* 説明の段落（前に空行、後ろに改行）。説明が NULL または空なら何も出さない */
static bool append_description(clopts_buf_t *out, const char *description)
{
    if (!has_text(description))
    {
        return true;
    }

    return clopts_buf_append_byte(out, '\n') && clopts_buf_append_str(out, description)
           && clopts_buf_append_byte(out, '\n');
}

/* オプションの左列: "-c, --name" または "    --name" に、値 " <VALUE>"、リスト値 " <VALUE>[,<VALUE>...]" を続ける */
static size_t option_left_width(const clopts_option_t *option)
{
    size_t width = 4 + 2 + strlen(option->long_name);
    size_t value_len;

    if (option->kind == CLOPTS_FLAG)
    {
        return width;
    }

    value_len = strlen(clopts_value_name(option));
    width += 1 + value_len + 2;
    if (option->kind == CLOPTS_LIST)
    {
        width += 2 + value_len + 2 + 3 + 1;
    }

    return width;
}

static bool append_option_left(clopts_buf_t *out, const clopts_option_t *option)
{
    const char *value_name;
    bool ok;

    if (option->short_name != '\0')
    {
        ok = clopts_buf_append_byte(out, '-') && clopts_buf_append_byte(out, (unsigned char)option->short_name)
             && clopts_buf_append_str(out, ", ");
    }
    else
    {
        ok = append_spaces(out, 4);
    }
    ok = ok && clopts_buf_append_str(out, "--") && clopts_buf_append_str(out, option->long_name);
    if (!ok || (option->kind == CLOPTS_FLAG))
    {
        return ok;
    }

    value_name = clopts_value_name(option);
    ok = clopts_buf_append_str(out, " <") && clopts_buf_append_str(out, value_name) && clopts_buf_append_byte(out, '>');
    if (ok && (option->kind == CLOPTS_LIST))
    {
        ok = clopts_buf_append_str(out, "[,<") && clopts_buf_append_str(out, value_name)
             && clopts_buf_append_str(out, ">...]");
    }

    return ok;
}

/* 説明と注記 " (required)"、" (default: X)"。説明がなければ注記の先頭の空白を出さない */
static bool append_option_text(clopts_buf_t *out, const clopts_option_t *option)
{
    bool has_help = has_text(option->help);
    bool ok = true;

    if (has_help)
    {
        ok = clopts_buf_append_str(out, option->help);
    }
    if (ok && option->required)
    {
        ok = clopts_buf_append_str(out, has_help ? " (required)" : "(required)");
        has_help = true;
    }
    if (ok && (option->default_value != NULL))
    {
        ok = clopts_buf_append_str(out, has_help ? " (default: " : "(default: ")
             && clopts_buf_append_str(out, option->default_value) && clopts_buf_append_byte(out, ')');
    }

    return ok;
}

static bool append_option_row(clopts_buf_t *out, const clopts_option_t *option, size_t max_width)
{
    size_t left_width = option_left_width(option);

    if (!append_spaces(out, HELP_INDENT) || !append_option_left(out, option))
    {
        return false;
    }

    if (has_text(option->help) || option->required || (option->default_value != NULL))
    {
        if (!append_pad(out, left_width, max_width) || !append_option_text(out, option))
        {
            return false;
        }
    }

    return clopts_buf_append_byte(out, '\n');
}

/* 位置引数の使い方: 必須は <NAME>、省略可は [<NAME>]、上限なしの最後は <NAME>... または [<NAME>...] */
static bool append_positional_usage(clopts_buf_t *out, const clopts_command_t *command)
{
    size_t i;

    for (i = 0; i < command->positional_count; i++)
    {
        bool optional = (i >= command->min_positionals);
        bool repeated = (command->max_positionals == CLOPTS_UNLIMITED) && (i + 1 == command->positional_count);
        bool ok = clopts_buf_append_byte(out, ' ');

        ok = ok && (!optional || clopts_buf_append_byte(out, '['));
        ok = ok && clopts_buf_append_byte(out, '<') && clopts_buf_append_str(out, clopts_positional_name(command, i))
             && clopts_buf_append_byte(out, '>');
        ok = ok && (!repeated || clopts_buf_append_str(out, "..."));
        ok = ok && (!optional || clopts_buf_append_byte(out, ']'));
        if (!ok)
        {
            return false;
        }
    }

    return true;
}

/* 選択中のコマンド（経路の最後の段） */
static const clopts_command_t *selected_command(const clopts_result_t *result)
{
    return result->levels[result->level_count - 1].command;
}

/* ヘルプの説明: description、なければ summary */
static const char *command_description(const clopts_command_t *command)
{
    if (command->description != NULL)
    {
        return command->description;
    }

    return command->summary;
}

/* 経路の文字列: プログラム名に、段 1 以降のコマンド名を空白 1 つで区切って続ける */
static bool append_path(const clopts_result_t *result, clopts_buf_t *out)
{
    size_t level;

    if (!clopts_buf_append_str(out, result->program->name))
    {
        return false;
    }

    for (level = 1; level < result->level_count; level++)
    {
        if (!clopts_buf_append_byte(out, ' ')
            || !clopts_buf_append_str(out, result->levels[level].command->name))
        {
            return false;
        }
    }

    return true;
}

/* 一覧の左列の最大幅を更新する */
static size_t wider(size_t max_width, size_t width)
{
    return (width > max_width) ? width : max_width;
}

/* 使い方の行（9.1、9.2、9.3）。子を持つコマンドでは、有効なオプションがあるときだけ経路の直後に " [<options>]" */
static bool append_usage_line(const clopts_result_t *result, clopts_buf_t *out)
{
    const clopts_command_t *command = selected_command(result);

    if (!clopts_buf_append_str(out, "Usage: ") || !append_path(result, out))
    {
        return false;
    }

    if (command->command_count > 0)
    {
        if ((clopts_effective_count(result) > 0) && !clopts_buf_append_str(out, " [<options>]"))
        {
            return false;
        }

        return clopts_buf_append_str(out, " <command> [<options>] [<arguments>]\n");
    }

    return clopts_buf_append_str(out, " [<options>]") && append_positional_usage(out, command)
           && clopts_buf_append_byte(out, '\n');
}

/* Commands: の段落。子コマンドの一覧の末尾に help の行（9.2） */
static bool append_commands_section(const clopts_command_t *command, clopts_buf_t *out)
{
    size_t max_width = strlen(HELP_COMMAND_NAME);
    size_t i;

    for (i = 0; i < command->command_count; i++)
    {
        max_width = wider(max_width, strlen(command->commands[i].name));
    }

    if (!append_heading(out, "Commands:"))
    {
        return false;
    }

    for (i = 0; i < command->command_count; i++)
    {
        const clopts_command_t *child = &command->commands[i];

        if (!append_text_row(out, child->name, max_width, child->summary))
        {
            return false;
        }
    }

    return append_text_row(out, HELP_COMMAND_NAME, max_width, HELP_COMMAND_TEXT);
}

/* Arguments: の段落（9.3）。位置引数がなければ何も出さない */
static bool append_arguments_section(const clopts_command_t *command, clopts_buf_t *out)
{
    size_t max_width = 0;
    size_t i;

    if (command->positional_count == 0)
    {
        return true;
    }

    for (i = 0; i < command->positional_count; i++)
    {
        max_width = wider(max_width, strlen(command->positionals[i].name) + 2);
    }

    if (!append_heading(out, "Arguments:"))
    {
        return false;
    }

    for (i = 0; i < command->positional_count; i++)
    {
        const clopts_positional_t *positional = &command->positionals[i];
        bool ok = append_spaces(out, HELP_INDENT) && clopts_buf_append_byte(out, '<')
                  && clopts_buf_append_str(out, positional->name) && clopts_buf_append_byte(out, '>')
                  && finish_row(out, strlen(positional->name) + 2, max_width, positional->help);

        if (!ok)
        {
            return false;
        }
    }

    return true;
}

/*
 * Options: の段落（9.1、9.4〜9.8）。有効なオプション（そのコマンドのオプション、親から祖先へ向かう順の
 * 引き継いだオプション）、-h, --help、ルートコマンドでは --version の順に並べる
 */
static bool append_options_section(const clopts_result_t *result, clopts_buf_t *out)
{
    size_t option_count = clopts_effective_count(result);
    bool is_root = (result->level_count == 1);
    size_t max_width = strlen(HELP_OPTION_LEFT);
    size_t i;

    for (i = 0; i < option_count; i++)
    {
        max_width = wider(max_width,
                          option_left_width(clopts_option_at(result, clopts_effective_at(result, i))));
    }
    if (is_root)
    {
        max_width = wider(max_width, strlen(VERSION_OPTION_LEFT));
    }

    if (!append_heading(out, "Options:"))
    {
        return false;
    }

    for (i = 0; i < option_count; i++)
    {
        if (!append_option_row(out, clopts_option_at(result, clopts_effective_at(result, i)),
                               max_width))
        {
            return false;
        }
    }

    if (!append_text_row(out, HELP_OPTION_LEFT, max_width, HELP_OPTION_TEXT))
    {
        return false;
    }

    /* --version の行はルートコマンドのヘルプにだけ出す（9.4） */
    return !is_root || append_text_row(out, VERSION_OPTION_LEFT, max_width, VERSION_OPTION_TEXT);
}

/*
 * コマンドのヘルプは 1 つの書式で、要素をコマンドの性質で決める: 子を持つコマンドは Commands: と末尾の案内、
 * 末端コマンドは Arguments:、ルートコマンドは --version の行を持つ
 */
static bool append_help(const clopts_result_t *result, clopts_buf_t *out)
{
    const clopts_command_t *command = selected_command(result);
    bool has_children = (command->command_count > 0);

    if (!append_usage_line(result, out) || !append_description(out, command_description(command)))
    {
        return false;
    }

    if (has_children)
    {
        if (!append_commands_section(command, out))
        {
            return false;
        }
    }
    else if (!append_arguments_section(command, out))
    {
        return false;
    }

    if (!append_options_section(result, out))
    {
        return false;
    }

    if (!has_children)
    {
        return true;
    }

    return clopts_buf_append_str(out, "\nRun '") && append_path(result, out)
           && clopts_buf_append_str(out, " <command> --help' for help on a command.\n");
}

static bool append_version(const clopts_program_t *program, clopts_buf_t *out)
{
    return clopts_buf_append_str(out, program->name) && clopts_buf_append_byte(out, ' ')
           && clopts_buf_append_str(out, program->version) && clopts_buf_append_byte(out, '\n');
}

static bool append_usage_hint(const clopts_result_t *result, clopts_buf_t *out)
{
    return clopts_buf_append_str(out, "Run '") && append_path(result, out)
           && clopts_buf_append_str(out, " --help' for usage.\n");
}

/* 組み立てに成功したら、内容を確保した文字列として返す。失敗（メモリ不足）なら NULL を返す */
static char *finish_text(clopts_buf_t *buf, bool ok)
{
    char *text = NULL;

    if (ok)
    {
        text = clopts_buf_take(buf);
    }

    clopts_buf_free(buf);

    return text;
}

char *clopts_format_help(const clopts_result_t *result)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;

    return finish_text(&buf, append_help(result, &buf));
}

char *clopts_format_version(const clopts_program_t *program)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;

    return finish_text(&buf, append_version(program, &buf));
}

char *clopts_format_usage_hint(const clopts_result_t *result)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;

    return finish_text(&buf, append_usage_hint(result, &buf));
}

char *clopts_format_path(const clopts_result_t *result)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;

    return finish_text(&buf, append_path(result, &buf));
}

void clopts_text_free(char *text)
{
    free(text);
}
