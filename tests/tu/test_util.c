/* tu のテスト用の補助関数 */

#include "test_util.h"

#include "foundation/diag.h"
#include "po/po_read.h"

#include <stdio.h>
#include <string.h>

/* 説明に載せる前後のバイト数（片側） */
#define TEST_CONTEXT_BYTES 16

/* diags の各診断を note として表示し、1 つにつき 1 回、診断がないことの検査を失敗させる */
static void report_diags(const mnd_diag_list_t *diags)
{
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);
        const char *path = (diag->path != NULL) ? diag->path : "(unknown)";
        const char *severity = (diag->severity == MND_SEVERITY_ERROR) ? "error" : "warning";

        if (diag->line > 0)
        {
            NAOS_NOTE("%s:%zu: %s: %s", path, diag->line, severity, diag->message);
        }
        else
        {
            NAOS_NOTE("%s: %s: %s", path, severity, diag->message);
        }

        NAOS_CHECK_UINT_EQ(0, count);
    }

    if (diags->dropped)
    {
        NAOS_NOTE("some diagnostics were dropped (out of memory)");
        NAOS_CHECK_FALSE(diags->dropped);
    }
}

mnd_tu_doc_t *test_scan(const char *text, const char *const *keys, size_t key_count)
{
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;
    bool ok;

    options.path = "test.md";
    options.front_matter_keys = keys;
    options.front_matter_key_count = key_count;
    ok = mnd_tu_scan(text, strlen(text), &options, &doc, &diags);
    report_diags(&diags);
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);

    return doc;
}

void test_read_po(const char *text, mnd_po_catalog_t *catalog)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    mnd_po_catalog_init(catalog);
    ok = mnd_po_read(text, strlen(text), "test.po", catalog, &diags);
    report_diags(&diags);
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
}

bool test_find_difference(const char *actual, size_t actual_len,
                          const char *expected, size_t expected_len, size_t *offset)
{
    size_t common = (actual_len < expected_len) ? actual_len : expected_len;
    size_t i;

    for (i = 0; i < common; i++)
    {
        if (actual[i] != expected[i])
        {
            *offset = i;
            return true;
        }
    }
    if (actual_len != expected_len)
    {
        *offset = common;
        return true;
    }

    return false;
}

/* 1 バイトを表示用にエスケープして out に加える。メモリ不足ならテストを止める */
static void append_escaped_byte(mnd_buf_t *out, unsigned char byte)
{
    char hex[8];

    if (byte == '\\')
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "\\\\"));
    }
    else if (byte == '"')
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "\\\""));
    }
    else if (byte == '\n')
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "\\n"));
    }
    else if (byte == '\r')
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "\\r"));
    }
    else if (byte == '\t')
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "\\t"));
    }
    else if ((byte < 0x20) || (byte >= 0x7f))
    {
        /* 前後を切り詰めると UTF-8 の途中で切れ得るため、非 ASCII もエスケープする */
        (void)snprintf(hex, sizeof(hex), "\\x%02X", (unsigned int)byte);
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, hex));
    }
    else
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, byte));
    }
}

/*
 * data[0..len) の offset の前後を "...前[後]..." の形で out に加える。
 * 前後はそれぞれ最大 TEST_CONTEXT_BYTES バイトで、切り詰めた側に ... を付ける
 */
static void append_context(mnd_buf_t *out, const char *data, size_t len, size_t offset)
{
    size_t start = (offset > TEST_CONTEXT_BYTES) ? (offset - TEST_CONTEXT_BYTES) : 0;
    size_t end = ((len - offset) > TEST_CONTEXT_BYTES) ? (offset + TEST_CONTEXT_BYTES) : len;
    size_t i;

    NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, '"'));
    if (start > 0)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "..."));
    }
    for (i = start; i < end; i++)
    {
        if (i == offset)
        {
            NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, '['));
        }
        append_escaped_byte(out, (unsigned char)data[i]);
    }
    if (offset == end)
    {
        /* offset が末尾なら、後ろは空として [] を付ける */
        NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, '['));
    }
    NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, ']'));
    if (end < len)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, "..."));
    }
    NAOS_REQUIRE_TRUE(mnd_buf_append_byte(out, '"'));
}

bool test_describe_difference(const char *actual, size_t actual_len,
                              const char *expected, size_t expected_len, mnd_buf_t *out)
{
    char head[160];
    size_t offset = 0;
    size_t line = 1;
    size_t i;

    mnd_buf_clear(out);
    if (!test_find_difference(actual, actual_len, expected, expected_len, &offset))
    {
        return false;
    }

    /* 行番号は expected の offset より前の LF の数 + 1（offset は expected_len を超えない） */
    for (i = 0; i < offset; i++)
    {
        if (expected[i] == '\n')
        {
            line++;
        }
    }

    (void)snprintf(head, sizeof(head),
                   "first difference at offset %zu (line %zu), actual length %zu, expected length %zu: actual ",
                   offset, line, actual_len, expected_len);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, head));
    append_context(out, actual, actual_len, offset);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, " expected "));
    append_context(out, expected, expected_len, offset);

    return true;
}

void test_check_bytes(const char *actual, size_t actual_len,
                      const char *expected, size_t expected_len)
{
    mnd_buf_t text = MND_BUF_INIT;
    bool differ;

    differ = test_describe_difference(actual, actual_len, expected, expected_len, &text);
    if (differ)
    {
        NAOS_NOTE("%s", mnd_buf_cstr(&text));
    }
    mnd_buf_free(&text);
    NAOS_CHECK_FALSE(differ);
}

void test_check_translated(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                           const mnd_tu_translate_options_t *options,
                           const char *expected, size_t expected_len, mnd_tu_stats_t *stats)
{
    mnd_buf_t out = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    ok = mnd_tu_translate(doc, catalog, options, &out, stats, &diags);
    report_diags(&diags);
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        mnd_buf_free(&out);
        NAOS_FAIL("mnd_tu_translate failed");
    }

    test_check_bytes(out.data, out.len, expected, expected_len);
    mnd_buf_free(&out);
}

/* NUL 終端の expected と buf の内容（長さを含む）が同じことを検査する */
static void check_buf_text(const mnd_buf_t *buf, const char *expected)
{
    NAOS_CHECK_STR_EQ(expected, mnd_buf_cstr(buf));
    NAOS_CHECK_UINT_EQ(strlen(expected), buf->len);
}

void test_check_segments(const mnd_tu_doc_t *doc, const test_segment_expect_t *expected, size_t count)
{
    size_t actual_count = mnd_tu_doc_segment_count(doc);
    size_t common = (actual_count < count) ? actual_count : count;
    size_t i;

    NAOS_CHECK_UINT_EQ(count, actual_count);
    for (i = 0; i < common; i++)
    {
        const mnd_tu_segment_t *seg = mnd_tu_doc_segment(doc, i);
        const test_segment_expect_t *want = &expected[i];

        NAOS_TRACE_PUSH("segment %zu (expected msgid \"%s\")", i, want->msgid);
        NAOS_CHECK_INT_EQ((int)want->origin, (int)seg->origin);
        NAOS_CHECK_UINT_EQ(want->line, seg->line);
        NAOS_CHECK_UINT_EQ(want->start, seg->span.start);
        NAOS_CHECK_UINT_EQ(want->end, seg->span.end);
        check_buf_text(&seg->msgid, want->msgid);
        check_buf_text(&seg->kind, want->kind);
        NAOS_TRACE_POP();
    }

    /* 余ったセグメント（実際に多い側）の msgid を表示する */
    for (i = common; i < actual_count; i++)
    {
        NAOS_NOTE("unexpected segment %zu: msgid \"%s\"", i, mnd_buf_cstr(&mnd_tu_doc_segment(doc, i)->msgid));
        NAOS_CHECK_UINT_LT(i, common);
    }
}
