#ifndef MONDE_PO_PO_WRITE_H
#define MONDE_PO_PO_WRITE_H

/* Writer: カタログを gettext と同じ形式の PO として書き出す */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "po/po.h"

#include <stdbool.h>

typedef struct mnd_po_write_options
{
    bool no_wrap;   /* 真なら文字列を \n の直後でだけ区切る（参照は msgcat と同じく折り返す） */
    mnd_eol_t eol;  /* MND_EOL_CRLF なら CRLF、それ以外は LF */
} mnd_po_write_options_t;

#define MND_PO_WRITE_OPTIONS_INIT { false, MND_EOL_LF }

/*
 * options が NULL なら既定（折り返しあり、LF）。重複があれば何も書かずに false。
 * out の末尾に追加する。失敗時は、out の長さを呼び出し前に戻す
 */
bool mnd_po_write(const mnd_po_catalog_t *catalog, const mnd_po_write_options_t *options,
                  mnd_buf_t *out, mnd_diag_list_t *diags);

#endif
