#include "clopts/errors.h"
#include "clopts_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* 配列を倍増で拡張するときの最初の要素数 */
#define CLOPTS_ERRORS_MIN_CAP 4

void clopts_errors_init(clopts_errors_t *errors)
{
    errors->messages = NULL;
    errors->count = 0;
    errors->cap = 0;
    errors->dropped = false;
}

/* 書式から必要な長さを求めてから確保する（長さの上限なし）。失敗は NULL */
static char *clopts_errors_vformat(const char *format, va_list args)
{
    va_list sizing;
    int needed;
    int written;
    char *message;

    va_copy(sizing, args);
    needed = vsnprintf(NULL, 0, format, sizing);
    va_end(sizing);
    if (needed < 0)
    {
        return NULL;
    }

    message = (char *)malloc((size_t)needed + 1);
    if (message == NULL)
    {
        return NULL;
    }

    written = vsnprintf(message, (size_t)needed + 1, format, args);
    if (written != needed)
    {
        free(message);
        return NULL;
    }

    return message;
}

/* 1 件を追加できる容量を確保する */
static bool clopts_errors_grow(clopts_errors_t *errors)
{
    size_t new_cap;
    char **new_messages;

    if (errors->count < errors->cap)
    {
        return true;
    }

    if (errors->cap == 0)
    {
        new_cap = CLOPTS_ERRORS_MIN_CAP;
    }
    else
    {
        if (errors->cap > (SIZE_MAX / 2 / sizeof(char *)))
        {
            return false;
        }

        new_cap = errors->cap * 2;
    }

    new_messages = (char **)realloc(errors->messages, new_cap * sizeof(char *));
    if (new_messages == NULL)
    {
        return false;
    }

    errors->messages = new_messages;
    errors->cap = new_cap;

    return true;
}

void clopts_errors_add(clopts_errors_t *errors, const char *format, ...)
{
    va_list args;
    char *message;

    va_start(args, format);
    message = clopts_errors_vformat(format, args);
    va_end(args);
    if (message == NULL)
    {
        errors->dropped = true;
        return;
    }

    if (!clopts_errors_grow(errors))
    {
        free(message);
        errors->dropped = true;
        return;
    }

    errors->messages[errors->count] = message;
    errors->count++;
}

void clopts_errors_free(clopts_errors_t *errors)
{
    size_t i;

    for (i = 0; i < errors->count; i++)
    {
        free(errors->messages[i]);
    }

    free(errors->messages);
    clopts_errors_init(errors);
}

size_t clopts_errors_count(const clopts_errors_t *errors)
{
    return errors->count;
}

const char *clopts_errors_at(const clopts_errors_t *errors, size_t index)
{
    if (index >= errors->count)
    {
        return NULL;
    }

    return errors->messages[index];
}

bool clopts_errors_dropped(const clopts_errors_t *errors)
{
    return errors->dropped;
}
