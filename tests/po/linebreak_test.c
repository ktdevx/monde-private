#include <naos/naos.h>

#include "po/po_internal.h"

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
    NAOS_ADD_TEST(linebreak_is_mark_for_mn_and_mc)
)
