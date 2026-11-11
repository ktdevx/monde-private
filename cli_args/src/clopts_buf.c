#include "clopts_buf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 容量を倍増で拡張するときの最初の容量 */
#define CLOPTS_BUF_MIN_CAP 16

/* 追加で additional バイトを再確保なしに追加できる容量を確保する */
static bool clopts_buf_reserve(clopts_buf_t *buf, size_t additional)
{
    size_t need;
    size_t new_cap;
    char *new_data;

    /* len + additional + 1（終端の NUL）がオーバーフローするなら失敗 */
    if (additional > (SIZE_MAX - buf->len - 1))
    {
        return false;
    }

    need = buf->len + additional + 1;
    if (need <= buf->cap)
    {
        return true;
    }

    new_cap = (buf->cap != 0) ? buf->cap : CLOPTS_BUF_MIN_CAP;
    while (new_cap < need)
    {
        if (new_cap > (SIZE_MAX / 2))
        {
            new_cap = need;
            break;
        }

        new_cap *= 2;
    }

    new_data = (char *)realloc(buf->data, new_cap);
    if (new_data == NULL)
    {
        return false;
    }

    if (buf->data == NULL)
    {
        new_data[0] = '\0';
    }

    buf->data = new_data;
    buf->cap = new_cap;

    return true;
}

static bool clopts_buf_append(clopts_buf_t *buf, const void *bytes, size_t len)
{
    if (len == 0)
    {
        return true;
    }

    if (!clopts_buf_reserve(buf, len))
    {
        return false;
    }

    memcpy(buf->data + buf->len, bytes, len);
    buf->len += len;
    buf->data[buf->len] = '\0';

    return true;
}

bool clopts_buf_append_str(clopts_buf_t *buf, const char *str)
{
    return clopts_buf_append(buf, str, strlen(str));
}

bool clopts_buf_append_byte(clopts_buf_t *buf, unsigned char byte)
{
    return clopts_buf_append(buf, &byte, 1);
}

const char *clopts_buf_cstr(const clopts_buf_t *buf)
{
    return (buf->data != NULL) ? buf->data : "";
}

char *clopts_buf_take(clopts_buf_t *buf)
{
    char *text;

    if (buf->data == NULL)
    {
        text = (char *)malloc(1);
        if (text != NULL)
        {
            text[0] = '\0';
        }

        return text;
    }

    text = buf->data;
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;

    return text;
}

void clopts_buf_free(clopts_buf_t *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}
