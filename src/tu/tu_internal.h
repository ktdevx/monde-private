#ifndef MONDE_TU_TU_INTERNAL_H
#define MONDE_TU_TU_INTERNAL_H

/*
 * モジュール内部の共有の宣言（文書と内部セグメントの構造体、セグメントの追加、
 * MarkdownSegments・FrontMatterSegments・Splicer・StructureCheck の関数）。
 * src/tu/ の .c と tests/tu/ だけが include する
 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "markdown/md.h"
#include "tu/tu.h"
#include "yaml/yaml.h"

#include <stdbool.h>
#include <stddef.h>

/* 内部セグメント: 公開部に、セグメントを作った上流のノードへのポインタを添えたもの */
typedef struct mnd_tu_seg
{
    mnd_tu_segment_t pub;
    const mnd_md_node_t *node;      /* Markdown のセグメントの段落または見出し。それ以外は NULL */
    const mnd_yaml_node_t *scalar;  /* フロントマターのセグメントのスカラー。それ以外は NULL */
} mnd_tu_seg_t;

/* 文書（Scanner が作り、mnd_tu_doc_free が解放する） */
struct mnd_tu_doc
{
    const char *data;       /* 走査した原文（呼び出し側が所有する） */
    size_t len;
    char *path;             /* 診断に付けるパスの複製。NULL 可 */
    mnd_md_doc_t *md;       /* markdown の文書 */
    mnd_yaml_doc_t *yaml;   /* yaml の文書。解析しなかった場合は NULL */
    mnd_tu_seg_t *segs;     /* セグメントの配列（data の順） */
    size_t seg_count;
    size_t seg_cap;
};

/* Scanner（tu_scan.c） */

/*
 * doc のセグメントの配列の末尾に 1 件を追加し、初期化した内部セグメントを返す（配列の伸長と初期化）。
 * メモリ不足なら NULL。MarkdownSegments と FrontMatterSegments はこれを使い、配列を直接触らない
 */
mnd_tu_seg_t *mnd_tu_doc_push_segment(mnd_tu_doc_t *doc);

/* MarkdownSegments（tu_markdown.c） */

/* md の木を走査し、セグメントを doc の配列の末尾に加える。メモリ不足なら false */
bool mnd_tu_collect_markdown(mnd_tu_doc_t *doc);

/* node（段落または見出し）の #. のラベルを out の後ろに追加する（ラベルの規則）。メモリ不足なら false */
bool mnd_tu_kind_label(const mnd_md_node_t *node, mnd_buf_t *out);

/* FrontMatterSegments（tu_front_matter.c） */

/*
 * keys の各キーパスを doc->yaml で照合し、セグメントを doc の配列の末尾に加える。
 * 非スカラーの一致は警告。不正なキーパスとメモリ不足は false（診断は上流または本関数が追加する）
 */
bool mnd_tu_collect_front_matter(mnd_tu_doc_t *doc, const char *const *keys, size_t key_count,
                                 mnd_diag_list_t *diags);

/* Splicer（tu_splice.c） */

/*
 * Markdown のセグメント seg を text[0..len)（LF 改行）で置き換えるバイト列を out の後ろに追加する。
 * 「Markdown の書き戻し」の規則に従う。eol は原文より多い行の境界に使う。メモリ不足なら false
 */
bool mnd_tu_splice_markdown(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                            const char *text, size_t len, mnd_eol_t eol, mnd_buf_t *out);

/*
 * フロントマターのセグメント seg を text[0..len) で置き換えるバイト列を out の後ろに追加する。
 * lead_newlines / trail_newlines の \n を補ってから mnd_yaml_format_value を呼ぶ。失敗なら false（診断は上流が追加する）
 */
bool mnd_tu_splice_front_matter(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                                const char *text, size_t len, mnd_diag_list_t *diags, mnd_buf_t *out);

/* StructureCheck（tu_check.c） */

typedef enum mnd_tu_check_result
{
    MND_TU_CHECK_OK,
    MND_TU_CHECK_BLANK_LINE,
    MND_TU_CHECK_HEADING_LINE_BREAK,
    MND_TU_CHECK_BLOCK_MARKER
} mnd_tu_check_result_t;

/*
 * text[0..len) を「構造の検査」の表で検査し、最初に当てはまった項目を返す。
 * is_atx_heading は seg が ATX 見出しのとき真。BLOCK_MARKER のときは *line_index に 0 起点の行番号を入れる。
 * メモリを確保しない
 */
mnd_tu_check_result_t mnd_tu_check_structure(const char *text, size_t len, bool is_atx_heading,
                                             size_t *line_index);

#endif
