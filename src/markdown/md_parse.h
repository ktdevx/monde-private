#ifndef MONDE_MARKDOWN_MD_PARSE_H
#define MONDE_MARKDOWN_MD_PARSE_H

/* Markdown の解析の入口（Parse）: 解析のオプション、確保関数、mnd_md_parse */

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_dialect.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_md_allocator
{
    void *(*alloc)(void *ctx, size_t size);  /* 失敗したら NULL */
    void (*free)(void *ctx, void *ptr);      /* NULL に対しても安全であること */
    void *ctx;
} mnd_md_allocator_t;

typedef struct mnd_md_options
{
    const mnd_md_dialect_t *dialect;        /* NULL なら mnd_md_commonmark */
    size_t body_start;                      /* 本文の開始位置（バイトオフセット） */
    const char *path;                       /* 診断に付けるパス。NULL 可 */
    const mnd_md_allocator_t *allocator;    /* NULL なら malloc と free */
} mnd_md_options_t;

#define MND_MD_OPTIONS_INIT { NULL, 0, NULL, NULL }

/*
 * data[0..len) を解析する。成功したら *out に文書を設定して true を返す。
 * 失敗したら *out を NULL にし、diags にエラーを追加して false を返す。
 * options は NULL 可（MND_MD_OPTIONS_INIT と同じ）。data は len == 0 なら NULL 可。
 * out と diags は NULL でない。data は BOM を除去した後の内容である（BOM の除去は呼び出し側が行う）。
 * 成功したときは diags に何も追加しない。文書は data を指し続ける
 */
bool mnd_md_parse(const char *data, size_t len, const mnd_md_options_t *options,
                  mnd_md_doc_t **out, mnd_diag_list_t *diags);

#endif
