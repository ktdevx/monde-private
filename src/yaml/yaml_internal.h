#ifndef MONDE_YAML_YAML_INTERNAL_H
#define MONDE_YAML_YAML_INTERNAL_H

/*
 * モジュール内部の共有の宣言（確保関数の既定値、アリーナ、文書の内部構造、ノードの作成、キーの比較、
 * 解析の作業状態とカーソル）。src/yaml/ の .c と tests/yaml/ だけが include する
 */

#include "foundation/diag.h"
#include "foundation/eol.h"
#include "foundation/source.h"
#include "yaml/yaml.h"
#include "yaml/yaml_lines.h"
#include "yaml/yaml_parse.h"

#include <stdbool.h>
#include <stddef.h>

/* 確保関数（yaml_node.c） */

/* allocator が NULL なら、malloc と free を使う既定の確保関数を返す */
const mnd_yaml_allocator_t *mnd_yaml_allocator_or_default(const mnd_yaml_allocator_t *allocator);

/*
 * 容量の計算: count * elem_size を *out に入れる。オーバーフローするなら false（*out は変更しない）。
 * 作業用の配列とアリーナの配列の大きさの計算は、この関数を通す
 */
bool mnd_yaml_size_mul(size_t count, size_t elem_size, size_t *out);

/* 文書とアリーナ（yaml_node.c） */

/* アリーナの塊。見出し（次の塊、容量、切り出したバイト数）の定義は yaml_node.c に置く */
typedef struct mnd_yaml_arena_chunk mnd_yaml_arena_chunk_t;

/*
 * 文書。内容を指すポインタ、解析した範囲、根、行末記号、アリーナ、確保関数を持つ。
 * 文書の構造体自体は確保関数から直接確保し、ノード・ノードの配列・論理値はアリーナに確保する。
 * アリーナは確保関数から大きな塊を確保し、塊の連結リスト（chunks。先頭が切り出し中の塊）を持つ。
 * 解放は塊を順に free するだけで、木をたどらない
 */
struct mnd_yaml_doc
{
    const char *data;                   /* 内容全体（変更しない） */
    size_t len;
    mnd_span_t content;                 /* 解析した範囲 */
    mnd_eol_t eol;                      /* MND_EOL_LF か MND_EOL_CRLF */
    mnd_yaml_node_t *root;              /* 空のドキュメントなら NULL */
    mnd_yaml_allocator_t allocator;     /* 確保関数の複製（既定値を解決済み） */
    mnd_yaml_arena_chunk_t *chunks;     /* アリーナの塊の連結リスト */
};

/*
 * data[0..len) を指し、content を解析した範囲とする空の文書（root == NULL）を作る。
 * data は len == 0 なら NULL 可。eol は MND_EOL_LF か MND_EOL_CRLF（MND_EOL_NONE は LF として持つ）。
 * allocator が NULL なら malloc と free。Parse とテストが使う。メモリ不足なら NULL
 */
mnd_yaml_doc_t *mnd_yaml_doc_new(const char *data, size_t len, mnd_span_t content, mnd_eol_t eol,
                                 const mnd_yaml_allocator_t *allocator);

/* 文書の根を node にする（node は NULL 可。node の parent は変更しない） */
void mnd_yaml_doc_set_root(mnd_yaml_doc_t *doc, mnd_yaml_node_t *node);

/*
 * 文書のアリーナから、0 で埋めた size バイトを、どの型にも使える境界に揃えて確保する。
 * size が 0 でも NULL でないポインタを返す。メモリ不足または大きさのオーバーフローなら NULL
 */
void *mnd_yaml_arena_alloc(mnd_yaml_doc_t *doc, size_t size);

/*
 * 文書のアリーナに kind のノードを作る。ほかの欄は 0（ポインタは NULL、スカラーは PLAIN と CLIP）。
 * 範囲・行番号・親・論理値は作成した側が設定する。メモリ不足なら NULL
 */
mnd_yaml_node_t *mnd_yaml_node_new(mnd_yaml_doc_t *doc, mnd_yaml_kind_t kind);

/*
 * data[0..len) の複製に NUL を付けて文書のアリーナに確保する（論理値の文字列に使う）。
 * data は len == 0 なら NULL 可（空文字列を返す）。メモリ不足なら NULL
 */
char *mnd_yaml_arena_strdup(mnd_yaml_doc_t *doc, const char *data, size_t len);

/*
 * 2 つのスカラーの論理値が長さと内容で一致するか（Parse と Flow が重複キーの検出に使う）。
 * どちらかがスカラーでなければ false
 */
bool mnd_yaml_key_equal(const mnd_yaml_node_t *a, const mnd_yaml_node_t *b);

/* 解析の作業状態（Parse が作る。テストでは parser_fixture が作る。補助関数は yaml_scalar.c） */

/*
 * 行の表の上の位置。line_index は lines.items の添字（範囲の終わりでは lines.count）、
 * offset は文書全体の先頭を基準とするオフセット（offset は line_index の行の中か、その行末記号の直後）
 */
typedef struct mnd_yaml_cursor
{
    size_t line_index;
    size_t offset;
} mnd_yaml_cursor_t;

/*
 * 1 回の解析の作業状態。作業用の配列（行の表など）は確保関数から直接確保し、作った側が成功・失敗の
 * どちらでも解放する。文書（doc）は成功したときだけ呼び出し側に渡し、失敗したときは作った側が解放する。
 * 最初のエラーで解析を止める（failed が true になったら、以降の診断は追加しない）
 */
typedef struct mnd_yaml_parser
{
    const char *data;                       /* 内容全体（変更しない） */
    size_t len;
    mnd_span_t content;                     /* 解析する範囲 */
    const char *path;                       /* 診断に付けるパス（オプションの path）。NULL 可 */
    const mnd_yaml_allocator_t *allocator;  /* 確保関数（既定値を解決済み。NULL でない） */
    mnd_yaml_doc_t *doc;                    /* 作成中の文書 */
    mnd_yaml_lines_t lines;                 /* content の行の表 */
    mnd_diag_list_t *diags;                 /* 診断の追加先（NULL でない） */
    bool failed;                            /* エラーの診断を追加済み（構文エラーまたはメモリ不足） */
} mnd_yaml_parser_t;

/*
 * 解析のエラーを報告する: failed でなければ、message（固定の英語の文字列）のエラーの診断
 * （パスは p->path、行番号は line）を追加し、failed を true にする。failed なら何もしない
 * （1 回の解析で返す診断は 1 つ）
 */
void mnd_yaml_parser_error(mnd_yaml_parser_t *p, size_t line, const char *message);

/*
 * 引数を持つメッセージ（"duplicate mapping key \"%s\"" など）を報告するための関数: failed でなければ
 * failed を true にして true を返す。呼び出し側は true のときだけ、書式文字列を固定の文字列にして
 * mnd_diag_add(p->diags, MND_SEVERITY_ERROR, p->path, line, ...) を呼ぶ（キーなどの入力は %s の引数にする）。
 * failed なら false
 */
bool mnd_yaml_parser_claim_error(mnd_yaml_parser_t *p);

/* メモリ不足を報告する（mnd_yaml_parser_error と同じ規則で "out of memory"、行番号 0） */
void mnd_yaml_parser_oom(mnd_yaml_parser_t *p);

/* offset（content.start <= offset <= content.end）の行番号（行の表で求める） */
size_t mnd_yaml_parser_line_of(const mnd_yaml_parser_t *p, size_t offset);

/* node の span を設定し、start_line と end_line を span.start と span.end の行番号にする */
void mnd_yaml_parser_set_span(const mnd_yaml_parser_t *p, mnd_yaml_node_t *node, mnd_span_t span);

#endif
