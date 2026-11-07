/* スイート extract: 抽出の規則（3.1〜3.7、3.9） */

#include <naos/naos.h>

#include "test_util.h"
#include "tu/tu.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_write.h"

#include <stddef.h>
#include <string.h>

/* doc を ref_file で catalog に抽出し、true が返り診断がないことを検査する */
static void extract_ok(const mnd_tu_doc_t *doc, const char *ref_file, mnd_po_catalog_t *catalog)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_TRUE(mnd_tu_extract(doc, ref_file, catalog, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

/* entry の参照 index 番目が file:line であることを検査する */
static void check_ref(const mnd_po_entry_t *entry, size_t index, const char *file, size_t line)
{
    NAOS_REQUIRE_UINT_GT(entry->refs.count, index);
    NAOS_CHECK_STR_EQ(file, mnd_buf_cstr(&entry->refs.items[index].file));
    NAOS_CHECK_UINT_EQ(line, entry->refs.items[index].line);
}

/* セグメントのない文書からはエントリを加えず、成功する */
NAOS_TEST(extract_empty_document_adds_no_entries)
{
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN("", NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* エントリは各 msgid の最初の出現の順で、フロントマターが本文より前（3.1、3.3） */
NAOS_TEST(extract_orders_by_first_occurrence_front_matter_first)
{
    static const char *const keys[] = { "title" };
    static const char text[] =
        "---\n"
        "title: Zeta\n"
        "---\n"
        "Alpha\n"
        "\n"
        "Beta\n"
        "\n"
        "Alpha\n";
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(3, catalog.count);
    NAOS_CHECK_STR_EQ("Zeta", mnd_buf_cstr(&catalog.entries[0].msgid));
    NAOS_CHECK_STR_EQ("Alpha", mnd_buf_cstr(&catalog.entries[1].msgid));
    NAOS_CHECK_STR_EQ("Beta", mnd_buf_cstr(&catalog.entries[2].msgid));
    NAOS_CHECK_UINT_EQ(2, catalog.entries[1].refs.count);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 同じ msgid の 3 回の出現は 1 つのエントリにまとまり、参照が出現順に 3 つ付く（3.2、3.4） */
NAOS_TEST(extract_same_msgid_three_times_has_three_refs)
{
    static const char text[] = "Same\n\nSame\n\nSame\n";
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;
    const mnd_po_entry_t *entry;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "dir/doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(1, catalog.count);
    entry = &catalog.entries[0];
    NAOS_CHECK_STR_EQ("Same", mnd_buf_cstr(&entry->msgid));
    NAOS_REQUIRE_UINT_EQ(3, entry->refs.count);
    check_ref(entry, 0, "dir/doc.md", 1);
    check_ref(entry, 1, "dir/doc.md", 3);
    check_ref(entry, 2, "dir/doc.md", 5);
    NAOS_REQUIRE_UINT_EQ(1, entry->extracted_comments.count);
    NAOS_CHECK_STR_EQ("paragraph", mnd_buf_cstr(&entry->extracted_comments.items[0]));
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 異なる種類での出現は種類ごとに 1 行ずつ、出現順の抽出コメントになる（3.5、3.6） */
NAOS_TEST(extract_different_kinds_have_two_comments)
{
    static const char *const keys[] = { "title" };
    static const char text[] =
        "---\n"
        "title: Same\n"
        "---\n"
        "# Same\n"
        "\n"
        "Same\n";
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;
    const mnd_po_entry_t *entry;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(1, catalog.count);
    entry = &catalog.entries[0];
    NAOS_REQUIRE_UINT_EQ(3, entry->extracted_comments.count);
    NAOS_CHECK_STR_EQ("front matter: title", mnd_buf_cstr(&entry->extracted_comments.items[0]));
    NAOS_CHECK_STR_EQ("heading (level 1)", mnd_buf_cstr(&entry->extracted_comments.items[1]));
    NAOS_CHECK_STR_EQ("paragraph", mnd_buf_cstr(&entry->extracted_comments.items[2]));
    NAOS_CHECK_UINT_EQ(3, entry->refs.count);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 本文の 2 種類の出現では抽出コメントが 2 行になる（3.6） */
NAOS_TEST(extract_heading_and_paragraph_have_two_comments)
{
    static const char text[] = "# Same\n\nSame\n\nSame\n";
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;
    const mnd_po_entry_t *entry;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(1, catalog.count);
    entry = &catalog.entries[0];
    NAOS_REQUIRE_UINT_EQ(2, entry->extracted_comments.count);
    NAOS_CHECK_STR_EQ("heading (level 1)", mnd_buf_cstr(&entry->extracted_comments.items[0]));
    NAOS_CHECK_STR_EQ("paragraph", mnd_buf_cstr(&entry->extracted_comments.items[1]));
    NAOS_CHECK_UINT_EQ(3, entry->refs.count);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 同じ種類の出現では抽出コメントは 1 行だけ（3.6） */
NAOS_TEST(extract_same_kind_has_one_comment)
{
    static const char text[] = "- Same\n- Same\n";
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;
    const mnd_po_entry_t *entry;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(1, catalog.count);
    entry = &catalog.entries[0];
    NAOS_REQUIRE_UINT_EQ(1, entry->extracted_comments.count);
    NAOS_CHECK_STR_EQ("list item > paragraph", mnd_buf_cstr(&entry->extracted_comments.items[0]));
    NAOS_REQUIRE_UINT_EQ(2, entry->refs.count);
    check_ref(entry, 0, "doc.md", 1);
    check_ref(entry, 1, "doc.md", 2);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 新しいエントリは msgstr が要素なし、フラグなし、msgctxt なし、廃止でない単数形（3.7） */
NAOS_TEST(extract_entry_has_empty_msgstr_and_no_flags)
{
    static const char text[] = "Hello\n";
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;
    const mnd_po_entry_t *entry;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(1, catalog.count);
    entry = &catalog.entries[0];
    NAOS_CHECK_UINT_EQ(0, entry->msgstr.count);
    NAOS_CHECK_UINT_EQ(0, entry->flags.count);
    NAOS_CHECK_UINT_EQ(0, entry->translator_comments.count);
    NAOS_CHECK_FALSE(entry->has_msgctxt);
    NAOS_CHECK_FALSE(entry->has_msgid_plural);
    NAOS_CHECK_FALSE(entry->obsolete);
    NAOS_CHECK_UINT_EQ(MND_PO_STATE_UNTRANSLATED, mnd_po_entry_state(entry));
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* ヘッダを先に入れたカタログでは、ヘッダを変えずにその後ろへ加える（3.1） */
NAOS_TEST(extract_appends_after_preseeded_header)
{
    static const char text[] = "Hello\n\nWorld\n";
    mnd_po_catalog_t catalog;
    mnd_po_entry_t *header;
    mnd_tu_doc_t *doc = NULL;

    mnd_po_catalog_init(&catalog);
    header = mnd_po_catalog_append(&catalog);
    NAOS_REQUIRE_NOT_NULL(header);
    NAOS_REQUIRE_TRUE(mnd_po_header_template(header, NULL, "2026-01-01 00:00+0000"));
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_REQUIRE_UINT_EQ(3, catalog.count);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&catalog.entries[0].msgid));
    NAOS_CHECK_UINT_EQ(0, catalog.entries[0].refs.count);
    NAOS_CHECK_UINT_EQ(0, catalog.entries[0].extracted_comments.count);
    NAOS_CHECK_UINT_EQ(1, catalog.entries[0].msgstr.count);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&catalog.entries[0], "fuzzy"));
    NAOS_CHECK_STR_EQ("Hello", mnd_buf_cstr(&catalog.entries[1].msgid));
    NAOS_CHECK_STR_EQ("World", mnd_buf_cstr(&catalog.entries[2].msgid));
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 既にある msgctxt のない同じ msgid のエントリに統合し、msgctxt 付きのエントリには統合しない（3.2） */
NAOS_TEST(extract_merges_into_existing_entry_without_msgctxt)
{
    static const char first[] = "Shared\n\nOnly A\n";
    static const char second[] = "Shared\n\nOnly B\n";
    mnd_po_catalog_t catalog;
    mnd_po_entry_t *ctx_entry;
    mnd_tu_doc_t *doc = NULL;
    const mnd_po_entry_t *entry;

    mnd_po_catalog_init(&catalog);
    ctx_entry = mnd_po_catalog_append(&catalog);
    NAOS_REQUIRE_NOT_NULL(ctx_entry);
    ctx_entry->has_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&ctx_entry->msgctxt, "menu"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&ctx_entry->msgid, "Shared"));

    TEST_SCAN(first, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "a.md", &catalog);
    mnd_tu_doc_free(doc);
    doc = NULL;
    TEST_SCAN(second, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "b.md", &catalog);

    NAOS_REQUIRE_UINT_EQ(4, catalog.count);
    NAOS_CHECK_UINT_EQ(0, catalog.entries[0].refs.count);
    entry = &catalog.entries[1];
    NAOS_CHECK_STR_EQ("Shared", mnd_buf_cstr(&entry->msgid));
    NAOS_CHECK_FALSE(entry->has_msgctxt);
    NAOS_REQUIRE_UINT_EQ(2, entry->refs.count);
    check_ref(entry, 0, "a.md", 1);
    check_ref(entry, 1, "b.md", 1);
    NAOS_CHECK_UINT_EQ(1, entry->extracted_comments.count);
    NAOS_CHECK_STR_EQ("Only A", mnd_buf_cstr(&catalog.entries[2].msgid));
    NAOS_CHECK_STR_EQ("Only B", mnd_buf_cstr(&catalog.entries[3].msgid));
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* ヘッダ付きで抽出した結果を po-format で書き出すと、重複エラーにならず期待の .pot と一致する（3.9） */
NAOS_TEST(extract_written_pot_matches_expected)
{
    static const char *const keys[] = { "title" };
    static const char text[] =
        "---\n"
        "title: Guide\n"
        "---\n"
        "# Guide\n"
        "\n"
        "He said \"hi\"\n"
        "and left.\n"
        "\n"
        "- Item\n"
        "- Item\n";
    static const char expected[] =
        "#, fuzzy\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: PACKAGE VERSION\\n\"\n"
        "\"Report-Msgid-Bugs-To: \\n\"\n"
        "\"POT-Creation-Date: 2026-01-01 00:00+0000\\n\"\n"
        "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n"
        "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n"
        "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n"
        "\"Language: \\n\"\n"
        "\"MIME-Version: 1.0\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Content-Transfer-Encoding: 8bit\\n\"\n"
        "\n"
        "#. front matter: title\n"
        "#. heading (level 1)\n"
        "#: doc.md:2 doc.md:4\n"
        "msgid \"Guide\"\n"
        "msgstr \"\"\n"
        "\n"
        "#. paragraph\n"
        "#: doc.md:6\n"
        "msgid \"\"\n"
        "\"He said \\\"hi\\\"\\n\"\n"
        "\"and left.\"\n"
        "msgstr \"\"\n"
        "\n"
        "#. list item > paragraph\n"
        "#: doc.md:9 doc.md:10\n"
        "msgid \"Item\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t catalog;
    mnd_po_entry_t *header;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_tu_doc_t *doc = NULL;

    mnd_po_catalog_init(&catalog);
    header = mnd_po_catalog_append(&catalog);
    NAOS_REQUIRE_NOT_NULL(header);
    NAOS_REQUIRE_TRUE(mnd_po_header_template(header, NULL, "2026-01-01 00:00+0000"));
    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    extract_ok(doc, "doc.md", &catalog);
    NAOS_CHECK_TRUE(mnd_po_write(&catalog, NULL, &out, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    TEST_CHECK_BYTES(mnd_buf_cstr(&out), out.len, expected, strlen(expected));
    mnd_buf_free(&out);
    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

NAOS_SUITE(extract,
    NAOS_ADD_TEST(extract_empty_document_adds_no_entries),
    NAOS_ADD_TEST(extract_orders_by_first_occurrence_front_matter_first),
    NAOS_ADD_TEST(extract_same_msgid_three_times_has_three_refs),
    NAOS_ADD_TEST(extract_different_kinds_have_two_comments),
    NAOS_ADD_TEST(extract_heading_and_paragraph_have_two_comments),
    NAOS_ADD_TEST(extract_same_kind_has_one_comment),
    NAOS_ADD_TEST(extract_entry_has_empty_msgstr_and_no_flags),
    NAOS_ADD_TEST(extract_appends_after_preseeded_header),
    NAOS_ADD_TEST(extract_merges_into_existing_entry_without_msgctxt),
    NAOS_ADD_TEST(extract_written_pot_matches_expected)
)
