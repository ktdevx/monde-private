#ifndef MONDE_MARKDOWN_MD_DIALECT_H
#define MONDE_MARKDOWN_MD_DIALECT_H

/*
 * 方言（Dialect）: ブロックの規則、インラインの規則、後処理と、規則が使うコンテキスト関数。
 * コンテキストは解析中だけ有効であり、規則はコンテキストを保持しない。
 * 規則が作るノードの種類は MND_MD_KIND_DIALECT 以上とし、規則ごとに異なる値にする
 */

#include "markdown/md.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_md_block_ctx mnd_md_block_ctx_t;
typedef struct mnd_md_inline_ctx mnd_md_inline_ctx_t;
typedef struct mnd_md_post_ctx mnd_md_post_ctx_t;

/* ブロックの規則のコンテキストが示す、現在の行の状態 */
typedef struct mnd_md_line_view
{
    const char *data;       /* 内容全体 */
    size_t pos;             /* 現在の位置（コンテナが使ったバイトの後ろ） */
    size_t end;             /* 行末記号の直前 */
    size_t column;          /* pos の桁（タブは 4 桁ごとの位置まで進む） */
    size_t indent;          /* pos から最初の空白でない文字までの桁数 */
    size_t first_nonspace;  /* 最初の空白でない文字のオフセット（なければ end） */
    bool blank;
    size_t line;            /* 行番号 */
} mnd_md_line_view_t;

typedef struct mnd_md_block_rule
{
    mnd_md_kind_t kind;
    bool container;             /* 子のブロックを持つ */
    bool inline_content;        /* リーフの行をインラインとして解析する（container が偽のとき） */
    bool can_interrupt_paragraph;
    /* 新しいブロックを開始できるか試す。開始するなら、使ったバイトを進めて mnd_md_block_ctx_open を呼び、true を返す */
    bool (*try_start)(mnd_md_block_ctx_t *ctx);
    /* 開いている node が現在の行で続くか。続くなら、使ったバイトを進めて true を返す */
    bool (*try_continue)(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node);
    /* node を閉じるときの後処理（NULL 可） */
    bool (*finalize)(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node);
} mnd_md_block_rule_t;

typedef struct mnd_md_delim_run
{
    size_t length;          /* 残っている区切り記号の数 */
    size_t original_length; /* 元の連なりの長さ */
    bool can_open;
    bool can_close;
} mnd_md_delim_run_t;

typedef struct mnd_md_inline_rule
{
    mnd_md_kind_t kind;
    unsigned char trigger;      /* 開始文字（CommonMark の特別な文字であってはならない） */
    bool delimiter;             /* 真なら区切り記号形式、偽なら開始文字形式 */
    /* 開始文字形式: 現在の位置から要素を認識したら、mnd_md_inline_ctx_emit でノードを作って true を返す */
    bool (*try_match)(mnd_md_inline_ctx_t *ctx);
    /* 区切り記号形式: opener と closer の対で使う数を *use に入れて true を返す。対にしないなら false */
    bool (*match_delims)(const mnd_md_delim_run_t *opener, const mnd_md_delim_run_t *closer,
                         size_t *use);
} mnd_md_inline_rule_t;

typedef struct mnd_md_dialect
{
    const char *name;
    const mnd_md_block_rule_t *block_rules;
    size_t block_rule_count;
    const mnd_md_inline_rule_t *inline_rules;
    size_t inline_rule_count;
    bool (*postprocess)(mnd_md_post_ctx_t *ctx, mnd_md_node_t *root); /* NULL 可 */
} mnd_md_dialect_t;

/* 何も追加しない方言（CommonMark そのもの） */
extern const mnd_md_dialect_t mnd_md_commonmark;

/* ブロックの規則のコンテキスト関数 */
mnd_md_line_view_t mnd_md_block_ctx_line(const mnd_md_block_ctx_t *ctx);
void mnd_md_block_ctx_advance(mnd_md_block_ctx_t *ctx, size_t bytes);      /* 行末を超えない */
void mnd_md_block_ctx_advance_columns(mnd_md_block_ctx_t *ctx, size_t columns); /* タブの一部は pad になる */
mnd_md_node_t *mnd_md_block_ctx_open(mnd_md_block_ctx_t *ctx, mnd_md_kind_t kind); /* 現在の位置から始まるブロックを開く */
mnd_md_node_t *mnd_md_block_ctx_paragraph(const mnd_md_block_ctx_t *ctx); /* 中断しようとしている段落。なければ NULL */
bool mnd_md_block_ctx_take_paragraph_lines(mnd_md_block_ctx_t *ctx, size_t count); /* 段落の最後の count 行を、直後に開くブロックの最初の行にする */
void *mnd_md_block_ctx_alloc(mnd_md_block_ctx_t *ctx, size_t size);     /* アリーナから確保。失敗したら NULL */

/* インラインの規則のコンテキスト関数（開始文字形式は、現在の行の中だけを見る） */
mnd_md_line_view_t mnd_md_inline_ctx_line(const mnd_md_inline_ctx_t *ctx);
mnd_md_node_t *mnd_md_inline_ctx_emit(mnd_md_inline_ctx_t *ctx, mnd_md_kind_t kind, size_t length); /* 現在の位置から length バイトのノードを作り、位置を進める */
void *mnd_md_inline_ctx_alloc(mnd_md_inline_ctx_t *ctx, size_t size);

/* 後処理のコンテキスト関数 */
mnd_md_node_t *mnd_md_post_new_node(mnd_md_post_ctx_t *ctx, mnd_md_kind_t kind, mnd_span_t span);
bool mnd_md_post_split_text(mnd_md_post_ctx_t *ctx, mnd_md_node_t *text, size_t offset); /* テキストを offset で 2 つに分ける */
void mnd_md_post_insert_after(mnd_md_post_ctx_t *ctx, mnd_md_node_t *sibling, mnd_md_node_t *node);
void mnd_md_post_wrap(mnd_md_post_ctx_t *ctx, mnd_md_node_t *first, mnd_md_node_t *last, mnd_md_node_t *wrapper); /* first〜last の兄弟を wrapper の子にする */
void *mnd_md_post_alloc(mnd_md_post_ctx_t *ctx, size_t size);

#endif
