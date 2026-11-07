/* スイート flow: フロー形式のコレクション（2.3、6.2、6.4、6.9） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "parser_fixture.h"
#include "test_alloc.h"
#include "tree_check.h"
#include "yaml/yaml_flow.h"
#include "yaml/yaml_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * 1 回の解析の状態。fx は parser.diags が fx.diags を指すため、初期化したあとは移動しない
 */
typedef struct flow_run
{
    mnd_yaml_fixture_t fx;
    mnd_yaml_cursor_t at;
    mnd_yaml_node_t *node;
    bool ok;
} flow_run_t;

/*
 * data[0..len) 全体を content とする作業状態を作り、offset からフロー形式のコレクションを読む。
 * 作業状態を作れなければ false（r->ok も false）
 */
static bool flow_run_len(flow_run_t *r, const char *data, size_t len, size_t offset,
                         const mnd_yaml_allocator_t *allocator)
{
    mnd_span_t content;

    content.start = 0;
    content.end = len;
    r->node = NULL;
    r->ok = false;
    if (!mnd_yaml_fixture_init(&r->fx, data, len, content, allocator))
    {
        return false;
    }
    r->at = mnd_yaml_fixture_cursor(&r->fx, offset);
    r->ok = mnd_yaml_flow_parse(&r->fx.parser, &r->at, &r->node);

    return true;
}

static bool flow_run(flow_run_t *r, const char *text, size_t offset)
{
    return flow_run_len(r, text, strlen(text), offset, NULL);
}

/*
 * 成功した解析の共通の確認: 診断がない、カーソルが範囲の終わり（閉じ括弧の直後）にある、
 * 根として木の検査を満たす
 */
static void check_success(flow_run_t *r, const char *raw_start, const char *raw)
{
    size_t raw_len = strlen(raw);

    NAOS_CHECK_TRUE(r->ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&r->fx.diags));
    NAOS_REQUIRE_TRUE(r->ok && (r->node != NULL));
    NAOS_CHECK_TRUE(r->node->parent == NULL);
    NAOS_CHECK_MEM_EQ(raw, raw_len, raw_start + r->node->span.start, r->node->span.end - r->node->span.start);
    NAOS_CHECK_UINT_EQ(r->node->span.end, r->at.offset);
    NAOS_CHECK_UINT_EQ(mnd_yaml_lines_index_of(&r->fx.parser.lines, r->node->span.end), r->at.line_index);
    mnd_yaml_doc_set_root(r->fx.parser.doc, r->node);
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(r->fx.parser.doc));
}

/* node がスタイル style、論理値 value、原文 raw、親 parent のスカラーで、flow の印を持つことを確かめる */
static void check_scalar(const char *text, const mnd_yaml_node_t *node, const mnd_yaml_node_t *parent,
                         mnd_yaml_style_t style, const char *value, const char *raw)
{
    NAOS_REQUIRE_TRUE(node != NULL);
    NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)node->kind);
    if (node->kind != MND_YAML_SCALAR)
    {
        return;
    }
    NAOS_CHECK_TRUE(node->parent == parent);
    NAOS_CHECK_INT_EQ((int)style, (int)node->as.scalar.style);
    NAOS_CHECK_TRUE(node->as.scalar.flow);
    NAOS_CHECK_MEM_EQ(value, strlen(value), node->as.scalar.value, node->as.scalar.value_len);
    NAOS_CHECK_INT_EQ(0, (int)node->as.scalar.value[node->as.scalar.value_len]);
    NAOS_CHECK_MEM_EQ(raw, strlen(raw), text + node->span.start, node->span.end - node->span.start);
}

/*
 * text の offset から読むと、message のエラーの診断が line の行で 1 つだけ追加され、
 * false が返り、*out と *at が変わらないことを確かめる
 */
static void check_error_len(const char *data, size_t len, size_t offset, const char *message, size_t line)
{
    flow_run_t r;
    const mnd_diag_t *d = NULL;
    mnd_yaml_cursor_t before;

    NAOS_REQUIRE_TRUE(flow_run_len(&r, data, len, offset, NULL));
    before = mnd_yaml_fixture_cursor(&r.fx, offset);
    NAOS_CHECK_FALSE(r.ok);
    NAOS_CHECK_TRUE(r.node == NULL);
    NAOS_CHECK_UINT_EQ(before.offset, r.at.offset);
    NAOS_CHECK_UINT_EQ(before.line_index, r.at.line_index);
    NAOS_CHECK_TRUE(r.fx.parser.failed);
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&r.fx.diags));
    if (mnd_diag_list_count(&r.fx.diags) == 1)
    {
        d = mnd_diag_list_at(&r.fx.diags, 0);
        NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
        NAOS_CHECK_STR_EQ(message, d->message);
        NAOS_CHECK_UINT_EQ(line, d->line);
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

static void check_error(const char *text, size_t offset, const char *message, size_t line)
{
    check_error_len(text, strlen(text), offset, message, line);
}

/* シーケンスのプレーン・シングルクオート・ダブルクオートの項目（2.3） */
NAOS_TEST(flow_sequence_of_scalars)
{
    const char *text = "[a, 'b', \"c\"]";
    flow_run_t r;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 0));
    check_success(&r, text, text);
    seq = r.node;
    if (r.ok && (seq != NULL))
    {
        NAOS_CHECK_INT_EQ((int)MND_YAML_SEQUENCE, (int)seq->kind);
        NAOS_CHECK_UINT_EQ(0, seq->span.start);
        NAOS_CHECK_UINT_EQ(13, seq->span.end);
        NAOS_CHECK_UINT_EQ(1, seq->start_line);
        NAOS_CHECK_UINT_EQ(1, seq->end_line);
        NAOS_REQUIRE_UINT_EQ(3, seq->as.sequence.count);
        check_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "a", "a");
        check_scalar(text, seq->as.sequence.items[1], seq, MND_YAML_SINGLE_QUOTED, "b", "'b'");
        check_scalar(text, seq->as.sequence.items[2], seq, MND_YAML_DOUBLE_QUOTED, "c", "\"c\"");
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* マッピングと、値としての入れ子のシーケンス（2.3） */
NAOS_TEST(flow_mapping_with_nested_sequence)
{
    const char *text = "key: {k: v, l: [1, 2]} # c\n";
    flow_run_t r;
    const mnd_yaml_node_t *map = NULL;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 5));
    check_success(&r, text, "{k: v, l: [1, 2]}");
    map = r.node;
    if (r.ok && (map != NULL))
    {
        NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)map->kind);
        NAOS_CHECK_UINT_EQ(5, map->span.start);
        NAOS_CHECK_UINT_EQ(22, map->span.end);
        NAOS_REQUIRE_UINT_EQ(2, map->as.mapping.count);
        check_scalar(text, map->as.mapping.pairs[0].key, map, MND_YAML_PLAIN, "k", "k");
        check_scalar(text, map->as.mapping.pairs[0].value, map, MND_YAML_PLAIN, "v", "v");
        check_scalar(text, map->as.mapping.pairs[1].key, map, MND_YAML_PLAIN, "l", "l");
        seq = map->as.mapping.pairs[1].value;
        NAOS_REQUIRE_TRUE(seq != NULL);
        NAOS_CHECK_INT_EQ((int)MND_YAML_SEQUENCE, (int)seq->kind);
        NAOS_CHECK_TRUE(seq->parent == map);
        NAOS_CHECK_UINT_EQ(15, seq->span.start);
        NAOS_CHECK_UINT_EQ(21, seq->span.end);
        NAOS_REQUIRE_UINT_EQ(2, seq->as.sequence.count);
        check_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "1", "1");
        check_scalar(text, seq->as.sequence.items[1], seq, MND_YAML_PLAIN, "2", "2");
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* 空のコレクション */
NAOS_TEST(flow_empty_collections)
{
    const char *text = "[[], {}]";
    flow_run_t r;
    const mnd_yaml_node_t *seq = NULL;
    const mnd_yaml_node_t *child = NULL;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 0));
    check_success(&r, text, text);
    seq = r.node;
    if (r.ok && (seq != NULL))
    {
        NAOS_REQUIRE_UINT_EQ(2, seq->as.sequence.count);
        child = seq->as.sequence.items[0];
        NAOS_CHECK_INT_EQ((int)MND_YAML_SEQUENCE, (int)child->kind);
        NAOS_CHECK_UINT_EQ(0, child->as.sequence.count);
        NAOS_CHECK_UINT_EQ(1, child->span.start);
        NAOS_CHECK_UINT_EQ(3, child->span.end);
        child = seq->as.sequence.items[1];
        NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)child->kind);
        NAOS_CHECK_UINT_EQ(0, child->as.mapping.count);
        NAOS_CHECK_UINT_EQ(5, child->span.start);
        NAOS_CHECK_UINT_EQ(7, child->span.end);
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/*
 * 複数の行（コメント、空行、CRLF、複数行のスカラー）と末尾の ,（2.3）。
 * カーソルは閉じ括弧の行に進む
 */
NAOS_TEST(flow_multi_line_and_trailing_comma)
{
    const char *text = "key: [\r\n  a,  # c\r\n\r\n  'b\r\n  c',\r\n  {x: y,},\r\n  d\r\n  e,\r\n]\r\nnext: 1\r\n";
    flow_run_t r;
    const mnd_yaml_node_t *seq = NULL;
    const mnd_yaml_node_t *map = NULL;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 5));
    check_success(&r, text, "[\r\n  a,  # c\r\n\r\n  'b\r\n  c',\r\n  {x: y,},\r\n  d\r\n  e,\r\n]");
    seq = r.node;
    if (r.ok && (seq != NULL))
    {
        NAOS_CHECK_INT_EQ((int)MND_YAML_SEQUENCE, (int)seq->kind);
        NAOS_CHECK_UINT_EQ(1, seq->start_line);
        NAOS_CHECK_UINT_EQ(9, seq->end_line);
        NAOS_CHECK_UINT_EQ(8, r.at.line_index);
        NAOS_REQUIRE_UINT_EQ(4, seq->as.sequence.count);
        check_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "a", "a");
        NAOS_CHECK_UINT_EQ(2, seq->as.sequence.items[0]->start_line);
        check_scalar(text, seq->as.sequence.items[1], seq, MND_YAML_SINGLE_QUOTED, "b c", "'b\r\n  c'");
        NAOS_CHECK_UINT_EQ(4, seq->as.sequence.items[1]->start_line);
        NAOS_CHECK_UINT_EQ(5, seq->as.sequence.items[1]->end_line);
        map = seq->as.sequence.items[2];
        NAOS_REQUIRE_TRUE(map != NULL);
        NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)map->kind);
        NAOS_CHECK_TRUE(map->parent == seq);
        NAOS_REQUIRE_UINT_EQ(1, map->as.mapping.count);
        check_scalar(text, map->as.mapping.pairs[0].key, map, MND_YAML_PLAIN, "x", "x");
        check_scalar(text, map->as.mapping.pairs[0].value, map, MND_YAML_PLAIN, "y", "y");
        check_scalar(text, seq->as.sequence.items[3], seq, MND_YAML_PLAIN, "d e", "d\r\n  e");
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/*
 * 値のないキー: {k} はキーの直後、{k: } は : の直後の空のプレーンスカラー（2.3、範囲の規則）
 */
NAOS_TEST(flow_mapping_empty_values)
{
    const char *text = "{k, l: , m:\n}";
    flow_run_t r;
    const mnd_yaml_node_t *map = NULL;
    const mnd_yaml_node_t *value = NULL;

    NAOS_REQUIRE_TRUE(flow_run(&r, "{k}", 0));
    check_success(&r, "{k}", "{k}");
    map = r.node;
    if (r.ok && (map != NULL))
    {
        NAOS_REQUIRE_UINT_EQ(1, map->as.mapping.count);
        check_scalar("{k}", map->as.mapping.pairs[0].key, map, MND_YAML_PLAIN, "k", "k");
        value = map->as.mapping.pairs[0].value;
        check_scalar("{k}", value, map, MND_YAML_PLAIN, "", "");
        NAOS_CHECK_UINT_EQ(2, value->span.start);
        NAOS_CHECK_UINT_EQ(2, value->span.end);
        NAOS_CHECK_UINT_EQ(1, value->start_line);
        NAOS_CHECK_UINT_EQ(1, value->end_line);
    }
    mnd_yaml_fixture_cleanup(&r.fx);

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 0));
    check_success(&r, text, text);
    map = r.node;
    if (r.ok && (map != NULL))
    {
        NAOS_REQUIRE_UINT_EQ(3, map->as.mapping.count);
        value = map->as.mapping.pairs[0].value;
        check_scalar(text, value, map, MND_YAML_PLAIN, "", "");
        NAOS_CHECK_UINT_EQ(2, value->span.start);
        value = map->as.mapping.pairs[1].value;
        check_scalar(text, value, map, MND_YAML_PLAIN, "", "");
        NAOS_CHECK_UINT_EQ(6, value->span.start);
        NAOS_CHECK_UINT_EQ(6, value->span.end);
        value = map->as.mapping.pairs[2].value;
        check_scalar(text, value, map, MND_YAML_PLAIN, "", "");
        NAOS_CHECK_UINT_EQ(11, value->span.start);
        NAOS_CHECK_UINT_EQ(11, value->span.end);
        NAOS_CHECK_UINT_EQ(1, value->start_line);
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* 引用符付きのキーと、引用符付きのキーの直後の : */
NAOS_TEST(flow_mapping_quoted_keys)
{
    const char *text = "{\"a b\":1, 'c': \"d\"}";
    flow_run_t r;
    const mnd_yaml_node_t *map = NULL;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 0));
    check_success(&r, text, text);
    map = r.node;
    if (r.ok && (map != NULL))
    {
        NAOS_REQUIRE_UINT_EQ(2, map->as.mapping.count);
        check_scalar(text, map->as.mapping.pairs[0].key, map, MND_YAML_DOUBLE_QUOTED, "a b", "\"a b\"");
        check_scalar(text, map->as.mapping.pairs[0].value, map, MND_YAML_PLAIN, "1", "1");
        check_scalar(text, map->as.mapping.pairs[1].key, map, MND_YAML_SINGLE_QUOTED, "c", "'c'");
        check_scalar(text, map->as.mapping.pairs[1].value, map, MND_YAML_DOUBLE_QUOTED, "d", "\"d\"");
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* フロー形式の中のすべてのスカラーが flow の印を持つ（深さ 3 の木を作業用の配列でたどる） */
NAOS_TEST(flow_all_scalars_have_flow_flag)
{
    const char *text = "{a: [b, {c: 'd'}], \"e\": f}";
    flow_run_t r;
    const mnd_yaml_node_t *stack[32];
    const mnd_yaml_node_t *node = NULL;
    size_t depth = 0;
    size_t scalars = 0;
    size_t i;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 0));
    check_success(&r, text, text);
    if (r.ok && (r.node != NULL))
    {
        stack[depth++] = r.node;
        while (depth > 0)
        {
            node = stack[--depth];
            if (node->kind == MND_YAML_SCALAR)
            {
                ++scalars;
                NAOS_CHECK_TRUE(node->as.scalar.flow);
            }
            else if (node->kind == MND_YAML_SEQUENCE)
            {
                for (i = 0; (i < node->as.sequence.count) && (depth < 32); ++i)
                {
                    stack[depth++] = node->as.sequence.items[i];
                }
            }
            else
            {
                for (i = 0; (i < node->as.mapping.count) && ((depth + 1) < 32); ++i)
                {
                    stack[depth++] = node->as.mapping.pairs[i].key;
                    stack[depth++] = node->as.mapping.pairs[i].value;
                }
            }
        }
    }
    NAOS_CHECK_UINT_EQ(6, scalars);
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* 空の項目、ブロックスカラー、項目の後ろの余分な内容、閉じ括弧の不一致（6.2） */
NAOS_TEST(flow_invalid_entries)
{
    check_error("[a, , b]", 0, "invalid flow collection entry", 1);
    check_error("[,]", 0, "invalid flow collection entry", 1);
    check_error("{,}", 0, "invalid flow collection entry", 1);
    check_error("[a,\n , b]", 0, "invalid flow collection entry", 2);
    check_error("[|]", 0, "invalid flow collection entry", 1);
    check_error("{k: >\n  x}", 0, "invalid flow collection entry", 1);
    check_error("[a, \"b\" c]", 0, "invalid flow collection entry", 1);
    check_error("[a: b]", 0, "invalid flow collection entry", 1);
    check_error("[a}", 0, "invalid flow collection entry", 1);
    check_error("{a: b]", 0, "invalid flow collection entry", 1);
    check_error("{[a]: b}", 0, "invalid flow collection entry", 1);
    check_error("{a: b: c}", 0, "invalid flow collection entry", 1);
    check_error("x", 0, "invalid flow collection entry", 1);
}

/* 項目のスカラーのエラーはスカラーの走査が報告する */
NAOS_TEST(flow_scalar_errors_are_reported)
{
    check_error("[&a b]", 0, "unsupported YAML syntax: anchor", 1);
    check_error("[a, 'b]", 0, "unterminated quoted scalar", 1);
    check_error("[\"\\q\"]", 0, "invalid escape sequence in double-quoted scalar", 1);
    check_error("{? a: b}", 0, "unsupported YAML syntax: complex mapping key", 1);
    check_error("[a,\r b]", 0, "carriage return without line feed", 1);
}

/* 閉じ括弧がないまま内容が終わる（6.2。開き括弧の行） */
NAOS_TEST(flow_unterminated)
{
    check_error("[a", 0, "unterminated flow collection", 1);
    check_error("[", 0, "unterminated flow collection", 1);
    check_error("{k: v,", 0, "unterminated flow collection", 1);
    check_error("{k", 0, "unterminated flow collection", 1);
    check_error("{k:", 0, "unterminated flow collection", 1);
    check_error("x:\n  [a,\n  [b]\n", 5, "unterminated flow collection", 2);
    check_error("[a, # ]\n", 0, "unterminated flow collection", 1);
}

/* 同じキー（6.4。後のキーの行番号） */
NAOS_TEST(flow_duplicate_key)
{
    check_error("{k: 1, k: 2}", 0, "duplicate mapping key \"k\"", 1);
    check_error("{k: 1,\n 'k': 2}", 0, "duplicate mapping key \"k\"", 2);
    check_error("{a,\n\n b, a}", 0, "duplicate mapping key \"a\"", 3);
    check_error("[{k: 1}, {k: 2, x: [{y, y}]}]", 0, "duplicate mapping key \"y\"", 1);
}

/* 異なるマッピングの同じキーと、論理値が異なるキーはエラーにしない */
NAOS_TEST(flow_distinct_keys_are_accepted)
{
    const char *text = "[{k: 1}, {k: 2, K: 3, 'k ': 4}]";
    flow_run_t r;

    NAOS_REQUIRE_TRUE(flow_run(&r, text, 0));
    check_success(&r, text, text);
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* 10,000 段の [ は再帰を使わずに処理する（6.9） */
NAOS_TEST(flow_deep_nesting)
{
    const size_t depth = 10000;
    char *data = NULL;
    flow_run_t r;
    const mnd_yaml_node_t *node = NULL;
    size_t levels = 0;
    size_t i;

    data = (char *)malloc((depth * 2) + 1);
    NAOS_REQUIRE_TRUE(data != NULL);

    /* 閉じていない 10,000 段の [ はエラーの診断を返す */
    memset(data, '[', depth);
    check_error_len(data, depth, 0, "unterminated flow collection", 1);

    /* 閉じた 10,000 段の入れ子は成功し、木の検査を満たす */
    for (i = 0; i < depth; ++i)
    {
        data[depth + i] = ']';
    }
    data[depth * 2] = '\0';
    NAOS_REQUIRE_TRUE(flow_run_len(&r, data, depth * 2, 0, NULL));
    check_success(&r, data, data);
    node = r.node;
    while ((node != NULL) && (node->kind == MND_YAML_SEQUENCE))
    {
        ++levels;
        node = (node->as.sequence.count == 1) ? node->as.sequence.items[0] : NULL;
    }
    NAOS_CHECK_UINT_EQ(depth, levels);
    mnd_yaml_fixture_cleanup(&r.fx);
    free(data);
}

/*
 * メモリ不足では false と out of memory を返し、*out と *at を変えず、片付けたあとに確保が残らない（6.7）。
 * 作業用の配列とスタックが伸びる大きさの入力を使う
 */
NAOS_TEST(flow_out_of_memory)
{
    static char text[2048];
    mnd_yaml_test_alloc_t ta;
    flow_run_t r;
    mnd_span_t content;
    const mnd_diag_t *d = NULL;
    size_t n;
    size_t pos = 0;
    size_t i;
    size_t failures = 0;
    bool ok = false;

    for (i = 0; i < 40; ++i)
    {
        text[pos++] = '[';
    }
    for (i = 0; i < 200; ++i)
    {
        text[pos++] = 'a';
        text[pos++] = ',';
    }
    text[pos++] = '{';
    for (i = 0; i < 100; ++i)
    {
        text[pos++] = (char)('A' + (char)(i % 26));
        text[pos++] = (char)('a' + (char)(i / 26));
        text[pos++] = ',';
    }
    text[pos++] = '}';
    for (i = 0; i < 40; ++i)
    {
        text[pos++] = ']';
    }
    text[pos] = '\0';
    content.start = 0;
    content.end = pos;
    for (n = 1; (n < 1000) && !ok; ++n)
    {
        mnd_yaml_test_alloc_init(&ta, 0);
        NAOS_REQUIRE_TRUE(mnd_yaml_fixture_init(&r.fx, text, pos, content, &ta.allocator));
        ta.fail_at = ta.calls + n;
        r.at = mnd_yaml_fixture_cursor(&r.fx, 0);
        r.node = NULL;
        ok = mnd_yaml_flow_parse(&r.fx.parser, &r.at, &r.node);
        if (ok)
        {
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&r.fx.diags));
            NAOS_CHECK_UINT_EQ(pos, r.at.offset);
        }
        else
        {
            ++failures;
            NAOS_CHECK_TRUE(r.node == NULL);
            NAOS_CHECK_UINT_EQ(0, r.at.offset);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&r.fx.diags));
            d = mnd_diag_list_at(&r.fx.diags, 0);
            NAOS_CHECK_STR_EQ("out of memory", d->message);
            NAOS_CHECK_UINT_EQ(0, d->line);
        }
        mnd_yaml_fixture_cleanup(&r.fx);
        NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    }
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_TRUE(failures > 2);
}

NAOS_SUITE(flow,
    NAOS_ADD_TEST(flow_sequence_of_scalars),
    NAOS_ADD_TEST(flow_mapping_with_nested_sequence),
    NAOS_ADD_TEST(flow_empty_collections),
    NAOS_ADD_TEST(flow_multi_line_and_trailing_comma),
    NAOS_ADD_TEST(flow_mapping_empty_values),
    NAOS_ADD_TEST(flow_mapping_quoted_keys),
    NAOS_ADD_TEST(flow_all_scalars_have_flow_flag),
    NAOS_ADD_TEST(flow_invalid_entries),
    NAOS_ADD_TEST(flow_scalar_errors_are_reported),
    NAOS_ADD_TEST(flow_unterminated),
    NAOS_ADD_TEST(flow_duplicate_key),
    NAOS_ADD_TEST(flow_distinct_keys_are_accepted),
    NAOS_ADD_TEST(flow_deep_nesting),
    NAOS_ADD_TEST(flow_out_of_memory)
)
