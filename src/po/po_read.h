#ifndef MONDE_PO_PO_READ_H
#define MONDE_PO_PO_READ_H

/* Reader: PO の内容を検査し、カタログに変換する */

#include "foundation/diag.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * data[0..len) を PO として読み込み、out（初期化済みで空）にエントリを加える。
 * path は診断に使うファイル名（NULL 可）。エラーが 1 つでもあれば false を返し、out を空にする。
 * diags は NULL でない。警告は、成功・失敗のどちらでも追加しうる
 */
bool mnd_po_read(const char *data, size_t len, const char *path,
                 mnd_po_catalog_t *out, mnd_diag_list_t *diags);

#endif
