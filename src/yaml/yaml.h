#ifndef MONDE_YAML_YAML_H
#define MONDE_YAML_YAML_H

/*
 * YAML の構文木（Model）の公開ヘッダ: ノード・スカラー・文書の型、アクセサ、解放。
 * 構文木は内容を複製せずオフセットで指し、ノード・配列・論理値は文書のアリーナが所有する
 */

#include "foundation/eol.h"
#include "foundation/source.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum mnd_yaml_kind
{
    MND_YAML_SCALAR,
    MND_YAML_MAPPING,
    MND_YAML_SEQUENCE
} mnd_yaml_kind_t;

typedef enum mnd_yaml_style
{
    MND_YAML_PLAIN,
    MND_YAML_SINGLE_QUOTED,
    MND_YAML_DOUBLE_QUOTED,
    MND_YAML_LITERAL,   /* | */
    MND_YAML_FOLDED     /* > */
} mnd_yaml_style_t;

typedef enum mnd_yaml_chomp
{
    MND_YAML_CHOMP_CLIP,   /* 指示子なし */
    MND_YAML_CHOMP_STRIP,  /* - */
    MND_YAML_CHOMP_KEEP    /* + */
} mnd_yaml_chomp_t;

typedef struct mnd_yaml_node mnd_yaml_node_t;

typedef struct mnd_yaml_scalar
{
    mnd_yaml_style_t style;
    const char *value;          /* 論理値（UTF-8、NUL 終端、改行は LF）。アリーナが所有 */
    size_t value_len;
    bool flow;                  /* フロー形式のコレクションの中にある */
    /* ブロックスカラーのみ（それ以外は CLIP、false、0、0） */
    mnd_yaml_chomp_t chomp;
    bool has_indent_indicator;
    unsigned indent_indicator;  /* 1〜9 */
    size_t content_indent;      /* 内容の行の先頭の空白の数（行頭からの桁） */
} mnd_yaml_scalar_t;

typedef struct mnd_yaml_pair
{
    mnd_yaml_node_t *key;       /* 常にスカラー */
    mnd_yaml_node_t *value;
} mnd_yaml_pair_t;

/*
 * 不変条件: span は文書の内容の範囲内で start <= end。子は親の内側。兄弟は重ならず原文の順に並ぶ。
 * start_line と end_line は span.start と span.end の行番号（1 起点。基準は文書全体の先頭）
 */
struct mnd_yaml_node
{
    mnd_yaml_kind_t kind;
    mnd_span_t span;
    size_t start_line;
    size_t end_line;
    mnd_yaml_node_t *parent;    /* ルートは NULL */
    union
    {
        mnd_yaml_scalar_t scalar;
        struct
        {
            mnd_yaml_pair_t *pairs;
            size_t count;
        } mapping;
        struct
        {
            mnd_yaml_node_t **items;
            size_t count;
        } sequence;
    } as;
};

/* 不透明。内容へのポインタ、解析した範囲、根、eol、アリーナ、確保関数を持つ（yaml_internal.h） */
typedef struct mnd_yaml_doc mnd_yaml_doc_t;

/* 空のドキュメントなら NULL */
const mnd_yaml_node_t *mnd_yaml_doc_root(const mnd_yaml_doc_t *doc);

/* 文書が指す内容全体（解析に渡した data と len） */
const char *mnd_yaml_doc_data(const mnd_yaml_doc_t *doc);
size_t mnd_yaml_doc_len(const mnd_yaml_doc_t *doc);

/* 解析した範囲 */
mnd_span_t mnd_yaml_doc_content(const mnd_yaml_doc_t *doc);

/* MND_EOL_LF か MND_EOL_CRLF（改行がなければ LF） */
mnd_eol_t mnd_yaml_doc_eol(const mnd_yaml_doc_t *doc);

/* 文書とそのすべてのノード・配列・論理値を解放する。NULL に対しても安全 */
void mnd_yaml_doc_free(mnd_yaml_doc_t *doc);

#endif
