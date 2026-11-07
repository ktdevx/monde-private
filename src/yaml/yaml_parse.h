#ifndef MONDE_YAML_YAML_PARSE_H
#define MONDE_YAML_YAML_PARSE_H

/* YAML の解析の入口（Parse）の公開ヘッダ: 確保関数、解析のオプション、mnd_yaml_parse */

#include "foundation/diag.h"
#include "foundation/source.h"
#include "yaml/yaml.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_yaml_allocator
{
    void *(*alloc)(void *ctx, size_t size);  /* 失敗したら NULL */
    void (*free)(void *ctx, void *ptr);      /* NULL に対しても安全であること */
    void *ctx;
} mnd_yaml_allocator_t;

typedef struct mnd_yaml_options
{
    const char *path;                         /* 診断に付けるパス。NULL 可 */
    const mnd_yaml_allocator_t *allocator;    /* NULL なら malloc と free */
} mnd_yaml_options_t;

#define MND_YAML_OPTIONS_INIT { NULL, NULL }

/*
 * data[0..len) のうち content の範囲を 1 つの YAML ドキュメントとして解析する。
 * 成功したら *out に文書を設定して true を返す。失敗したら *out を NULL にし、diags にエラーを追加して false を返す。
 * options は NULL 可。content は data の範囲内であること（範囲外ならエラー "content span out of range"）。
 * オフセットと行番号は data の先頭を基準にする。文書は data を指し続ける。
 * 成功したときは diags に何も追加しない
 */
bool mnd_yaml_parse(const char *data, size_t len, mnd_span_t content, const mnd_yaml_options_t *options,
                    mnd_yaml_doc_t **out, mnd_diag_list_t *diags);

#endif
