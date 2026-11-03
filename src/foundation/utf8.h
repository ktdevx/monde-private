#ifndef MONDE_FOUNDATION_UTF8_H
#define MONDE_FOUNDATION_UTF8_H

#include <stdbool.h>
#include <stddef.h>

/* 妥当なら true。不正なら false を返し、*invalid_offset に最初の不正なシーケンスの先頭オフセットを入れる。
 * len == 0 なら data は NULL 可で、true を返す */
bool mnd_utf8_validate(const char *data, size_t len, size_t *invalid_offset);

#endif
