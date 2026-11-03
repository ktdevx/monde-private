#include "foundation/diag.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 配列を倍増で拡張するときの最初の要素数 */
#define MND_DIAG_MIN_CAP 4

void mnd_diag_list_init(mnd_diag_list *list)
{
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
    list->dropped = false;
}

static char *mnd_diag_strdup(const char *s)
{
    size_t len;
    char *copy;

    len = strlen(s);
    copy = (char *)malloc(len + 1);
    if (copy != NULL)
    {
        memcpy(copy, s, len + 1);
    }

    return copy;
}

/* 書式から必要な長さを求めてから確保する（長さの上限なし） */
static char *mnd_diag_vformat(const char *format, va_list args)
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

static bool mnd_diag_list_grow(mnd_diag_list *list)
{
    size_t new_cap;
    mnd_diag *new_items;

    if (list->count < list->cap)
    {
        return true;
    }

    if (list->cap == 0)
    {
        new_cap = MND_DIAG_MIN_CAP;
    }
    else
    {
        if (list->cap > (SIZE_MAX / 2 / sizeof(mnd_diag)))
        {
            return false;
        }

        new_cap = list->cap * 2;
    }

    new_items = (mnd_diag *)realloc(list->items, new_cap * sizeof(mnd_diag));
    if (new_items == NULL)
    {
        return false;
    }

    list->items = new_items;
    list->cap = new_cap;

    return true;
}

void mnd_diag_add(mnd_diag_list *list, mnd_severity severity,
                  const char *path, size_t line, const char *format, ...)
{
    va_list args;
    char *message;
    char *path_copy = NULL;
    mnd_diag *d;

    va_start(args, format);
    message = mnd_diag_vformat(format, args);
    va_end(args);
    if (message == NULL)
    {
        list->dropped = true;
        return;
    }

    if (path != NULL)
    {
        path_copy = mnd_diag_strdup(path);
        if (path_copy == NULL)
        {
            free(message);
            list->dropped = true;
            return;
        }
    }

    if (!mnd_diag_list_grow(list))
    {
        free(path_copy);
        free(message);
        list->dropped = true;
        return;
    }

    d = &list->items[list->count];
    d->severity = severity;
    d->message = message;
    d->path = path_copy;
    d->line = line;
    list->count++;
}

void mnd_diag_list_free(mnd_diag_list *list)
{
    size_t i;

    for (i = 0; i < list->count; i++)
    {
        free(list->items[i].message);
        free(list->items[i].path);
    }

    free(list->items);
    mnd_diag_list_init(list);
}

size_t mnd_diag_list_count(const mnd_diag_list *list)
{
    return list->count;
}

const mnd_diag *mnd_diag_list_at(const mnd_diag_list *list, size_t index)
{
    if (index >= list->count)
    {
        return NULL;
    }

    return &list->items[index];
}

bool mnd_diag_list_has_error(const mnd_diag_list *list)
{
    size_t i;

    if (list->dropped)
    {
        return true;
    }

    for (i = 0; i < list->count; i++)
    {
        if (list->items[i].severity == MND_SEVERITY_ERROR)
        {
            return true;
        }
    }

    return false;
}
