/* スイート front_matter: 走査の規則（フロントマター）と文書のライフサイクル（1.8〜1.12、2.6、2.7、7.1〜7.3） */

#include <naos/naos.h>

#include "tu/tu.h"
#include "tu/tu_internal.h"
#include "test_util.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
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

/*
 * diags の中にエラーの診断がちょうど 1 つあり、その最初のものの path が "test.md" で、
 * メッセージが needle を含むことを検査する
 */
static void check_single_error(const mnd_diag_list_t *diags, const char *needle)
{
    size_t i;
    size_t errors = 0;
    const mnd_diag_t *first = NULL;

    for (i = 0; i < mnd_diag_list_count(diags); i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        if (diag->severity == MND_SEVERITY_ERROR)
        {
            if (first == NULL)
            {
                first = diag;
            }
            errors++;
        }
    }
    NAOS_CHECK_UINT_EQ(1, errors);
    NAOS_REQUIRE_NOT_NULL(first);
    NAOS_REQUIRE_NOT_NULL(first->path);
    NAOS_CHECK_STR_EQ("test.md", first->path);
    NAOS_CHECK_TRUE(strstr(first->message, needle) != NULL);
}

/* フロントマターのない文書: YAML は解析せず、Markdown は先頭から解析する（1.9） */
NAOS_TEST(front_matter_scan_without_front_matter)
{
    static const char text[] = "# Title\n\nBody text.\n";
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_TRUE(doc->yaml == NULL);
    NAOS_CHECK_TRUE(doc->data == text);
    NAOS_CHECK_UINT_EQ(strlen(text), doc->len);
    NAOS_REQUIRE_NOT_NULL(doc->path);
    NAOS_CHECK_STR_EQ("test.md", doc->path);
    NAOS_CHECK_UINT_EQ(2, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);
}

/* キーパスの指定がなければ、フロントマターがあっても YAML は解析しない（1.9） */
NAOS_TEST(front_matter_scan_without_keys_skips_yaml)
{
    static const char text[] = "---\ntitle: Hello\n---\n# Heading\n";
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_TRUE(doc->yaml == NULL);
    NAOS_CHECK_UINT_EQ(1, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);
}

/* キーパスを指定し、フロントマターが正しい YAML なら、YAML を解析して文書が保持する */
NAOS_TEST(front_matter_scan_with_keys_parses_yaml)
{
    static const char text[] = "---\ntitle: Hello\n---\n# Heading\n";
    static const char *const keys[] = { "title" };
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_TRUE(doc->yaml != NULL);
    /* 本文の見出しは最後のセグメント（フロントマターのセグメントは本文より前に並ぶ） */
    NAOS_REQUIRE_TRUE(mnd_tu_doc_segment_count(doc) >= 1);
    NAOS_CHECK_STR_EQ("Heading",
                      mnd_tu_doc_segment(doc, mnd_tu_doc_segment_count(doc) - 1)->msgid.data);
    mnd_tu_doc_free(doc);
}

/* キーパスを指定してもフロントマターがなければ、YAML は解析しない */
NAOS_TEST(front_matter_scan_with_keys_without_front_matter)
{
    static const char text[] = "Body only.\n";
    static const char *const keys[] = { "title" };
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_TRUE(doc->yaml == NULL);
    NAOS_CHECK_UINT_EQ(1, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);
}

/* キーパスを指定し、フロントマターが不正な YAML なら、上流のエラーで失敗して *out は NULL（7.2） */
NAOS_TEST(front_matter_scan_with_keys_invalid_yaml_fails)
{
    static const char text[] = "---\ntitle: [unclosed\n---\nBody.\n";
    static const char *const keys[] = { "title" };
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = (mnd_tu_doc_t *)&diags;  /* NULL に戻ることを確かめるための仮の値 */

    options.path = "test.md";
    options.front_matter_keys = keys;
    options.front_matter_key_count = 1;
    NAOS_CHECK_FALSE(mnd_tu_scan(text, strlen(text), &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));
    if (mnd_diag_list_count(&diags) > 0)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(&diags, 0);

        NAOS_CHECK_TRUE(diag->severity == MND_SEVERITY_ERROR);
        NAOS_REQUIRE_NOT_NULL(diag->path);
        NAOS_CHECK_STR_EQ("test.md", diag->path);
        NAOS_CHECK_TRUE(diag->line > 0);
    }
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* キーパスの指定がなければ、フロントマターが不正な YAML でも成功する（1.9） */
NAOS_TEST(front_matter_scan_without_keys_invalid_yaml_succeeds)
{
    static const char text[] = "---\ntitle: [unclosed\n---\nBody.\n";
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_TRUE(doc->yaml == NULL);
    NAOS_CHECK_UINT_EQ(1, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);
}

/* 不正な UTF-8 は上流のエラー（invalid UTF-8 sequence）で失敗し、*out は NULL（7.1） */
NAOS_TEST(front_matter_scan_invalid_utf8_fails)
{
    static const char text[] = "Hello \xff world\n";
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = (mnd_tu_doc_t *)&diags;  /* NULL に戻ることを確かめるための仮の値 */

    options.path = "test.md";
    NAOS_CHECK_FALSE(mnd_tu_scan(text, strlen(text), &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, "invalid UTF-8 sequence");
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* キーパスを指定していても、不正な UTF-8 は YAML の解析の前に失敗する（7.1） */
NAOS_TEST(front_matter_scan_invalid_utf8_with_keys_fails)
{
    static const char text[] = "---\ntitle: \xc3\x28\n---\nBody.\n";
    static const char *const keys[] = { "title" };
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = (mnd_tu_doc_t *)&diags;  /* NULL に戻ることを確かめるための仮の値 */

    options.path = "test.md";
    options.front_matter_keys = keys;
    options.front_matter_key_count = 1;
    NAOS_CHECK_FALSE(mnd_tu_scan(text, strlen(text), &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, "invalid UTF-8 sequence");
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* 空の文書（data == NULL、len == 0、options == NULL）: 成功し、セグメントは 0 件で診断もない */
NAOS_TEST(front_matter_scan_empty_document)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;

    NAOS_CHECK_TRUE(mnd_tu_scan(NULL, 0, NULL, &doc, &diags));
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_TRUE(doc->yaml == NULL);
    NAOS_CHECK_TRUE(doc->path == NULL);
    NAOS_CHECK_UINT_EQ(0, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* 区切りだけのフロントマターと空の本文も、キーパスの指定の有無にかかわらず成功する */
NAOS_TEST(front_matter_scan_empty_front_matter)
{
    static const char text[] = "---\n---\n";
    static const char *const keys[] = { "title" };
    mnd_tu_doc_t *doc = NULL;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_UINT_EQ(0, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);

    doc = NULL;
    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_TRUE(doc->md != NULL);
    NAOS_CHECK_UINT_EQ(0, mnd_tu_doc_segment_count(doc));
    mnd_tu_doc_free(doc);
}

/* セグメントの追加: 配列を伸ばし、追加した内部セグメントを初期化して返す */
NAOS_TEST(front_matter_push_segment_grows_and_initializes)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;
    size_t i;

    NAOS_REQUIRE_TRUE(mnd_tu_scan(NULL, 0, NULL, &doc, &diags));
    NAOS_REQUIRE_NOT_NULL(doc);
    for (i = 0; i < 40; i++)
    {
        mnd_tu_seg_t *seg = mnd_tu_doc_push_segment(doc);

        NAOS_REQUIRE_NOT_NULL(seg);
        NAOS_CHECK_UINT_EQ(i + 1, mnd_tu_doc_segment_count(doc));
        NAOS_CHECK_TRUE(seg == &doc->segs[i]);
        NAOS_CHECK_TRUE(doc->seg_count <= doc->seg_cap);
        NAOS_CHECK_TRUE(seg->node == NULL);
        NAOS_CHECK_TRUE(seg->scalar == NULL);
        NAOS_CHECK_TRUE(seg->pub.msgid.data == NULL);
        NAOS_CHECK_UINT_EQ(0, seg->pub.msgid.len);
        NAOS_CHECK_TRUE(seg->pub.kind.data == NULL);
        NAOS_CHECK_UINT_EQ(0, seg->pub.kind.len);
        NAOS_CHECK_UINT_EQ(0, seg->pub.span.start);
        NAOS_CHECK_UINT_EQ(0, seg->pub.span.end);
        NAOS_CHECK_UINT_EQ(0, seg->pub.line);
        NAOS_CHECK_UINT_EQ(0, seg->pub.lead_newlines);
        NAOS_CHECK_UINT_EQ(0, seg->pub.trail_newlines);
        /* 文書の解放で msgid と kind も解放されることを確かめるため、内容を持たせる */
        NAOS_REQUIRE_TRUE(mnd_buf_append(&seg->pub.msgid, "text", 4));
        NAOS_REQUIRE_TRUE(mnd_buf_append(&seg->pub.kind, "paragraph", 9));
    }
    NAOS_CHECK_STR_EQ("text", mnd_buf_cstr(&mnd_tu_doc_segment(doc, 39)->msgid));
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(front_matter,
    NAOS_ADD_TEST(front_matter_options_init_has_no_keys),
    NAOS_ADD_TEST(front_matter_util_scan_empty_has_no_segments),
    NAOS_ADD_TEST(front_matter_util_read_po_builds_catalog),
    NAOS_ADD_TEST(front_matter_util_difference_reports_first_offset),
    NAOS_ADD_TEST(front_matter_scan_without_front_matter),
    NAOS_ADD_TEST(front_matter_scan_without_keys_skips_yaml),
    NAOS_ADD_TEST(front_matter_scan_with_keys_parses_yaml),
    NAOS_ADD_TEST(front_matter_scan_with_keys_without_front_matter),
    NAOS_ADD_TEST(front_matter_scan_with_keys_invalid_yaml_fails),
    NAOS_ADD_TEST(front_matter_scan_without_keys_invalid_yaml_succeeds),
    NAOS_ADD_TEST(front_matter_scan_invalid_utf8_fails),
    NAOS_ADD_TEST(front_matter_scan_invalid_utf8_with_keys_fails),
    NAOS_ADD_TEST(front_matter_scan_empty_document),
    NAOS_ADD_TEST(front_matter_scan_empty_front_matter),
    NAOS_ADD_TEST(front_matter_push_segment_grows_and_initializes)
)
