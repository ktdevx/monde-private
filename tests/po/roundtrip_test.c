#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"
#include "po/po.h"
#include "po/po_read.h"
#include "po/po_write.h"

#include <string.h>

/*
 * roundtrip: data/ の正解ファイル（Ubuntu 24.04 の msgcat 0.21 の出力。作り方は data/README.md）を
 * 読み込んで書き出し、バイト単位で比べる。正解ファイルはバイト列のまま読み、全環境で同じ結果を求める
 */

/* 既定の設定でバイト単位の往復を確かめる正解ファイル（nowrap_option.po は折り返しなしで確かめる） */
static const char *const golden_files[] = {
    "basic.po", "wrap_latin.po", "wrap_cjk.po", "refs.po", "nowrap_flag.po",
};

#define GOLDEN_FILE_COUNT (sizeof(golden_files) / sizeof(golden_files[0]))

/* テスト用: data/<name> をバイト列のまま content に読む。BOM がなく、空でないことも確かめる */
static void load_golden(const char *name, mnd_buf_t *content)
{
    mnd_buf_t path = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&path, MND_PO_TEST_DATA_DIR "/")
                      && mnd_buf_append_str(&path, name));
    NAOS_REQUIRE_TRUE(mnd_file_read(mnd_buf_cstr(&path), content, &had_bom, &diags));
    NAOS_REQUIRE_FALSE(had_bom);
    NAOS_REQUIRE_UINT_GT(content->len, 0);
    /* 正解ファイルは LF だけで書かれている（チェックアウトで CRLF に変わっていない） */
    NAOS_REQUIRE_NULL(memchr(content->data, '\r', content->len));
    mnd_buf_free(&path);
    mnd_diag_list_free(&diags);
}

/* テスト用: data[0..len) を読み込み、診断（警告を含む）が 1 つもないことを確かめる */
static void read_clean(const char *data, size_t len, const char *path, mnd_po_catalog_t *catalog)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = mnd_po_read(data, len, path, catalog, &diags);

    if (mnd_diag_list_count(&diags) > 0)
    {
        NAOS_NOTE("first diagnostic: line %zu: %s", mnd_diag_list_at(&diags, 0)->line,
                  mnd_diag_list_at(&diags, 0)->message);
    }

    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
}

/* テスト用: catalog を書き出し、診断がないことを確かめる。no_wrap と eol は書き出しのオプション */
static void write_clean(const mnd_po_catalog_t *catalog, bool no_wrap, mnd_eol_t eol,
                        mnd_buf_t *out)
{
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    options.no_wrap = no_wrap;
    options.eol = eol;
    NAOS_REQUIRE_TRUE(mnd_po_write(catalog, &options, out, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

/* テスト用: 文字列の比較（内容は NUL を含まない。差分の位置を表示するため STR_EQ で比べる） */
static void check_same_text(const mnd_buf_t *actual, const mnd_buf_t *expected)
{
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(actual), mnd_buf_cstr(expected));
    NAOS_CHECK_UINT_EQ(actual->len, expected->len);
}

static void check_same_strings(const mnd_po_strings_t *a, const mnd_po_strings_t *b)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(a->count, b->count);
    for (i = 0; i < a->count; i++)
    {
        NAOS_CHECK_MEM_EQ(a->items[i].data, a->items[i].len, b->items[i].data, b->items[i].len);
    }
}

static void check_same_optional(bool has_a, const mnd_buf_t *a, bool has_b, const mnd_buf_t *b)
{
    NAOS_CHECK_INT_EQ(has_a, has_b);
    if (has_a && has_b)
    {
        NAOS_CHECK_MEM_EQ(a->data, a->len, b->data, b->len);
    }
}

/* テスト用: 2 つのカタログのエントリの列が、入力上の開始行を除いて同じであることを確かめる（7.1、7.2） */
static void check_same_catalog(const mnd_po_catalog_t *a, const mnd_po_catalog_t *b)
{
    size_t i;
    size_t k;

    NAOS_REQUIRE_UINT_EQ(a->count, b->count);
    for (i = 0; i < a->count; i++)
    {
        const mnd_po_entry_t *x = &a->entries[i];
        const mnd_po_entry_t *y = &b->entries[i];

        NAOS_TRACE_PUSH("entry %zu", i);
        check_same_strings(&x->translator_comments, &y->translator_comments);
        check_same_strings(&x->extracted_comments, &y->extracted_comments);
        NAOS_REQUIRE_UINT_EQ(x->refs.count, y->refs.count);
        for (k = 0; k < x->refs.count; k++)
        {
            NAOS_CHECK_MEM_EQ(x->refs.items[k].file.data, x->refs.items[k].file.len,
                              y->refs.items[k].file.data, y->refs.items[k].file.len);
            NAOS_CHECK_UINT_EQ(x->refs.items[k].line, y->refs.items[k].line);
        }
        check_same_strings(&x->flags, &y->flags);
        check_same_optional(x->has_prev_msgctxt, &x->prev_msgctxt, y->has_prev_msgctxt,
                            &y->prev_msgctxt);
        check_same_optional(x->has_prev_msgid, &x->prev_msgid, y->has_prev_msgid, &y->prev_msgid);
        check_same_optional(x->has_prev_msgid_plural, &x->prev_msgid_plural,
                            y->has_prev_msgid_plural, &y->prev_msgid_plural);
        check_same_optional(x->has_msgctxt, &x->msgctxt, y->has_msgctxt, &y->msgctxt);
        NAOS_CHECK_MEM_EQ(x->msgid.data, x->msgid.len, y->msgid.data, y->msgid.len);
        check_same_optional(x->has_msgid_plural, &x->msgid_plural, y->has_msgid_plural,
                            &y->msgid_plural);
        check_same_strings(&x->msgstr, &y->msgstr);
        NAOS_CHECK_INT_EQ(x->obsolete, y->obsolete);
        NAOS_TRACE_POP();
    }
}

/* テスト用: LF を CRLF に変えた内容を out に作る */
static void to_crlf(const mnd_buf_t *lf, mnd_buf_t *out)
{
    size_t i;

    for (i = 0; i < lf->len; i++)
    {
        if (lf->data[i] == '\n')
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, '\r'));
        }

        NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, lf->data[i]));
    }
}

NAOS_TEST(roundtrip_golden_files_are_written_back_byte_for_byte)
{
    size_t i;

    /* 7.3、6.1、6.2、6.4、6.6、6.7: msgcat の既定の出力を読み込み、既定の設定で書き出すと同じになる */
    for (i = 0; i < GOLDEN_FILE_COUNT; i++)
    {
        mnd_buf_t golden = MND_BUF_INIT;
        mnd_buf_t out = MND_BUF_INIT;
        mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;

        NAOS_TRACE_PUSH("%s", golden_files[i]);
        load_golden(golden_files[i], &golden);
        read_clean(golden.data, golden.len, golden_files[i], &catalog);
        write_clean(&catalog, false, MND_EOL_LF, &out);
        check_same_text(&out, &golden);
        NAOS_TRACE_POP();
        mnd_buf_free(&golden);
        mnd_buf_free(&out);
        mnd_po_catalog_free(&catalog);
    }
}

NAOS_TEST(roundtrip_no_wrap_matches_msgcat_no_wrap)
{
    static const char *const sources[] = { "wrap_cjk.po", "nowrap_option.po" };
    mnd_buf_t expected = MND_BUF_INIT;
    size_t i;

    /* 6.5: wrap_cjk.po とそれを msgcat --no-wrap で書いた nowrap_option.po を、折り返しなしで書き出す */
    load_golden("nowrap_option.po", &expected);
    for (i = 0; i < (sizeof(sources) / sizeof(sources[0])); i++)
    {
        mnd_buf_t golden = MND_BUF_INIT;
        mnd_buf_t out = MND_BUF_INIT;
        mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;

        NAOS_TRACE_PUSH("%s", sources[i]);
        load_golden(sources[i], &golden);
        read_clean(golden.data, golden.len, sources[i], &catalog);
        write_clean(&catalog, true, MND_EOL_LF, &out);
        check_same_text(&out, &expected);
        NAOS_TRACE_POP();
        mnd_buf_free(&golden);
        mnd_buf_free(&out);
        mnd_po_catalog_free(&catalog);
    }

    mnd_buf_free(&expected);
}

NAOS_TEST(roundtrip_crlf_input_is_written_back_as_lf)
{
    size_t i;

    /* 2.8: CRLF に変えた正解ファイルは、LF で書き出すと元に、CRLF で書き出すと CRLF の内容に戻る */
    for (i = 0; i < GOLDEN_FILE_COUNT; i++)
    {
        mnd_buf_t golden = MND_BUF_INIT;
        mnd_buf_t crlf = MND_BUF_INIT;
        mnd_buf_t out = MND_BUF_INIT;
        mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
        mnd_po_catalog_t lf_catalog = MND_PO_CATALOG_INIT;

        NAOS_TRACE_PUSH("%s", golden_files[i]);
        load_golden(golden_files[i], &golden);
        to_crlf(&golden, &crlf);
        read_clean(crlf.data, crlf.len, golden_files[i], &catalog);
        read_clean(golden.data, golden.len, golden_files[i], &lf_catalog);
        check_same_catalog(&catalog, &lf_catalog);
        write_clean(&catalog, false, MND_EOL_LF, &out);
        check_same_text(&out, &golden);
        mnd_buf_clear(&out);
        write_clean(&catalog, false, MND_EOL_CRLF, &out);
        check_same_text(&out, &crlf);
        NAOS_TRACE_POP();
        mnd_buf_free(&golden);
        mnd_buf_free(&crlf);
        mnd_buf_free(&out);
        mnd_po_catalog_free(&catalog);
        mnd_po_catalog_free(&lf_catalog);
    }
}

NAOS_TEST(roundtrip_written_content_reads_back_as_same_catalog)
{
    size_t i;

    /* 7.1、7.2: 既定の設定と折り返しなしのどちらで書き出しても、読み直すと同じカタログになる */
    for (i = 0; i < GOLDEN_FILE_COUNT; i++)
    {
        mnd_buf_t golden = MND_BUF_INIT;
        mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
        int no_wrap;

        NAOS_TRACE_PUSH("%s", golden_files[i]);
        load_golden(golden_files[i], &golden);
        read_clean(golden.data, golden.len, golden_files[i], &catalog);
        for (no_wrap = 0; no_wrap <= 1; no_wrap++)
        {
            mnd_buf_t out = MND_BUF_INIT;
            mnd_po_catalog_t again = MND_PO_CATALOG_INIT;

            NAOS_TRACE_PUSH("no_wrap=%d", no_wrap);
            write_clean(&catalog, no_wrap != 0, MND_EOL_LF, &out);
            read_clean(out.data, out.len, "written.po", &again);
            check_same_catalog(&again, &catalog);
            NAOS_TRACE_POP();
            mnd_buf_free(&out);
            mnd_po_catalog_free(&again);
        }

        NAOS_TRACE_POP();
        mnd_buf_free(&golden);
        mnd_po_catalog_free(&catalog);
    }
}

NAOS_TEST(roundtrip_other_line_splitting_is_reformatted_by_the_rules)
{
    size_t i;

    /*
     * 7.4: 折り返しなしで書き、エントリの間の空行を 3 つに増やした内容（gettext の既定の形式でない）を
     * 読み込んで既定の設定で書き出すと、規則どおりの整形（正解ファイル）になる
     */
    for (i = 0; i < GOLDEN_FILE_COUNT; i++)
    {
        mnd_buf_t golden = MND_BUF_INIT;
        mnd_buf_t unwrapped = MND_BUF_INIT;
        mnd_buf_t spaced = MND_BUF_INIT;
        mnd_buf_t out = MND_BUF_INIT;
        mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
        mnd_po_catalog_t again = MND_PO_CATALOG_INIT;
        size_t k;

        NAOS_TRACE_PUSH("%s", golden_files[i]);
        load_golden(golden_files[i], &golden);
        read_clean(golden.data, golden.len, golden_files[i], &catalog);
        write_clean(&catalog, true, MND_EOL_LF, &unwrapped);
        for (k = 0; k < unwrapped.len; k++)
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&spaced, unwrapped.data[k]));
            if ((unwrapped.data[k] == '\n') && ((k + 1) < unwrapped.len)
                && (unwrapped.data[k + 1] == '\n'))
            {
                NAOS_REQUIRE_TRUE(mnd_buf_append_str(&spaced, "\n\n"));
            }
        }

        read_clean(spaced.data, spaced.len, "spaced.po", &again);
        write_clean(&again, false, MND_EOL_LF, &out);
        check_same_text(&out, &golden);
        NAOS_TRACE_POP();
        mnd_buf_free(&golden);
        mnd_buf_free(&unwrapped);
        mnd_buf_free(&spaced);
        mnd_buf_free(&out);
        mnd_po_catalog_free(&catalog);
        mnd_po_catalog_free(&again);
    }
}

NAOS_TEST(roundtrip_irregular_input_is_reformatted_by_the_rules)
{
    /* 7.4: 文字列の分け方、空白、空行、コメントの順、フラグの書き方が gettext と違う入力 */
    static const char input[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; \"\n"
        "\"charset=UTF-8\\n\"\n"
        "  \"Language: ja\\n\"\n"
        "\n"
        "\n"
        "\n"
        "#,fuzzy ,  c-format\n"
        "#. extracted\n"
        "# translator\n"
        "#:   b.c:2\ta.c:1\n"
        "\n"
        "msgid \"Hello, \"\n"
        "    \"world\"\n"
        "msgstr \"\"\n"
        "\"こんにちは\"\n"
        "\"、世界\"\n"
        "msgid \"\"\n"
        "\"sh\"\n"
        "\"ort\"\n"
        "msgstr \"\"\n"
        "msgid \"one two three four \"\n"
        "\"five six \"\n"
        "\"seven eight nine ten eleven twelve thirteen fourteen\"\n"
        "msgstr \"\"\n"
        "#~msgid \"old\"\n"
        "#~   msgstr \"古い\"\n";
    static const char expected[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Language: ja\\n\"\n"
        "\n"
        "# translator\n"
        "#. extracted\n"
        "#: b.c:2 a.c:1\n"
        "#, fuzzy, c-format\n"
        "msgid \"Hello, world\"\n"
        "msgstr \"こんにちは、世界\"\n"
        "\n"
        "msgid \"short\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"\"\n"
        "\"one two three four five six seven eight nine ten eleven twelve thirteen \"\n"
        "\"fourteen\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"old\"\n"
        "#~ msgstr \"古い\"\n";
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t out = MND_BUF_INIT;

    read_clean(input, sizeof(input) - 1, "irregular.po", &catalog);
    write_clean(&catalog, false, MND_EOL_LF, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), expected);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(roundtrip_refs_outside_golden_files_read_back)
{
    /*
     * 7.2: 正解ファイルに入れられない参照（msgcat 0.21 は空白を含むファイル名を囲まず、読むときに
     * 空白で分けるため）と、空のファイル名の参照も、書き出して読み直すと同じになる
     */
    static const struct
    {
        const char *file;
        size_t line;
    } refs[] = {
        { "docs/user guide.md", 3 }, { "tab\there.c", 0 }, { "", 5 }, { "", 0 }, { "b.c", 0 },
    };
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_catalog_t again = MND_PO_CATALOG_INIT;
    mnd_po_entry_t *entry = mnd_po_catalog_append(&catalog);
    mnd_buf_t out = MND_BUF_INIT;
    size_t i;

    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid, "a"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry->msgstr, "", 0));
    for (i = 0; i < (sizeof(refs) / sizeof(refs[0])); i++)
    {
        NAOS_REQUIRE_TRUE(mnd_po_refs_push(&entry->refs, refs[i].file, refs[i].line));
    }

    write_clean(&catalog, false, MND_EOL_LF, &out);
    read_clean(out.data, out.len, "refs.po", &again);
    check_same_catalog(&again, &catalog);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
    mnd_po_catalog_free(&again);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_golden_files_are_written_back_byte_for_byte),
    NAOS_ADD_TEST(roundtrip_no_wrap_matches_msgcat_no_wrap),
    NAOS_ADD_TEST(roundtrip_crlf_input_is_written_back_as_lf),
    NAOS_ADD_TEST(roundtrip_written_content_reads_back_as_same_catalog),
    NAOS_ADD_TEST(roundtrip_other_line_splitting_is_reformatted_by_the_rules),
    NAOS_ADD_TEST(roundtrip_irregular_input_is_reformatted_by_the_rules),
    NAOS_ADD_TEST(roundtrip_refs_outside_golden_files_read_back)
)
