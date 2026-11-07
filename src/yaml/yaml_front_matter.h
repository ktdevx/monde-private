#ifndef MONDE_YAML_YAML_FRONT_MATTER_H
#define MONDE_YAML_YAML_FRONT_MATTER_H

/* フロントマターの検出（FrontMatter）の公開ヘッダ。Model に依存せず、メモリを確保しない */

#include "foundation/diag.h"
#include "foundation/source.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_yaml_front_matter
{
    bool present;
    mnd_span_t span;      /* 開きの行の先頭から閉じの行の行末記号の直後まで。present でなければ {0, 0} */
    mnd_span_t content;   /* 開きと閉じの行に挟まれた内容。present でなければ {0, 0} */
    size_t body_start;    /* 本文の開始位置。present でなければ 0 */
} mnd_yaml_front_matter_t;

/*
 * data[0..len)（BOM 除去後）の先頭のフロントマターを検出し *out に設定して true を返す。
 * 不正な UTF-8 なら diags にエラーを追加し、*out を {false, {0,0}, {0,0}, 0} にして false を返す。
 * path は診断に付けるパス（NULL 可）。data は len == 0 なら NULL 可
 */
bool mnd_yaml_front_matter_detect(const char *data, size_t len, const char *path,
                                  mnd_yaml_front_matter_t *out, mnd_diag_list_t *diags);

#endif
