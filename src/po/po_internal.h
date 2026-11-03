#ifndef MONDE_PO_PO_INTERNAL_H
#define MONDE_PO_PO_INTERNAL_H

/* モジュール内部の宣言。src/po/ の .c と tests/po/ だけが include する */

#include "foundation/buf.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 重複の検出（po_entry.c） */

/* from 以降で最初に見つかった、廃止エントリでない重複の組（first < second）を返す。なければ false */
bool mnd_po_find_duplicate(const mnd_po_catalog_t *catalog, size_t from, size_t *first,
                           size_t *second);

/* Escape（po_escape.c） */

typedef enum mnd_po_unescape_status
{
    MND_PO_UNESCAPE_OK,
    MND_PO_UNESCAPE_BAD_ESCAPE,  /* *bad_offset に \ の位置 */
    MND_PO_UNESCAPE_NUL,         /* 結果に NUL が含まれる */
    MND_PO_UNESCAPE_BAD_UTF8,    /* 結果が UTF-8 として不正 */
    MND_PO_UNESCAPE_NO_MEMORY
} mnd_po_unescape_status_t;

/* 引用符の内側の lit[0..len) をアンエスケープして out の末尾に加える */
mnd_po_unescape_status_t mnd_po_unescape(const char *lit, size_t len, mnd_buf_t *out,
                                         size_t *bad_offset);
/* s[0..len) をエスケープして out の末尾に加える */
bool mnd_po_escape(const char *s, size_t len, mnd_buf_t *out);

/* Wrap（po_wrap.c） */

/*
 * prefix（"", "#~ ", "#| ", "#~| "）と keyword（"msgid", "msgstr[1]" など）で s を整形し、
 * 各行の内容を eol で区切って out に加える。wrap が偽なら \n の直後でだけ区切る
 */
bool mnd_po_wrap_string(const char *prefix, const char *keyword, const char *s, size_t len,
                        bool wrap, const char *eol, mnd_buf_t *out);

/* LineBreak と Ucd（po_linebreak.c、po_ucd.c、po_ucd_tables.c） */

/* 行分割クラス。OP〜ZWJ はペアテーブルの添字、SP 以降は解決前または文脈で処理するクラス */
typedef enum mnd_po_lb
{
    MND_PO_LB_OP, MND_PO_LB_CL, MND_PO_LB_CP, MND_PO_LB_QU, MND_PO_LB_GL, MND_PO_LB_NS,
    MND_PO_LB_EX, MND_PO_LB_SY, MND_PO_LB_IS, MND_PO_LB_PR, MND_PO_LB_PO, MND_PO_LB_NU,
    MND_PO_LB_AL, MND_PO_LB_HL, MND_PO_LB_ID, MND_PO_LB_IN, MND_PO_LB_HY, MND_PO_LB_BA,
    MND_PO_LB_BB, MND_PO_LB_B2, MND_PO_LB_ZW, MND_PO_LB_CM, MND_PO_LB_WJ, MND_PO_LB_H2,
    MND_PO_LB_H3, MND_PO_LB_JL, MND_PO_LB_JV, MND_PO_LB_JT, MND_PO_LB_RI, MND_PO_LB_EB,
    MND_PO_LB_EM, MND_PO_LB_ZWJ,
    MND_PO_LB_SP, MND_PO_LB_BK, MND_PO_LB_CR, MND_PO_LB_LF, MND_PO_LB_NL,
    MND_PO_LB_AI, MND_PO_LB_SA, MND_PO_LB_CJ, MND_PO_LB_SG, MND_PO_LB_XX
} mnd_po_lb_t;

mnd_po_lb_t mnd_po_ucd_lb(uint32_t cp);  /* 解決前のクラス */
int mnd_po_ucd_width(uint32_t cp);       /* 0、1、2 */

/*
 * s[0..len)（正しい UTF-8）について、breaks[i] に「バイト位置 i の直前で区切れるなら 1」を入れる。
 * breaks は len 要素。文字の途中の位置と i == 0 は常に 0
 */
void mnd_po_linebreaks(const char *s, size_t len, unsigned char *breaks);

/*
 * Ucd の表（po_ucd_tables.c。tools/gen_po_ucd.py が生成する）。
 * 各表はコードポイントの範囲 [first, last] を first の昇順に、重ならずに並べる。
 * 要素数は *_count で表す（配列の要素数ではない）
 */
typedef struct mnd_po_ucd_range
{
    uint32_t first;
    uint32_t last;
    unsigned char value;
} mnd_po_ucd_range_t;

/* value は mnd_po_lb_t。表にないコードポイントのクラスは po_ucd.c が決める */
extern const mnd_po_ucd_range_t mnd_po_ucd_lb_ranges[];
extern const size_t mnd_po_ucd_lb_count;
/* value は文字幅（0 または 2）。表にないコードポイントの幅は po_ucd.c が決める */
extern const mnd_po_ucd_range_t mnd_po_ucd_width_ranges[];
extern const size_t mnd_po_ucd_width_count;

#endif
