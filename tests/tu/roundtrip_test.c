/* スイート roundtrip: spec.txt の全例のラウンドトリップ（5.1〜5.4） */

#include <naos/naos.h>

#include "../markdown/spec_reader.h"
#include "test_util.h"
#include "tu/tu.h"
#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* spec.txt（タグ 0.31.2）の例の数 */
#define SPEC_EXAMPLE_COUNT 652u

/* 例の Markdown から作る入力の版 */
typedef enum roundtrip_variant
{
    VARIANT_ORIGINAL,         /* 例の Markdown そのまま（LF） */
    VARIANT_CRLF,             /* LF をすべて CRLF にした版 */
    VARIANT_NO_FINAL_NEWLINE  /* 末尾の改行（LF）を 1 つ取り除いた版 */
} roundtrip_variant_t;

/* 1 つの版に対して行う確認 */
typedef enum roundtrip_check
{
    CHECK_EMPTY_CATALOG,     /* (a) 空のカタログで出力が入力と同一 */
    CHECK_IDENTITY_CATALOG,  /* (b) msgstr = msgid のカタログで出力が入力と同一 */
    CHECK_DIRECT_SPLICE      /* (c) 短絡を通さない書き戻しが原文の範囲と同一 */
} roundtrip_check_t;

static const char *variant_name(roundtrip_variant_t variant)
{
    switch (variant)
    {
    case VARIANT_CRLF:
        return "CRLF";
    case VARIANT_NO_FINAL_NEWLINE:
        return "no final newline";
    case VARIANT_ORIGINAL:
    default:
        return "original";
    }
}

static const char *check_name(roundtrip_check_t check)
{
    switch (check)
    {
    case CHECK_IDENTITY_CATALOG:
        return "msgstr == msgid catalog";
    case CHECK_DIRECT_SPLICE:
        return "direct splice";
    case CHECK_EMPTY_CATALOG:
    default:
        return "empty catalog";
    }
}

/* spec.txt を読み込み、652 個の例があることを確かめる */
static void load_spec(mnd_md_spec_t *spec)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = mnd_md_spec_load(MND_MD_SPEC_FILE, spec, &diags);

    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
    NAOS_REQUIRE_UINT_EQ(SPEC_EXAMPLE_COUNT, spec->count);
}

/* 例 ex の Markdown から版 variant の入力を out（初期化済み。内容は捨てる）に作る。メモリ不足なら false */
static bool make_input(const mnd_md_spec_example_t *ex, roundtrip_variant_t variant, mnd_buf_t *out)
{
    const char *md = mnd_buf_cstr(&ex->markdown);
    size_t len = ex->markdown.len;
    size_t i;

    mnd_buf_clear(out);
    if (variant == VARIANT_CRLF)
    {
        for (i = 0; i < len; i++)
        {
            if (md[i] == '\n')
            {
                if (!mnd_buf_append(out, "\r\n", 2))
                {
                    return false;
                }
            }
            else if (!mnd_buf_append_byte(out, (unsigned char)md[i]))
            {
                return false;
            }
        }

        return true;
    }

    if ((variant == VARIANT_NO_FINAL_NEWLINE) && (len > 0) && (md[len - 1] == '\n'))
    {
        len--;
    }

    return mnd_buf_append(out, md, len);
}

/* diags の最初の診断を why の後ろに説明として加える（診断がなければ何もしない） */
static void describe_first_diag(const mnd_diag_list_t *diags, mnd_buf_t *why)
{
    const mnd_diag_t *diag;

    if (mnd_diag_list_count(diags) == 0)
    {
        return;
    }
    diag = mnd_diag_list_at(diags, 0);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, ": "));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, diag->message));
}

/*
 * msgstr を持たないエントリには msgid と同じ msgstr を足し、持つエントリは msgstr[0] を msgid に
 * 置き換える（mnd_tu_extract のカタログには msgstr の要素がない）。メモリ不足なら false
 */
static bool make_identity_catalog(mnd_po_catalog_t *catalog)
{
    size_t i;

    for (i = 0; i < catalog->count; i++)
    {
        mnd_po_entry_t *entry = &catalog->entries[i];

        if (entry->msgstr.count == 0)
        {
            if (!mnd_po_strings_push(&entry->msgstr, mnd_buf_cstr(&entry->msgid), entry->msgid.len))
            {
                return false;
            }
        }
        else
        {
            mnd_buf_clear(&entry->msgstr.items[0]);
            if (!mnd_buf_append(&entry->msgstr.items[0], mnd_buf_cstr(&entry->msgid), entry->msgid.len))
            {
                return false;
            }
        }
    }

    return true;
}

/*
 * (a) / (b): doc を再構成し、出力が data[0..len) と同一で、診断がなく、統計が期待どおりであることを
 * 確かめる。identity なら msgstr = msgid のカタログ、そうでなければ空のカタログを使う。
 * 一致しなければ理由を why（初期化済み。空）に入れて false を返す
 */
static bool check_translate(const mnd_tu_doc_t *doc, const char *data, size_t len, bool identity,
                            mnd_buf_t *why)
{
    mnd_po_catalog_t catalog;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    size_t count = mnd_tu_doc_segment_count(doc);
    size_t expect_translated = identity ? count : 0;
    size_t expect_missing = identity ? 0 : count;
    bool ok = false;

    mnd_po_catalog_init(&catalog);
    if (identity && !mnd_tu_extract(doc, "test.md", &catalog, &diags))
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "mnd_tu_extract failed"));
        describe_first_diag(&diags, why);
    }
    else if (mnd_diag_list_count(&diags) != 0)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "mnd_tu_extract reported diagnostics"));
        describe_first_diag(&diags, why);
    }
    else if (identity && !make_identity_catalog(&catalog))
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "out of memory while making the catalog"));
    }
    else if (!mnd_tu_translate(doc, &catalog, NULL, &out, &stats, &diags))
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "mnd_tu_translate failed"));
        describe_first_diag(&diags, why);
    }
    else if (mnd_diag_list_count(&diags) != 0)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "mnd_tu_translate reported diagnostics"));
        describe_first_diag(&diags, why);
    }
    else if (test_describe_difference(mnd_buf_cstr(&out), out.len, data, len, why))
    {
        /* why に最初に違う位置の説明が入った */
    }
    else if ((stats.total != count) || (stats.translated != expect_translated)
             || (stats.missing != expect_missing) || (stats.fuzzy != 0) || (stats.untranslated != 0))
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "unexpected stats"));
    }
    else
    {
        ok = true;
    }
    mnd_buf_free(&out);
    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&catalog);

    return ok;
}

/*
 * (c): doc の各 Markdown セグメントに、短絡を通さず mnd_tu_splice_markdown を msgid で直接呼び、
 * 結果が原文の範囲 data[span] と同一であることを確かめる。eol には原文と異なる改行コードを渡し、
 * 原文より多い行の境界の経路を通っていないことも確かめる。確かめたセグメントの数を *checked に足す。
 * 一致しなければ理由を why（初期化済み。空）に入れて false を返す
 */
static bool check_direct_splice(const mnd_tu_doc_t *doc, roundtrip_variant_t variant, size_t *checked,
                                mnd_buf_t *why)
{
    mnd_eol_t eol = (variant == VARIANT_CRLF) ? MND_EOL_LF : MND_EOL_CRLF;
    mnd_buf_t piece = MND_BUF_INIT;
    mnd_buf_t diff = MND_BUF_INIT;
    bool ok = true;
    size_t i;

    for (i = 0; i < doc->seg_count; i++)
    {
        const mnd_tu_seg_t *seg = &doc->segs[i];
        const mnd_buf_t *msgid = &seg->pub.msgid;
        const char *original = doc->data + seg->pub.span.start;
        size_t original_len = seg->pub.span.end - seg->pub.span.start;
        char header[96];

        if (seg->pub.origin != MND_TU_ORIGIN_MARKDOWN)
        {
            continue;
        }
        (*checked)++;
        mnd_buf_clear(&piece);
        if (!mnd_tu_splice_markdown(doc, seg, mnd_buf_cstr(msgid), msgid->len, eol, &piece))
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, "mnd_tu_splice_markdown failed"));
            ok = false;
            break;
        }
        if (test_describe_difference(mnd_buf_cstr(&piece), piece.len, original, original_len, &diff))
        {
            snprintf(header, sizeof(header), "segment %zu (line %zu): ", i, seg->pub.line);
            NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, header));
            NAOS_REQUIRE_TRUE(mnd_buf_append_str(why, mnd_buf_cstr(&diff)));
            ok = false;
            break;
        }
    }
    mnd_buf_free(&piece);
    mnd_buf_free(&diff);

    return ok;
}

/*
 * spec の全例について、版 variant の入力を作って走査し、確認 check を行う。
 * 失敗した例は番号・行・節・理由（最初に違う位置を含む）をメモに出して検査を失敗させ（止めない）、
 * 最後に失敗の数をメモに出して、その数が 0 であることを検査する
 */
static void run_roundtrip(const mnd_md_spec_t *spec, roundtrip_variant_t variant, roundtrip_check_t check)
{
    mnd_buf_t input = MND_BUF_INIT;
    mnd_buf_t why = MND_BUF_INIT;
    size_t failures = 0;
    size_t checked = 0;
    size_t i;

    for (i = 0; i < spec->count; i++)
    {
        const mnd_md_spec_example_t *ex = &spec->examples[i];
        mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        mnd_tu_doc_t *doc = NULL;
        bool ok = false;

        NAOS_REQUIRE_TRUE(make_input(ex, variant, &input));
        mnd_buf_clear(&why);
        options.path = "test.md";
        if (!mnd_tu_scan(mnd_buf_cstr(&input), input.len, &options, &doc, &diags))
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append_str(&why, "mnd_tu_scan failed"));
            describe_first_diag(&diags, &why);
        }
        else if (mnd_diag_list_count(&diags) != 0)
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append_str(&why, "mnd_tu_scan reported diagnostics"));
            describe_first_diag(&diags, &why);
        }
        else if (check == CHECK_DIRECT_SPLICE)
        {
            ok = check_direct_splice(doc, variant, &checked, &why);
        }
        else
        {
            ok = check_translate(doc, mnd_buf_cstr(&input), input.len, (check == CHECK_IDENTITY_CATALOG),
                                 &why);
        }

        if (!ok)
        {
            failures++;
            NAOS_NOTE("example %zu (line %zu, section \"%s\") [%s, %s]: %s", ex->number, ex->start_line,
                      mnd_buf_cstr(&ex->section), variant_name(variant), check_name(check),
                      mnd_buf_cstr(&why));
            NAOS_CHECK_TRUE(false);
        }
        mnd_tu_doc_free(doc);
        mnd_diag_list_free(&diags);
    }

    if (failures > 0)
    {
        NAOS_NOTE("%zu of %zu examples failed [%s, %s]", failures, spec->count, variant_name(variant),
                  check_name(check));
    }
    NAOS_CHECK_UINT_EQ(0, failures);
    if (check == CHECK_DIRECT_SPLICE)
    {
        /* 確認が空振りでないこと（spec.txt には段落と見出しが多数ある） */
        NAOS_CHECK_UINT_GT(checked, 0);
    }
    mnd_buf_free(&input);
    mnd_buf_free(&why);
}

/* (a) 空のカタログで、全例の出力が入力とバイト単位で同一（5.1、5.3） */
NAOS_TEST(roundtrip_empty_catalog_is_identity)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    load_spec(&spec);
    run_roundtrip(&spec, VARIANT_ORIGINAL, CHECK_EMPTY_CATALOG);
    mnd_md_spec_free(&spec);
}

/* (b) 抽出したすべての msgid に msgstr = msgid を与えたカタログで、全例の出力が入力と同一（5.2、5.3） */
NAOS_TEST(roundtrip_identity_catalog_is_identity)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    load_spec(&spec);
    run_roundtrip(&spec, VARIANT_ORIGINAL, CHECK_IDENTITY_CATALOG);
    mnd_md_spec_free(&spec);
}

/* (c) 短絡を通さず、各 Markdown セグメントを msgid で書き戻した結果が原文の範囲と同一（5.2、5.3） */
NAOS_TEST(roundtrip_direct_splice_is_identity)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    load_spec(&spec);
    run_roundtrip(&spec, VARIANT_ORIGINAL, CHECK_DIRECT_SPLICE);
    mnd_md_spec_free(&spec);
}

/* (d) LF をすべて CRLF にした版で (a)〜(c)（5.4） */
NAOS_TEST(roundtrip_crlf_is_identity)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    load_spec(&spec);
    run_roundtrip(&spec, VARIANT_CRLF, CHECK_EMPTY_CATALOG);
    run_roundtrip(&spec, VARIANT_CRLF, CHECK_IDENTITY_CATALOG);
    run_roundtrip(&spec, VARIANT_CRLF, CHECK_DIRECT_SPLICE);
    mnd_md_spec_free(&spec);
}

/* (e) 末尾の改行を取り除いた版で (a)〜(c)（5.4） */
NAOS_TEST(roundtrip_no_final_newline_is_identity)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    load_spec(&spec);
    run_roundtrip(&spec, VARIANT_NO_FINAL_NEWLINE, CHECK_EMPTY_CATALOG);
    run_roundtrip(&spec, VARIANT_NO_FINAL_NEWLINE, CHECK_IDENTITY_CATALOG);
    run_roundtrip(&spec, VARIANT_NO_FINAL_NEWLINE, CHECK_DIRECT_SPLICE);
    mnd_md_spec_free(&spec);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_empty_catalog_is_identity),
    NAOS_ADD_TEST(roundtrip_identity_catalog_is_identity),
    NAOS_ADD_TEST(roundtrip_direct_splice_is_identity),
    NAOS_ADD_TEST(roundtrip_crlf_is_identity),
    NAOS_ADD_TEST(roundtrip_no_final_newline_is_identity)
)
