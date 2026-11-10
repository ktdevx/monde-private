#include "cli_args/args_internal.h"

/* DeclCheck: 宣言の妥当性の検査と、子コマンド・有効なオプションの検索と列挙 */

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

/* parent の index 番目の子コマンド名を検査する。誤りの文面はこの時点では改訂前のまま */
static void check_command_name(const mnd_args_command_t *parent, size_t index,
                               mnd_diag_list_t *diags, bool *ok)
{
    const char *name = parent->commands[index].name;
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
        if (names_equal(parent->commands[j].name, name))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                         DECL_PREFIX "duplicate command '%s'", name);
            *ok = false;
            return;
        }
    }
}

/* cmd はメッセージ上のコマンドの名前（ルートコマンドではプログラム名） */
static void check_option(const mnd_args_command_t *command, const char *cmd, size_t index,
                         mnd_diag_list_t *diags, bool *ok)
{
    const mnd_args_option_t *option = &command->options[index];
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

static void check_positionals(const mnd_args_command_t *command, const char *cmd,
                              mnd_diag_list_t *diags, bool *ok)
{
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

/* 1 つのコマンドのオプションと位置引数を検査する */
static void check_command(const mnd_args_command_t *command, const char *cmd,
                          mnd_diag_list_t *diags, bool *ok)
{
    size_t k;

    for (k = 0; k < command->option_count; ++k)
    {
        check_option(command, cmd, k, diags, ok);
    }

    check_positionals(command, cmd, diags, ok);
}

bool mnd_args_decl_check(const mnd_args_program_t *program, mnd_diag_list_t *diags)
{
    const mnd_args_command_t *root = program->root;
    bool ok = true;
    size_t i;

    if (is_empty(program->name) || is_empty(program->version))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     DECL_PREFIX "program name and version are required");
        ok = false;
    }

    if (root == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, DECL_PREFIX "root command is required");
        return false;
    }

    /* ルートコマンド自身にも同じ規則を適用する。子を持たないルートコマンドも妥当な宣言である（1.4） */
    check_command(root, shown(program->name), diags, &ok);

    for (i = 0; i < root->command_count; ++i)
    {
        const mnd_args_command_t *command = &root->commands[i];

        check_command_name(root, i, diags, &ok);
        check_command(command, shown(command->name), diags, &ok);
    }

    return ok;
}

const mnd_args_command_t *mnd_args_find_child(const mnd_args_command_t *parent,
                                              const char *name, size_t name_len)
{
    size_t i;

    for (i = 0; i < parent->command_count; ++i)
    {
        if (name_matches(parent->commands[i].name, name, name_len))
        {
            return &parent->commands[i];
        }
    }

    return NULL;
}

/*
 * 有効なオプションのうち、順序で skip 個を飛ばした次の 1 つを探す。match が NULL でなければ、
 * match が true を返す最初の 1 つを探す（skip は 0 で呼ぶ）。
 * 順序は、最後の段のすべてのオプション、続いて親から祖先へ向かって引き継ぐオプション（12.1、12.2）
 */
typedef bool (*mnd_args_option_match_t)(const mnd_args_option_t *option, const void *key);

static bool find_effective(const mnd_args_result_t *result, size_t skip,
                           mnd_args_option_match_t match, const void *key, mnd_args_ref_t *ref)
{
    size_t level;

    if (result->level_count == 0)
    {
        return false;
    }

    level = result->level_count;
    while (level > 0)
    {
        const mnd_args_command_t *command;
        bool last;
        size_t i;

        --level;
        command = result->levels[level].command;
        last = (level + 1 == result->level_count);

        for (i = 0; i < command->option_count; ++i)
        {
            const mnd_args_option_t *option = &command->options[i];

            if (!last && !option->inherited)
            {
                continue;
            }

            if ((match != NULL) && !match(option, key))
            {
                continue;
            }

            if (skip > 0)
            {
                --skip;
                continue;
            }

            ref->level = level;
            ref->index = i;
            return true;
        }
    }

    return false;
}

/* 長い名前の照合に使う、name_len バイトの名前 */
typedef struct mnd_args_name_key
{
    const char *name;
    size_t len;
} mnd_args_name_key_t;

static bool match_long(const mnd_args_option_t *option, const void *key)
{
    const mnd_args_name_key_t *name = (const mnd_args_name_key_t *)key;

    return name_matches(option->long_name, name->name, name->len);
}

static bool match_short(const mnd_args_option_t *option, const void *key)
{
    return option->short_name == *(const char *)key;
}

bool mnd_args_find_long(const mnd_args_result_t *result, const char *name, size_t name_len,
                        mnd_args_ref_t *ref)
{
    mnd_args_name_key_t key;

    key.name = name;
    key.len = name_len;

    return find_effective(result, 0, match_long, &key, ref);
}

bool mnd_args_find_short(const mnd_args_result_t *result, char short_name, mnd_args_ref_t *ref)
{
    if (short_name == '\0')
    {
        return false;
    }

    return find_effective(result, 0, match_short, &short_name, ref);
}

size_t mnd_args_effective_count(const mnd_args_result_t *result)
{
    size_t count = 0;
    size_t level;
    size_t i;

    if (result->level_count == 0)
    {
        return 0;
    }

    for (level = 0; level < result->level_count; ++level)
    {
        const mnd_args_command_t *command = result->levels[level].command;

        if (level + 1 == result->level_count)
        {
            count += command->option_count;
            continue;
        }

        for (i = 0; i < command->option_count; ++i)
        {
            if (command->options[i].inherited)
            {
                ++count;
            }
        }
    }

    return count;
}

mnd_args_ref_t mnd_args_effective_at(const mnd_args_result_t *result, size_t i)
{
    mnd_args_ref_t ref;

    ref.level = 0;
    ref.index = 0;
    (void)find_effective(result, i, NULL, NULL, &ref);

    return ref;
}

const mnd_args_option_t *mnd_args_option_at(const mnd_args_result_t *result, mnd_args_ref_t ref)
{
    return &result->levels[ref.level].command->options[ref.index];
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
