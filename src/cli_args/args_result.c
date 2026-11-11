#include "cli_args/args.h"
#include "cli_args/args_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ResultStore: 解析結果の保持と取得 */

#define MND_ARGS_MIN_CAP 4

/* 要素の大きさが elem_size の配列を need 個以上に広げる容量を求める。オーバーフローなら false */
static bool mnd_args_next_cap(size_t cap, size_t need, size_t elem_size, size_t *out_cap)
{
    size_t new_cap = (cap != 0) ? cap : MND_ARGS_MIN_CAP;

    while (new_cap < need)
    {
        if (new_cap > (SIZE_MAX / 2))
        {
            new_cap = need;
            break;
        }

        new_cap *= 2;
    }

    if (new_cap > (SIZE_MAX / elem_size))
    {
        return false;
    }

    *out_cap = new_cap;

    return true;
}

static void mnd_args_value_free(mnd_args_value_t *value)
{
    size_t i;

    for (i = 0; i < value->item_count; i++)
    {
        free(value->items[i]);
    }

    free(value->items);
    value->explicit_set = false;
    value->value = NULL;
    value->items = NULL;
    value->item_count = 0;
    value->item_cap = 0;
}

/* 段の値の配列を解放する */
static void mnd_args_level_free(mnd_args_level_t *level)
{
    size_t i;

    if ((level->values != NULL) && (level->command != NULL))
    {
        for (i = 0; i < level->command->option_count; i++)
        {
            mnd_args_value_free(&level->values[i]);
        }
    }

    free(level->values);
    level->values = NULL;
    level->command = NULL;
}

static mnd_args_value_t *mnd_args_value_ref(mnd_args_result_t *result, mnd_args_ref_t ref)
{
    return &result->levels[ref.level].values[ref.index];
}

void mnd_args_result_init(mnd_args_result_t *result)
{
    result->status = MND_ARGS_NO_COMMAND;
    result->program = NULL;
    result->levels = NULL;
    result->level_count = 0;
    result->level_cap = 0;
    result->positionals = NULL;
    result->positional_count = 0;
    result->positional_cap = 0;
}

void mnd_args_result_free(mnd_args_result_t *result)
{
    mnd_args_result_truncate(result, 0);
    free(result->levels);
    free((void *)result->positionals);
    mnd_args_result_init(result);
}

bool mnd_args_result_push(mnd_args_result_t *result, const mnd_args_command_t *command)
{
    mnd_args_value_t *values = NULL;
    size_t count = command->option_count;
    mnd_args_level_t *level;

    if (result->level_count >= result->level_cap)
    {
        size_t new_cap;
        mnd_args_level_t *grown;

        if (result->level_count == SIZE_MAX)
        {
            return false;
        }

        if (!mnd_args_next_cap(result->level_cap, result->level_count + 1, sizeof(mnd_args_level_t),
                               &new_cap))
        {
            return false;
        }

        grown = (mnd_args_level_t *)realloc(result->levels, new_cap * sizeof(mnd_args_level_t));
        if (grown == NULL)
        {
            return false;
        }

        result->levels = grown;
        result->level_cap = new_cap;
    }

    if (count > 0)
    {
        if (count > (SIZE_MAX / sizeof(mnd_args_value_t)))
        {
            return false;
        }

        values = (mnd_args_value_t *)calloc(count, sizeof(mnd_args_value_t));
        if (values == NULL)
        {
            return false;
        }
    }

    level = &result->levels[result->level_count];
    level->command = command;
    level->values = values;
    result->level_count++;

    return true;
}

void mnd_args_result_truncate(mnd_args_result_t *result, size_t level_count)
{
    while (result->level_count > level_count)
    {
        result->level_count--;
        mnd_args_level_free(&result->levels[result->level_count]);
    }
}

void mnd_args_result_set_flag(mnd_args_result_t *result, mnd_args_ref_t ref)
{
    mnd_args_value_ref(result, ref)->explicit_set = true;
}

void mnd_args_result_set_value(mnd_args_result_t *result, mnd_args_ref_t ref, const char *value)
{
    mnd_args_value_t *target = mnd_args_value_ref(result, ref);

    target->explicit_set = true;
    target->value = value;
}

mnd_args_status_t mnd_args_result_append_list(mnd_args_result_t *result, mnd_args_ref_t ref,
                                              const char *value, bool explicit_set)
{
    mnd_args_value_t *target = mnd_args_value_ref(result, ref);
    const char *p;
    const char *start;
    size_t new_items = 1;
    size_t need;
    size_t added = 0;

    /* 先に項目を数え、空の項目がないことを確かめる。この段階では何も変えない */
    start = value;
    for (p = value; ; p++)
    {
        if ((*p == ',') || (*p == '\0'))
        {
            if (p == start)
            {
                return MND_ARGS_USAGE_ERROR;
            }

            if (*p == '\0')
            {
                break;
            }

            new_items++;
            start = p + 1;
        }
    }

    if (new_items > (SIZE_MAX - target->item_count))
    {
        return MND_ARGS_OUT_OF_MEMORY;
    }

    need = target->item_count + new_items;
    if (need > target->item_cap)
    {
        size_t new_cap;
        char **grown;

        if (!mnd_args_next_cap(target->item_cap, need, sizeof(char *), &new_cap))
        {
            return MND_ARGS_OUT_OF_MEMORY;
        }

        grown = (char **)realloc(target->items, new_cap * sizeof(char *));
        if (grown == NULL)
        {
            return MND_ARGS_OUT_OF_MEMORY;
        }

        target->items = grown;
        target->item_cap = new_cap;
    }

    /* 項目をコピーする。途中で確保に失敗したら、この呼び出しで加えた項目を取り消す */
    start = value;
    for (p = value; ; p++)
    {
        if ((*p == ',') || (*p == '\0'))
        {
            size_t len = (size_t)(p - start);
            char *item = (char *)malloc(len + 1);

            if (item == NULL)
            {
                while (added > 0)
                {
                    added--;
                    free(target->items[target->item_count + added]);
                }

                return MND_ARGS_OUT_OF_MEMORY;
            }

            memcpy(item, start, len);
            item[len] = '\0';
            target->items[target->item_count + added] = item;
            added++;

            if (*p == '\0')
            {
                break;
            }

            start = p + 1;
        }
    }

    target->item_count += added;
    if (explicit_set)
    {
        target->explicit_set = true;
    }

    return MND_ARGS_OK;
}

bool mnd_args_result_add_positional(mnd_args_result_t *result, const char *arg)
{
    if (result->positional_count >= result->positional_cap)
    {
        size_t new_cap;
        const char **grown;

        if (result->positional_count == SIZE_MAX)
        {
            return false;
        }

        if (!mnd_args_next_cap(result->positional_cap, result->positional_count + 1,
                               sizeof(const char *), &new_cap))
        {
            return false;
        }

        grown = (const char **)realloc((void *)result->positionals, new_cap * sizeof(const char *));
        if (grown == NULL)
        {
            return false;
        }

        result->positionals = grown;
        result->positional_cap = new_cap;
    }

    result->positionals[result->positional_count] = arg;
    result->positional_count++;

    return true;
}

mnd_args_status_t mnd_args_result_apply_defaults(mnd_args_result_t *result)
{
    mnd_args_ref_t ref;

    /* 経路上のすべての段で、明示的に指定されていないオプションに既定値を適用する（5.6、12.6） */
    for (ref.level = 0; ref.level < result->level_count; ref.level++)
    {
        const mnd_args_level_t *level = &result->levels[ref.level];
        const mnd_args_command_t *command = level->command;

        if (level->values == NULL)
        {
            continue;
        }

        for (ref.index = 0; ref.index < command->option_count; ref.index++)
        {
            const mnd_args_option_t *option = &command->options[ref.index];
            mnd_args_value_t *value = &level->values[ref.index];

            if ((value->explicit_set) || (option->default_value == NULL))
            {
                continue;
            }

            if (option->kind == MND_ARGS_VALUE)
            {
                value->value = option->default_value;
            }
            else if (option->kind == MND_ARGS_LIST)
            {
                mnd_args_status_t status;

                if (value->item_count > 0)
                {
                    continue;
                }

                status = mnd_args_result_append_list(result, ref, option->default_value, false);
                if (status != MND_ARGS_OK)
                {
                    return status;
                }
            }
        }
    }

    return MND_ARGS_OK;
}

const mnd_args_command_t *mnd_args_command(const mnd_args_result_t *result)
{
    if (result->level_count == 0)
    {
        return NULL;
    }

    return result->levels[result->level_count - 1].command;
}

size_t mnd_args_depth(const mnd_args_result_t *result)
{
    return result->level_count;
}

const mnd_args_command_t *mnd_args_command_at(const mnd_args_result_t *result, size_t level)
{
    if (level >= result->level_count)
    {
        return NULL;
    }

    return result->levels[level].command;
}

/*
 * long_name の値を返す。最後の段から段 0 へ向かって、各段のコマンドが宣言したすべてのオプション
 * （引き継ぐかどうかによらない）を探す（5.7）。宣言されていなければ NULL
 */
static const mnd_args_value_t *mnd_args_lookup(const mnd_args_result_t *result, const char *long_name)
{
    size_t level;

    if ((result == NULL) || (long_name == NULL))
    {
        return NULL;
    }

    level = result->level_count;
    while (level > 0)
    {
        const mnd_args_level_t *entry;
        size_t i;

        level--;
        entry = &result->levels[level];
        if (entry->values == NULL)
        {
            continue;
        }

        for (i = 0; i < entry->command->option_count; i++)
        {
            const char *name = entry->command->options[i].long_name;

            if ((name != NULL) && (strcmp(name, long_name) == 0))
            {
                return &entry->values[i];
            }
        }
    }

    return NULL;
}

bool mnd_args_is_set(const mnd_args_result_t *result, const char *long_name)
{
    const mnd_args_value_t *value = mnd_args_lookup(result, long_name);

    return (value != NULL) && (value->explicit_set);
}

bool mnd_args_flag(const mnd_args_result_t *result, const char *long_name)
{
    const mnd_args_value_t *value = mnd_args_lookup(result, long_name);

    return (value != NULL) && (value->explicit_set);
}

const char *mnd_args_value(const mnd_args_result_t *result, const char *long_name)
{
    const mnd_args_value_t *value = mnd_args_lookup(result, long_name);

    return (value != NULL) ? value->value : NULL;
}

size_t mnd_args_list_count(const mnd_args_result_t *result, const char *long_name)
{
    const mnd_args_value_t *value = mnd_args_lookup(result, long_name);

    return (value != NULL) ? value->item_count : 0;
}

const char *mnd_args_list_at(const mnd_args_result_t *result, const char *long_name, size_t index)
{
    const mnd_args_value_t *value = mnd_args_lookup(result, long_name);

    if ((value == NULL) || (index >= value->item_count))
    {
        return NULL;
    }

    return value->items[index];
}

size_t mnd_args_positional_count(const mnd_args_result_t *result)
{
    return result->positional_count;
}

const char *mnd_args_positional_at(const mnd_args_result_t *result, size_t index)
{
    if (index >= result->positional_count)
    {
        return NULL;
    }

    return result->positionals[index];
}
