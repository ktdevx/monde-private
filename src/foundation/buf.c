#include "foundation/buf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 容量を倍増で拡張するときの最初の容量 */
#define MND_BUF_MIN_CAP 16

void mnd_buf_init(mnd_buf *buf)
{
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

bool mnd_buf_reserve(mnd_buf *buf, size_t additional)
{
    size_t need;
    size_t new_cap;
    char *new_data;

    /* len + additional + 1（終端の NUL）がオーバーフローするなら失敗 */
    if (additional > SIZE_MAX - buf->len - 1) {
        return false;
    }
    need = buf->len + additional + 1;
    if (need <= buf->cap) {
        return true;
    }

    new_cap = buf->cap != 0 ? buf->cap : MND_BUF_MIN_CAP;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = need;
            break;
        }
        new_cap *= 2;
    }

    new_data = (char *)realloc(buf->data, new_cap);
    if (new_data == NULL) {
        return false;
    }
    if (buf->data == NULL) {
        new_data[0] = '\0';
    }
    buf->data = new_data;
    buf->cap = new_cap;
    return true;
}

bool mnd_buf_append(mnd_buf *buf, const void *bytes, size_t len)
{
    if (len == 0) {
        return true;
    }
    if (!mnd_buf_reserve(buf, len)) {
        return false;
    }
    memcpy(buf->data + buf->len, bytes, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return true;
}

bool mnd_buf_append_str(mnd_buf *buf, const char *str)
{
    return mnd_buf_append(buf, str, strlen(str));
}

bool mnd_buf_append_byte(mnd_buf *buf, unsigned char byte)
{
    return mnd_buf_append(buf, &byte, 1);
}

const char *mnd_buf_cstr(const mnd_buf *buf)
{
    return buf->data != NULL ? buf->data : "";
}

void mnd_buf_clear(mnd_buf *buf)
{
    if (buf->data != NULL) {
        buf->len = 0;
        buf->data[0] = '\0';
    }
}

void mnd_buf_free(mnd_buf *buf)
{
    free(buf->data);
    mnd_buf_init(buf);
}
