#ifndef MONDE_FOUNDATION_EOL_H
#define MONDE_FOUNDATION_EOL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum mnd_eol {
    MND_EOL_NONE,  /* 改行が 1 つもない */
    MND_EOL_LF,
    MND_EOL_CRLF
} mnd_eol;

typedef struct mnd_eol_info {
    mnd_eol dominant;   /* 多い方。同数なら先に現れた方。改行なしなら MND_EOL_NONE */
    bool mixed;         /* LF のみの改行と CRLF の両方がある */
    size_t lf_count;    /* 直前が CR でない LF の数 */
    size_t crlf_count;  /* CR LF の組の数 */
} mnd_eol_info;

mnd_eol_info mnd_eol_detect(const char *data, size_t len);

#endif
