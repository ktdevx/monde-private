/* スイート splice: 書き戻し（Splicer）（2.7、4.8〜4.10、5.2、5.4） */

#include <naos/naos.h>

#include "test_util.h"
#include "tu/tu.h"
#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "yaml/yaml.h"
#include "yaml/yaml_path.h"

#include <stddef.h>
#include <string.h>

/*
 * 原文 data[0..len) のセグメント seg の範囲を replacement[0..replacement_len) で置き換えた文書を
 * out（初期化済み。内容は捨てる）に入れる
 */
static void splice_rebuild(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                           const char *replacement, size_t replacement_len, mnd_buf_t *out)
{
    mnd_buf_clear(out);
    NAOS_REQUIRE_TRUE(mnd_buf_append(out, doc->data, seg->pub.span.start));
    NAOS_REQUIRE_TRUE(mnd_buf_append(out, replacement, replacement_len));
    NAOS_REQUIRE_TRUE(mnd_buf_append(out, doc->data + seg->pub.span.end, doc->len - seg->pub.span.end));
}

/*
 * text を走査し、index 番目のセグメント（Markdown）を translation で書き戻した文書が expected と
 * バイト単位で同じことを検査する
 */
static void check_markdown(const char *text, size_t index, const char *translation, mnd_eol_t eol,
                           const char *expected)
{
    mnd_tu_doc_t *doc = NULL;
    mnd_buf_t replacement = MND_BUF_INIT;
    mnd_buf_t rebuilt = MND_BUF_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_UINT_GT(doc->seg_count, index);
    NAOS_REQUIRE_TRUE(doc->segs[index].node != NULL);
    NAOS_REQUIRE_TRUE(mnd_tu_splice_markdown(doc, &doc->segs[index], translation, strlen(translation),
                                             eol, &replacement));
    splice_rebuild(doc, &doc->segs[index], replacement.data, replacement.len, &rebuilt);
    TEST_CHECK_BYTES(rebuilt.data, rebuilt.len, expected, strlen(expected));
    mnd_buf_free(&rebuilt);
    mnd_buf_free(&replacement);
    mnd_tu_doc_free(doc);
}

#define CHECK_MARKDOWN(text, index, translation, eol, expected) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_MARKDOWN"); \
        check_markdown((text), (index), (translation), (eol), (expected)); \
        NAOS_TRACE_POP(); \
    } while (0)

/*
 * text を走査し、すべての Markdown のセグメントを msgid で書き戻すと data[span] とバイト単位で
 * 同じことを検査する（短絡を通さない）。セグメントが 1 つ以上あることも確かめる
 */
static void check_identity(const char *text)
{
    mnd_tu_doc_t *doc = NULL;
    mnd_buf_t out = MND_BUF_INIT;
    size_t i;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_UINT_GT(doc->seg_count, 0);
    for (i = 0; i < doc->seg_count; i++)
    {
        const mnd_tu_seg_t *seg = &doc->segs[i];

        NAOS_REQUIRE_TRUE(seg->node != NULL);
        mnd_buf_clear(&out);
        NAOS_REQUIRE_TRUE(mnd_tu_splice_markdown(doc, seg, seg->pub.msgid.data, seg->pub.msgid.len,
                                                 MND_EOL_LF, &out));
        TEST_CHECK_BYTES(out.data, out.len, doc->data + seg->pub.span.start,
                         seg->pub.span.end - seg->pub.span.start);
    }
    mnd_buf_free(&out);
    mnd_tu_doc_free(doc);
}

#define CHECK_IDENTITY(text) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_IDENTITY: " text); \
        check_identity((text)); \
        NAOS_TRACE_POP(); \
    } while (0)

/*
 * text（フロントマターのキーパス key）を走査し、そのフロントマターのセグメントを translation で
 * 書き戻した文書が expected とバイト単位で同じことを検査する
 */
static void check_front_matter(const char *text, const char *key, const char *translation,
                               const char *expected)
{
    const char *const keys[] = { key };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;
    mnd_buf_t replacement = MND_BUF_INIT;
    mnd_buf_t rebuilt = MND_BUF_INIT;

    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_UINT_GT(doc->seg_count, 0);
    NAOS_REQUIRE_TRUE(doc->segs[0].scalar != NULL);
    NAOS_REQUIRE_TRUE(mnd_tu_splice_front_matter(doc, &doc->segs[0], translation, strlen(translation),
                                                 &diags, &replacement));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    splice_rebuild(doc, &doc->segs[0], replacement.data, replacement.len, &rebuilt);
    TEST_CHECK_BYTES(rebuilt.data, rebuilt.len, expected, strlen(expected));
    mnd_buf_free(&rebuilt);
    mnd_buf_free(&replacement);
    mnd_diag_list_free(&diags);
    mnd_tu_doc_free(doc);
}

#define CHECK_FRONT_MATTER(text, key, translation, expected) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_FRONT_MATTER"); \
        check_front_matter((text), (key), (translation), (expected)); \
        NAOS_TRACE_POP(); \
    } while (0)

/* 走査した文書は原文を複製せずに指し、内部の構造体からセグメントの配列を参照できる */
NAOS_TEST(splice_doc_points_to_source)
{
    static const char data[] = "";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;

    NAOS_REQUIRE_TRUE(mnd_tu_scan(data, 0, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc->data == data);
    NAOS_CHECK_UINT_EQ(0, doc->len);
    NAOS_CHECK_UINT_EQ(0, doc->seg_count);
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/*
 * 引用内の 2 行の段落: 1 行の訳文では 2 行目のプレフィックスは出ない。2 行では原文の境界を使う。
 * 3 行では最後の行のプレフィックス（原文のバイト列）を繰り返す（4.8）
 */
NAOS_TEST(splice_block_quote_two_line_paragraph)
{
    CHECK_MARKDOWN(">  a\n>   b\n", 0, "X", MND_EOL_LF, ">  X\n");
    CHECK_MARKDOWN(">  a\n>   b\n", 0, "X\nY", MND_EOL_LF, ">  X\n>   Y\n");
    CHECK_MARKDOWN(">  a\n>   b\n", 0, "X\nY\nZ", MND_EOL_LF, ">  X\n>   Y\n>   Z\n");
    /* 1 行の段落では祖先から組み立てる */
    CHECK_MARKDOWN(">a\n", 0, "X\nY", MND_EOL_LF, ">X\n> Y\n");
    CHECK_MARKDOWN("> > a\n", 0, "X\nY", MND_EOL_LF, "> > X\n> > Y\n");
}

/* リスト項目内の段落と、項目内の引用・引用内の項目・入れ子の項目の追加行（4.8） */
NAOS_TEST(splice_list_item_extra_lines)
{
    CHECK_MARKDOWN("- a\n", 0, "X\nY", MND_EOL_LF, "- X\n  Y\n");
    CHECK_MARKDOWN("1.  a\n", 0, "X\nY\nZ", MND_EOL_LF, "1.  X\n    Y\n    Z\n");
    CHECK_MARKDOWN(" -  a\n", 0, "X\nY", MND_EOL_LF, " -  X\n    Y\n");
    CHECK_MARKDOWN("- > a\n", 0, "X\nY", MND_EOL_LF, "- > X\n  > Y\n");
    CHECK_MARKDOWN("> - a\n", 0, "X\nY", MND_EOL_LF, "> - X\n>   Y\n");
    CHECK_MARKDOWN("- a\n  - b\n", 1, "X\nY", MND_EOL_LF, "- a\n  - X\n    Y\n");
    CHECK_MARKDOWN("10. a\n    1. b\n", 1, "X\nY", MND_EOL_LF, "10. a\n    1. X\n       Y\n");
    /* 2 行目が遅延継続行でなければ、そのプレフィックスを繰り返す */
    CHECK_MARKDOWN("- a\n   b\n", 0, "X\nY\nZ", MND_EOL_LF, "- X\n   Y\n   Z\n");
}

/* 遅延継続行: 原文の境界はそのまま、追加行は祖先から組み立てる（4.8） */
NAOS_TEST(splice_lazy_continuation_line)
{
    CHECK_MARKDOWN("> a\nb\n", 0, "X\nY\nZ", MND_EOL_LF, "> X\nY\n> Z\n");
    CHECK_MARKDOWN("- a\nb\n", 0, "X\nY\nZ", MND_EOL_LF, "- X\nY\n  Z\n");
    CHECK_MARKDOWN("> - a\nb\n", 0, "X\nY\nZ", MND_EOL_LF, "> - X\nY\n>   Z\n");
}

/* タブを含むインデントは原文のバイト列のまま再現し、最後の行のプレフィックスも繰り返す（4.8） */
NAOS_TEST(splice_tab_indentation)
{
    CHECK_MARKDOWN(">\ta\n>\tb\n", 0, "X\nY\nZ", MND_EOL_LF, ">\tX\n>\tY\n>\tZ\n");
    CHECK_MARKDOWN("-\ta\n\tb\n", 0, "X\nY\nZ", MND_EOL_LF, "-\tX\n\tY\n\tZ\n");
    CHECK_MARKDOWN("a\n\tb\n", 0, "X\nY\nZ", MND_EOL_LF, "X\n\tY\n\tZ\n");
}

/* CRLF の原文: 対応する行は原文の CRLF、追加行は呼び出し側の改行コード（4.9） */
NAOS_TEST(splice_crlf_source)
{
    CHECK_MARKDOWN("> a\r\n> b\r\n", 0, "X\nY\nZ", MND_EOL_CRLF, "> X\r\n> Y\r\n> Z\r\n");
    CHECK_MARKDOWN("> a\r\n> b\r\n", 0, "X\nY\nZ", MND_EOL_LF, "> X\r\n> Y\n> Z\r\n");
    CHECK_MARKDOWN("- a\r\n", 0, "X\nY", MND_EOL_CRLF, "- X\r\n  Y\r\n");
}

/* 改行コードが MND_EOL_NONE なら追加行の境界は LF（4.9） */
NAOS_TEST(splice_eol_none_uses_lf)
{
    CHECK_MARKDOWN("a", 0, "X\nY", MND_EOL_NONE, "X\nY");
    CHECK_MARKDOWN("> a\r\n", 0, "X\nY", MND_EOL_NONE, "> X\n> Y\r\n");
}

/* LF と CRLF の混在: 対応する行はそれぞれの原文の行末記号を再現する（4.9） */
NAOS_TEST(splice_mixed_line_endings)
{
    CHECK_MARKDOWN("a\r\nb\nc\r\n", 0, "X\nY\nZ\nW", MND_EOL_LF, "X\r\nY\nZ\nW\r\n");
    CHECK_MARKDOWN("a\nb\r\nc\n", 0, "X\nY\nZ\nW", MND_EOL_CRLF, "X\nY\r\nZ\r\nW\n");
}

/* ATX 見出しへの 2 行の訳文: プレフィックスは祖先から組み立てる（警告は StructureCheck の役割） */
NAOS_TEST(splice_atx_heading_two_lines)
{
    CHECK_MARKDOWN("# Title  \n", 0, "X\nY", MND_EOL_LF, "# X\nY  \n");
    CHECK_MARKDOWN("> ## Title ##\n", 0, "X\nY", MND_EOL_LF, "> ## X\n> Y ##\n");
    CHECK_MARKDOWN("a\nb\n===\n", 0, "X\nY\nZ", MND_EOL_LF, "X\nY\nZ\n===\n");
}

/* 訳文の後ろは原文の span.end から続き、行の数が減ると以降の行は出力されない（4.8） */
NAOS_TEST(splice_fewer_lines_drops_rest)
{
    CHECK_MARKDOWN("a\nb\nc  \nd\n", 0, "X\nY", MND_EOL_LF, "X\nY\n");
    CHECK_MARKDOWN("a  \n", 0, "X", MND_EOL_LF, "X  \n");
    CHECK_MARKDOWN("a\n", 0, "", MND_EOL_LF, "\n");
}

/* 結果は out の後ろに追加する */
NAOS_TEST(splice_markdown_appends_to_out)
{
    mnd_tu_doc_t *doc = NULL;
    mnd_buf_t out = MND_BUF_INIT;

    TEST_SCAN("> a\n", NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_UINT_EQ(1, doc->seg_count);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "pre:"));
    NAOS_CHECK_TRUE(mnd_tu_splice_markdown(doc, &doc->segs[0], "X\nY", 3, MND_EOL_LF, &out));
    NAOS_CHECK_STR_EQ("pre:X\n> Y", mnd_buf_cstr(&out));
    mnd_buf_free(&out);
    mnd_tu_doc_free(doc);
}

/* msgid と同じ訳文では、出力は原文の範囲とバイト単位で同じ（5.2、5.4） */
NAOS_TEST(splice_msgid_reproduces_span)
{
    CHECK_IDENTITY("a\nb\nc\n");
    CHECK_IDENTITY(">  a\n>   b\nlazy\n");
    CHECK_IDENTITY("- a\n  b\n\n  > c\n  d\n");
    CHECK_IDENTITY("1. > - a\n     >   b\n");
    CHECK_IDENTITY(">\ta\n>\t\tb\n-\tc\n\td\n");
    CHECK_IDENTITY("a  \nb\\\nc   \n");
    CHECK_IDENTITY("# Title #\n\nSet\next\n---\n");
    CHECK_IDENTITY("a\r\nb\nc\r\n> d\r\n> e");
    CHECK_IDENTITY("a\rb\r\rc");
    CHECK_IDENTITY("   a\n    b\n\n- [x](<y>) *z*\n  &amp; \\*\n");
    CHECK_IDENTITY("no trailing newline");
}

/* `|` の値: 末尾の改行を補い、元のスタイルとインデントを保つ（2.7、4.10） */
NAOS_TEST(splice_front_matter_literal_keeps_style)
{
    CHECK_FRONT_MATTER("---\ndesc: |\n  Hello\n  World\n---\nBody\n", "desc", "Bonjour\nMonde",
                       "---\ndesc: |\n  Bonjour\n  Monde\n---\nBody\n");
    CHECK_FRONT_MATTER("---\ndesc: |+\n  Hello\n\n\nnext: 1\n---\n", "desc", "Bonjour",
                       "---\ndesc: |+\n  Bonjour\n\n\nnext: 1\n---\n");
    CHECK_FRONT_MATTER("---\ntitle: Hello\n---\n", "title", "Bonjour", "---\ntitle: Bonjour\n---\n");
    CHECK_FRONT_MATTER("---\ntitle: 'Hello'\n---\n", "title", "it's", "---\ntitle: 'it''s'\n---\n");
}

/* `key:` の空の値（セグメントにはならないが、スカラーを直接指定すると整形の結果を出す: 4.10） */
NAOS_TEST(splice_front_matter_empty_value)
{
    static const char *const keys[] = { "title" };
    static const char text[] = "---\ntitle:\n---\nBody\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_matches_t matches = { NULL, 0 };
    mnd_tu_doc_t *doc = NULL;
    mnd_tu_seg_t seg;
    mnd_buf_t replacement = MND_BUF_INIT;
    mnd_buf_t rebuilt = MND_BUF_INIT;

    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_TRUE(doc->yaml != NULL);
    /* 空の値はセグメントにならない（1.12） */
    NAOS_CHECK_UINT_EQ(1, doc->seg_count);
    NAOS_REQUIRE_TRUE(mnd_yaml_path_find(doc->yaml, "title", 5, &matches, &diags));
    NAOS_REQUIRE_UINT_EQ(1, matches.count);
    NAOS_REQUIRE_TRUE(matches.nodes[0]->kind == MND_YAML_SCALAR);

    memset(&seg, 0, sizeof(seg));
    seg.pub.origin = MND_TU_ORIGIN_FRONT_MATTER;
    seg.pub.span = matches.nodes[0]->span;
    seg.scalar = matches.nodes[0];
    NAOS_REQUIRE_TRUE(mnd_tu_splice_front_matter(doc, &seg, "Hello", 5, &diags, &replacement));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    splice_rebuild(doc, &seg, replacement.data, replacement.len, &rebuilt);
    TEST_CHECK_BYTES(rebuilt.data, rebuilt.len, "---\ntitle: Hello\n---\nBody\n", 26);

    mnd_buf_free(&rebuilt);
    mnd_buf_free(&replacement);
    mnd_yaml_matches_free(doc->yaml, &matches);
    mnd_diag_list_free(&diags);
    mnd_tu_doc_free(doc);
}

/* 先頭の空行の数も補う（2.7） */
NAOS_TEST(splice_front_matter_restores_leading_newlines)
{
    CHECK_FRONT_MATTER("---\ndesc: |\n\n  Hello\n---\n", "desc", "Bonjour",
                       "---\ndesc: |\n\n  Bonjour\n---\n");
}

NAOS_SUITE(splice,
    NAOS_ADD_TEST(splice_doc_points_to_source),
    NAOS_ADD_TEST(splice_block_quote_two_line_paragraph),
    NAOS_ADD_TEST(splice_list_item_extra_lines),
    NAOS_ADD_TEST(splice_lazy_continuation_line),
    NAOS_ADD_TEST(splice_tab_indentation),
    NAOS_ADD_TEST(splice_crlf_source),
    NAOS_ADD_TEST(splice_eol_none_uses_lf),
    NAOS_ADD_TEST(splice_mixed_line_endings),
    NAOS_ADD_TEST(splice_atx_heading_two_lines),
    NAOS_ADD_TEST(splice_fewer_lines_drops_rest),
    NAOS_ADD_TEST(splice_markdown_appends_to_out),
    NAOS_ADD_TEST(splice_msgid_reproduces_span),
    NAOS_ADD_TEST(splice_front_matter_literal_keeps_style),
    NAOS_ADD_TEST(splice_front_matter_empty_value),
    NAOS_ADD_TEST(splice_front_matter_restores_leading_newlines)
)
