/* msgmerge 0.21 の出力である正解ファイルとの比較（9.1〜9.3） */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po_merge/merge.h"
#include "test_util.h"

#include <stdbool.h>
#include <string.h>

/*
 * data/<case_name>/ の old.po と template.pot（msgcat 0.21 で整形済み）を読み込み、
 * fuzzy_matching の設定でマージして既定の設定で書き出した結果が、expected.po
 * （msgmerge 0.21 --previous の出力。作り方は data/README.md）とバイト単位で一致することを確かめる。
 * 入力は msgcat の正規形なので、読み込んで書き出すと元のファイルに一致することも確かめる
 */
static void golden_run(const char *case_name, bool fuzzy_matching)
{
    mnd_buf_t name = MND_BUF_INIT;
    mnd_buf_t old_text = MND_BUF_INIT;
    mnd_buf_t template_text = MND_BUF_INIT;
    mnd_buf_t expected_text = MND_BUF_INIT;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t out;
    mnd_merge_options_t options = MND_MERGE_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&name, case_name) && mnd_buf_append_str(&name, "/old.po"));
    TEST_READ_DATA_FILE(mnd_buf_cstr(&name), &old_text);
    mnd_buf_clear(&name);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&name, case_name)
                      && mnd_buf_append_str(&name, "/template.pot"));
    TEST_READ_DATA_FILE(mnd_buf_cstr(&name), &template_text);
    mnd_buf_clear(&name);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&name, case_name)
                      && mnd_buf_append_str(&name, "/expected.po"));
    TEST_READ_DATA_FILE(mnd_buf_cstr(&name), &expected_text);

    /* 正解ファイルは LF だけで書かれている（チェックアウトで CRLF に変わっていない） */
    NAOS_REQUIRE_NULL(memchr(old_text.data, '\r', old_text.len));
    NAOS_REQUIRE_NULL(memchr(template_text.data, '\r', template_text.len));
    NAOS_REQUIRE_UINT_GT(expected_text.len, 0);
    NAOS_REQUIRE_NULL(memchr(expected_text.data, '\r', expected_text.len));

    TEST_READ_PO(mnd_buf_cstr(&old_text), &old_catalog);
    TEST_READ_PO(mnd_buf_cstr(&template_text), &template_catalog);
    TEST_CHECK_WRITTEN(&old_catalog, mnd_buf_cstr(&old_text));
    TEST_CHECK_WRITTEN(&template_catalog, mnd_buf_cstr(&template_text));

    mnd_po_catalog_init(&out);
    options.fuzzy_matching = fuzzy_matching;
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "template.pot", &old_catalog, "old.po",
                                         &options, &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    TEST_CHECK_WRITTEN(&out, mnd_buf_cstr(&expected_text));

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
    mnd_buf_free(&expected_text);
    mnd_buf_free(&template_text);
    mnd_buf_free(&old_text);
    mnd_buf_free(&name);
}

/* 完全一致、翻訳者コメントの保持、参照・抽出コメント・no-wrap の更新、fuzzy の既存エントリ */
NAOS_TEST(golden_exact)
{
    golden_run("exact", true);
}

/* あいまい一致（前回の msgid、msgctxt の異なる候補、同点、短い msgid、日本語の msgid） */
NAOS_TEST(golden_fuzzy)
{
    golden_run("fuzzy", true);
}

/* 新規、廃止エントリ化、訳のないエントリの削除、廃止エントリの復活、元からの廃止エントリ */
NAOS_TEST(golden_obsolete)
{
    golden_run("obsolete", true);
}

/* ヘッダの更新 */
NAOS_TEST(golden_header)
{
    golden_run("header", true);
}

/* テンプレートにだけヘッダがある */
NAOS_TEST(golden_no_header)
{
    golden_run("no_header", true);
}

/* 単数形と複数形の食い違い、新規の複数形エントリ */
NAOS_TEST(golden_plural)
{
    golden_run("plural", true);
}

/* fuzzy と同じ入力を、あいまい一致なしで（msgmerge --no-fuzzy-matching）（9.3） */
NAOS_TEST(golden_nofuzzy)
{
    golden_run("nofuzzy", false);
}

NAOS_SUITE(golden,
    NAOS_ADD_TEST(golden_exact),
    NAOS_ADD_TEST(golden_fuzzy),
    NAOS_ADD_TEST(golden_obsolete),
    NAOS_ADD_TEST(golden_header),
    NAOS_ADD_TEST(golden_no_header),
    NAOS_ADD_TEST(golden_plural),
    NAOS_ADD_TEST(golden_nofuzzy)
)
