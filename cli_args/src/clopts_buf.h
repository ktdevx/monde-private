#ifndef CLOPTS_BUF_H
#define CLOPTS_BUF_H

/* 文字列を連結する可変長バッファ。ライブラリの内部だけで使い、公開ヘッダには出さない */

#include <stdbool.h>
#include <stddef.h>

typedef struct clopts_buf
{
    char *data;  /* NULL または len + 1 バイト以上の領域。data[len] == '\0' */
    size_t len;  /* 内容のバイト数（終端の NUL を含まない） */
    size_t cap;  /* 確保済みのバイト数 */
} clopts_buf_t;

#define CLOPTS_BUF_INIT { NULL, 0, 0 }

/* 失敗（メモリ不足）なら false を返し、内容は変えない */
bool clopts_buf_append_str(clopts_buf_t *buf, const char *str);
bool clopts_buf_append_byte(clopts_buf_t *buf, unsigned char byte);

/* 内容を C 文字列として返す。data == NULL なら "" を返す */
const char *clopts_buf_cstr(const clopts_buf_t *buf);

/*
 * 内容の所有権を呼び出し側へ移し、バッファを初期状態に戻す。
 * 空なら "" の複製を返す。失敗（メモリ不足）なら NULL を返し、バッファは変えない
 */
char *clopts_buf_take(clopts_buf_t *buf);

/* 領域を解放し、初期状態に戻す。初期状態・解放済みのバッファに対しても安全 */
void clopts_buf_free(clopts_buf_t *buf);

#endif
