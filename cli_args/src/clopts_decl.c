#include "clopts_internal.h"
#include "clopts_buf.h"

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

/*
 * 検査中のコマンドの木の 1 段。ルートコマンドから現在のコマンドまでの祖先の列を、
 * parent をたどる連結リストとして持つ（祖先との重複と循環の検出に使う）
 */
typedef struct decl_frame decl_frame_t;

struct decl_frame
{
    decl_frame_t *parent;              /* ルートコマンドでは NULL */
    const mnd_args_command_t *command;
    const char *name;                  /* 経路上の名前。ルートコマンドではプログラム名 */
    clopts_buf_t path;                 /* 経路の文字列。最初の誤りの報告のときに作る */
    bool path_ready;
    bool path_failed;                  /* 経路の文字列を作れなかった（メモリ不足） */
};

typedef struct decl_check
{
    clopts_errors_t *errors;
    bool ok;
} decl_check_t;

static void frame_init(decl_frame_t *frame, decl_frame_t *parent,
                       const mnd_args_command_t *command, const char *name)
{
    frame->parent = parent;
    frame->command = command;
    frame->name = name;
    frame->path.data = NULL;
    frame->path.len = 0;
    frame->path.cap = 0;
    frame->path_ready = false;
    frame->path_failed = false;
}

static bool append_path(clopts_buf_t *buf, const decl_frame_t *frame)
{
    if (frame->parent != NULL)
    {
        if (!append_path(buf, frame->parent) || !clopts_buf_append_byte(buf, ' '))
        {
            return false;
        }
    }

    return clopts_buf_append_str(buf, shown(frame->name));
}

/*
 * frame の経路の文字列（例: "nest remote"）。各段の名前を空白で区切る。
 * メモリ不足で作れなければ、代わりにコマンド自身の名前を返す
 */
static const char *frame_path(decl_frame_t *frame)
{
    if (!frame->path_ready)
    {
        frame->path_ready = true;
        if (!append_path(&frame->path, frame))
        {
            clopts_buf_free(&frame->path);
            frame->path_failed = true;
        }
    }

    if (frame->path_failed)
    {
        return shown(frame->name);
    }

    return clopts_buf_cstr(&frame->path);
}

/* frame の祖先（frame 自身を除く）で、長い名前が name のオプションを持つ最も近い段。なければ NULL */
static decl_frame_t *find_ancestor_long(const decl_frame_t *frame, const char *name)
{
    decl_frame_t *ancestor;
    size_t i;

    for (ancestor = frame->parent; ancestor != NULL; ancestor = ancestor->parent)
    {
        for (i = 0; i < ancestor->command->option_count; ++i)
        {
            if (names_equal(ancestor->command->options[i].long_name, name))
            {
                return ancestor;
            }
        }
    }

    return NULL;
}

/* frame の祖先（frame 自身を除く）で、短い名前が short_name のオプションを持つ最も近い段。なければ NULL */
static decl_frame_t *find_ancestor_short(const decl_frame_t *frame, char short_name)
{
    decl_frame_t *ancestor;
    size_t i;

    for (ancestor = frame->parent; ancestor != NULL; ancestor = ancestor->parent)
    {
        for (i = 0; i < ancestor->command->option_count; ++i)
        {
            if (ancestor->command->options[i].short_name == short_name)
            {
                return ancestor;
            }
        }
    }

    return NULL;
}

/* frame のコマンドの index 番目の子コマンド名を検査する */
static void check_command_name(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const mnd_args_command_t *parent = frame->command;
    const char *name = parent->commands[index].name;
    size_t j;

    if (is_empty(name) || (name[0] == '-'))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid command name '%s' in command '%s'", shown(name),
                          frame_path(frame));
        check->ok = false;
        return;
    }

    if (strcmp(name, "help") == 0)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "command name 'help' is reserved in command '%s'",
                          frame_path(frame));
        check->ok = false;
        return;
    }

    for (j = 0; j < index; ++j)
    {
        if (names_equal(parent->commands[j].name, name))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "duplicate command '%s' in command '%s'", name,
                              frame_path(frame));
            check->ok = false;
            return;
        }
    }
}

/* frame のコマンドの index 番目のオプションの長い名前を検査する */
static void check_long_name(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const mnd_args_command_t *command = frame->command;
    const char *name = command->options[index].long_name;
    decl_frame_t *ancestor;
    size_t j;

    if (is_empty(name) || (name[0] == '-') || (strchr(name, '=') != NULL))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid option name '%s' in command '%s'", shown(name),
                          frame_path(frame));
        check->ok = false;
        return;
    }

    if ((strcmp(name, "help") == 0) || (strcmp(name, "version") == 0))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' is reserved in command '%s'", name,
                          frame_path(frame));
        check->ok = false;
        return;
    }

    for (j = 0; j < index; ++j)
    {
        if (names_equal(command->options[j].long_name, name))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "duplicate option '--%s' in command '%s'", name,
                              frame_path(frame));
            check->ok = false;
            return;
        }
    }

    /* 祖先のオプションとは、引き継ぐかどうかによらず重複できない（1.8） */
    ancestor = find_ancestor_long(frame, name);
    if (ancestor != NULL)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' in command '%s' duplicates an option of command '%s'",
                          name, frame_path(frame), frame_path(ancestor));
        check->ok = false;
    }
}

/* frame のコマンドの index 番目のオプションの短い名前を検査する */
static void check_short_name(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const mnd_args_command_t *command = frame->command;
    const mnd_args_option_t *option = &command->options[index];
    char short_name = option->short_name;
    decl_frame_t *ancestor;
    size_t j;

    if (short_name == '\0')
    {
        return;
    }

    if (short_name == 'h')
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '-h' is reserved in command '%s'", frame_path(frame));
        check->ok = false;
        return;
    }

    if (!is_ascii_alnum(short_name))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid short option for '--%s' in command '%s'",
                          shown(option->long_name), frame_path(frame));
        check->ok = false;
        return;
    }

    for (j = 0; j < index; ++j)
    {
        if (command->options[j].short_name == short_name)
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "duplicate option '-%c' in command '%s'", short_name,
                              frame_path(frame));
            check->ok = false;
            return;
        }
    }

    ancestor = find_ancestor_short(frame, short_name);
    if (ancestor != NULL)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '-%c' in command '%s' duplicates an option of command '%s'",
                          short_name, frame_path(frame), frame_path(ancestor));
        check->ok = false;
    }
}

/* frame のコマンドの index 番目のオプションを検査する */
static void check_option(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const mnd_args_option_t *option = &frame->command->options[index];
    const char *name = option->long_name;

    check_long_name(check, frame, index);
    check_short_name(check, frame, index);

    if (option->kind == MND_ARGS_FLAG)
    {
        if (option->required || (option->default_value != NULL))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "flag '--%s' cannot be required or have a default in command '%s'",
                              shown(name), frame_path(frame));
            check->ok = false;
        }

        return;
    }

    if (option->required && (option->default_value != NULL))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' cannot be both required and defaulted in command '%s'",
                          shown(name), frame_path(frame));
        check->ok = false;
    }

    if ((option->kind == MND_ARGS_LIST) && (option->default_value != NULL)
        && has_empty_item(option->default_value))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "default of '--%s' has an empty item in command '%s'",
                          shown(name), frame_path(frame));
        check->ok = false;
    }
}

static void check_positionals(decl_check_t *check, decl_frame_t *frame)
{
    const mnd_args_command_t *command = frame->command;
    bool names_match;

    /* 子を持つコマンドは位置引数を持たない（1.6）。範囲の規則は末端コマンドだけに適用する */
    if (command->command_count > 0)
    {
        if ((command->positional_count != 0) || (command->min_positionals != 0)
            || (command->max_positionals != 0))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "command '%s' cannot have both subcommands and positional arguments",
                              frame_path(frame));
            check->ok = false;
        }

        return;
    }

    if (command->min_positionals > command->max_positionals)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid positional range in command '%s'", frame_path(frame));
        check->ok = false;
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
        clopts_errors_add(check->errors,
                          DECL_PREFIX "positional names do not match the range in command '%s'",
                          frame_path(frame));
        check->ok = false;
    }
}

/*
 * frame のコマンドを検査し、子コマンドを深さ優先でたどる（深さに上限はない）。
 * 祖先と同じ宣言の子コマンドは循環として報告し、その先をたどらない
 */
static void check_tree(decl_check_t *check, decl_frame_t *frame)
{
    const mnd_args_command_t *command = frame->command;
    size_t i;

    for (i = 0; i < command->option_count; ++i)
    {
        check_option(check, frame, i);
    }

    check_positionals(check, frame);

    for (i = 0; i < command->command_count; ++i)
    {
        const mnd_args_command_t *child = &command->commands[i];
        decl_frame_t *ancestor;
        decl_frame_t child_frame;

        check_command_name(check, frame, i);

        for (ancestor = frame; ancestor != NULL; ancestor = ancestor->parent)
        {
            if (ancestor->command == child)
            {
                break;
            }
        }

        if (ancestor != NULL)
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "command '%s' contains itself", frame_path(ancestor));
            check->ok = false;
            continue;
        }

        frame_init(&child_frame, frame, child, child->name);
        check_tree(check, &child_frame);
        clopts_buf_free(&child_frame.path);
    }
}

bool mnd_args_decl_check(const mnd_args_program_t *program, clopts_errors_t *errors)
{
    decl_check_t check;
    decl_frame_t root;

    check.errors = errors;
    check.ok = true;

    if (is_empty(program->name) || is_empty(program->version))
    {
        clopts_errors_add(errors,
                          DECL_PREFIX "program name and version are required");
        check.ok = false;
    }

    if (program->root == NULL)
    {
        clopts_errors_add(errors, DECL_PREFIX "root command is required");
        return false;
    }

    /* ルートコマンドの経路上の名前はプログラム名。子を持たないルートコマンドも妥当な宣言である（1.4） */
    frame_init(&root, NULL, program->root, program->name);
    check_tree(&check, &root);
    clopts_buf_free(&root.path);

    return check.ok;
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
