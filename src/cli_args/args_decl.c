#include "cli_args/args_internal.h"

/* DeclCheck: 宣言の妥当性の検査と、サブコマンド・オプションの検索 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define DECL_PREFIX "invalid argument declaration: "

/* メッセージ上の名前。NULL は空文字列として示す */
static const char *shown(const char *name)
{
    if (name == NULL)
    {
        return "";
    }

    return name;
}

static bool is_empty(const char *s)
{
    return (s == NULL) || (s[0] == '\0');
}

static bool is_ascii_alnum(char c)
{
    return ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

static bool names_equal(const char *a, const char *b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

/* name が name_len バイトの列 bytes とバイト単位で完全に一致するなら true */
static bool name_matches(const char *name, const char *bytes, size_t name_len)
{
    if (name == NULL)
    {
        return false;
    }

    return (strlen(name) == name_len) && (memcmp(name, bytes, name_len) == 0);
}

/* カンマ区切りのリストに空の項目（空文字列、先頭・末尾のカンマ、",,"）があるなら true */
static bool has_empty_item(const char *list)
{
    const char *p;
    size_t item_len = 0;

    for (p = list; *p != '\0'; ++p)
    {
        if (*p == ',')
        {
            if (item_len == 0)
            {
                return true;
            }
            item_len = 0;
        }
        else
        {
            ++item_len;
        }
    }

    return item_len == 0;
}

static void check_command_name(const mnd_args_program_t *program, size_t index,
                                    mnd_diag_list_t *diags, bool *ok)
{
    const char *name = program->commands[index].name;
    size_t j;

    if (is_empty(name) || (name[0] == '-'))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "invalid command name '%s'", shown(name));
        *ok = false;

        return;
    }

    if (strcmp(name, "help") == 0)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "command name 'help' is reserved");
        *ok = false;

        return;
    }

    for (j = 0; j < index; ++j)
    {
        if (names_equal(program->commands[j].name, name))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                         DECL_PREFIX "duplicate command '%s'", name);
            *ok = false;

            return;
        }
    }
}

static void check_option(const mnd_args_command_t *command, size_t index,
                         mnd_diag_list_t *diags, bool *ok)
{
    const mnd_args_option_t *option = &command->options[index];
    const char *cmd = shown(command->name);
    const char *name = option->long_name;
    size_t j;

    if (is_empty(name) || (name[0] == '-') || (strchr(name, '=') != NULL))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "invalid option name '%s' in command '%s'", shown(name), cmd);
        *ok = false;
    }
    else if (strcmp(name, "help") == 0)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "option '--help' is reserved in command '%s'", cmd);
        *ok = false;
    }
    else
    {
        for (j = 0; j < index; ++j)
        {
            if (names_equal(command->options[j].long_name, name))
            {
                mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                             DECL_PREFIX "duplicate option '--%s' in command '%s'", name, cmd);
                *ok = false;
                break;
            }
        }
    }

    if (option->short_name == 'h')
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "option '-h' is reserved in command '%s'", cmd);
        *ok = false;
    }
    else if ((option->short_name != '\0') && !is_ascii_alnum(option->short_name))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "invalid short option for '--%s' in command '%s'",
                     shown(name), cmd);
        *ok = false;
    }
    else if (option->short_name != '\0')
    {
        for (j = 0; j < index; ++j)
        {
            if (command->options[j].short_name == option->short_name)
            {
                mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                             DECL_PREFIX "duplicate option '-%c' in command '%s'",
                             option->short_name, cmd);
                *ok = false;
                break;
            }
        }
    }

    if (option->kind == MND_ARGS_FLAG)
    {
        if (option->required || (option->default_value != NULL))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                         DECL_PREFIX "flag '--%s' cannot be required or have a default in command '%s'",
                         shown(name), cmd);
            *ok = false;
        }

        return;
    }

    if (option->required && (option->default_value != NULL))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "option '--%s' cannot be both required and defaulted in command '%s'",
                     shown(name), cmd);
        *ok = false;
    }

    if ((option->kind == MND_ARGS_LIST) && (option->default_value != NULL)
        && has_empty_item(option->default_value))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "default of '--%s' has an empty item in command '%s'",
                     shown(name), cmd);
        *ok = false;
    }
}

static void check_positionals(const mnd_args_command_t *command, mnd_diag_list_t *diags, bool *ok)
{
    const char *cmd = shown(command->name);
    bool names_match;

    if (command->min_positionals > command->max_positionals)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "invalid positional range in command '%s'", cmd);
        *ok = false;
    }

    if (command->max_positionals == MND_ARGS_UNLIMITED)
    {
        names_match = (command->positional_count >= 1);
    }
    else
    {
        names_match = (command->positional_count == command->max_positionals);
    }

    if (!names_match)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "positional names do not match the range in command '%s'", cmd);
        *ok = false;
    }
}

bool mnd_args_decl_check(const mnd_args_program_t *program, mnd_diag_list_t *diags)
{
    bool ok = true;
    size_t i;
    size_t k;

    if (is_empty(program->name) || is_empty(program->version))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "program name and version are required");
        ok = false;
    }

    for (i = 0; i < program->command_count; ++i)
    {
        const mnd_args_command_t *command = &program->commands[i];

        check_command_name(program, i, diags, &ok);
        for (k = 0; k < command->option_count; ++k)
        {
            check_option(command, k, diags, &ok);
        }
        check_positionals(command, diags, &ok);
    }

    return ok;
}

const mnd_args_command_t *mnd_args_find_command(const mnd_args_program_t *program,
                                                const char *name, size_t name_len)
{
    size_t i;

    for (i = 0; i < program->command_count; ++i)
    {
        if (name_matches(program->commands[i].name, name, name_len))
        {
            return &program->commands[i];
        }
    }

    return NULL;
}

size_t mnd_args_find_long(const mnd_args_command_t *command, const char *name, size_t name_len)
{
    size_t i;

    for (i = 0; i < command->option_count; ++i)
    {
        if (name_matches(command->options[i].long_name, name, name_len))
        {
            return i;
        }
    }

    return (size_t)-1;
}

size_t mnd_args_find_short(const mnd_args_command_t *command, char short_name)
{
    size_t i;

    if (short_name == '\0')
    {
        return (size_t)-1;
    }

    for (i = 0; i < command->option_count; ++i)
    {
        if (command->options[i].short_name == short_name)
        {
            return i;
        }
    }

    return (size_t)-1;
}

const char *mnd_args_value_name(const mnd_args_option_t *option)
{
    if (option->value_name == NULL)
    {
        return "VALUE";
    }

    return option->value_name;
}

const char *mnd_args_positional_name(const mnd_args_command_t *command, size_t index)
{
    if (command->positional_count == 0)
    {
        return NULL;
    }

    if (index >= command->positional_count)
    {
        index = command->positional_count - 1;
    }

    return command->positionals[index].name;
}
