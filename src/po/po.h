#ifndef MONDE_PO_PO_H
#define MONDE_PO_PO_H

/*
 * Model: カタログとエントリの型、状態、フラグ、検索、ヘッダの項目、日時の書式。
 * カタログはエントリを、エントリは文字列と配列を所有する。mnd_po_catalog_free がすべてを解放する。
 * カタログ内の文字列は、すべて NUL を含まない正しい UTF-8 とする。
 * メモリ不足のときは false（または NULL）を返し、診断は追加しない。
 */

#include "foundation/buf.h"
#include "foundation/source.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct mnd_po_strings
{
    mnd_buf_t *items;
    size_t count;
    size_t cap;
} mnd_po_strings_t;

typedef struct mnd_po_ref
{
    mnd_buf_t file;
    size_t line;  /* 1 起点。0 は行番号なし */
} mnd_po_ref_t;

typedef struct mnd_po_refs
{
    mnd_po_ref_t *items;
    size_t count;
    size_t cap;
} mnd_po_refs_t;

/* 省略可能な文字列は has_* で有無を表す。has_* が偽のときの文字列の内容は無視される */
typedef struct mnd_po_entry
{
    mnd_po_strings_t translator_comments;
    mnd_po_strings_t extracted_comments;
    mnd_po_refs_t refs;
    mnd_po_strings_t flags;
    bool has_prev_msgctxt;
    mnd_buf_t prev_msgctxt;
    bool has_prev_msgid;
    mnd_buf_t prev_msgid;
    bool has_prev_msgid_plural;
    mnd_buf_t prev_msgid_plural;
    bool has_msgctxt;
    mnd_buf_t msgctxt;
    mnd_buf_t msgid;
    bool has_msgid_plural;
    mnd_buf_t msgid_plural;
    mnd_po_strings_t msgstr;  /* 単数形は 1 個、複数形は添字の順 */
    bool obsolete;
    size_t line;              /* 入力上の開始行。入力から作られていなければ 0 */
} mnd_po_entry_t;

typedef struct mnd_po_catalog
{
    mnd_po_entry_t *entries;
    size_t count;
    size_t cap;
} mnd_po_catalog_t;

#define MND_PO_CATALOG_INIT { NULL, 0, 0 }

typedef enum mnd_po_state
{
    MND_PO_STATE_UNTRANSLATED,
    MND_PO_STATE_FUZZY,
    MND_PO_STATE_TRANSLATED
} mnd_po_state_t;

void mnd_po_entry_init(mnd_po_entry_t *entry);
void mnd_po_entry_free(mnd_po_entry_t *entry);  /* 初期状態に戻す */
bool mnd_po_strings_push(mnd_po_strings_t *list, const char *data, size_t len);
bool mnd_po_refs_push(mnd_po_refs_t *refs, const char *file, size_t line);

void mnd_po_catalog_init(mnd_po_catalog_t *catalog);

/* 初期状態・解放済みに対しても安全 */
void mnd_po_catalog_free(mnd_po_catalog_t *catalog);

/*
 * 初期化したエントリを末尾（または index の位置）に加えて返す。失敗時は NULL。
 * 既存のエントリへのポインタは無効になる
 */
mnd_po_entry_t *mnd_po_catalog_append(mnd_po_catalog_t *catalog);
mnd_po_entry_t *mnd_po_catalog_insert(mnd_po_catalog_t *catalog, size_t index);  /* index <= count */

mnd_po_state_t mnd_po_entry_state(const mnd_po_entry_t *entry);
bool mnd_po_flag_has(const mnd_po_entry_t *entry, const char *flag);
bool mnd_po_flag_add(mnd_po_entry_t *entry, const char *flag);     /* あれば何もせず true */
void mnd_po_flag_remove(mnd_po_entry_t *entry, const char *flag);  /* なければ何もしない */

/* msgctxt が NULL なら msgctxt のないエントリだけに一致する。廃止エントリは対象外 */
bool mnd_po_catalog_find(const mnd_po_catalog_t *catalog, const char *msgctxt,
                         const char *msgid, size_t *index);
bool mnd_po_catalog_header(const mnd_po_catalog_t *catalog, size_t *index);

/* ヘッダの項目。名前は大文字と小文字を区別せずに照合する。value は header の内部を指す */
bool mnd_po_header_get(const mnd_po_entry_t *header, const char *name, mnd_slice_t *value);
bool mnd_po_header_set(mnd_po_entry_t *header, const char *name, const char *value);

/* テンプレート用ヘッダを out（初期化済み）に作る。project_id_version が NULL なら "PACKAGE VERSION" */
bool mnd_po_header_template(mnd_po_entry_t *out, const char *project_id_version,
                            const char *creation_date);

/* "YYYY-MM-DD HH:MM+ZZZZ" を書く。utc_offset_minutes は東を正とする */
#define MND_PO_DATE_SIZE 32
bool mnd_po_date_format(const struct tm *local_time, long utc_offset_minutes,
                        char out[MND_PO_DATE_SIZE]);

#endif
