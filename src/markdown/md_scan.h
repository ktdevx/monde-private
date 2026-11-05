#ifndef MONDE_MARKDOWN_MD_SCAN_H
#define MONDE_MARKDOWN_MD_SCAN_H

/* Scan: 1 行の中のブロックの開始の判定とタブの桁の計算の内部の宣言（md_scan.c） */

#include "foundation/source.h"
#include "markdown/md.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 共通の約束:
 * - すべての関数は純粋で、data[pos..end) だけを読む。end は物理的な行の line_end（行末記号の直前）で、
 *   end より後ろ（行末記号を含む）は読まない。NUL では止めず、NUL はふつうの（空白でない）文字として扱う。
 * - ブロックの開始の判定は、pos に「インデントを除いた最初の空白でない文字」の位置を受け取る。
 *   インデントが 3 桁以下であることの確認（4 桁以上ならインデントコードやリスト項目の継続）は
 *   呼び出し側（BlockParser）が mnd_md_scan_indent で行う。
 * - 判定の順序（テーマ区切りとリスト記号の `* * *`、Setext の下線とリスト記号の `-` など、
 *   複数の判定が同時に真になる行）は呼び出し側が決める。
 * - 桁はタブを 4 桁ごとのタブストップとして数える。桁の基準（0 の位置）は呼び出し側が渡す
 *   （通常は物理的な行の先頭の桁 0。行の途中から始めるときはその位置の桁）。
 */

/* タブの桁の計算 */

/* 桁 column の位置にあるバイト c の次の桁。タブは次の 4 の倍数の桁、それ以外は column + 1 */
size_t mnd_md_scan_next_column(size_t column, char c);

/*
 * 行の中の位置（カーソル）。
 * - pos: 次に読むバイトの位置。
 * - column: 論理的な桁（取り除いた桁数を含む位置）。
 * - pad: data[pos] の前に残っている仮想の空白の数（タブの一部だけを取り除いたときの、
 *   そのタブの残りの桁数。0〜3）。data[pos] の桁は column + pad。
 * 行の先頭では { line_start, 0, 0 }。
 */
typedef struct mnd_md_scan_pos
{
    size_t pos;
    size_t column;
    size_t pad;
} mnd_md_scan_pos_t;

/*
 * cur から columns 桁分の空白（仮想の空白、空白、タブ）を取り除き、cur を進める。
 * 取り除いた桁数を返す（空白でない文字か end に達したら、columns より少なくなる）。
 * タブの一部だけを取り除く場合は、そのタブを越えて pos を進め（タブはプレフィックスに入る）、
 * 残りの桁数を pad に入れる（4.5）。cur->pad が 0 でなければ、まずそれを使う。
 */
size_t mnd_md_scan_skip_columns(const char *data, size_t end, mnd_md_scan_pos_t *cur, size_t columns);

/* mnd_md_scan_indent の結果 */
typedef struct mnd_md_scan_indent
{
    size_t pos;         /* 最初の空白でない文字の位置（空白だけの行では end） */
    size_t column;      /* pos の桁 */
    size_t indent;      /* cur の論理的な桁から column までの桁数（cur->pad を含む） */
    bool blank;         /* 行の残りが空白（空白とタブ）だけ、または空 */
} mnd_md_scan_indent_t;

/* cur から、行の残りのインデント（空白とタブの桁数）と最初の空白でない文字を求める。cur は変更しない */
void mnd_md_scan_indent(const char *data, size_t end, const mnd_md_scan_pos_t *cur,
                        mnd_md_scan_indent_t *out);

/* ブロックの開始の判定（pos はインデントを除いた最初の空白でない文字） */

/*
 * テーマ区切り: `*`、`-`、`_` のどれか 1 種類が 3 個以上、間と後ろに空白とタブだけ。
 * 真なら *ch にその文字を入れる（ch は NULL 可）
 */
bool mnd_md_scan_thematic_break(const char *data, size_t pos, size_t end, char *ch);

/*
 * ATX 見出しの開始: 1〜6 個の `#` のあとに空白、タブ、または行末。
 * 真なら out の level と content を設定し、setext を false にする（3.5）。content は、
 * 開きの並びの後ろの空白、閉じの並び（空白かタブが前にある `#` の並び。後ろは空白とタブだけ）、
 * その前後の空白を除いた範囲。内容が空なら、開きの並びの直後の位置の空の範囲。
 * バックスラッシュの直後の `#`（`\#`）は、前に空白がないので閉じの並びにならず、内容に残る
 */
bool mnd_md_scan_atx_heading(const char *data, size_t pos, size_t end, mnd_md_heading_t *out);

/*
 * Setext の下線: `=` が 1 個以上（レベル 1）か `-` が 1 個以上（レベル 2）で、後ろは空白とタブだけ。
 * レベルを返す。下線でなければ 0
 */
int mnd_md_scan_setext_underline(const char *data, size_t pos, size_t end);

/*
 * 開きフェンス: `` ` `` か `~` が 3 個以上。バッククォートのフェンスでは、行の残りに `` ` `` を
 * 含まないこと。真なら out の fenced、fence_char、fence_len、has_info、info（前後の空白とタブを
 * 除いた残り。エスケープは未処理）を設定し、fence_indent を 0、closed を false にする
 * （fence_indent は呼び出し側がインデントの桁数を入れる）
 */
bool mnd_md_scan_open_fence(const char *data, size_t pos, size_t end, mnd_md_code_block_t *out);

/* 閉じフェンス: fence_char が fence_len 個以上で、後ろは空白とタブだけ */
bool mnd_md_scan_close_fence(const char *data, size_t pos, size_t end, char fence_char,
                             size_t fence_len);

/* mnd_md_scan_list_marker の結果 */
typedef struct mnd_md_scan_list_marker
{
    bool ordered;
    char marker;            /* '-'、'+'、'*'、または順序付きの '.'、')'（mnd_md_list_t.marker と同じ） */
    unsigned long start;    /* 順序付きの開始番号（0〜999999999）。順序なしでは 0 */
    mnd_span_t span;        /* リスト記号（番号を含む）の範囲（3.6） */
    bool rest_blank;        /* 記号の後ろが空白とタブだけ、または行末（空のリスト項目の開始） */
} mnd_md_scan_list_marker_t;

/*
 * リスト記号: `-`、`+`、`*`、または 1〜9 桁の数字と `.` か `)`。記号の後ろは空白、タブ、または行末。
 * 段落を中断できる条件（空でないこと、順序付きでは開始番号が 1）と、リストを分ける判定は
 * 呼び出し側が rest_blank と ordered・start を使って行う
 */
bool mnd_md_scan_list_marker(const char *data, size_t pos, size_t end, mnd_md_scan_list_marker_t *out);

/*
 * リスト記号の後ろの空白の桁数（内容のインデント = 記号の終わりの桁 + この値）。
 * pos は記号の直後、column はその桁。記号の後ろの空白の桁数を W として、
 * 行の残りが空白だけなら 1、W が 5 以上なら 1（内容はインデントコードになりうる）、それ以外は W（1〜4）。
 * 内容の開始位置と pad は、この桁数で mnd_md_scan_skip_columns を呼んで求める
 */
size_t mnd_md_scan_list_padding(const char *data, size_t pos, size_t end, size_t column);

#endif
