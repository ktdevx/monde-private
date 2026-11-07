/* スイート front_matter: 走査の規則（フロントマター）と文書のライフサイクル（1.8〜1.12、2.6、2.7、7.1〜7.3） */

#include <naos/naos.h>

#include "tu/tu.h"
#include "test_util.h"

#include "foundation/buf.h"
#include "po/po.h"

#include <stddef.h>
#include <string.h>

/* オプションの既定値はパスなし・キーパスなし。文書の解放は NULL に対しても安全 */
NAOS_TEST(front_matter_options_init_has_no_keys)
{
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;

    NAOS_CHECK_TRUE(options.path == NULL);
    NAOS_CHECK_TRUE(options.front_matter_keys == NULL);
    NAOS_CHECK_UINT_EQ(0, options.front_matter_key_count);
    mnd_tu_doc_free(NULL);
}

/* 補助関数の雛形: 空の文書を走査するとセグメントは 0 件 */
NAOS_TEST(front_matter_util_scan_empty_has_no_segments)
{
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN("", NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_UINT_EQ(0, mnd_tu_doc_segment_count(doc));
    TEST_CHECK_SEGMENTS(doc, NULL, 0);
    mnd_tu_doc_free(doc);
}

/* 補助関数の雛形: 短い PO の文字列からカタログを作ると、ヘッダを含めて期待の件数になる */
NAOS_TEST(front_matter_util_read_po_builds_catalog)
{
    static const char po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"Hello\"\n"
        "msgstr \"こんにちは\"\n"
        "\n"
        "msgid \"World\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t catalog;

    TEST_READ_PO(po, &catalog);
    NAOS_CHECK_UINT_EQ(3, catalog.count);
    if (catalog.count == 3)
    {
        NAOS_CHECK_STR_EQ("Hello", mnd_buf_cstr(&catalog.entries[1].msgid));
        NAOS_CHECK_STR_EQ("World", mnd_buf_cstr(&catalog.entries[2].msgid));
    }
    mnd_po_catalog_free(&catalog);
}

/* 補助関数の雛形: 意図的に違う 2 つのバイト列の比較は、最初に違う位置を報告する */
NAOS_TEST(front_matter_util_difference_reports_first_offset)
{
    size_t offset = 999;
    mnd_buf_t text = MND_BUF_INIT;

    /* 途中で違う */
    NAOS_CHECK_TRUE(test_find_difference("abcXef", 6, "abcdef", 6, &offset));
    NAOS_CHECK_UINT_EQ(3, offset);

    /* 一方が他方の接頭辞なら短い方の長さ */
    NAOS_CHECK_TRUE(test_find_difference("abc", 3, "abcd", 4, &offset));
    NAOS_CHECK_UINT_EQ(3, offset);
    NAOS_CHECK_TRUE(test_find_difference("abcd", 4, "", 0, &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    /* NUL を含むバイト列も長さで比べる */
    NAOS_CHECK_TRUE(test_find_difference("a\0b", 3, "a\0c", 3, &offset));
    NAOS_CHECK_UINT_EQ(2, offset);

    /* 同じなら false で、offset は変わらない */
    offset = 999;
    NAOS_CHECK_FALSE(test_find_difference("same", 4, "same", 4, &offset));
    NAOS_CHECK_UINT_EQ(999, offset);
    NAOS_CHECK_FALSE(test_find_difference(NULL, 0, NULL, 0, &offset));

    /* 説明には位置・行番号・前後の内容（改行はエスケープ）が入る */
    NAOS_CHECK_TRUE(test_describe_difference("one\ntwX\n", 8, "one\ntwo\n", 8, &text));
    NAOS_CHECK_STR_EQ("first difference at offset 6 (line 2), actual length 8, expected length 8: "
                      "actual \"one\\ntw[X\\n]\" expected \"one\\ntw[o\\n]\"",
                      mnd_buf_cstr(&text));

    /* 前後は最大 16 バイトずつに切り詰め、切り詰めた側に ... を付ける */
    NAOS_CHECK_TRUE(test_describe_difference("0123456789abcdefghijXlmnopqrstuvwxyz0123", 40,
                                             "0123456789abcdefghijklmnopqrstuvwxyz0123", 40, &text));
    NAOS_CHECK_STR_EQ("first difference at offset 20 (line 1), actual length 40, expected length 40: "
                      "actual \"...456789abcdefghij[Xlmnopqrstuvwxyz]...\" "
                      "expected \"...456789abcdefghij[klmnopqrstuvwxyz]...\"",
                      mnd_buf_cstr(&text));

    NAOS_CHECK_FALSE(test_describe_difference("same", 4, "same", 4, &text));
    NAOS_CHECK_UINT_EQ(0, text.len);

    /* 同じバイト列の検査は合格する */
    TEST_CHECK_BYTES("same", 4, "same", 4);
    mnd_buf_free(&text);
}

NAOS_SUITE(front_matter,
    NAOS_ADD_TEST(front_matter_options_init_has_no_keys),
    NAOS_ADD_TEST(front_matter_util_scan_empty_has_no_segments),
    NAOS_ADD_TEST(front_matter_util_read_po_builds_catalog),
    NAOS_ADD_TEST(front_matter_util_difference_reports_first_offset)
)
