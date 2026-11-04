/* LineBreak: UAX #14 のペアテーブルによる分割の機会の計算 */

#include "po/po_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 規則は UAX #14（Unicode 15.0）による。gettext 0.21 の msgcat と同じ位置で
 * 区切るため、libunistring 1.1 の結果と異なる箇所は、libunistring 1.1 に合わせて注記した
 * （結果の観察による。コードは参照していない）。
 */

/*
 * ペアテーブル。mnd_po_lb_pairs[前][後] は、前のクラス（空白を除く直前の文字の、解決した
 * クラス）と後のクラスの組について、
 *   'D': 直接に区切れる（空白をはさむ場合も区切れる）
 *   'I': 空白をはさむ場合だけ区切れる（LB18）
 *   'P': 空白をはさんでも区切れない
 *   '-': 表の外の文脈で処理するため参照しない（ZW: LB7・LB8、CM・ZWJ: LB9・LB10・LB8a）
 * を持つ。列の順序は行と同じ（OP CL CP QU GL NS EX SY IS PR PO NU AL HL ID IN HY BA BB B2
 * ZW CM WJ H2 H3 JL JV JT RI EB EM ZWJ）。
 *
 * すべての行に共通する規則:
 *   LB11 × WJ（WJ の列は P）、LB12a [^SP BA HY] × GL、LB13 × CL・CP・EX・IS・SY（P）、
 *   LB19 × QU、LB21 × BA・HY・NS、LB22 × IN。どの規則にも当たらない組は LB31 で D、
 *   空白をはさむ組は LB18 で I になる。行ごとの注記は、それ以外に効く規則を示す。
 */
static const char mnd_po_lb_pairs[32][33] = {
    /*        OCCQGNESIPPNAHIIHBBBZCWHHJJJREEZ */
    /*        PLPULSXYSROULLDNYAB2WMJ23LVTIBMW */
    "PPPPPPPPPPPPPPPPPPPP--PPPPPPPPP-", /* OP: LB14 OP SP* × */
    "DPPIIPPPPIIDDDDIIIDD--PDDDDDDDD-", /* CL: LB16 CL SP* × NS、LB25 CL × PO・PR */
    /*
     * CP: LB25 CP × PO・PR、LB30 CP × AL・HL・NU（Unicode 15.0 の CP に東アジアの幅
     * F・W・H の文字はない）。LB16 では CP SP* × NS だが、libunistring 1.1 に合わせて
     * NS の列を I にする
     */
    "DPPIIIPPPIIIIIDIIIDD--PDDDDDDDD-",
    "PPPIIIPPPIIIIIIIIIII--PIIIIIIII-", /* QU: LB19 QU ×、LB15 QU SP* × OP */
    "IPPIIIPPPIIIIIIIIIII--PIIIIIIII-", /* GL: LB12 GL × */
    "DPPIIIPPPDDDDDDIIIDD--PDDDDDDDD-", /* NS: 共通の規則だけ */
    "DPPIIIPPPDDDDDDIIIDD--PDDDDDDDD-", /* EX: 共通の規則だけ */
    "DPPIIIPPPDDIDIDIIIDD--PDDDDDDDD-", /* SY: LB25 SY × NU、LB21b SY × HL */
    /*
     * IS: LB25 IS × NU。LB29 では IS × AL・HL だが、libunistring 1.1 に合わせて
     * AL と HL の列を D にする（"a,b" の , の後ろで区切れる）
     */
    "DPPIIIPPPDDIDDDIIIDD--PDDDDDDDD-",
    /* PR: LB23a PR × ID・EB・EM、LB24 PR × AL・HL、LB25 PR × OP・NU、LB27 PR × JL・JV・JT・H2・H3 */
    "IPPIIIPPPDDIIIIIIIDD--PIIIIIDII-",
    "IPPIIIPPPDDIIIDIIIDD--PDDDDDDDD-", /* PO: LB24 PO × AL・HL、LB25 PO × OP・NU */
    "IPPIIIPPPIIIIIDIIIDD--PDDDDDDDD-", /* NU: LB23 NU × AL・HL、LB25 NU × PO・PR・NU、LB30 */
    /*
     * AL: LB23 AL × NU、LB24 AL × PR・PO、LB28 AL × AL・HL、LB30 AL × OP
     * （東アジアの幅 F・W・H の OP は表の外で D）
     */
    "IPPIIIPPPIIIIIDIIIDD--PDDDDDDDD-",
    "IPPIIIPPPIIIIIDIIIDD--PDDDDDDDD-", /* HL: AL と同じ（LB23、LB24、LB28、LB30） */
    "DPPIIIPPPDIDDDDIIIDD--PDDDDDDDD-", /* ID: LB23a ID × PO */
    "DPPIIIPPPDDDDDDIIIDD--PDDDDDDDD-", /* IN: 共通の規則だけ */
    "DPPIDIPPPDDIDDDIIIDD--PDDDDDDDD-", /* HY: LB12a の例外で HY ÷ GL、LB25 HY × NU */
    "DPPIDIPPPDDDDDDIIIDD--PDDDDDDDD-", /* BA: LB12a の例外で BA ÷ GL */
    "IPPIIIPPPIIIIIIIIIII--PIIIIIIII-", /* BB: LB21 BB × */
    "DPPIIIPPPDDDDDDIIIDP--PDDDDDDDD-", /* B2: LB17 B2 SP* × B2 */
    "--------------------------------", /* ZW: LB8 で表の外 */
    "--------------------------------", /* CM: LB9・LB10 で表の外 */
    "IPPIIIPPPIIIIIIIIIII--PIIIIIIII-", /* WJ: LB11 WJ × */
    "DPPIIIPPPDIDDDDIIIDD--PDDDIIDDD-", /* H2: LB26 H2 × JV・JT、LB27 H2 × PO */
    "DPPIIIPPPDIDDDDIIIDD--PDDDDIDDD-", /* H3: LB26 H3 × JT、LB27 H3 × PO */
    "DPPIIIPPPDIDDDDIIIDD--PIIIIDDDD-", /* JL: LB26 JL × JL・JV・H2・H3、LB27 JL × PO */
    "DPPIIIPPPDIDDDDIIIDD--PDDDIIDDD-", /* JV: LB26 JV × JV・JT、LB27 JV × PO */
    "DPPIIIPPPDIDDDDIIIDD--PDDDDIDDD-", /* JT: LB26 JT × JT、LB27 JT × PO */
    "DPPIIIPPPDDDDDDIIIDD--PDDDDDDDD-", /* RI: RI と RI の組は表の外（LB30a） */
    "DPPIIIPPPDIDDDDIIIDD--PDDDDDDDI-", /* EB: LB23a EB × PO、LB30b EB × EM */
    "DPPIIIPPPDIDDDDIIIDD--PDDDDDDDD-", /* EM: LB23a EM × PO */
    "--------------------------------"  /* ZWJ: LB8a・LB9 で表の外 */
};

/*
 * 一般カテゴリが Cn（未割り当て）で Extended_Pictographic のコードポイントの範囲
 * （UCD 15.0.0 の emoji/emoji-data.txt と extracted/DerivedGeneralCategory.txt から導いた）。
 * LB30b により、これらの後ろの EM の前では区切らない。行分割クラスはすべて ID
 */
static const mnd_po_ucd_range_t mnd_po_lb_unassigned_pictographic[] = {
    {0x1F02C, 0x1F02F, 1}, {0x1F094, 0x1F09F, 1}, {0x1F0AF, 0x1F0B0, 1},
    {0x1F0C0, 0x1F0C0, 1}, {0x1F0D0, 0x1F0D0, 1}, {0x1F0F6, 0x1F0FF, 1},
    {0x1F1AE, 0x1F1E5, 1}, {0x1F203, 0x1F20F, 1}, {0x1F23C, 0x1F23F, 1},
    {0x1F249, 0x1F24F, 1}, {0x1F252, 0x1F25F, 1}, {0x1F266, 0x1F2FF, 1},
    {0x1F6D8, 0x1F6DB, 1}, {0x1F6ED, 0x1F6EF, 1}, {0x1F6FD, 0x1F6FF, 1},
    {0x1F777, 0x1F77A, 1}, {0x1F7DA, 0x1F7DF, 1}, {0x1F7EC, 0x1F7EF, 1},
    {0x1F7F1, 0x1F7FF, 1}, {0x1F80C, 0x1F80F, 1}, {0x1F848, 0x1F84F, 1},
    {0x1F85A, 0x1F85F, 1}, {0x1F888, 0x1F88F, 1}, {0x1F8AE, 0x1F8AF, 1},
    {0x1F8B2, 0x1F8FF, 1}, {0x1FA54, 0x1FA5F, 1}, {0x1FA6E, 0x1FA6F, 1},
    {0x1FA7D, 0x1FA7F, 1}, {0x1FA89, 0x1FA8F, 1}, {0x1FABE, 0x1FABE, 1},
    {0x1FAC6, 0x1FACD, 1}, {0x1FADC, 0x1FADF, 1}, {0x1FAE9, 0x1FAEF, 1},
    {0x1FAF9, 0x1FAFF, 1}, {0x1FC00, 0x1FFFD, 1}
};

static bool mnd_po_lb_is_unassigned_pictographic(uint32_t cp)
{
    size_t lo = 0;
    size_t hi = sizeof(mnd_po_lb_unassigned_pictographic)
                / sizeof(mnd_po_lb_unassigned_pictographic[0]);

    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (cp < mnd_po_lb_unassigned_pictographic[mid].first)
        {
            hi = mid;
        }
        else if (cp > mnd_po_lb_unassigned_pictographic[mid].last)
        {
            lo = mid + 1;
        }
        else
        {
            return true;
        }
    }

    return false;
}

/*
 * LB30 の例外: 東アジアの幅が F・W・H の OP の前では、AL・HL・NU の後ろでも区切れる。
 * UCD 15.0.0 の OP のうち、F・W は文字幅 2 の文字、H は U+FF62 だけである
 */
static bool mnd_po_lb_is_east_asian_op(uint32_t cp)
{
    return (cp == 0xFF62) || (mnd_po_ucd_width(cp) == 2);
}

/*
 * LB1 のクラスの解決。AI・SG・XX → AL、SA → AL（Mn・Mc は CM）、CJ → NS。
 * CB は、LB20 では前後で区切れるが、libunistring 1.1 に合わせて ID とする
 * （PR × ID、ID × PO が効く）。
 * なお libunistring 1.1 は SA の Mn・Mc も AL とするが、設計に従い CM とする。結果が異なるのは、
 * SA の結合文字が空白や AL 以外のクラスの文字の直後にある場合（通常の文では現れない）だけである
 */
static mnd_po_lb_t mnd_po_lb_resolve(uint32_t cp)
{
    mnd_po_lb_t cls = mnd_po_ucd_lb(cp);

    switch (cls)
    {
    case MND_PO_LB_AI:
    case MND_PO_LB_SG:
    case MND_PO_LB_XX:
        return MND_PO_LB_AL;
    case MND_PO_LB_SA:
        return mnd_po_ucd_is_mark(cp) ? MND_PO_LB_CM : MND_PO_LB_AL;
    case MND_PO_LB_CJ:
        return MND_PO_LB_NS;
    case MND_PO_LB_CB:
        return MND_PO_LB_ID;
    default:
        return cls;
    }
}

/*
 * s[0..len) の位置 i から 1 文字を読み、コードポイントを *cp に入れてバイト数を返す。
 * 入力は正しい UTF-8 の前提だが、不正な先頭バイトや途中で切れた文字は 1 バイトの文字
 * （U+FFFD とみなす）として扱い、範囲外を読まない
 */
static size_t mnd_po_lb_decode(const unsigned char *s, size_t len, size_t i, uint32_t *cp)
{
    unsigned char c = s[i];
    size_t n = 0;
    uint32_t value = 0;
    size_t k = 0;

    if (c < 0x80)
    {
        *cp = c;

        return 1;
    }
    if ((c & 0xE0) == 0xC0)
    {
        n = 2;
        value = c & 0x1F;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        n = 3;
        value = c & 0x0F;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        n = 4;
        value = c & 0x07;
    }
    else
    {
        *cp = 0xFFFD;

        return 1;
    }
    if (n > (len - i))
    {
        *cp = 0xFFFD;

        return 1;
    }
    for (k = 1; k < n; k++)
    {
        if ((s[i + k] & 0xC0) != 0x80)
        {
            *cp = 0xFFFD;

            return 1;
        }
        value = (value << 6) | (uint32_t)(s[i + k] & 0x3F);
    }
    *cp = value;

    return n;
}

/* 区切る行の始め（先頭、または BK・CR・LF・NL の後ろ）を表す、前のクラスの値 */
#define MND_PO_LB_START MND_PO_LB_BK

void mnd_po_linebreaks(const char *s, size_t len, unsigned char *breaks)
{
    const unsigned char *u = (const unsigned char *)s;
    /* 空白を除く直前の文字のクラス（LB9 で CM・ZWJ は前の文字に含める） */
    mnd_po_lb_t last = MND_PO_LB_START;
    /* すぐ前の文字の解決したクラス */
    mnd_po_lb_t prev = MND_PO_LB_START;
    /* last の後ろに空白があるか */
    bool space = false;
    /* すぐ前の 2 文字が HL と（HY または BA）か（LB21a） */
    bool hl_hyphen = false;
    /* LB30b: last が未割り当ての Extended_Pictographic か */
    bool last_pictographic = false;
    /* すぐ前に続く RI の個数（LB30a） */
    size_t ri_count = 0;
    size_t i = 0;

    while (i < len)
    {
        uint32_t cp = 0;
        size_t n = mnd_po_lb_decode(u, len, i, &cp);
        mnd_po_lb_t cls = mnd_po_lb_resolve(cp);
        bool brk = false;
        bool next_hl_hyphen = false;
        size_t k = 0;

        if ((cls == MND_PO_LB_BK) || (cls == MND_PO_LB_CR) || (cls == MND_PO_LB_LF)
            || (cls == MND_PO_LB_NL))
        {
            /*
             * LB6: 改行の文字の前では区切らない。LB4・LB5 の後ろの区切りは、改行そのものとして
             * 呼び出し側（Wrap）が扱う。ここでは次の行の始めとする（LB2 と同じく区切らない）
             */
            last = MND_PO_LB_START;
            space = false;
            last_pictographic = false;
        }
        else if (cls == MND_PO_LB_SP)
        {
            /* LB7: 空白の前では区切らない */
            space = true;
        }
        else if (cls == MND_PO_LB_ZW)
        {
            /* LB7: ZW の前では区切らない */
            last = MND_PO_LB_ZW;
            space = false;
            last_pictographic = false;
        }
        else if ((cls == MND_PO_LB_CM) || (cls == MND_PO_LB_ZWJ))
        {
            if (last == MND_PO_LB_START)
            {
                /* LB2（行の始め）、LB10: AL として扱う */
                last = MND_PO_LB_AL;
                space = false;
                last_pictographic = false;
            }
            else if ((last == MND_PO_LB_ZW) || space)
            {
                /*
                 * LB8: ZW の後ろ、LB18: 空白の後ろで区切れる。LB10: AL として扱う。
                 * libunistring 1.1 に合わせ、OP SP* の後ろの CM・ZWJ の前でも区切る（LB14 を適用しない）
                 */
                brk = true;
                last = MND_PO_LB_AL;
                space = false;
                last_pictographic = false;
            }
            /* それ以外は LB9: 前の文字に含めて、区切らない */
        }
        else
        {
            if (last == MND_PO_LB_START)
            {
                /* LB2: 行の始めでは区切らない */
                brk = false;
            }
            else if (last == MND_PO_LB_ZW)
            {
                /* LB8: ZW SP* ÷ */
                brk = true;
            }
            else if (prev == MND_PO_LB_ZWJ)
            {
                /* LB8a: ZWJ × */
                brk = false;
            }
            else if ((last == MND_PO_LB_RI) && (cls == MND_PO_LB_RI))
            {
                /*
                 * LB30a: 前に続く RI が偶数個なら区切れる。空白をはさめば LB18 で区切れる。
                 * libunistring 1.1 に合わせて、RI の数は間に CM がない連続だけを数える
                 */
                brk = space || ((ri_count % 2) == 0);
            }
            else if (hl_hyphen)
            {
                /*
                 * LB21a: HL (HY | BA) ×。libunistring 1.1 に合わせて、3 文字が間に CM なしで
                 * 続く場合だけとする
                 */
                brk = false;
            }
            else if (last_pictographic && (cls == MND_PO_LB_EM) && !space)
            {
                /* LB30b: [\p{Extended_Pictographic}&\p{Cn}] × EM */
                brk = false;
            }
            else if ((cls == MND_PO_LB_OP) && !space
                     && ((last == MND_PO_LB_AL) || (last == MND_PO_LB_HL)
                         || (last == MND_PO_LB_NU))
                     && mnd_po_lb_is_east_asian_op(cp))
            {
                /* LB30 の例外（東アジアの幅 F・W・H の OP）: LB31 で区切れる */
                brk = true;
            }
            else
            {
                char pair = mnd_po_lb_pairs[last][cls];

                brk = (pair == 'D') || ((pair == 'I') && space);
            }
            next_hl_hyphen = (prev == MND_PO_LB_HL)
                             && ((cls == MND_PO_LB_HY) || (cls == MND_PO_LB_BA));
            last = cls;
            space = false;
            last_pictographic = mnd_po_lb_is_unassigned_pictographic(cp);
        }

        breaks[i] = brk ? 1 : 0;
        for (k = 1; k < n; k++)
        {
            breaks[i + k] = 0;
        }
        hl_hyphen = next_hl_hyphen;
        ri_count = (cls == MND_PO_LB_RI) ? (ri_count + 1) : 0;
        prev = cls;
        i += n;
    }
}
