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

/* 解析の作業状態（md_parse.c） */

struct mnd_md_line_index; /* md_lines.h */

/*
 * 開いているブロック（開いているブロックの配列の要素）。配列は根（文書）から最も内側のブロックまでを
 * 順に持つ（再帰を使わない）。コンテナの照合や閉じる処理が使う状態は、担当するタスクがここに加える
 */
typedef struct mnd_md_open_block
{
    mnd_md_node_t *node;
    /*
     * 引用とリスト項目: 自身の記号が現れた最後の行の、空白でない最後のバイトの後ろ
     * （範囲の終わりの候補。リスト項目では最初の行だけ）
     */
    size_t marker_end;
    bool last_line_blank;   /* 最後に処理した行が、このブロックにとって空行だった（タイトとルーズの判定） */
    bool last_child_blank;  /* 最後の子が空行で終わる（子がなければ false） */
} mnd_md_open_block_t;

/*
 * 1 回の解析の作業状態。Parse が作り、ブロックの段階とインラインの段階に渡す。
 * 作業用の配列は確保関数から直接確保し、Parse が成功・失敗のどちらでも解放する。
 * 文書（doc）は成功したときだけ呼び出し側に渡し、失敗したときは Parse が解放する
 */
typedef struct mnd_md_parser
{
    const char *data;               /* 内容全体（変更しない） */
    size_t len;
    size_t body_start;              /* 本文の開始位置（len 以下） */
    const char *path;               /* 診断に付けるパス。NULL 可 */
    const mnd_md_allocator_t *allocator;    /* 確保関数（既定値を解決済み） */
    const mnd_md_dialect_t *dialect;        /* オプションの方言（NULL なら CommonMark） */
    mnd_md_doc_t *doc;              /* 作成中の文書 */
    const struct mnd_md_line_index *line_index; /* 行番号の表（文書のアリーナに確保） */
    mnd_md_array_t open;            /* 開いているブロック（mnd_md_open_block_t。先頭は文書） */
    mnd_md_array_t leaf_lines;      /* 開いているリーフの行（mnd_md_line_t） */
} mnd_md_parser_t;

/* offset（0 <= offset <= len）の位置の行番号（1 起点。基準は内容全体の先頭） */
size_t mnd_md_parser_line_of(const mnd_md_parser_t *parser, size_t offset);

/*
 * node の start_line と end_line を、範囲の規則（start_line は span.start の行、end_line は範囲の最後の
 * バイトの行。範囲が空なら span.start の行）に従って設定する
 */
void mnd_md_parser_set_node_lines(const mnd_md_parser_t *parser, mnd_md_node_t *node);

/*
 * ブロックの規則のコンテキスト（md_dialect.h の不透明な型の定義）。ブロックの段階が 1 行の処理の中で作り、
 * 方言のブロックの規則に渡す。解析中だけ有効である
 */
struct mnd_md_block_ctx
{
    mnd_md_parser_t *parser;
    mnd_md_line_view_t line;        /* 現在の行の状態 */
};

#endif
