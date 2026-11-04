#ifndef MONDE_MARKDOWN_MD_UNICODE_H
#define MONDE_MARKDOWN_MD_UNICODE_H

/* Unicode: 一般カテゴリの判定と大文字小文字の畳み込みの内部の宣言（md_unicode.c、md_unicode_tables.c） */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 1 つのコードポイントを畳み込んだ UTF-8 の列の最大のバイト数。
 * 畳み込みの表の最長の列（UCD 15.0.0 で 6 バイト）と、畳み込まないコードポイントの
 * UTF-8（最大 4 バイト）のどちらも収まる。tools/gen_md_ucd.py は表の列がこれを超えないことを確かめる
 */
#define MND_MD_UNICODE_FOLD_MAX 6

/* cp が Unicode の句読点（一般カテゴリ P または S）なら true を返す。U+10FFFF を超える値は false */
bool mnd_md_unicode_is_punct(uint32_t cp);

/*
 * cp が一般カテゴリ Zs（U+0020、U+00A0、U+3000 など）なら true を返す。
 * CommonMark の「Unicode の空白」に含まれるタブ・LF・FF・CR は Zs ではないため、呼び出し側で判定する
 */
bool mnd_md_unicode_is_space(uint32_t cp);

/*
 * cp を大文字小文字の畳み込み（CaseFolding.txt の状態 C と F）で畳み込み、その UTF-8 の列を
 * out[0..MND_MD_UNICODE_FOLD_MAX) の先頭に書き、書いたバイト数を返す。out は NUL で終端しない。
 * 畳み込みの対応がないコードポイントは、そのコードポイント自身の UTF-8 を書く。
 * cp が U+10FFFF を超えるか、サロゲート（U+D800〜U+DFFF）なら何も書かずに 0 を返す
 */
size_t mnd_md_unicode_fold(uint32_t cp, char out[MND_MD_UNICODE_FOLD_MAX]);

/*
 * 範囲の表（md_unicode_tables.c。tools/gen_md_ucd.py が UCD 15.0.0 から生成する）。
 * コードポイントの範囲 [first, last] を first の昇順に、重ならずに並べる。
 * 隣り合う範囲は 1 つにまとめる。要素数は *_count で表す
 */
typedef struct mnd_md_unicode_range
{
    uint32_t first;
    uint32_t last;
} mnd_md_unicode_range_t;

/* 一般カテゴリが P または S の範囲 */
extern const mnd_md_unicode_range_t mnd_md_unicode_punct_ranges[];
extern const size_t mnd_md_unicode_punct_count;
/* 一般カテゴリが Zs の範囲 */
extern const mnd_md_unicode_range_t mnd_md_unicode_space_ranges[];
extern const size_t mnd_md_unicode_space_count;

/*
 * 畳み込みの表（md_unicode_tables.c）。CaseFolding.txt の状態 C と F の対応を cp の昇順に並べる。
 * utf8[0..len) が畳み込んだ列の UTF-8 で、len は 1 以上 MND_MD_UNICODE_FOLD_MAX 以下
 */
typedef struct mnd_md_unicode_fold_entry
{
    uint32_t cp;
    unsigned char len;
    unsigned char utf8[MND_MD_UNICODE_FOLD_MAX];
} mnd_md_unicode_fold_entry_t;

extern const mnd_md_unicode_fold_entry_t mnd_md_unicode_fold_entries[];
extern const size_t mnd_md_unicode_fold_count;

#endif
