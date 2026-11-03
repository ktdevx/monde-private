/*
 * Ucd の表の仮のファイル。タスク 1.2 で tools/gen_po_ucd.py が生成するファイルに置き換える。
 * C では要素数 0 の配列を定義できないため、各表は番兵の要素を 1 つだけ持ち、要素数を 0 とする
 */

#include "po/po_internal.h"

#include <stddef.h>

const mnd_po_ucd_range_t mnd_po_ucd_lb_ranges[] = {
    { 0, 0, 0 }
};
const size_t mnd_po_ucd_lb_count = 0;

const mnd_po_ucd_range_t mnd_po_ucd_width_ranges[] = {
    { 0, 0, 0 }
};
const size_t mnd_po_ucd_width_count = 0;
