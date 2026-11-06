#ifndef MONDE_MARKDOWN_MD_INTERNAL_H
#define MONDE_MARKDOWN_MD_INTERNAL_H

/*
 * モジュール内部の共有の宣言（アリーナ、作業用の配列、ノードの作成と連結、解析の作業状態と
 * コンテキストの型）。src/markdown/ の .c と tests/markdown/ だけが include する
 */

#include "foundation/diag.h"
#include "foundation/source.h"
#include "markdown/md.h"
#include "markdown/md_dialect.h"
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
 * リンク参照定義の表の要素（md_linkref.c）。def が NULL なら空き。
 * hash と len は正規化したラベル（def->as.link_ref_def.norm_label）のハッシュ値とバイト数
 */
typedef struct mnd_md_linkref_slot
{
    size_t hash;
    size_t len;
    const mnd_md_node_t *def;
} mnd_md_linkref_slot_t;

/*
 * リンク参照定義の表（正規化したラベルのハッシュ表。開番地法）。要素の配列は文書のアリーナに確保し、
 * 伸長するときは倍の大きさの配列をアリーナに作り直す。cap は 0 か 2 のべき乗
 */
typedef struct mnd_md_linkref_table
{
    mnd_md_linkref_slot_t *slots;
    size_t cap;
    size_t count;
} mnd_md_linkref_table_t;

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
    mnd_md_linkref_table_t refs;    /* リンク参照定義の表（ブロックの段階が作る。md_linkref.c） */
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
    const mnd_md_dialect_t *dialect;        /* 方言（NULL でない。オプションが NULL なら mnd_md_commonmark） */
    mnd_md_doc_t *doc;              /* 作成中の文書 */
    const struct mnd_md_line_index *line_index; /* 行番号の表（文書のアリーナに確保） */
    mnd_md_array_t open;            /* 開いているブロック（mnd_md_open_block_t。先頭は文書） */
    mnd_md_array_t leaf_lines;      /* 開いているリーフの行（mnd_md_line_t） */
    mnd_md_array_t dialect_lines;   /* 方言の規則が段落から引き取った行（mnd_md_line_t。開く間だけ使う） */
    /*
     * 方言の規則が不正な結果を返した（段階の関数の false をメモリ不足と区別する）。
     * rule_index はその規則の添字、rule_line はその行の行番号
     */
    bool rule_failed;
    size_t rule_index;
    size_t rule_line;
} mnd_md_parser_t;

/* offset（0 <= offset <= len）の位置の行番号（1 起点。基準は内容全体の先頭） */
size_t mnd_md_parser_line_of(const mnd_md_parser_t *parser, size_t offset);

/*
 * node の start_line と end_line を、範囲の規則（start_line は span.start の行、end_line は範囲の最後の
 * バイトの行。範囲が空なら span.start の行）に従って設定する
 */
void mnd_md_parser_set_node_lines(const mnd_md_parser_t *parser, mnd_md_node_t *node);

/* ブロックの規則のコンテキストを渡すコールバック */
typedef enum mnd_md_block_ctx_mode
{
    MND_MD_BLOCK_CTX_START,         /* try_start */
    MND_MD_BLOCK_CTX_CONTINUE,      /* try_continue */
    MND_MD_BLOCK_CTX_FINALIZE       /* finalize */
} mnd_md_block_ctx_mode_t;

/*
 * ブロックの規則のコンテキスト（md_dialect.h の不透明な型の定義）。ブロックの段階が規則のコールバックを
 * 呼ぶたびに作り（mnd_md_block_ctx_init）、コンテキスト関数（md_dialect.c）がカーソルを進め、ブロックを開き、
 * 結果を記録する。ブロックの段階は、コールバックが true を返したときだけカーソルと結果を使う。
 * 解析中だけ有効である
 */
struct mnd_md_block_ctx
{
    mnd_md_parser_t *parser;
    mnd_md_block_ctx_mode_t mode;
    const mnd_md_block_rule_t *rule;    /* 呼んでいる規則 */
    size_t line_start;              /* 物理的な行の先頭 */
    size_t line_end;                /* 行末記号の直前 */
    size_t line_no;                 /* 物理的な行の行番号 */
    /* カーソル（mnd_md_scan_pos_t と同じ意味: 次に読む位置、論理的な桁、残っている仮想の空白） */
    size_t pos;
    size_t column;
    size_t pad;
    /* START のとき: 中断しようとしている段落（最も内側の開いている段落。なければ NULL）と、その行の数 */
    mnd_md_node_t *paragraph;
    size_t paragraph_lines;
    bool paragraph_matched;         /* 段落まですべてのブロックを照合できた（行を引き取れる） */
    mnd_md_node_t *opened;          /* mnd_md_block_ctx_open で作ったノード（木にはまだ連結しない） */
    size_t take;                    /* 引き取る段落の最後の行の数 */
    bool invalid;                   /* 規則が不正な操作をした（行末を超えて使った、など） */
    bool oom;                       /* コンテキスト関数の確保が失敗した */
};

/*
 * コンテキストを mode と rule で初期化する。カーソルと段落は 0（NULL）にし、行番号は
 * line_start の行にする（呼び出し側がカーソルと段落を設定する）
 */
void mnd_md_block_ctx_init(mnd_md_block_ctx_t *ctx, mnd_md_parser_t *parser, mnd_md_block_ctx_mode_t mode,
                           const mnd_md_block_rule_t *rule, size_t line_start, size_t line_end);

/* 方言（md_dialect.c） */

/*
 * 方言の規則を検査する（種類が MND_MD_KIND_DIALECT 以上で、ブロックとインラインのすべての規則で異なること、
 * 必須のコールバック、インラインの開始文字が CommonMark の特別な文字でないこと）。
 * 不正なら最初の不正な規則について `dialect "%s": invalid %s rule %zu` の診断を追加して false を返す
 */
bool mnd_md_dialect_check(const mnd_md_dialect_t *dialect, const char *path, mnd_diag_list_t *diags);

/* 診断に使う方言の名前（name が NULL なら空文字列） */
const char *mnd_md_dialect_name(const mnd_md_dialect_t *dialect);

/*
 * kind の種類のノードを作るブロックの規則を返し、*index にその添字を入れる（index は NULL 可）。
 * 方言の種類でないか、その種類のブロックの規則がなければ NULL
 */
const mnd_md_block_rule_t *mnd_md_dialect_block_rule(const mnd_md_dialect_t *dialect, mnd_md_kind_t kind,
                                                     size_t *index);

#endif
