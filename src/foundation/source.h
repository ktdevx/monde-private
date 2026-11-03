#ifndef MONDE_FOUNDATION_SOURCE_H
#define MONDE_FOUNDATION_SOURCE_H

#include <stdbool.h>
#include <stddef.h>

/* 内容中の半開区間。オフセットの基準は BOM 除去後の内容の先頭 */
typedef struct mnd_span {
    size_t start;  /* 含む */
    size_t end;    /* 含まない。start <= end */
} mnd_span;

typedef struct mnd_slice {
    const char *data;  /* 元の内容の内部を指す（コピーしない） */
    size_t len;
} mnd_slice;

/* start <= end かつ end <= content_len なら true */
bool mnd_span_is_valid(mnd_span span, size_t content_len);

/* offset（0 <= offset <= len）の位置の 1 起点の行番号を *line に入れる。offset > len なら false */
bool mnd_source_line_of(const char *data, size_t len, size_t offset, size_t *line);

/* 範囲が指す元の内容を *out に入れる。不正な範囲なら false（*out は変更しない） */
bool mnd_span_text(const char *data, size_t len, mnd_span span, mnd_slice *out);

#endif
