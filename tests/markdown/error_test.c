/* 不正な UTF-8、メモリ不足、病的な入力（8.x） */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* メモリ不足のテストで、失敗させる位置を変える回数の上限（成功するまでの確保の回数より十分に大きい） */
#define OOM_MAX_STEPS 5000

/*
 * 病的な入力の繰り返しの回数。時間が入力の長さに比例することを手で確かめるときは、
 * ビルドのときに大きな値を定義して、テストの時間を比べる
 */
#ifndef MND_MD_PATHOLOGICAL_N
#define MND_MD_PATHOLOGICAL_N 10000
#endif

/* 閉じないバッククォートの並びの長さの最大値（1, 2, …, この値の並びを 1 つずつ置く） */
#ifndef MND_MD_PATHOLOGICAL_TICKS
#define MND_MD_PATHOLOGICAL_TICKS 1000
#endif

/*
 * 全種類のブロックとインラインを含む中程度の文書。
 * ブロック: ATX と Setext の見出し、段落、インデントとフェンスのコード、引用（遅延継続行を含む）、
 * 箇条書きと順序付きのリスト、HTML ブロック、リンク参照定義、テーマ区切り。
 * インライン: テキスト、ソフトとハードの改行、エスケープ、文字参照、コードスパン、強調と強い強調、
 * インライン形式と参照形式のリンク、画像、自動リンク、生の HTML
 */
static const char medium_doc[] =
    "# Title *emph*\n"
    "\n"
    "Setext **strong**\n"
    "=================\n"
    "\n"
    "para before code\n"
    "\n"
    "    indented code\n"
    "    more\n"
    "\n"
    "> quote with `code` and [link](/u \"t\") and ![img *a*](/i.png)\n"
    "> lazy\n"
    "continuation\n"
    "\n"
    "- item one\\\n"
    "  hard break\n"
    "- item two <span>html</span> &amp; &#35; \\* <https://example.com>\n"
    "\n"
    "1. ordered\n"
    "2. [ref] and [full][ref] and [ref][] and <a@b.c>\n"
    "\n"
    "```js\n"
    "fenced\n"
    "```\n"
    "\n"
    "<div>\n"
    "html block\n"
    "</div>\n"
    "\n"
    "[ref]: /url 'title'\n"
    "\n"
    "***\n"
    "last paragraph  \n"
    "with soft\n"
    "break\n";

/* 文書の中の kind のノードの数（前順にたどる） */
static size_t count_kind(const mnd_md_doc_t *doc, mnd_md_kind_t kind)
{
    const mnd_md_node_t *root = mnd_md_doc_root(doc);
    const mnd_md_node_t *node = NULL;
    size_t count = 0;

    for (node = root; node != NULL; node = mnd_md_node_next(node, root))
    {
        if (node->kind == kind)
        {
            count++;
        }
    }

    return count;
}

/* 8.2: 中程度の文書の解析で、成功するまで失敗させる確保の位置を 1 から順に変える */
NAOS_TEST(error_out_of_memory_at_every_allocation)
{
    size_t fail_at = 0;
    size_t failures = 0;
    bool succeeded = false;

    for (fail_at = 1; (fail_at <= OOM_MAX_STEPS) && !succeeded; fail_at++)
    {
        mnd_md_test_alloc_t ta;
        mnd_md_options_t options = MND_MD_OPTIONS_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        mnd_md_doc_t *doc = (mnd_md_doc_t *)&options;

        mnd_md_test_alloc_init(&ta, fail_at);
        options.allocator = &ta.allocator;
        options.path = "oom.md";
        if (mnd_md_parse(medium_doc, sizeof(medium_doc) - 1, &options, &doc, &diags))
        {
            int kind = 0;

            succeeded = true;
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            NAOS_REQUIRE_NOT_NULL(doc);
            NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));

            /* 文書が本当に全種類のノードを含むこと（メモリ不足の経路をすべての部品で通すため） */
            for (kind = (int)MND_MD_DOCUMENT; kind <= (int)MND_MD_HTML_INLINE; kind++)
            {
                if (count_kind(doc, (mnd_md_kind_t)kind) == 0)
                {
                    NAOS_NOTE("missing node kind %d", kind);
                    NAOS_CHECK_TRUE(false);
                }
            }

            mnd_md_doc_free(doc);
        }
        else
        {
            failures++;
            NAOS_CHECK_UINT_EQ(1, ta.failures);
            NAOS_CHECK_NULL(doc);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
            NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)mnd_diag_list_at(&diags, 0)->severity);
            NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_at(&diags, 0)->line);
        }

        /* 失敗しても成功しても、文書の解放のあとは確保と解放の回数が等しい（8.2、8.5） */
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        mnd_diag_list_free(&diags);
    }

    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(failures, 10);
}

/* 確保された len バイトのバッファ。病的な入力を組み立てる */
typedef struct input
{
    char *data;
    size_t len;
    size_t cap;
} input_t;

static bool input_init(input_t *in, size_t cap)
{
    in->data = (char *)malloc(cap);
    in->len = 0;
    in->cap = cap;

    return (in->data != NULL);
}

/* s を count 回追加する（容量を超える分は捨て、テストの失敗にする） */
static void input_repeat(input_t *in, const char *s, size_t count)
{
    size_t n = strlen(s);
    size_t i = 0;

    for (i = 0; i < count; i++)
    {
        if ((in->cap - in->len) < n)
        {
            NAOS_CHECK_TRUE(false);
            return;
        }

        memcpy(in->data + in->len, s, n);
        in->len += n;
    }
}

static void input_free(input_t *in)
{
    free(in->data);
    in->data = NULL;
}

/*
 * 8.3: in の解析が異常終了せずに成功し、診断を増やさず、tree_check を満たし、解放のあとに確保と解放の回数が
 * 等しいことを確かめる。kind のノードの数が expected_count であることも確かめる（expected_count が SIZE_MAX なら
 * 確かめない）
 */
static void check_pathological(const char *name, input_t *in, mnd_md_kind_t kind, size_t expected_count)
{
    mnd_md_test_alloc_t ta;
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    bool ok = false;

    NAOS_NOTE("input: %s (%zu bytes)", name, in->len);
    mnd_md_test_alloc_init(&ta, 0);
    options.allocator = &ta.allocator;
    ok = mnd_md_parse(in->data, in->len, &options, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    if (ok && (doc != NULL))
    {
        NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
        if (expected_count != SIZE_MAX)
        {
            NAOS_CHECK_UINT_EQ(expected_count, count_kind(doc, kind));
        }
        mnd_md_doc_free(doc);
    }
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    mnd_diag_list_free(&diags);
    input_free(in);
}

/* 8.3: 10,000 段の引用（`>>>…>a` と、次の行で全段を続ける `> > …` の 2 行） */
NAOS_TEST(error_pathological_deep_block_quotes)
{
    input_t in;

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, ">", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "a\n", 1);
    input_repeat(&in, "> ", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "b\n", 1);
    check_pathological("deep block quotes", &in, MND_MD_BLOCK_QUOTE, MND_MD_PATHOLOGICAL_N);
}

/* 8.3: 10,000 段のリスト（`- - … - a`、`1. 1. … a`）と、引用とリストを交互に入れ子にしたもの */
NAOS_TEST(error_pathological_deep_lists)
{
    input_t in;

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 2) + 16));
    input_repeat(&in, "- ", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "a\n", 1);
    check_pathological("deep bullet lists", &in, MND_MD_ITEM, MND_MD_PATHOLOGICAL_N);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, "1. ", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "a\n", 1);
    check_pathological("deep ordered lists", &in, MND_MD_LIST, MND_MD_PATHOLOGICAL_N);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 4) + 16));
    input_repeat(&in, "> - ", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "a\n", 1);
    check_pathological("alternating quotes and lists", &in, MND_MD_BLOCK_QUOTE, MND_MD_PATHOLOGICAL_N);
}

/* 8.3: 10,000 個の `[`、入れ子の角括弧、`[` と `](` の繰り返し、閉じないリンク */
NAOS_TEST(error_pathological_brackets)
{
    input_t in;

    NAOS_REQUIRE_TRUE(input_init(&in, MND_MD_PATHOLOGICAL_N + 16));
    input_repeat(&in, "[", MND_MD_PATHOLOGICAL_N);
    check_pathological("open brackets", &in, MND_MD_LINK, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 2) + 16));
    input_repeat(&in, "[", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "a", 1);
    input_repeat(&in, "]", MND_MD_PATHOLOGICAL_N);
    check_pathological("nested brackets", &in, MND_MD_LINK, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, "[", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "](", MND_MD_PATHOLOGICAL_N);
    check_pathological("brackets then open parens", &in, MND_MD_LINK, SIZE_MAX);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, "[](", MND_MD_PATHOLOGICAL_N);
    check_pathological("repeated [](", &in, MND_MD_LINK, SIZE_MAX);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 5) + 16));
    input_repeat(&in, "[a](b", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed links", &in, MND_MD_LINK, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 6) + 16));
    input_repeat(&in, "[a](<b", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed angle links", &in, MND_MD_LINK, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 7) + 16));
    input_repeat(&in, "[a](b \"", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed titles", &in, MND_MD_LINK, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 2) + 16));
    input_repeat(&in, "![", MND_MD_PATHOLOGICAL_N);
    check_pathological("open image brackets", &in, MND_MD_IMAGE, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 4) + 32));
    input_repeat(&in, "[a] ", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "\n\n[a]: /u\n", 1);
    check_pathological("many references", &in, MND_MD_LINK, MND_MD_PATHOLOGICAL_N);
}

/* 8.3: `*a` の 10,000 回の繰り返しと、閉じない強調の区切り記号 */
NAOS_TEST(error_pathological_emphasis)
{
    input_t in;

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 2) + 16));
    input_repeat(&in, "*a", MND_MD_PATHOLOGICAL_N);
    check_pathological("repeated *a", &in, MND_MD_EMPH, MND_MD_PATHOLOGICAL_N / 2);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, "*a ", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed *a", &in, MND_MD_EMPH, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, "_a ", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed _a", &in, MND_MD_EMPH, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 2) + 16));
    input_repeat(&in, "*", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "a", 1);
    input_repeat(&in, "*", MND_MD_PATHOLOGICAL_N);
    check_pathological("nested emphasis", &in, MND_MD_STRONG, SIZE_MAX);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 6) + 16));
    input_repeat(&in, "a**b_ ", MND_MD_PATHOLOGICAL_N);
    check_pathological("mixed unmatched delimiters", &in, MND_MD_STRONG, SIZE_MAX);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 4) + 16));
    input_repeat(&in, "**a*", MND_MD_PATHOLOGICAL_N);
    check_pathological("**a* repeated", &in, MND_MD_EMPH, SIZE_MAX);
}

/* 8.3: 閉じないバッククォートの並び（長さ 1, 2, … の並びを 1 つずつ、と `` `a `` の繰り返し） */
NAOS_TEST(error_pathological_backticks)
{
    input_t in;
    size_t i = 0;

    NAOS_REQUIRE_TRUE(input_init(&in, ((MND_MD_PATHOLOGICAL_TICKS * (MND_MD_PATHOLOGICAL_TICKS + 3)) / 2) + 16));
    for (i = 1; i <= MND_MD_PATHOLOGICAL_TICKS; i++)
    {
        input_repeat(&in, "a", 1);
        input_repeat(&in, "`", i);
    }
    check_pathological("unclosed backtick runs", &in, MND_MD_CODE_SPAN, 0);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 16));
    input_repeat(&in, "``a", MND_MD_PATHOLOGICAL_N);
    input_repeat(&in, "`", 1);
    check_pathological("repeated ``a", &in, MND_MD_CODE_SPAN, MND_MD_PATHOLOGICAL_N / 2);
}

/* 8.3: 閉じない生の HTML（コメント、処理命令、CDATA、宣言、引用符の属性値）と閉じない自動リンク */
NAOS_TEST(error_pathological_raw_html)
{
    static const char *const units[] = { "a <!-- ", "a <? ", "a <![CDATA[ ", "a <!X ", "a <b c=\" ", "a <b c=' ",
                                         "a <http://x ", "a <x@y " };
    input_t in;
    size_t i = 0;

    for (i = 0; i < (sizeof(units) / sizeof(units[0])); i++)
    {
        NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * strlen(units[i])) + 16));
        input_repeat(&in, units[i], MND_MD_PATHOLOGICAL_N);
        check_pathological(units[i], &in, MND_MD_HTML_INLINE, 0);
    }
}

/* 8.3: 多数のリンク参照定義、閉じない定義のタイトル、多数の行にまたがる閉じないラベル */
NAOS_TEST(error_pathological_definitions)
{
    input_t in;

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 9) + 16));
    input_repeat(&in, "[a]: /u\n", MND_MD_PATHOLOGICAL_N);
    check_pathological("many duplicate definitions", &in, MND_MD_LINK_REF_DEF, MND_MD_PATHOLOGICAL_N);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 3) + 32));
    input_repeat(&in, "[a]: /u\n\"\n", 1);
    input_repeat(&in, "x\n", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed definition title", &in, MND_MD_LINK_REF_DEF, 1);

    NAOS_REQUIRE_TRUE(input_init(&in, (MND_MD_PATHOLOGICAL_N * 2) + 16));
    input_repeat(&in, "[\n", 1);
    input_repeat(&in, "a\n", MND_MD_PATHOLOGICAL_N);
    check_pathological("unclosed label over lines", &in, MND_MD_LINK_REF_DEF, 0);
}

/* 不正な UTF-8 の解析が、*out を NULL にし、line の行番号で 1 つの診断を返し、何も残さないことを確かめる */
static void check_invalid_utf8(const char *data, size_t len, size_t body_start, size_t line)
{
    mnd_md_test_alloc_t ta;
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = (mnd_md_doc_t *)&options;
    const mnd_diag_t *diag = NULL;

    mnd_md_test_alloc_init(&ta, 0);
    options.allocator = &ta.allocator;
    options.body_start = body_start;
    options.path = "bad.md";
    NAOS_CHECK_FALSE(mnd_md_parse(data, len, &options, &doc, &diags));
    NAOS_CHECK_NULL(doc);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)diag->severity);
    NAOS_CHECK_STR_EQ("invalid UTF-8 sequence", diag->message);
    NAOS_CHECK_STR_EQ("bad.md", diag->path);
    NAOS_CHECK_UINT_EQ(line, diag->line);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    mnd_diag_list_free(&diags);
}

/* 8.1: 途中の行と開始位置より前の不正な UTF-8 は、その行番号で `invalid UTF-8 sequence` になる */
NAOS_TEST(error_invalid_utf8_line_and_message)
{
    static const char middle[] = "ok\nstill ok\nbad \x80 byte\nafter\n";
    static const char before_body[] = "---\nt: \xFF\n---\n# body\n";
    static const char overlong[] = "a\n\nb \xC0\xAF\n";
    static const char surrogate[] = "a\r\nb\r\n\xED\xA0\x80\n";
    static const char too_large[] = "# h\n\n> \xF4\x90\x80\x80\n";
    static const char truncated[] = "a\nb\nc \xE3\x81";
    static const char lone_cr[] = "a\rb\r\xC3\n";
    static const char in_code[] = "```\n\xF8\n```\n";

    check_invalid_utf8(middle, sizeof(middle) - 1, 0, 3);
    check_invalid_utf8(before_body, sizeof(before_body) - 1, 14, 2);
    check_invalid_utf8(overlong, sizeof(overlong) - 1, 0, 3);
    check_invalid_utf8(surrogate, sizeof(surrogate) - 1, 0, 3);
    check_invalid_utf8(too_large, sizeof(too_large) - 1, 0, 3);
    check_invalid_utf8(truncated, sizeof(truncated) - 1, 0, 3);
    /* 単独の CR は行番号を数えない（mnd_source_line_of と同じ） */
    check_invalid_utf8(lone_cr, sizeof(lone_cr) - 1, 0, 1);
    check_invalid_utf8(in_code, sizeof(in_code) - 1, 0, 2);
}

/* 8.4、8.5: 成功した解析は診断を増やさず、文書の解放のあとに確保と解放の回数が等しい */
NAOS_TEST(error_success_adds_no_diagnostics_and_frees_everything)
{
    mnd_md_test_alloc_t ta;
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;

    /* 呼び出し側の既存の診断はそのまま残り、増えない */
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, "prev.md", 7, "earlier diagnostic");
    mnd_md_test_alloc_init(&ta, 0);
    options.allocator = &ta.allocator;
    options.path = "ok.md";
    NAOS_REQUIRE_TRUE(mnd_md_parse(medium_doc, sizeof(medium_doc) - 1, &options, &doc, &diags));
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    NAOS_CHECK_STR_EQ("earlier diagnostic", mnd_diag_list_at(&diags, 0)->message);
    NAOS_CHECK_TRUE(ta.allocs > 0);
    NAOS_CHECK_TRUE(mnd_md_test_alloc_live(&ta) > 0);
    mnd_md_doc_free(doc);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    NAOS_CHECK_UINT_EQ(0, ta.failures);

    /* 開始位置を指定した NUL を含む文書でも同じ */
    {
        static const char data[] = "front\n---\na\0b *c*\n";

        mnd_md_test_alloc_init(&ta, 0);
        options.body_start = 10;
        NAOS_REQUIRE_TRUE(mnd_md_parse(data, sizeof(data) - 1, &options, &doc, &diags));
        NAOS_REQUIRE_NOT_NULL(doc);
        NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
        NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
        mnd_md_doc_free(doc);
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    }

    mnd_diag_list_free(&diags);
}

NAOS_SUITE(error,
    NAOS_ADD_TEST(error_out_of_memory_at_every_allocation),
    NAOS_ADD_TEST(error_pathological_deep_block_quotes),
    NAOS_ADD_TEST(error_pathological_deep_lists),
    NAOS_ADD_TEST(error_pathological_brackets),
    NAOS_ADD_TEST(error_pathological_emphasis),
    NAOS_ADD_TEST(error_pathological_backticks),
    NAOS_ADD_TEST(error_pathological_raw_html),
    NAOS_ADD_TEST(error_pathological_definitions),
    NAOS_ADD_TEST(error_invalid_utf8_line_and_message),
    NAOS_ADD_TEST(error_success_adds_no_diagnostics_and_frees_everything)
)
