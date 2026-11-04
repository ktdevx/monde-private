#ifndef MONDE_PO_PO_INIT_H
#define MONDE_PO_PO_INIT_H

/* Init: テンプレートのカタログを、特定の言語の翻訳ファイルのカタログに変える */

#include "foundation/diag.h"
#include "po/po.h"

#include <stdbool.h>

/*
 * catalog（テンプレート）をその場で翻訳ファイルに変える。引数の検査に失敗した場合は catalog を変更しない。
 * メモリ不足が変更の途中で起きた場合は、catalog が部分的に変更されたまま false を返す（呼び出し側は破棄する）
 */
bool mnd_po_init(mnd_po_catalog_t *catalog, const char *language, const char *revision_date,
                 mnd_diag_list_t *diags);

#endif
