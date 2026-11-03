#include <naos/naos.h>

#include "foundation/buf.h"
#include "po/po.h"
#include "po/po_internal.h"

#include <stdint.h>
#include <string.h>

/* テスト用: 末尾にエントリを加え、msgctxt（NULL ならなし）と msgid を設定する */
static mnd_po_entry_t *add_entry(mnd_po_catalog_t *catalog, const char *msgctxt,
                                 const char *msgid)
{
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);

    if (entry == NULL)
    {
        return NULL;
    }

    if (msgctxt != NULL)
    {
        entry->has_msgctxt = true;
        if (!mnd_buf_append_str(&entry->msgctxt, msgctxt))
        {
            return NULL;
        }
    }

    if (!mnd_buf_append_str(&entry->msgid, msgid))
    {
        return NULL;
    }

    return entry;
}

static bool push_str(mnd_po_strings_t *list, const char *str)
{
    return mnd_po_strings_push(list, str, strlen(str));
}

NAOS_TEST(entry_catalog_init_macro_is_empty)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;

    NAOS_CHECK_NULL(catalog.entries);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
    NAOS_CHECK_UINT_EQ(0, catalog.cap);
}

NAOS_TEST(entry_init_sets_empty_state)
{
    mnd_po_entry_t entry;

    memset(&entry, 0xA5, sizeof(entry));
    mnd_po_entry_init(&entry);
    NAOS_CHECK_UINT_EQ(0, entry.translator_comments.count);
    NAOS_CHECK_UINT_EQ(0, entry.extracted_comments.count);
    NAOS_CHECK_UINT_EQ(0, entry.refs.count);
    NAOS_CHECK_UINT_EQ(0, entry.flags.count);
    NAOS_CHECK_UINT_EQ(0, entry.msgstr.count);
    NAOS_CHECK_NULL(entry.msgstr.items);
    NAOS_CHECK_FALSE(entry.has_prev_msgctxt);
    NAOS_CHECK_FALSE(entry.has_prev_msgid);
    NAOS_CHECK_FALSE(entry.has_prev_msgid_plural);
    NAOS_CHECK_FALSE(entry.has_msgctxt);
    NAOS_CHECK_FALSE(entry.has_msgid_plural);
    NAOS_CHECK_UINT_EQ(0, entry.msgid.len);
    NAOS_CHECK_NULL(entry.msgid.data);
    NAOS_CHECK_FALSE(entry.obsolete);
    NAOS_CHECK_UINT_EQ(0, entry.line);
}

NAOS_TEST(entry_free_returns_to_initial_state)
{
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(push_str(&entry.translator_comments, "translator"));
    NAOS_REQUIRE_TRUE(push_str(&entry.extracted_comments, "extracted"));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry.refs, "a.md", 3));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "x"));
    entry.has_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.msgctxt, "ctx"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.msgid, "id"));
    entry.has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.prev_msgid, "old"));
    entry.obsolete = true;
    entry.line = 7;

    mnd_po_entry_free(&entry);
    NAOS_CHECK_UINT_EQ(0, entry.translator_comments.count);
    NAOS_CHECK_NULL(entry.translator_comments.items);
    NAOS_CHECK_UINT_EQ(0, entry.refs.count);
    NAOS_CHECK_NULL(entry.refs.items);
    NAOS_CHECK_UINT_EQ(0, entry.flags.count);
    NAOS_CHECK_UINT_EQ(0, entry.msgstr.count);
    NAOS_CHECK_FALSE(entry.has_msgctxt);
    NAOS_CHECK_NULL(entry.msgctxt.data);
    NAOS_CHECK_NULL(entry.msgid.data);
    NAOS_CHECK_FALSE(entry.has_prev_msgid);
    NAOS_CHECK_NULL(entry.prev_msgid.data);
    NAOS_CHECK_FALSE(entry.obsolete);
    NAOS_CHECK_UINT_EQ(0, entry.line);

    /* 解放済みに対しても安全 */
    mnd_po_entry_free(&entry);
    NAOS_CHECK_UINT_EQ(0, entry.msgstr.count);
}

NAOS_TEST(entry_strings_push_keeps_order_and_bytes)
{
    mnd_po_entry_t entry;
    size_t i;
    char text[8];

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "abcdef", 3));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, NULL, 0));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "\xE6\x97\xA5"));

    /* 最小の容量を超えて広げても順序が保たれる */
    for (i = 0; i < 20; i++)
    {
        text[0] = (char)('a' + (int)i);
        text[1] = '\0';
        NAOS_REQUIRE_TRUE(push_str(&entry.translator_comments, text));
    }

    NAOS_REQUIRE_UINT_EQ(3, entry.msgstr.count);
    NAOS_CHECK_UINT_EQ(3, entry.msgstr.items[0].len);
    NAOS_CHECK_STR_EQ("abc", mnd_buf_cstr(&entry.msgstr.items[0]));
    NAOS_CHECK_UINT_EQ(0, entry.msgstr.items[1].len);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&entry.msgstr.items[1]));
    NAOS_CHECK_STR_EQ("\xE6\x97\xA5", mnd_buf_cstr(&entry.msgstr.items[2]));
    NAOS_REQUIRE_UINT_EQ(20, entry.translator_comments.count);
    NAOS_CHECK_UINT_GE(entry.translator_comments.cap, 20);
    for (i = 0; i < 20; i++)
    {
        text[0] = (char)('a' + (int)i);
        text[1] = '\0';
        NAOS_CHECK_STR_EQ(text, mnd_buf_cstr(&entry.translator_comments.items[i]));
    }

    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_refs_push_keeps_file_and_line)
{
    mnd_po_entry_t entry;
    size_t i;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry.refs, "docs/a.md", 12));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry.refs, "b c.md", 0));
    for (i = 0; i < 10; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry.refs, "x.md", i + 1));
    }

    NAOS_REQUIRE_UINT_EQ(12, entry.refs.count);
    NAOS_CHECK_STR_EQ("docs/a.md", mnd_buf_cstr(&entry.refs.items[0].file));
    NAOS_CHECK_UINT_EQ(12, entry.refs.items[0].line);
    NAOS_CHECK_STR_EQ("b c.md", mnd_buf_cstr(&entry.refs.items[1].file));
    NAOS_CHECK_UINT_EQ(0, entry.refs.items[1].line);
    for (i = 0; i < 10; i++)
    {
        NAOS_CHECK_STR_EQ("x.md", mnd_buf_cstr(&entry.refs.items[i + 2].file));
        NAOS_CHECK_UINT_EQ(i + 1, entry.refs.items[i + 2].line);
    }

    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_push_rejects_capacity_overflow_before_allocation)
{
    mnd_po_strings_t list;
    mnd_po_refs_t refs;

    /* 容量の計算がオーバーフローする状態。items には触れずに false を返す */
    list.items = NULL;
    list.count = (SIZE_MAX / 2) + 1;
    list.cap = list.count;
    NAOS_CHECK_FALSE(mnd_po_strings_push(&list, "a", 1));
    NAOS_CHECK_UINT_EQ((SIZE_MAX / 2) + 1, list.count);
    NAOS_CHECK_NULL(list.items);

    list.count = SIZE_MAX / sizeof(mnd_buf_t);
    list.cap = list.count;
    NAOS_CHECK_FALSE(mnd_po_strings_push(&list, "a", 1));
    NAOS_CHECK_NULL(list.items);

    refs.items = NULL;
    refs.count = SIZE_MAX / sizeof(mnd_po_ref_t);
    refs.cap = refs.count;
    NAOS_CHECK_FALSE(mnd_po_refs_push(&refs, "a.md", 1));
    NAOS_CHECK_NULL(refs.items);
}

NAOS_TEST(entry_catalog_append_rejects_capacity_overflow)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;

    catalog.count = SIZE_MAX / sizeof(mnd_po_entry_t);
    catalog.cap = catalog.count;
    NAOS_CHECK_NULL(mnd_po_catalog_append(&catalog));
    NAOS_CHECK_NULL(mnd_po_catalog_insert(&catalog, 0));
    NAOS_CHECK_NULL(catalog.entries);
}

NAOS_TEST(entry_catalog_free_is_safe_on_initial_and_freed)
{
    mnd_po_catalog_t catalog;

    mnd_po_catalog_init(&catalog);
    NAOS_CHECK_NULL(catalog.entries);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
    mnd_po_catalog_free(&catalog);
    mnd_po_catalog_free(&catalog);

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "c", "a"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "b"));
    NAOS_REQUIRE_TRUE(push_str(&catalog.entries[1].msgstr, "B"));
    mnd_po_catalog_free(&catalog);
    NAOS_CHECK_NULL(catalog.entries);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
    NAOS_CHECK_UINT_EQ(0, catalog.cap);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_catalog_append_and_insert_keep_order)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    const char *expected[] = {"first", "a", "b", "middle", "c", "d", "e", "f", "g", "last"};
    size_t i;

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "b"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "c"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "d"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "e"));

    /* 先頭への挿入。挿入したエントリは初期化されている */
    entry = mnd_po_catalog_insert(&catalog, 0);
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_CHECK_PTR_EQ(&catalog.entries[0], entry);
    NAOS_CHECK_UINT_EQ(0, entry->msgid.len);
    NAOS_CHECK_FALSE(entry->has_msgctxt);
    NAOS_CHECK_UINT_EQ(0, entry->msgstr.count);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid, "first"));

    /* 途中への挿入 */
    entry = mnd_po_catalog_insert(&catalog, 3);
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_CHECK_PTR_EQ(&catalog.entries[3], entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid, "middle"));

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "f"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "g"));

    /* index == count は末尾への追加 */
    entry = mnd_po_catalog_insert(&catalog, catalog.count);
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_CHECK_PTR_EQ(&catalog.entries[catalog.count - 1], entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid, "last"));

    /* index > count は失敗し、カタログは変わらない */
    NAOS_CHECK_NULL(mnd_po_catalog_insert(&catalog, catalog.count + 1));

    NAOS_REQUIRE_UINT_EQ(10, catalog.count);
    for (i = 0; i < 10; i++)
    {
        NAOS_CHECK_STR_EQ(expected[i], mnd_buf_cstr(&catalog.entries[i].msgid));
    }

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_distinguishes_empty_and_absent_msgctxt)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    size_t index = 99;

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "", "hello"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "hello"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "menu", "hello"));

    NAOS_REQUIRE_TRUE(mnd_po_catalog_find(&catalog, NULL, "hello", &index));
    NAOS_CHECK_UINT_EQ(1, index);
    NAOS_REQUIRE_TRUE(mnd_po_catalog_find(&catalog, "", "hello", &index));
    NAOS_CHECK_UINT_EQ(0, index);
    NAOS_REQUIRE_TRUE(mnd_po_catalog_find(&catalog, "menu", "hello", &index));
    NAOS_CHECK_UINT_EQ(2, index);

    index = 99;
    NAOS_CHECK_FALSE(mnd_po_catalog_find(&catalog, "other", "hello", &index));
    NAOS_CHECK_FALSE(mnd_po_catalog_find(&catalog, NULL, "hell", &index));
    NAOS_CHECK_FALSE(mnd_po_catalog_find(&catalog, "", "hello!", &index));
    NAOS_CHECK_UINT_EQ(99, index);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_ignores_has_false_msgctxt_content)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t index = 99;

    /* has_msgctxt が偽なら msgctxt の内容は無視される */
    entry = add_entry(&catalog, NULL, "id");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgctxt, "stale"));

    NAOS_CHECK_FALSE(mnd_po_catalog_find(&catalog, "stale", "id", &index));
    NAOS_REQUIRE_TRUE(mnd_po_catalog_find(&catalog, NULL, "id", &index));
    NAOS_CHECK_UINT_EQ(0, index);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_excludes_obsolete)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t index = 99;

    entry = add_entry(&catalog, NULL, "gone");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    entry = add_entry(&catalog, "c", "dup");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "c", "dup"));

    NAOS_CHECK_FALSE(mnd_po_catalog_find(&catalog, NULL, "gone", &index));
    NAOS_CHECK_UINT_EQ(99, index);
    NAOS_REQUIRE_TRUE(mnd_po_catalog_find(&catalog, "c", "dup", &index));
    NAOS_CHECK_UINT_EQ(2, index);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_on_empty_catalog)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    size_t index = 99;

    NAOS_CHECK_FALSE(mnd_po_catalog_find(&catalog, NULL, "", &index));
    NAOS_CHECK_FALSE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_UINT_EQ(99, index);
}

NAOS_TEST(entry_header_skips_msgctxt_and_obsolete)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t index = 99;

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "not empty"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "", ""));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "ctx", ""));
    entry = add_entry(&catalog, NULL, "");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;

    /* まだヘッダはない */
    NAOS_CHECK_FALSE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_UINT_EQ(99, index);

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, ""));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, ""));
    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_UINT_EQ(4, index);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_header_is_first_entry)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    size_t index = 99;

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, ""));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));
    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_UINT_EQ(0, index);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_state_three_way)
{
    mnd_po_entry_t entry;

    /* msgstr がない */
    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.msgid, "id"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_UNTRANSLATED, mnd_po_entry_state(&entry));

    /* 単数形の空の msgstr */
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "", 0));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_UNTRANSLATED, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);

    /* 単数形の空でない msgstr */
    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "x"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_TRANSLATED, mnd_po_entry_state(&entry));

    /* fuzzy は msgstr にかかわらず fuzzy */
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "c-format"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_FUZZY, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_FUZZY, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);

    /* 複数形: すべて空でなければ翻訳済み、1 つだけ空なら未翻訳 */
    mnd_po_entry_init(&entry);
    entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.msgid_plural, "ids"));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "one"));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "two"));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "many"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_TRANSLATED, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);

    mnd_po_entry_init(&entry);
    entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "one"));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, ""));
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "many"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_UNTRANSLATED, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);

    /* フラグの比較は完全一致: "Fuzzy" や "fuzzy " は fuzzy ではない */
    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "x"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "Fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy "));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_TRANSLATED, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_state_applies_to_header_and_obsolete)
{
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(push_str(&entry.msgstr, "Content-Type: text/plain; charset=UTF-8\n"));
    NAOS_CHECK_INT_EQ(MND_PO_STATE_TRANSLATED, mnd_po_entry_state(&entry));
    entry.obsolete = true;
    NAOS_CHECK_INT_EQ(MND_PO_STATE_TRANSLATED, mnd_po_entry_state(&entry));
    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_flag_add_does_not_duplicate)
{
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "c-format"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "c-format"));
    NAOS_REQUIRE_UINT_EQ(2, entry.flags.count);
    NAOS_CHECK_STR_EQ("fuzzy", mnd_buf_cstr(&entry.flags.items[0]));
    NAOS_CHECK_STR_EQ("c-format", mnd_buf_cstr(&entry.flags.items[1]));
    NAOS_CHECK_TRUE(mnd_po_flag_has(&entry, "fuzzy"));
    NAOS_CHECK_TRUE(mnd_po_flag_has(&entry, "c-format"));
    NAOS_CHECK_FALSE(mnd_po_flag_has(&entry, "c-forma"));
    NAOS_CHECK_FALSE(mnd_po_flag_has(&entry, "c-format-x"));
    NAOS_CHECK_FALSE(mnd_po_flag_has(&entry, ""));

    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_flag_remove_keeps_other_order)
{
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "a"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "b"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "c"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "d"));

    mnd_po_flag_remove(&entry, "fuzzy");
    NAOS_REQUIRE_UINT_EQ(4, entry.flags.count);
    NAOS_CHECK_STR_EQ("a", mnd_buf_cstr(&entry.flags.items[0]));
    NAOS_CHECK_STR_EQ("b", mnd_buf_cstr(&entry.flags.items[1]));
    NAOS_CHECK_STR_EQ("c", mnd_buf_cstr(&entry.flags.items[2]));
    NAOS_CHECK_STR_EQ("d", mnd_buf_cstr(&entry.flags.items[3]));
    NAOS_CHECK_FALSE(mnd_po_flag_has(&entry, "fuzzy"));

    /* ないフラグの削除は何もしない */
    mnd_po_flag_remove(&entry, "fuzzy");
    mnd_po_flag_remove(&entry, "");
    NAOS_REQUIRE_UINT_EQ(4, entry.flags.count);

    /* 先頭と末尾の削除 */
    mnd_po_flag_remove(&entry, "a");
    mnd_po_flag_remove(&entry, "d");
    NAOS_REQUIRE_UINT_EQ(2, entry.flags.count);
    NAOS_CHECK_STR_EQ("b", mnd_buf_cstr(&entry.flags.items[0]));
    NAOS_CHECK_STR_EQ("c", mnd_buf_cstr(&entry.flags.items[1]));

    /* 削除の後に加えると末尾に付く */
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "a"));
    NAOS_REQUIRE_UINT_EQ(3, entry.flags.count);
    NAOS_CHECK_STR_EQ("a", mnd_buf_cstr(&entry.flags.items[2]));

    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_flag_remove_removes_directly_pushed_duplicates)
{
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(push_str(&entry.flags, "fuzzy"));
    NAOS_REQUIRE_TRUE(push_str(&entry.flags, "x"));
    NAOS_REQUIRE_TRUE(push_str(&entry.flags, "fuzzy"));
    mnd_po_flag_remove(&entry, "fuzzy");
    NAOS_REQUIRE_UINT_EQ(1, entry.flags.count);
    NAOS_CHECK_STR_EQ("x", mnd_buf_cstr(&entry.flags.items[0]));
    NAOS_CHECK_FALSE(mnd_po_flag_has(&entry, "fuzzy"));

    mnd_po_entry_free(&entry);
}

NAOS_TEST(entry_find_duplicate_none)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t first = 99;
    size_t second = 99;

    NAOS_CHECK_FALSE(mnd_po_find_duplicate(&catalog, 0, &first, &second));

    /* msgctxt の有無・値が異なるもの、廃止エントリとの一致は重複ではない */
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "", "a"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "x", "a"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "y", "a"));
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "ab"));
    entry = add_entry(&catalog, NULL, "a");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    entry = add_entry(&catalog, NULL, "a");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;

    NAOS_CHECK_FALSE(mnd_po_find_duplicate(&catalog, 0, &first, &second));
    NAOS_CHECK_UINT_EQ(99, first);
    NAOS_CHECK_UINT_EQ(99, second);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_duplicate_ignores_preceding_obsolete)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t first = 99;
    size_t second = 99;

    /* 廃止エントリが先にあり、同じ msgid の有効なエントリが後にある */
    entry = add_entry(&catalog, NULL, "a");
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));

    NAOS_CHECK_FALSE(mnd_po_find_duplicate(&catalog, 0, &first, &second));

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_duplicate_enumerates_pairs_from_index)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t first = 99;
    size_t second = 99;

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));   /* 0 */
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "c", "b"));    /* 1 */
    entry = add_entry(&catalog, NULL, "a");                  /* 2: 廃止 */
    NAOS_REQUIRE_NOT_NULL(entry);
    entry->obsolete = true;
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "", "a"));     /* 3 */
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "c", "b"));    /* 4: 1 と重複 */
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));   /* 5: 0 と重複 */
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, "", "a"));     /* 6: 3 と重複 */
    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));   /* 7: 0 と重複 */

    NAOS_REQUIRE_TRUE(mnd_po_find_duplicate(&catalog, 0, &first, &second));
    NAOS_CHECK_UINT_EQ(1, first);
    NAOS_CHECK_UINT_EQ(4, second);

    NAOS_REQUIRE_TRUE(mnd_po_find_duplicate(&catalog, second + 1, &first, &second));
    NAOS_CHECK_UINT_EQ(0, first);
    NAOS_CHECK_UINT_EQ(5, second);

    NAOS_REQUIRE_TRUE(mnd_po_find_duplicate(&catalog, second + 1, &first, &second));
    NAOS_CHECK_UINT_EQ(3, first);
    NAOS_CHECK_UINT_EQ(6, second);

    /* 先のエントリは最初の定義を返す */
    NAOS_REQUIRE_TRUE(mnd_po_find_duplicate(&catalog, second + 1, &first, &second));
    NAOS_CHECK_UINT_EQ(0, first);
    NAOS_CHECK_UINT_EQ(7, second);

    NAOS_CHECK_FALSE(mnd_po_find_duplicate(&catalog, second + 1, &first, &second));
    NAOS_CHECK_FALSE(mnd_po_find_duplicate(&catalog, catalog.count + 5, &first, &second));

    /* 後のエントリが from 以降なら、先のエントリは from より前でもよい */
    NAOS_REQUIRE_TRUE(mnd_po_find_duplicate(&catalog, 6, &first, &second));
    NAOS_CHECK_UINT_EQ(3, first);
    NAOS_CHECK_UINT_EQ(6, second);

    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(entry_find_duplicate_ignores_has_false_msgctxt_content)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry;
    size_t first = 99;
    size_t second = 99;

    NAOS_REQUIRE_NOT_NULL(add_entry(&catalog, NULL, "a"));
    entry = add_entry(&catalog, NULL, "a");
    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgctxt, "stale"));

    NAOS_REQUIRE_TRUE(mnd_po_find_duplicate(&catalog, 0, &first, &second));
    NAOS_CHECK_UINT_EQ(0, first);
    NAOS_CHECK_UINT_EQ(1, second);

    mnd_po_catalog_free(&catalog);
}

NAOS_SUITE(entry,
    NAOS_ADD_TEST(entry_catalog_init_macro_is_empty),
    NAOS_ADD_TEST(entry_init_sets_empty_state),
    NAOS_ADD_TEST(entry_free_returns_to_initial_state),
    NAOS_ADD_TEST(entry_strings_push_keeps_order_and_bytes),
    NAOS_ADD_TEST(entry_refs_push_keeps_file_and_line),
    NAOS_ADD_TEST(entry_push_rejects_capacity_overflow_before_allocation),
    NAOS_ADD_TEST(entry_catalog_append_rejects_capacity_overflow),
    NAOS_ADD_TEST(entry_catalog_free_is_safe_on_initial_and_freed),
    NAOS_ADD_TEST(entry_catalog_append_and_insert_keep_order),
    NAOS_ADD_TEST(entry_find_distinguishes_empty_and_absent_msgctxt),
    NAOS_ADD_TEST(entry_find_ignores_has_false_msgctxt_content),
    NAOS_ADD_TEST(entry_find_excludes_obsolete),
    NAOS_ADD_TEST(entry_find_on_empty_catalog),
    NAOS_ADD_TEST(entry_header_skips_msgctxt_and_obsolete),
    NAOS_ADD_TEST(entry_header_is_first_entry),
    NAOS_ADD_TEST(entry_state_three_way),
    NAOS_ADD_TEST(entry_state_applies_to_header_and_obsolete),
    NAOS_ADD_TEST(entry_flag_add_does_not_duplicate),
    NAOS_ADD_TEST(entry_flag_remove_keeps_other_order),
    NAOS_ADD_TEST(entry_flag_remove_removes_directly_pushed_duplicates),
    NAOS_ADD_TEST(entry_find_duplicate_none),
    NAOS_ADD_TEST(entry_find_duplicate_ignores_preceding_obsolete),
    NAOS_ADD_TEST(entry_find_duplicate_enumerates_pairs_from_index),
    NAOS_ADD_TEST(entry_find_duplicate_ignores_has_false_msgctxt_content)
)
