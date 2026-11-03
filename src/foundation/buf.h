#ifndef MONDE_FOUNDATION_BUF_H
#define MONDE_FOUNDATION_BUF_H

#include <stdbool.h>
#include <stddef.h>

/* NUL を含む任意のバイト列を連結して保持する可変長バッファ */
typedef struct mnd_buf_t
{
    char *data;  /* NULL または len + 1 バイト以上の領域。data[len] == '\0' */
    size_t len;  /* 内容のバイト数（終端の NUL を含まない） */
    size_t cap;  /* 確保済みのバイト数 */
} mnd_buf_t;

#define MND_BUF_INIT { NULL, 0, 0 }

void mnd_buf_init(mnd_buf_t *buf);
/* 追加で additional バイトを再確保なしに追加できる容量を確保する */
bool mnd_buf_reserve(mnd_buf_t *buf, size_t additional);
bool mnd_buf_append(mnd_buf_t *buf, const void *bytes, size_t len); /* len == 0 なら bytes は NULL 可 */
bool mnd_buf_append_str(mnd_buf_t *buf, const char *str);
bool mnd_buf_append_byte(mnd_buf_t *buf, unsigned char byte);
/* 内容を C 文字列として返す。data == NULL なら "" を返す */
const char *mnd_buf_cstr(const mnd_buf_t *buf);
/* 内容を破棄し、確保済みの領域は再利用のため残す（len = 0） */
void mnd_buf_clear(mnd_buf_t *buf);
/* 領域を解放し、初期状態に戻す。初期状態・解放済みのバッファに対しても安全 */
void mnd_buf_free(mnd_buf_t *buf);

#endif
