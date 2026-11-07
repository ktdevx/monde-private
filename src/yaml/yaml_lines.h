#ifndef MONDE_YAML_YAML_LINES_H
#define MONDE_YAML_YAML_LINES_H

/*
 * 物理的な行の表（Lines）の内部ヘッダ。src/yaml/ の .c と tests/yaml/ だけが include する。
 * 行末記号は LF と CRLF。単独の CR は行末にしない（行の内容に残る）
 */

#include "foundation/source.h"
#include "yaml/yaml_parse.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_yaml_line
{
    size_t line;          /* 行番号（文書全体の先頭を 1 とする） */
    size_t start;         /* 行の先頭のオフセット */
    size_t indent;        /* 行頭の空白（U+0020）の数 */
    mnd_span_t content;   /* [start, 行末記号の前) */
    unsigned char eol_len;/* 0、1、2（0 は範囲の最後の行で行末記号がない） */
    bool blank;           /* 空白（U+0020 とタブ）だけ、または空 */
    bool comment;         /* 行頭の空白とタブの並びのあとが # */
    bool tab_in_indent;   /* 行頭の空白とタブの並びにタブがある */
} mnd_yaml_line_t;

/*
 * 範囲の行の表。items は確保関数から直接確保する作業用の配列（アリーナは使わない）。
 * 範囲が空なら count == 0。範囲が行末記号で終わるとき、その後ろに空の行は作らない
 */
typedef struct mnd_yaml_lines
{
    mnd_yaml_line_t *items;
    size_t count;
    size_t cap;           /* items の容量（要素数） */
    mnd_span_t range;     /* 行に分けた範囲（mnd_yaml_lines_build の content） */
    size_t first_line;    /* range.start の行番号（count == 0 でも有効） */
} mnd_yaml_lines_t;

/*
 * data[0..len) の content の範囲を行に分けて作業用の配列に入れる。
 * content は data の範囲内であること（呼び出し側が検証する）。allocator は NULL でない。
 * 最初の行の行番号は mnd_source_line_of(data, len, content.start) で求め、以降は行ごとに 1 ずつ増やす。
 * メモリ不足なら、確保したものを解放し *out を空にして false
 */
bool mnd_yaml_lines_build(const char *data, size_t len, mnd_span_t content,
                          const mnd_yaml_allocator_t *allocator, mnd_yaml_lines_t *out);

/* 作業用の配列を解放し、空の表にする。空の表・解放済みの表に対しても安全 */
void mnd_yaml_lines_free(const mnd_yaml_allocator_t *allocator, mnd_yaml_lines_t *lines);

/*
 * offset（range.start <= offset <= range.end）を含む行の添字（start <= offset である最後の行）。
 * count == 0 なら 0
 */
size_t mnd_yaml_lines_index_of(const mnd_yaml_lines_t *lines, size_t offset);

/*
 * offset（range.start <= offset <= range.end）の行番号。mnd_source_line_of と同じ数え方
 * （行末記号の直後は次の行）。count == 0 なら first_line
 */
size_t mnd_yaml_lines_line_of(const mnd_yaml_lines_t *lines, size_t offset);

#endif
