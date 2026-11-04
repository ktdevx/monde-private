#ifndef MONDE_PO_MERGE_MERGE_H
#define MONDE_PO_MERGE_MERGE_H

/*
 * テンプレートと既存の翻訳ファイルのカタログをマージする（msgmerge --previous 0.21 と同じ規則）。
 * I/O をせず、入力のカタログを変更しない。エラーは diags に追加して false を返す。
 */

#include "foundation/diag.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_merge_options
{
    bool fuzzy_matching;  /* 偽ならあいまい一致を行わない（msgmerge --no-fuzzy-matching） */
} mnd_merge_options_t;

#define MND_MERGE_OPTIONS_INIT { true }

typedef struct mnd_merge_stats
{
    size_t matched;    /* 完全一致（廃止エントリの復活を含む） */
    size_t fuzzy;      /* あいまい一致 */
    size_t added;      /* どちらにも一致しなかった新規 */
    size_t obsoleted;  /* 今回新たに廃止エントリにした既存のエントリ */
} mnd_merge_stats_t;

#define MND_MERGE_STATS_INIT { 0, 0, 0, 0 }

/*
 * template_catalog と old_catalog をマージし、out（初期化済みで空）にエントリを加える。
 * template_path と old_path は診断に使うファイル名（NULL 可）。options が NULL なら既定（あいまい一致あり）。
 * stats は NULL 可。失敗時は false を返し、out を空に、*stats を 0 にする。diags は NULL でない
 */
bool mnd_merge_catalogs(const mnd_po_catalog_t *template_catalog, const char *template_path,
                        const mnd_po_catalog_t *old_catalog, const char *old_path,
                        const mnd_merge_options_t *options,
                        mnd_po_catalog_t *out, mnd_merge_stats_t *stats,
                        mnd_diag_list_t *diags);

#endif
