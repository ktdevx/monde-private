/* スイート error: メモリ不足、病的な入力、診断の扱い（6.1、6.6〜6.10） */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/source.h"
#include "test_alloc.h"
#include "tree_check.h"
#include "yaml/yaml.h"
#include "yaml/yaml_format.h"
#include "yaml/yaml_front_matter.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_parse.h"
#include "yaml/yaml_path.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 全種類のノードの文書の走査に使うスタックの大きさ（文書の入れ子は浅い） */
#define ERROR_STACK_MAX 128

/* 病的な入力の入れ子の深さ */
#define ERROR_DEEP 10000

/*
 * 全種類のノード（プレーン・シングルクオート・ダブルクオート・リテラル・折り返しのスカラー、
 * ブロックのマッピングとシーケンス、フロー形式のシーケンスとマッピング、空の値、コメント、空行）を含む
 * フロントマター付きの文書（LF）
 */
static const char all_kinds_lf[] =
    "---\n"
    "# comment\n"
    "title: Hello world\n"
    "single: 'it''s'\n"
    "double: \"a\\tb \\u00e9\"\n"
    "literal: |\n"
    "  line one\n"
    "  line two\n"
    "\n"
    "folded: >-\n"
    "  folded one\n"
    "  folded two\n"
    "seo:\n"
    "  title: Nested  # trailing comment\n"
    "  empty:\n"
    "tags:\n"
    "  - a\n"
    "  - 'b'\n"
    "  - name: c\n"
    "    note: \"d\"\n"
    "  - - e\n"
    "flow_seq: [x, 'y', \"z\", [n, m]]\n"
    "flow_map: {p: q, r: [s, t], u}\n"
    "empty:\n"
    "---\n"
    "body\n";

/* 同じ文書の CRLF 版 */
static const char all_kinds_crlf[] =
    "---\r\n"
    "# comment\r\n"
    "title: Hello world\r\n"
    "single: 'it''s'\r\n"
    "double: \"a\\tb \\u00e9\"\r\n"
    "literal: |\r\n"
    "  line one\r\n"
    "  line two\r\n"
    "\r\n"
    "folded: >-\r\n"
    "  folded one\r\n"
    "  folded two\r\n"
    "seo:\r\n"
    "  title: Nested  # trailing comment\r\n"
    "  empty:\r\n"
    "tags:\r\n"
    "  - a\r\n"
    "  - 'b'\r\n"
    "  - name: c\r\n"
    "    note: \"d\"\r\n"
    "  - - e\r\n"
    "flow_seq: [x, 'y', \"z\", [n, m]]\r\n"
    "flow_map: {p: q, r: [s, t], u}\r\n"
    "empty:\r\n"
    "---\r\n"
    "body\r\n";

/* 照合するキーパスと、それぞれに当たるノードの数 */
static const char *const all_kinds_paths[] = {
    "title", "single", "double", "literal", "folded", "seo", "seo.title", "seo.empty",
    "tags", "tags.name", "tags.note", "flow_seq", "flow_map.r", "flow_map.u", "empty", "missing"
};
static const size_t all_kinds_path_counts[] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    4, 1, 1, 5, 2, 1, 1, 0
};

/* 整形する訳文 */
static const char *const all_kinds_texts[] = {
    "plain text", "a: b # c", "line1\nline2\n", "", " lead and trail ", "true", "日本語"
};

/*
 * diags の base 番目より後ろに診断が 1 つ（エラー、message、path、line）だけ追加されていることを確かめる。
 * path が NULL なら診断のパスも NULL
 */
static void check_single_error(const mnd_diag_list_t *diags, size_t base, const char *message, const char *path,
                               size_t line)
{
    const mnd_diag_t *d = NULL;

    NAOS_CHECK_UINT_EQ(base + 1, mnd_diag_list_count(diags));
    if (mnd_diag_list_count(diags) != (base + 1))
    {
        return;
    }
    d = mnd_diag_list_at(diags, base);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
    NAOS_CHECK_STR_EQ(message, d->message);
    NAOS_CHECK_UINT_EQ(line, d->line);
    if (path == NULL)
    {
        NAOS_CHECK_NULL(d->path);
    }
    else
    {
        NAOS_CHECK_STR_EQ(path, d->path);
    }
}

/*
 * 文書のすべてのスカラー（キーを含む）を原文の順に集める（再帰を使わない）。集めた数を返す
 */
static size_t collect_scalars(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t **out, size_t max)
{
    const mnd_yaml_node_t *stack[ERROR_STACK_MAX];
    const mnd_yaml_node_t *node = NULL;
    size_t depth = 0;
    size_t count = 0;
    size_t i;

    if (mnd_yaml_doc_root(doc) == NULL)
    {
        return 0;
    }
    stack[depth++] = mnd_yaml_doc_root(doc);
    while (depth > 0)
    {
        node = stack[--depth];
        if (node->kind == MND_YAML_SCALAR)
        {
            if (count < max)
            {
                out[count++] = node;
            }
        }
        else if (node->kind == MND_YAML_MAPPING)
        {
            for (i = node->as.mapping.count; (i > 0) && ((depth + 2) <= ERROR_STACK_MAX); --i)
            {
                stack[depth++] = node->as.mapping.pairs[i - 1].value;
                stack[depth++] = node->as.mapping.pairs[i - 1].key;
            }
        }
        else
        {
            for (i = node->as.sequence.count; (i > 0) && (depth < ERROR_STACK_MAX); --i)
            {
                stack[depth++] = node->as.sequence.items[i - 1];
            }
        }
    }

    return count;
}

/*
 * 失敗させる確保の位置を 1 から順に変えて、検出・解析・照合・整形を続けて実行する（6.7、6.10）。
 * 失敗した確保があれば、その処理が false を返し、文書（または照合の結果）を返さず、out of memory（行 0）を
 * 1 つだけ追加し、片付けたあとに確保と解放の回数が等しいこと。すべて成功するまで続け、成功したときは
 * 診断が増えないこと。整形は確保関数を使わないので、整形の前後で確保の回数が変わらないこと
 */
static void run_out_of_memory_loop(const char *text, size_t len)
{
    const mnd_yaml_node_t *scalars[ERROR_STACK_MAX];
    mnd_yaml_front_matter_t fm;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    size_t n;
    size_t i;
    size_t k;
    size_t scalar_count;
    size_t parse_failures = 0;
    size_t path_failures = 0;
    size_t calls_before;
    bool succeeded = false;

    NAOS_REQUIRE_TRUE(mnd_yaml_front_matter_detect(text, len, "a.md", &fm, &diags));
    NAOS_REQUIRE_TRUE(fm.present);
    NAOS_REQUIRE_UINT_EQ(0, mnd_diag_list_count(&diags));

    for (n = 1; (n < 100000) && !succeeded; ++n)
    {
        mnd_yaml_test_alloc_t ta;
        mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
        mnd_yaml_doc_t *doc = (mnd_yaml_doc_t *)&diags;
        bool ok;
        bool path_failed = false;

        mnd_yaml_test_alloc_init(&ta, n);
        options.path = "a.md";
        options.allocator = &ta.allocator;
        ok = mnd_yaml_parse(text, len, fm.content, &options, &doc, &diags);
        if (!ok)
        {
            ++parse_failures;
            NAOS_CHECK_UINT_EQ(1, ta.failures);
            NAOS_CHECK_TRUE(doc == NULL);
            check_single_error(&diags, 0, "out of memory", "a.md", 0);
            NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
            mnd_diag_list_free(&diags);
            continue;
        }
        NAOS_CHECK_UINT_EQ(0, ta.failures);
        NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
        NAOS_REQUIRE_TRUE(doc != NULL);

        /* 照合: 失敗させる位置が解析の確保を過ぎると、ここで失敗する */
        for (i = 0; (i < (sizeof(all_kinds_paths) / sizeof(all_kinds_paths[0]))) && !path_failed; ++i)
        {
            mnd_yaml_matches_t matches;
            size_t failures_before = ta.failures;

            ok = mnd_yaml_path_find(doc, all_kinds_paths[i], strlen(all_kinds_paths[i]), &matches, &diags);
            if (ta.failures != failures_before)
            {
                ++path_failures;
                path_failed = true;
                NAOS_CHECK_FALSE(ok);
                NAOS_CHECK_TRUE(matches.nodes == NULL);
                NAOS_CHECK_UINT_EQ(0, matches.count);
                check_single_error(&diags, 0, "out of memory", NULL, 0);
            }
            else
            {
                NAOS_CHECK_TRUE(ok);
                NAOS_CHECK_UINT_EQ(all_kinds_path_counts[i], matches.count);
                NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            }
            mnd_yaml_matches_free(doc, &matches);
        }

        /* 整形: すべてのスカラーにすべての訳文を整形する */
        calls_before = ta.calls;
        scalar_count = collect_scalars(doc, scalars, ERROR_STACK_MAX);
        NAOS_CHECK_TRUE(scalar_count > 30);
        for (i = 0; i < scalar_count; ++i)
        {
            for (k = 0; k < (sizeof(all_kinds_texts) / sizeof(all_kinds_texts[0])); ++k)
            {
                mnd_buf_t out = MND_BUF_INIT;
                size_t before = mnd_diag_list_count(&diags);

                NAOS_CHECK_TRUE(mnd_yaml_format_value(doc, scalars[i], all_kinds_texts[k],
                                                      strlen(all_kinds_texts[k]), "a.md", &out, &diags));
                NAOS_CHECK_UINT_EQ(before, mnd_diag_list_count(&diags));
                mnd_buf_free(&out);
            }
        }
        NAOS_CHECK_UINT_EQ(calls_before, ta.calls);

        if (!path_failed)
        {
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
            succeeded = true;
        }
        mnd_yaml_doc_free(doc);
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        mnd_diag_list_free(&diags);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_TRUE(parse_failures > 10);
    NAOS_CHECK_TRUE(path_failures > 20);
}

/* 雛形: メモリ不足は行番号 0 の "out of memory" のエラーとして報告される */
NAOS_TEST(error_scaffold_parser_oom_message)
{
    mnd_diag_list_t diags;
    mnd_yaml_parser_t p;
    const mnd_diag_t *d = NULL;

    memset(&p, 0, sizeof(p));
    mnd_diag_list_init(&diags);
    p.diags = &diags;
    mnd_yaml_parser_oom(&p);
    NAOS_CHECK_TRUE(p.failed);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 0);
    NAOS_REQUIRE_TRUE(d != NULL);
    NAOS_CHECK_STR_EQ("out of memory", d->message);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));
    mnd_diag_list_free(&diags);
}

/*
 * メモリ不足: 全種類のノードを含む LF の文書で、失敗させる確保の位置を 1 から順に変えて、解析・照合・整形する（6.7、6.10）
 */
NAOS_TEST(error_out_of_memory_all_node_kinds_lf)
{
    run_out_of_memory_loop(all_kinds_lf, sizeof(all_kinds_lf) - 1);
}

/*
 * メモリ不足: 同じ文書の CRLF 版（6.7、6.10）
 */
NAOS_TEST(error_out_of_memory_all_node_kinds_crlf)
{
    run_out_of_memory_loop(all_kinds_crlf, sizeof(all_kinds_crlf) - 1);
}

/*
 * 整形の出力先が伸ばせないとき（整形は確保関数を受け取らず、出力先の mnd_buf_t の確保が失敗したとき）、
 * 整形は false を返し、行 0 の out of memory を 1 つだけ追加する（6.7）。
 * 長さが SIZE_MAX - 1 の出力先は、1 バイトの追加でも容量の計算があふれるため、領域に触れずに失敗する
 */
NAOS_TEST(error_format_output_cannot_grow_reports_out_of_memory)
{
    static char storage[1];
    const mnd_yaml_node_t *scalars[ERROR_STACK_MAX];
    const size_t len = sizeof(all_kinds_lf) - 1;
    mnd_yaml_front_matter_t fm;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;
    size_t count;
    size_t i;

    NAOS_REQUIRE_TRUE(mnd_yaml_front_matter_detect(all_kinds_lf, len, NULL, &fm, &diags));
    NAOS_REQUIRE_TRUE(mnd_yaml_parse(all_kinds_lf, len, fm.content, NULL, &doc, &diags));
    count = collect_scalars(doc, scalars, ERROR_STACK_MAX);
    NAOS_CHECK_TRUE(count > 30);
    for (i = 0; i < count; ++i)
    {
        mnd_buf_t out;

        out.data = storage;
        out.len = SIZE_MAX - 1;
        out.cap = SIZE_MAX;
        NAOS_CHECK_FALSE(mnd_yaml_format_value(doc, scalars[i], "text: value", 11, "a.md", &out, &diags));
        check_single_error(&diags, 0, "out of memory", "a.md", 0);
        NAOS_CHECK_UINT_EQ(SIZE_MAX - 1, out.len);
        NAOS_CHECK_TRUE(out.data == storage);
        mnd_diag_list_free(&diags);
    }
    mnd_yaml_doc_free(doc);
}

/*
 * 病的な入力: 10,000 段の [。入口から解析し、閉じた入力は成功し、閉じていない入力と最も深い位置の誤りは
 * エラーを 1 つ返し、どちらも確保と解放の回数が等しい（6.9、6.10）
 */
NAOS_TEST(error_deep_flow_brackets)
{
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_matches_t matches;
    mnd_span_t content;
    char *data = NULL;
    size_t len = 0;
    size_t i;

    data = (char *)malloc((ERROR_DEEP * 2) + 16);
    NAOS_REQUIRE_TRUE(data != NULL);
    options.allocator = &ta.allocator;

    /* "k: " + "[" * depth + "x" + "]" * depth */
    memcpy(data, "k: ", 3);
    len = 3;
    for (i = 0; i < ERROR_DEEP; ++i)
    {
        data[len++] = '[';
    }
    data[len++] = 'x';
    for (i = 0; i < ERROR_DEEP; ++i)
    {
        data[len++] = ']';
    }
    data[len++] = '\n';
    content.start = 0;
    content.end = len;
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_TRUE(mnd_yaml_parse(data, len, content, &options, &doc, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    if (doc != NULL)
    {
        NAOS_CHECK_TRUE(mnd_yaml_path_find(doc, "k", 1, &matches, &diags));
        NAOS_CHECK_UINT_EQ(1, matches.count);
        NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
        mnd_yaml_matches_free(doc, &matches);
    }
    mnd_yaml_doc_free(doc);
    doc = NULL;
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);

    /* 最も深い位置のスカラーを @ で始める */
    data[3 + ERROR_DEEP] = '@';
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_FALSE(mnd_yaml_parse(data, len, content, &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, 0, "reserved indicator \"@\" cannot start a scalar", NULL, 1);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    mnd_diag_list_free(&diags);

    /* 閉じていない（最も内側の開き括弧の行で報告する） */
    content.end = 3 + ERROR_DEEP;
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_FALSE(mnd_yaml_parse(data, len, content, &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, 0, "unterminated flow collection", NULL, 1);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    mnd_diag_list_free(&diags);
    free(data);
}

/*
 * 病的な入力: 10,000 段のインデントのマッピング（i 段目は i 個の空白と a:、約 50MB）。
 * 成功する場合は block スイートが確かめるので、ここでは最も深い位置に誤りを置いた 1 回だけ解析し、
 * 深い入れ子を開いたままのエラーでも異常終了せず、エラーを 1 つ返し、確保と解放の回数が等しいことを確かめる（6.9）
 */
NAOS_TEST(error_deep_indented_mapping)
{
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;
    char *data = NULL;
    size_t len = 0;
    size_t pos = 0;
    size_t i;

    for (i = 0; i < ERROR_DEEP; ++i)
    {
        len += i + 3;
    }
    len += ERROR_DEEP + 2;
    data = (char *)malloc(len);
    NAOS_REQUIRE_TRUE(data != NULL);
    for (i = 0; i <= ERROR_DEEP; ++i)
    {
        memset(data + pos, ' ', i);
        pos += i;
        if (i < ERROR_DEEP)
        {
            data[pos++] = 'a';
            data[pos++] = ':';
        }
        else
        {
            data[pos++] = '@';
        }
        data[pos++] = '\n';
    }
    NAOS_REQUIRE_UINT_EQ(len, pos);
    content.start = 0;
    content.end = len;
    options.allocator = &ta.allocator;
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_FALSE(mnd_yaml_parse(data, len, content, &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, 0, "reserved indicator \"@\" cannot start a scalar", NULL, ERROR_DEEP + 1);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    mnd_diag_list_free(&diags);
    free(data);
}

/*
 * 病的な入力: 1 行の 10,000 段の -。成功し、最も深いスカラーを整形でき、最も深い位置の誤りはエラーを 1 つ返し、
 * どちらも確保と解放の回数が等しい（6.9、6.10）
 */
NAOS_TEST(error_deep_block_sequence)
{
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;
    const mnd_yaml_node_t *node = NULL;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_span_t content;
    char *data = NULL;
    size_t levels = 0;
    size_t i;

    data = (char *)malloc((ERROR_DEEP * 2) + 2);
    NAOS_REQUIRE_TRUE(data != NULL);
    for (i = 0; i < ERROR_DEEP; ++i)
    {
        data[(i * 2)] = '-';
        data[(i * 2) + 1] = ' ';
    }
    data[ERROR_DEEP * 2] = 'x';
    data[(ERROR_DEEP * 2) + 1] = '\n';
    content.start = 0;
    content.end = (ERROR_DEEP * 2) + 2;
    options.allocator = &ta.allocator;
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_TRUE(mnd_yaml_parse(data, content.end, content, &options, &doc, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    node = mnd_yaml_doc_root(doc);
    while ((node != NULL) && (node->kind == MND_YAML_SEQUENCE) && (node->as.sequence.count == 1))
    {
        ++levels;
        node = node->as.sequence.items[0];
    }
    NAOS_CHECK_UINT_EQ(ERROR_DEEP, levels);
    NAOS_CHECK_TRUE((node != NULL) && (node->kind == MND_YAML_SCALAR));
    if ((node != NULL) && (node->kind == MND_YAML_SCALAR))
    {
        NAOS_CHECK_TRUE(mnd_yaml_format_value(doc, node, "a: b", 4, NULL, &out, &diags));
        NAOS_CHECK_STR_EQ("\"a: b\"", mnd_buf_cstr(&out));
        NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    }
    mnd_buf_free(&out);
    mnd_yaml_doc_free(doc);
    doc = NULL;
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);

    /* 最も深い位置を閉じていない引用符にする */
    data[ERROR_DEEP * 2] = '"';
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_FALSE(mnd_yaml_parse(data, content.end, content, &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, 0, "unterminated quoted scalar", NULL, 1);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    mnd_diag_list_free(&diags);
    free(data);
}

/*
 * 解析のエラーの表: 入力、設計の「メッセージ」の文面、行番号。すべての入力は "ok: 1\n" などの前置きを持ち、
 * 行番号が 1 でない位置でも正しく報告されることを確かめる
 */
typedef struct error_parse_case
{
    const char *text;
    const char *message;
    size_t line;
} error_parse_case_t;

static const error_parse_case_t error_parse_cases[] = {
    { "ok: 1\na: \xff\n", "invalid UTF-8 sequence", 2 },
    { "ok: 1\na: &x b\n", "unsupported YAML syntax: anchor", 2 },
    { "ok: 1\na: *x\n", "unsupported YAML syntax: alias", 2 },
    { "ok: 1\na: !!str b\n", "unsupported YAML syntax: tag", 2 },
    { "%YAML 1.2\na: 1\n", "unsupported YAML syntax: directive", 1 },
    { "ok: 1\n? k\n: v\n", "unsupported YAML syntax: complex mapping key", 2 },
    { "ok: 1\na:\n\tb: c\n", "tab character in indentation", 3 },
    { "a: 1\nb: 2\na: 3\n", "duplicate mapping key \"a\"", 3 },
    { "a: {k: 1,\n  k: 2}\n", "duplicate mapping key \"k\"", 2 },
    { "ok: 1\na: \"x\\qy\"\n", "invalid escape sequence in double-quoted scalar", 2 },
    { "ok: 1\na: 'x\n\nb: 2\n", "unterminated quoted scalar", 2 },
    { "ok: 1\na: [x,\n  y\n", "unterminated flow collection", 2 },
    { "ok: 1\na:\n    b: 1\n  c: 2\n", "inconsistent indentation", 4 },
    { "ok: 1\n  - x\n", "inconsistent indentation", 2 },
    { "ok: 1\nfoo\n", "expected a mapping entry or a sequence item", 2 },
    { "ok: 1\na: 'x' y\n", "unexpected content after value", 2 },
    { "ok: 1\na: [x, , y]\n", "invalid flow collection entry", 2 },
    { "ok: 1\na: [|]\n", "invalid flow collection entry", 2 },
    { "ok: 1\na: @x\n", "reserved indicator \"@\" cannot start a scalar", 2 },
    { "ok: 1\na: `x\n", "reserved indicator \"`\" cannot start a scalar", 2 },
    { "ok: 1\na: 1\rb: 2\n", "carriage return without line feed", 2 },
    { "ok: 1\na: |0\n  x\n", "invalid block scalar", 2 },
    { "ok: 1\na: |x\n  x\n", "invalid block scalar", 2 }
};

/*
 * 解析の各エラーが、設計どおりの英語の文面、行番号、パスを持つ診断を 1 つだけ追加し、文書を返さず、
 * それまでの診断を変えず、確保と解放の回数が等しい（6.1〜6.6、6.8、6.10）
 */
NAOS_TEST(error_parse_messages_are_exact)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    size_t i;

    /* 既存の診断の後ろに追加されることを確かめるため、警告を 1 つ入れておく */
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, NULL, 0, "%s", "existing");
    for (i = 0; i < (sizeof(error_parse_cases) / sizeof(error_parse_cases[0])); ++i)
    {
        const error_parse_case_t *c = &error_parse_cases[i];
        mnd_yaml_test_alloc_t ta;
        mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
        mnd_yaml_doc_t *doc = (mnd_yaml_doc_t *)&diags;
        mnd_span_t content;

        NAOS_NOTE("case %u: %s", (unsigned)i, c->message);
        content.start = 0;
        content.end = strlen(c->text);
        mnd_yaml_test_alloc_init(&ta, 0);
        options.path = "a.md";
        options.allocator = &ta.allocator;
        NAOS_CHECK_FALSE(mnd_yaml_parse(c->text, content.end, content, &options, &doc, &diags));
        NAOS_CHECK_TRUE(doc == NULL);
        check_single_error(&diags, 1, c->message, "a.md", c->line);
        NAOS_CHECK_STR_EQ("existing", mnd_diag_list_at(&diags, 0)->message);
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        mnd_diag_list_free(&diags);
        mnd_diag_add(&diags, MND_SEVERITY_WARNING, NULL, 0, "%s", "existing");
    }
    mnd_diag_list_free(&diags);
}

/*
 * 検出・解析・照合・整形の残りのエラーの文面: 検出の不正な UTF-8、フロントマターの中の誤りの行番号が文書全体の
 * 行番号であること、範囲の誤り、不正なキーパス、整形の NUL と不正な UTF-8（6.6、6.8、4.7）
 */
NAOS_TEST(error_other_messages_are_exact)
{
    static const char fm_text[] = "---\ntitle: x\ntitle: y\n---\nbody\n";
    static const char bad_fm[] = "---\nt: x\n\xc3\x28\n---\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_matches_t matches;
    const mnd_yaml_node_t *root = NULL;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_span_t content;

    /* 検出の不正な UTF-8 は、不正なバイトの行 */
    NAOS_CHECK_FALSE(mnd_yaml_front_matter_detect(bad_fm, sizeof(bad_fm) - 1, "a.md", &fm, &diags));
    NAOS_CHECK_FALSE(fm.present);
    check_single_error(&diags, 0, "invalid UTF-8 sequence", "a.md", 3);
    mnd_diag_list_free(&diags);

    /* フロントマターの内容の中の誤りは、文書全体の行番号で報告する */
    NAOS_REQUIRE_TRUE(mnd_yaml_front_matter_detect(fm_text, sizeof(fm_text) - 1, NULL, &fm, &diags));
    NAOS_CHECK_FALSE(mnd_yaml_parse(fm_text, sizeof(fm_text) - 1, fm.content, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, 0, "duplicate mapping key \"title\"", NULL, 3);
    mnd_diag_list_free(&diags);

    /* 範囲の誤り（行 0） */
    content.start = 0;
    content.end = sizeof(fm_text);
    doc = (mnd_yaml_doc_t *)&diags;
    NAOS_CHECK_FALSE(mnd_yaml_parse(fm_text, sizeof(fm_text) - 1, content, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    check_single_error(&diags, 0, "content span out of range", NULL, 0);
    mnd_diag_list_free(&diags);

    /* 照合の不正なキーパス（行 0、パスなし） */
    content.start = 0;
    content.end = 7;
    NAOS_REQUIRE_TRUE(mnd_yaml_parse("a: b\n\n\n", 7, content, NULL, &doc, &diags));
    NAOS_CHECK_FALSE(mnd_yaml_path_find(doc, "a..b", 4, &matches, &diags));
    NAOS_CHECK_TRUE(matches.nodes == NULL);
    NAOS_CHECK_UINT_EQ(0, matches.count);
    check_single_error(&diags, 0, "invalid key path \"a..b\"", NULL, 0);
    mnd_diag_list_free(&diags);
    NAOS_CHECK_FALSE(mnd_yaml_path_find(doc, "", 0, &matches, &diags));
    check_single_error(&diags, 0, "invalid key path \"\"", NULL, 0);
    mnd_diag_list_free(&diags);

    /* 整形の NUL と不正な UTF-8（行 0、渡したパス） */
    root = mnd_yaml_doc_root(doc);
    NAOS_REQUIRE_TRUE((root != NULL) && (root->kind == MND_YAML_MAPPING) && (root->as.mapping.count == 1));
    NAOS_CHECK_FALSE(mnd_yaml_format_value(doc, root->as.mapping.pairs[0].value, "a\0b", 3, "a.md", &out, &diags));
    check_single_error(&diags, 0, "NUL in value", "a.md", 0);
    mnd_diag_list_free(&diags);
    NAOS_CHECK_FALSE(mnd_yaml_format_value(doc, root->as.mapping.pairs[0].value, "a\xff", 2, NULL, &out, &diags));
    check_single_error(&diags, 0, "invalid UTF-8 sequence", NULL, 0);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&out);
    mnd_yaml_doc_free(doc);
}

/*
 * 成功した検出・解析・照合・整形は、既存の診断を変えず、何も追加しない（6.8）
 */
NAOS_TEST(error_success_adds_no_diagnostics)
{
    const size_t len = sizeof(all_kinds_crlf) - 1;
    const mnd_yaml_node_t *scalars[ERROR_STACK_MAX];
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_matches_t matches;
    size_t count;
    size_t i;

    mnd_diag_add(&diags, MND_SEVERITY_WARNING, "x.md", 7, "%s", "existing");
    NAOS_CHECK_TRUE(mnd_yaml_front_matter_detect(all_kinds_crlf, len, "a.md", &fm, &diags));
    NAOS_CHECK_TRUE(mnd_yaml_parse(all_kinds_crlf, len, fm.content, NULL, &doc, &diags));
    NAOS_REQUIRE_TRUE(doc != NULL);
    for (i = 0; i < (sizeof(all_kinds_paths) / sizeof(all_kinds_paths[0])); ++i)
    {
        NAOS_CHECK_TRUE(mnd_yaml_path_find(doc, all_kinds_paths[i], strlen(all_kinds_paths[i]), &matches, &diags));
        mnd_yaml_matches_free(doc, &matches);
    }
    count = collect_scalars(doc, scalars, ERROR_STACK_MAX);
    for (i = 0; i < count; ++i)
    {
        mnd_buf_t out = MND_BUF_INIT;

        NAOS_CHECK_TRUE(mnd_yaml_format_value(doc, scalars[i], "x\ny", 3, "a.md", &out, &diags));
        mnd_buf_free(&out);
    }
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_WARNING, (int)mnd_diag_list_at(&diags, 0)->severity);
    NAOS_CHECK_STR_EQ("existing", mnd_diag_list_at(&diags, 0)->message);
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&diags));
    mnd_yaml_doc_free(doc);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(error,
    NAOS_ADD_TEST(error_scaffold_parser_oom_message),
    NAOS_ADD_TEST(error_out_of_memory_all_node_kinds_lf),
    NAOS_ADD_TEST(error_out_of_memory_all_node_kinds_crlf),
    NAOS_ADD_TEST(error_format_output_cannot_grow_reports_out_of_memory),
    NAOS_ADD_TEST(error_deep_flow_brackets),
    NAOS_ADD_TEST(error_deep_indented_mapping),
    NAOS_ADD_TEST(error_deep_block_sequence),
    NAOS_ADD_TEST(error_parse_messages_are_exact),
    NAOS_ADD_TEST(error_other_messages_are_exact),
    NAOS_ADD_TEST(error_success_adds_no_diagnostics)
)
