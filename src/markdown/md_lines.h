#ifndef MONDE_MARKDOWN_MD_LINES_H
#define MONDE_MARKDOWN_MD_LINES_H

/* LineIndex: 行の分割（LF、CRLF、CR）と行番号の表の内部の宣言（md_lines.c） */

#include "markdown/md_internal.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 物理的な行（CommonMark の行）。[line_start, line_end) が行末記号を含まない部分で、
 * その直後に eol_len バイトの行末記号（LF か CR なら 1、CRLF なら 2、最後の行で行末記号がなければ 0）が続く
 */
typedef struct mnd_md_phys_line
{
    size_t line_start;
    size_t line_end;
    size_t eol_len;
} mnd_md_phys_line_t;

/*
 * data[*pos..end) から次の物理的な行を取り出して *out に入れ、*pos を次の行の先頭
 * （line_end + eol_len）に進める。*pos >= end なら行はなく false を返す（*out と *pos は変更しない）。
 * したがって、行末記号で終わる内容のあとに空の行は返さない。NUL では止めない。
 * end の直前の CR は、data[end] を読まずに単独の CR として扱う
 */
bool mnd_md_lines_next(const char *data, size_t end, size_t *pos, mnd_md_phys_line_t *out);

/*
 * 行番号の表。内容の中の LF の位置を昇順に並べたもの（CRLF は LF の位置で数え、単独の CR は数えない）。
 * 行番号は foundation の mnd_source_line_of と同じ（1 起点、基準は内容全体の先頭）
 */
typedef struct mnd_md_line_index
{
    const size_t *lf;   /* LF の位置。lf_count が 0 なら NULL */
    size_t lf_count;
    size_t len;         /* 内容の長さ */
} mnd_md_line_index_t;

/*
 * data[0..len) の行番号の表を *index に作る。表は arena に確保する（アリーナの解放で解放される）。
 * data は len == 0 なら NULL 可。メモリ不足または容量の計算がオーバーフローするなら false
 * （*index は空の表になる）
 */
bool mnd_md_line_index_build(mnd_md_line_index_t *index, const char *data, size_t len,
                             mnd_md_arena_t *arena);

/*
 * offset（0 <= offset <= len）の位置の行番号を、二分探索で *line に入れる。
 * offset > len なら false（*line は変更しない）
 */
bool mnd_md_line_index_line_of(const mnd_md_line_index_t *index, size_t offset, size_t *line);

#endif
