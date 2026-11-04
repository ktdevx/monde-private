#include <naos/naos.h>

#include "po/po_internal.h"

#include <string.h>

/*
 * s の分割の機会を、区切れる位置の直前に '|' を入れた文字列にして out に書く。
 * 文字の途中（UTF-8 の継続バイト）で区切れる場合は '!' を入れて、テストが失敗するようにする
 */
static const char *lb_mark(const char *s, char *out, size_t cap)
{
    unsigned char breaks[256];
    size_t len = strlen(s);
    size_t o = 0;
    size_t i = 0;

    memset(breaks, 0xAA, sizeof(breaks));
    if ((len > sizeof(breaks)) || (((len * 2) + 1) > cap))
    {
        return "(too long)";
    }
    mnd_po_linebreaks(s, len, breaks);
    for (i = 0; i < len; i++)
    {
        if (breaks[i] > 1)
        {
            out[o] = '?';
            o++;
        }
        else if (breaks[i] == 1)
        {
            out[o] = ((((unsigned char)s[i]) & 0xC0) == 0x80) ? '!' : '|';
            o++;
        }
        out[o] = s[i];
        o++;
    }
    out[o] = '\0';

    return out;
}

#define LB_CHECK(expected, input)                                          \
    do                                                                     \
    {                                                                      \
        char lb_out_[512];                                                 \
        NAOS_CHECK_STR_EQ((expected), lb_mark((input), lb_out_, sizeof(lb_out_))); \
    } while (0)

NAOS_TEST(linebreak_pair_table_classes_come_first)
{
    /* ペアテーブルの 32 クラス（OP〜ZWJ）が先に並び、解決前だけのクラスがその後ろに続く */
    NAOS_CHECK_INT_EQ(0, MND_PO_LB_OP);
    NAOS_CHECK_INT_EQ(31, MND_PO_LB_ZWJ);
    NAOS_CHECK_INT_EQ(32, MND_PO_LB_SP);
    NAOS_CHECK_INT_EQ(41, MND_PO_LB_XX);
    NAOS_CHECK_INT_EQ(42, MND_PO_LB_CB);
}

NAOS_TEST(linebreak_width_of_representative_characters)
{
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x0061));   /* a */
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0x3042));   /* あ（W） */
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0xFF21));   /* Ａ（F） */
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0x1F600));  /* 😀（W） */
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x0301));   /* 結合文字（Mn） */
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x2026));   /* …（A） */
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x0488));   /* Me */
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x200B));   /* ZERO WIDTH SPACE（Cf） */
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x0903));   /* Mc は 0 にしない */
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x00A0));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0xFF61));   /* ｡（H） */
}

NAOS_TEST(linebreak_width_of_controls_and_hangul_jungseong)
{
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x0000));
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x0009));
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x001F));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x0020));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x007E));
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x007F));
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x009F));
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0x1100));   /* 初声（W） */
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x1160));   /* 中声と終声は 0 */
    NAOS_CHECK_INT_EQ(0, mnd_po_ucd_width(0x11FF));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x1200));
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0xAC00));
}

NAOS_TEST(linebreak_width_of_unassigned_code_points)
{
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x0378));   /* 既定値 N */
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0xFADA));   /* CJK 互換漢字の未割り当て（W） */
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0x2A6E0));  /* 第 2 面の未割り当て（W） */
    NAOS_CHECK_INT_EQ(2, mnd_po_ucd_width(0x3FFFD));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x3FFFE));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x10FFFF));
    NAOS_CHECK_INT_EQ(1, mnd_po_ucd_width(0x110000)); /* 範囲外 */
}

NAOS_TEST(linebreak_class_of_ascii_characters)
{
    NAOS_CHECK_INT_EQ(MND_PO_LB_SP, mnd_po_ucd_lb(0x0020));
    NAOS_CHECK_INT_EQ(MND_PO_LB_AL, mnd_po_ucd_lb(0x0061));  /* a */
    NAOS_CHECK_INT_EQ(MND_PO_LB_OP, mnd_po_ucd_lb(0x0028));  /* ( */
    NAOS_CHECK_INT_EQ(MND_PO_LB_CP, mnd_po_ucd_lb(0x0029));  /* ) */
    NAOS_CHECK_INT_EQ(MND_PO_LB_HY, mnd_po_ucd_lb(0x002D));  /* - */
    NAOS_CHECK_INT_EQ(MND_PO_LB_NU, mnd_po_ucd_lb(0x0030));  /* 0 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_PO, mnd_po_ucd_lb(0x0025));  /* % */
    NAOS_CHECK_INT_EQ(MND_PO_LB_PR, mnd_po_ucd_lb(0x0024));  /* $ */
    NAOS_CHECK_INT_EQ(MND_PO_LB_EX, mnd_po_ucd_lb(0x0021));  /* ! */
    NAOS_CHECK_INT_EQ(MND_PO_LB_SY, mnd_po_ucd_lb(0x002F));  /* / */
    NAOS_CHECK_INT_EQ(MND_PO_LB_IS, mnd_po_ucd_lb(0x002C));  /* , */
    NAOS_CHECK_INT_EQ(MND_PO_LB_QU, mnd_po_ucd_lb(0x0022));  /* " */
    NAOS_CHECK_INT_EQ(MND_PO_LB_BA, mnd_po_ucd_lb(0x0009));
    NAOS_CHECK_INT_EQ(MND_PO_LB_LF, mnd_po_ucd_lb(0x000A));
    NAOS_CHECK_INT_EQ(MND_PO_LB_BK, mnd_po_ucd_lb(0x000B));
    NAOS_CHECK_INT_EQ(MND_PO_LB_CR, mnd_po_ucd_lb(0x000D));
    NAOS_CHECK_INT_EQ(MND_PO_LB_CM, mnd_po_ucd_lb(0x0000));
}

NAOS_TEST(linebreak_class_of_non_ascii_characters)
{
    NAOS_CHECK_INT_EQ(MND_PO_LB_NL, mnd_po_ucd_lb(0x0085));
    NAOS_CHECK_INT_EQ(MND_PO_LB_GL, mnd_po_ucd_lb(0x00A0));
    NAOS_CHECK_INT_EQ(MND_PO_LB_AI, mnd_po_ucd_lb(0x00A7));  /* § */
    NAOS_CHECK_INT_EQ(MND_PO_LB_BB, mnd_po_ucd_lb(0x00B4));
    NAOS_CHECK_INT_EQ(MND_PO_LB_CM, mnd_po_ucd_lb(0x0301));
    NAOS_CHECK_INT_EQ(MND_PO_LB_HL, mnd_po_ucd_lb(0x05D0));  /* א */
    NAOS_CHECK_INT_EQ(MND_PO_LB_SA, mnd_po_ucd_lb(0x0E01));  /* ก */
    NAOS_CHECK_INT_EQ(MND_PO_LB_ZW, mnd_po_ucd_lb(0x200B));
    NAOS_CHECK_INT_EQ(MND_PO_LB_ZWJ, mnd_po_ucd_lb(0x200D));
    NAOS_CHECK_INT_EQ(MND_PO_LB_B2, mnd_po_ucd_lb(0x2014));  /* — */
    NAOS_CHECK_INT_EQ(MND_PO_LB_IN, mnd_po_ucd_lb(0x2026));  /* … */
    NAOS_CHECK_INT_EQ(MND_PO_LB_WJ, mnd_po_ucd_lb(0x2060));
    NAOS_CHECK_INT_EQ(MND_PO_LB_SG, mnd_po_ucd_lb(0xD800));
    NAOS_CHECK_INT_EQ(MND_PO_LB_CB, mnd_po_ucd_lb(0xFFFC));
}

NAOS_TEST(linebreak_class_of_cjk_characters)
{
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0x3042));  /* あ */
    NAOS_CHECK_INT_EQ(MND_PO_LB_CJ, mnd_po_ucd_lb(0x3063));  /* っ */
    NAOS_CHECK_INT_EQ(MND_PO_LB_CJ, mnd_po_ucd_lb(0x30FC));  /* ー */
    NAOS_CHECK_INT_EQ(MND_PO_LB_NS, mnd_po_ucd_lb(0x3005));  /* 々 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_CL, mnd_po_ucd_lb(0x3002));  /* 。 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_CL, mnd_po_ucd_lb(0x3001));  /* 、 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_OP, mnd_po_ucd_lb(0x300C));  /* 「 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_CL, mnd_po_ucd_lb(0x300D));  /* 」 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_OP, mnd_po_ucd_lb(0xFF08));  /* （ */
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0x6F22));  /* 漢 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_JL, mnd_po_ucd_lb(0x1100));
    NAOS_CHECK_INT_EQ(MND_PO_LB_JV, mnd_po_ucd_lb(0x1160));
    NAOS_CHECK_INT_EQ(MND_PO_LB_JT, mnd_po_ucd_lb(0x11A8));
    NAOS_CHECK_INT_EQ(MND_PO_LB_H2, mnd_po_ucd_lb(0xAC00));  /* 가 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_H3, mnd_po_ucd_lb(0xAC01));  /* 각 */
}

NAOS_TEST(linebreak_class_of_emoji)
{
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0x1F600));  /* 😀 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_EB, mnd_po_ucd_lb(0x1F466));  /* 👦 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_EM, mnd_po_ucd_lb(0x1F3FB));  /* 肌の色の修飾子 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_RI, mnd_po_ucd_lb(0x1F1E6));
    NAOS_CHECK_INT_EQ(MND_PO_LB_RI, mnd_po_ucd_lb(0x1F1FF));
}

NAOS_TEST(linebreak_class_of_unassigned_code_points)
{
    NAOS_CHECK_INT_EQ(MND_PO_LB_XX, mnd_po_ucd_lb(0x0378));
    NAOS_CHECK_INT_EQ(MND_PO_LB_XX, mnd_po_ucd_lb(0xE000));   /* 私用領域 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0xFADA));   /* CJK 互換漢字の未割り当て */
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0x2A6E0));  /* 第 2 面の未割り当て */
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0x3FFFD));
    NAOS_CHECK_INT_EQ(MND_PO_LB_XX, mnd_po_ucd_lb(0x3FFFE));
    NAOS_CHECK_INT_EQ(MND_PO_LB_ID, mnd_po_ucd_lb(0x1FFFD));  /* 第 1 面の絵文字の範囲 */
    NAOS_CHECK_INT_EQ(MND_PO_LB_PR, mnd_po_ucd_lb(0x20C1));   /* 通貨記号の未割り当て */
    NAOS_CHECK_INT_EQ(MND_PO_LB_XX, mnd_po_ucd_lb(0x10FFFF));
    NAOS_CHECK_INT_EQ(MND_PO_LB_XX, mnd_po_ucd_lb(0x110000)); /* 範囲外 */
}

NAOS_TEST(linebreak_is_mark_for_mn_and_mc)
{
    NAOS_CHECK_TRUE(mnd_po_ucd_is_mark(0x0301));   /* Mn */
    NAOS_CHECK_TRUE(mnd_po_ucd_is_mark(0x0903));   /* Mc */
    NAOS_CHECK_TRUE(mnd_po_ucd_is_mark(0x0E31));   /* タイ文字の母音記号（Mn、SA） */
    NAOS_CHECK_TRUE(mnd_po_ucd_is_mark(0x1A55));   /* タイ・タム文字（Mc、SA） */
    NAOS_CHECK_FALSE(mnd_po_ucd_is_mark(0x0E01));  /* Lo、SA */
    NAOS_CHECK_FALSE(mnd_po_ucd_is_mark(0x0061));
    NAOS_CHECK_FALSE(mnd_po_ucd_is_mark(0x0488));  /* Me */
    NAOS_CHECK_FALSE(mnd_po_ucd_is_mark(0x0378));
    NAOS_CHECK_FALSE(mnd_po_ucd_is_mark(0x110000));
}

NAOS_TEST(linebreak_after_spaces)
{
    LB_CHECK("ab |cd", "ab cd");
    LB_CHECK("ab   |cd", "ab   cd");
    LB_CHECK("a\t|b", "a\tb");            /* タブは BA */
    LB_CHECK("  a", "  a");               /* 先頭の空白の後ろでは区切らない */
    LB_CHECK("ab  ", "ab  ");             /* 空白の前では区切らない（LB7） */
    LB_CHECK("a |( b", "a ( b");          /* OP SP* ×（LB14） */
    LB_CHECK("a )", "a )");               /* × CP（LB13） */
}

NAOS_TEST(linebreak_after_hyphen)
{
    LB_CHECK("well-|known", "well-known");
    LB_CHECK("a-|b-|c", "a-b-c");
    LB_CHECK("-5", "-5");                 /* HY × NU（LB25） */
    LB_CHECK("a--|b", "a--b");
    LB_CHECK("a|\xE2\x80\x94|b", "a\xE2\x80\x94" "b");        /* — の前後（B2） */
    LB_CHECK("\xE2\x80\x94\xE2\x80\x94", "\xE2\x80\x94\xE2\x80\x94"); /* B2 × B2（LB17） */
}

NAOS_TEST(linebreak_between_cjk_and_korean)
{
    /* あいう */
    LB_CHECK("\xE3\x81\x82|\xE3\x81\x84|\xE3\x81\x86", "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86");
    /* 日本語 */
    LB_CHECK("\xE6\x97\xA5|\xE6\x9C\xAC|\xE8\xAA\x9E", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    /* ひらがなと漢字の間 */
    LB_CHECK("\xE3\x81\xAE|\xE6\x97\xA5", "\xE3\x81\xAE\xE6\x97\xA5");
    /* 한국어 */
    LB_CHECK("\xED\x95\x9C|\xEA\xB5\xAD|\xEC\x96\xB4", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4");
    /* 字母の連なり（JL JV JT）は 1 つの音節として区切らない（LB26） */
    LB_CHECK("\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8|\xE1\x84\x80",
             "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8\xE1\x84\x80");
    /* 英字と漢字の間、全角英字の間 */
    LB_CHECK("a|\xE6\x97\xA5|b", "a\xE6\x97\xA5" "b");
    LB_CHECK("\xEF\xBC\xA1|\xEF\xBC\xA2", "\xEF\xBC\xA1\xEF\xBC\xA2");
}

NAOS_TEST(linebreak_not_before_closing_punctuation)
{
    /* で|す。、」の前（CL）、あ、い */
    LB_CHECK("\xE3\x81\xA7|\xE3\x81\x99\xE3\x80\x82", "\xE3\x81\xA7\xE3\x81\x99\xE3\x80\x82");
    LB_CHECK("\xE3\x81\x82\xE3\x80\x81|\xE3\x81\x84", "\xE3\x81\x82\xE3\x80\x81\xE3\x81\x84");
    /* 「あい」。 */
    LB_CHECK("\xE3\x80\x8C\xE3\x81\x82|\xE3\x81\x84\xE3\x80\x8D\xE3\x80\x82",
             "\xE3\x80\x8C\xE3\x81\x82\xE3\x81\x84\xE3\x80\x8D\xE3\x80\x82");
    /* 」の後ろでは区切れる */
    LB_CHECK("\xE3\x80\x8D|\xE3\x81\x82", "\xE3\x80\x8D\xE3\x81\x82");
    LB_CHECK("a.", "a.");
    LB_CHECK("a!", "a!");
}

NAOS_TEST(linebreak_not_before_small_kana)
{
    /* あっ|た: っ（CJ → NS）の前では区切らない */
    LB_CHECK("\xE3\x81\x82\xE3\x81\xA3|\xE3\x81\x9F", "\xE3\x81\x82\xE3\x81\xA3\xE3\x81\x9F");
    /* カー|ド: ー（CJ） */
    LB_CHECK("\xE3\x82\xAB\xE3\x83\xBC|\xE3\x83\x89", "\xE3\x82\xAB\xE3\x83\xBC\xE3\x83\x89");
    /* 人々: 々（NS） */
    LB_CHECK("\xE4\xBA\xBA\xE3\x80\x85", "\xE4\xBA\xBA\xE3\x80\x85");
}

NAOS_TEST(linebreak_not_after_opening_bracket)
{
    /* （あ）: （ の後ろと ） の前では区切らない */
    LB_CHECK("\xEF\xBC\x88\xE3\x81\x82\xEF\xBC\x89", "\xEF\xBC\x88\xE3\x81\x82\xEF\xBC\x89");
    /* 「あ */
    LB_CHECK("\xE3\x80\x8C\xE3\x81\x82", "\xE3\x80\x8C\xE3\x81\x82");
    /* あ（: 開き括弧の前では区切れる */
    LB_CHECK("\xE3\x81\x82|\xEF\xBC\x88", "\xE3\x81\x82\xEF\xBC\x88");
    /* a（b: 全角の OP の前は LB30 の例外で区切れ、半角の ( の前は区切れない */
    LB_CHECK("a|\xEF\xBC\x88" "b", "a\xEF\xBC\x88" "b");
    LB_CHECK("a(b)", "a(b)");
    LB_CHECK("(a)", "(a)");
}

NAOS_TEST(linebreak_not_between_digit_and_percent)
{
    LB_CHECK("10%", "10%");
    LB_CHECK("$10", "$10");
    LB_CHECK("1.5", "1.5");
    LB_CHECK("10 |%", "10 %");            /* 空白をはさめば区切れる */
    LB_CHECK("a1b", "a1b");               /* LB23 */
    /* 100％（全角の % は PO） */
    LB_CHECK("100\xEF\xBC\x85", "100\xEF\xBC\x85");
}

NAOS_TEST(linebreak_emoji_modifier_and_zwj)
{
    /* 👦🏻（EB × EM、LB30b）、👦🏻👦 */
    LB_CHECK("\xF0\x9F\x91\xA6\xF0\x9F\x8F\xBB", "\xF0\x9F\x91\xA6\xF0\x9F\x8F\xBB");
    LB_CHECK("\xF0\x9F\x91\xA6\xF0\x9F\x8F\xBB|\xF0\x9F\x91\xA6",
             "\xF0\x9F\x91\xA6\xF0\x9F\x8F\xBB\xF0\x9F\x91\xA6");
    /* 😀 は EB でないため、修飾子の前で区切れる */
    LB_CHECK("\xF0\x9F\x98\x80|\xF0\x9F\x8F\xBB", "\xF0\x9F\x98\x80\xF0\x9F\x8F\xBB");
    /* 未割り当ての Extended_Pictographic（U+1FFFD）の後ろの修飾子の前では区切らない */
    LB_CHECK("\xF0\x9F\xBF\xBD\xF0\x9F\x8F\xBB", "\xF0\x9F\xBF\xBD\xF0\x9F\x8F\xBB");
    /* 👨‍👩: ZWJ の後ろでは区切らない（LB8a）、ZWJ の前も区切らない（LB9） */
    LB_CHECK("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9",
             "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9");
}

NAOS_TEST(linebreak_regional_indicator_pairs)
{
    /* 🇯🇵🇺🇸: 対の内部では区切らず、対の間で区切れる（LB30a） */
    LB_CHECK("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5|\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8",
             "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8");
    /* 3 つ目は新しい対の始め */
    LB_CHECK("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5|\xF0\x9F\x87\xBA",
             "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5\xF0\x9F\x87\xBA");
    /* 空白をはさめば区切れる */
    LB_CHECK("\xF0\x9F\x87\xAF |\xF0\x9F\x87\xB5", "\xF0\x9F\x87\xAF \xF0\x9F\x87\xB5");
}

NAOS_TEST(linebreak_combining_marks)
{
    /* e + U+0301 は前の文字に含める（LB9） */
    LB_CHECK("e\xCC\x81x", "e\xCC\x81x");
    /* あ + U+0301 + い: ID として扱う */
    LB_CHECK("\xE3\x81\x82\xCC\x81|\xE3\x81\x84", "\xE3\x81\x82\xCC\x81\xE3\x81\x84");
    /* 空白の後ろの結合文字の前で区切れ、AL として扱う（LB10） */
    LB_CHECK("a |\xCC\x81" "b", "a \xCC\x81" "b");
    /* 先頭の結合文字 */
    LB_CHECK("\xCC\x81" "a", "\xCC\x81" "a");
    /* 制御文字は CM */
    LB_CHECK("a\x01" "b", "a\x01" "b");
}

NAOS_TEST(linebreak_zero_width_space_and_word_joiner)
{
    LB_CHECK("a\xE2\x80\x8B|b", "a\xE2\x80\x8B" "b");           /* ZW ÷（LB8） */
    LB_CHECK("a\xE2\x80\x8B |b", "a\xE2\x80\x8B b");
    /* WJ */
    LB_CHECK("\xE3\x81\x82\xE2\x81\xA0\xE3\x81\x84", "\xE3\x81\x82\xE2\x81\xA0\xE3\x81\x84");
    LB_CHECK("a\xC2\xA0" "b", "a\xC2\xA0" "b");                 /* GL（NBSP） */
}

NAOS_TEST(linebreak_resolved_classes)
{
    /* SA: タイ文字は AL（กขค）、母音記号（Mn）は CM */
    LB_CHECK("\xE0\xB8\x81\xE0\xB8\x82\xE0\xB8\x84", "\xE0\xB8\x81\xE0\xB8\x82\xE0\xB8\x84");
    LB_CHECK("\xE0\xB8\x81\xE0\xB9\x88 |\xE0\xB8\x82", "\xE0\xB8\x81\xE0\xB9\x88 \xE0\xB8\x82");
    /* AI（§）は AL */
    LB_CHECK("a\xC2\xA7" "b", "a\xC2\xA7" "b");
    LB_CHECK("\xE3\x81\x82|\xC2\xA7", "\xE3\x81\x82\xC2\xA7");
    /* XX（私用領域 U+E000）は AL */
    LB_CHECK("a\xEE\x80\x80" "b", "a\xEE\x80\x80" "b");
    /* CB（U+FFFC）は libunistring 1.1 に合わせて ID */
    LB_CHECK("a|\xEF\xBF\xBC|b", "a\xEF\xBF\xBC" "b");
    LB_CHECK("$\xEF\xBF\xBC", "$\xEF\xBF\xBC");
    LB_CHECK("\xEF\xBF\xBC%", "\xEF\xBF\xBC%");
}

NAOS_TEST(linebreak_libunistring_compatible_rules)
{
    /* IS × AL（LB29）は使わず、, の後ろで区切れる */
    LB_CHECK("a,|b", "a,b");
    LB_CHECK("1,000", "1,000");
    /* CP SP* × NS（LB16）は CP では空白をはさむと区切れる */
    LB_CHECK("a) |\xE3\x80\x85", "a) \xE3\x80\x85");
    LB_CHECK("\xE3\x80\x8D \xE3\x80\x85", "\xE3\x80\x8D \xE3\x80\x85");
    /* HL (HY | BA) ×（LB21a）: א-א */
    LB_CHECK("\xD7\x90-\xD7\x90", "\xD7\x90-\xD7\x90");
    /* 間に結合文字がある RI は別の対として数える */
    LB_CHECK("\xF0\x9F\x87\xA6\xCC\x81|\xF0\x9F\x87\xA6",
             "\xF0\x9F\x87\xA6\xCC\x81\xF0\x9F\x87\xA6");
}

NAOS_TEST(linebreak_newlines_and_empty)
{
    unsigned char breaks[1] = {0xAA};

    /* 改行の前と、改行の後ろの行の始めでは区切らない（改行は Wrap が扱う） */
    LB_CHECK("ab\n", "ab\n");
    LB_CHECK("a |b\nc |d", "a b\nc d");
    LB_CHECK("a\r\nb", "a\r\nb");
    mnd_po_linebreaks("", 0, breaks);
    NAOS_CHECK_INT_EQ(0xAA, breaks[0]);
}

NAOS_TEST(linebreak_never_at_start_or_inside_character)
{
    unsigned char breaks[8];
    size_t i = 0;

    memset(breaks, 0xAA, sizeof(breaks));
    /* あ😀: 先頭は 0、文字の途中は 0、😀 の前は 1 */
    mnd_po_linebreaks("\xE3\x81\x82\xF0\x9F\x98\x80", 7, breaks);
    for (i = 0; i < 7; i++)
    {
        NAOS_CHECK_INT_EQ((i == 3) ? 1 : 0, breaks[i]);
    }
    NAOS_CHECK_INT_EQ(0xAA, breaks[7]);
    memset(breaks, 0xAA, sizeof(breaks));
    mnd_po_linebreaks("\xE3\x81\x82", 3, breaks);
    NAOS_CHECK_INT_EQ(0, breaks[0]);
}

NAOS_SUITE(linebreak,
    NAOS_ADD_TEST(linebreak_pair_table_classes_come_first),
    NAOS_ADD_TEST(linebreak_width_of_representative_characters),
    NAOS_ADD_TEST(linebreak_width_of_controls_and_hangul_jungseong),
    NAOS_ADD_TEST(linebreak_width_of_unassigned_code_points),
    NAOS_ADD_TEST(linebreak_class_of_ascii_characters),
    NAOS_ADD_TEST(linebreak_class_of_non_ascii_characters),
    NAOS_ADD_TEST(linebreak_class_of_cjk_characters),
    NAOS_ADD_TEST(linebreak_class_of_emoji),
    NAOS_ADD_TEST(linebreak_class_of_unassigned_code_points),
    NAOS_ADD_TEST(linebreak_is_mark_for_mn_and_mc),
    NAOS_ADD_TEST(linebreak_after_spaces),
    NAOS_ADD_TEST(linebreak_after_hyphen),
    NAOS_ADD_TEST(linebreak_between_cjk_and_korean),
    NAOS_ADD_TEST(linebreak_not_before_closing_punctuation),
    NAOS_ADD_TEST(linebreak_not_before_small_kana),
    NAOS_ADD_TEST(linebreak_not_after_opening_bracket),
    NAOS_ADD_TEST(linebreak_not_between_digit_and_percent),
    NAOS_ADD_TEST(linebreak_emoji_modifier_and_zwj),
    NAOS_ADD_TEST(linebreak_regional_indicator_pairs),
    NAOS_ADD_TEST(linebreak_combining_marks),
    NAOS_ADD_TEST(linebreak_zero_width_space_and_word_joiner),
    NAOS_ADD_TEST(linebreak_resolved_classes),
    NAOS_ADD_TEST(linebreak_libunistring_compatible_rules),
    NAOS_ADD_TEST(linebreak_newlines_and_empty),
    NAOS_ADD_TEST(linebreak_never_at_start_or_inside_character)
)
