#ifndef MONDE_MARKDOWN_MD_INTERNAL_H
#define MONDE_MARKDOWN_MD_INTERNAL_H

/*
 * モジュール内部の共有の宣言（アリーナ、作業用の配列、ノードの作成と連結、解析の作業状態と
 * コンテキストの型）。src/markdown/ の .c と tests/markdown/ だけが include する
 */

#include "foundation/source.h"
#include "markdown/md.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>

/* 確保関数（md_arena.c） */

/* allocator が NULL なら、malloc と free を使う既定の確保関数を返す */
const mnd_md_allocator_t *mnd_md_allocator_or_default(const mnd_md_allocator_t *allocator);

/*
 * 容量の計算: count * elem_size を *out に入れる。オーバーフローするなら false（*out は変更しない）。
 * 確保の前の容量の計算は、この関数か同等の検査を通す
 */
bool mnd_md_size_mul(size_t count, size_t elem_size, size_t *out);

/* アリーナ（md_arena.c） */

typedef struct mnd_md_arena_chunk mnd_md_arena_chunk_t;

/*
 * 塊を確保関数から確保し、その中から順に切り出すアリーナ。個別には解放せず、
 * mnd_md_arena_free ですべての塊をまとめて解放する。
 * 確保関数は、どの型にも使える境界に揃えたメモリを返すこと（malloc と同じ）
 */
typedef struct mnd_md_arena
{
    mnd_md_allocator_t allocator;   /* 確保関数の複製（既定値を解決済み） */
    mnd_md_arena_chunk_t *chunks;   /* 先頭が切り出し中の塊 */
} mnd_md_arena_t;

/* allocator が NULL なら malloc と free。確保はしない */
void mnd_md_arena_init(mnd_md_arena_t *arena, const mnd_md_allocator_t *allocator);

/*
 * 0 で埋めた size バイトを、どの型にも使える境界に揃えて確保する。size が 0 でも NULL でない
 * ポインタを返す。メモリ不足または大きさのオーバーフローなら NULL
 */
void *mnd_md_arena_alloc(mnd_md_arena_t *arena, size_t size);

/* 0 で埋めた count 個の elem_size バイトの配列を確保する。count * elem_size がオーバーフローするなら NULL */
void *mnd_md_arena_alloc_array(mnd_md_arena_t *arena, size_t count, size_t elem_size);

/* src[0..size) の複製を確保する（作業用の配列からリーフの行を移すときなど）。失敗したら NULL */
void *mnd_md_arena_dup(mnd_md_arena_t *arena, const void *src, size_t size);

/* すべての塊を解放し、空のアリーナに戻す（続けて使える） */
void mnd_md_arena_free(mnd_md_arena_t *arena);

/* 作業用の配列（md_arena.c） */

/*
 * 要素の大きさを固定した、伸長できる配列。確保関数から直接確保する（アリーナは使わない）。
 * 開いているブロックのスタック、リーフの行、区切り記号と角括弧のスタックに使う
 */
typedef struct mnd_md_array
{
    mnd_md_allocator_t allocator;   /* 確保関数の複製（既定値を解決済み） */
    void *items;
    size_t elem_size;               /* 1 以上 */
    size_t count;
    size_t cap;
} mnd_md_array_t;

/* elem_size は 1 以上。allocator が NULL なら malloc と free。確保はしない */
void mnd_md_array_init(mnd_md_array_t *array, size_t elem_size,
                       const mnd_md_allocator_t *allocator);

/*
 * あと additional 個の要素を追加できるようにする。容量は倍に伸ばす。
 * メモリ不足または容量の計算がオーバーフローするなら false（配列は変更しない）
 */
bool mnd_md_array_reserve(mnd_md_array_t *array, size_t additional);

/* 0 で埋めた要素を末尾に 1 つ追加し、そのポインタを返す。失敗したら NULL（配列は変更しない） */
void *mnd_md_array_push(mnd_md_array_t *array);

/* elem（elem_size バイト）の複製を末尾に追加する。失敗したら false */
bool mnd_md_array_append(mnd_md_array_t *array, const void *elem);

/* index < count の要素へのポインタ。範囲外なら NULL */
void *mnd_md_array_at(const mnd_md_array_t *array, size_t index);

/* 末尾の要素へのポインタ。空なら NULL */
void *mnd_md_array_last(const mnd_md_array_t *array);

/* 末尾の要素を取り除く。空なら何もしない */
void mnd_md_array_pop(mnd_md_array_t *array);

/* 要素数を 0 にする（容量は残す） */
void mnd_md_array_clear(mnd_md_array_t *array);

/* 確保した領域を解放し、空の配列に戻す（要素の大きさと確保関数は残す） */
void mnd_md_array_free(mnd_md_array_t *array);

/* 文書（md_node.c） */

/*
 * 文書。内容を指すポインタ、根、アリーナを持つ。文書の構造体自体は確保関数から直接確保し、
 * ノード・行の配列・正規化したラベル・方言のデータはアリーナに確保する。
 * 解析の段階で使う値（行番号の表など）は、担当するタスクがここに追加する
 */
struct mnd_md_doc
{
    const char *data;
    size_t len;
    mnd_md_node_t *root;            /* MND_MD_DOCUMENT */
    mnd_md_arena_t arena;
};

/*
 * data[0..len) を指す空の文書（範囲 [0, len) の MND_MD_DOCUMENT の根だけを持つ）を作る。
 * data は len == 0 なら NULL 可。allocator が NULL なら malloc と free。
 * メモリ不足なら、確保したものをすべて解放して NULL を返す
 */
mnd_md_doc_t *mnd_md_doc_create(const char *data, size_t len, const mnd_md_allocator_t *allocator);

/* ノードの作成と連結（md_node.c） */

/*
 * 文書のアリーナに、kind と span を持つノードを作る。ほかの欄は 0（ポインタは NULL）。
 * メモリ不足なら NULL
 */
mnd_md_node_t *mnd_md_node_new(mnd_md_doc_t *doc, mnd_md_kind_t kind, mnd_span_t span);

/* node を木から外す（親・兄弟の連結を直す）。どこにも連結していなければ何もしない */
void mnd_md_node_unlink(mnd_md_node_t *node);

/* node を parent の最後の子にする。node がどこかに連結していれば、先に外す */
void mnd_md_node_append_child(mnd_md_node_t *parent, mnd_md_node_t *node);

/* node を sibling の直前の兄弟にする（親は sibling の親）。node がどこかに連結していれば、先に外す */
void mnd_md_node_insert_before(mnd_md_node_t *sibling, mnd_md_node_t *node);

/* node を sibling の直後の兄弟にする（親は sibling の親）。node がどこかに連結していれば、先に外す */
void mnd_md_node_insert_after(mnd_md_node_t *sibling, mnd_md_node_t *node);

#endif
