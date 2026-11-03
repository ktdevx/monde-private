#ifndef MONDE_FOUNDATION_FILE_IO_H
#define MONDE_FOUNDATION_FILE_IO_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/buf.h"
#include "foundation/diag.h"

/* path のファイルを読み、BOM を除いた内容を content に入れる。
 * content は初期化済みのバッファで、既存の内容は置き換えられる。
 * 成功: true、*had_bom に BOM の有無。
 * 失敗: diags にエラーを 1 つ追加して false。content は空、*had_bom は false。 */
bool mnd_file_read(const char *path, mnd_buf *content, bool *had_bom,
                   mnd_diag_list *diags);

/* data の len バイトを path に書く（既存のファイルは切り詰めて上書き）。
 * add_bom が true なら先頭に EF BB BF を書く。既定の使い方は false。
 * 失敗: diags にエラーを 1 つ追加して false。 */
bool mnd_file_write(const char *path, const char *data, size_t len, bool add_bom,
                    mnd_diag_list *diags);

#endif
