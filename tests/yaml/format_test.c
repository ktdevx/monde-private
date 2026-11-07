/* スイート format: 訳文の整形（5.1〜5.8） */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/source.h"
#include "tree_check.h"
#include "yaml/yaml.h"
#include "yaml/yaml_format.h"
#include "yaml/yaml_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 雛形: 5 種類のスタイルと 3 種類のチョンプが区別でき、既定値（0）が PLAIN と CLIP */
NAOS_TEST(format_scaffold_styles_and_chomps)
{
    NAOS_CHECK_INT_EQ(0, MND_YAML_PLAIN);
    NAOS_CHECK_INT_EQ(0, MND_YAML_CHOMP_CLIP);
    NAOS_CHECK_INT_NE(MND_YAML_SINGLE_QUOTED, MND_YAML_DOUBLE_QUOTED);
    NAOS_CHECK_INT_NE(MND_YAML_LITERAL, MND_YAML_FOLDED);
    NAOS_CHECK_INT_NE(MND_YAML_CHOMP_STRIP, MND_YAML_CHOMP_KEEP);
}

/* 根からノードまでの道筋（再解析した文書で同じ位置のスカラーを探すため） */

#define FORMAT_PATH_MAX 32
#define FORMAT_STACK_MAX 256

typedef enum format_step_kind
{
    FORMAT_STEP_KEY,    /* マッピングの index 番目の組のキー */
    FORMAT_STEP_VALUE,  /* マッピングの index 番目の組の値 */
    FORMAT_STEP_ITEM    /* シーケンスの index 番目の要素 */
} format_step_kind_t;

typedef struct format_step
{
    format_step_kind_t kind;
    size_t index;
} format_step_t;

typedef struct format_path
{
    format_step_t steps[FORMAT_PATH_MAX];
    size_t count;
} format_path_t;

/*
 * 根から node までの道筋を *path に入れる。深すぎるか親子の関係が壊れていれば false
 */
static bool format_path_of(const mnd_yaml_node_t *node, format_path_t *path)
{
    const mnd_yaml_node_t *child = node;
    const mnd_yaml_node_t *parent = NULL;
    format_step_t reversed[FORMAT_PATH_MAX];
    size_t count = 0;
    size_t i;
    bool found;

    while (child->parent != NULL)
    {
        parent = child->parent;
        found = false;
        if (count >= FORMAT_PATH_MAX)
        {
            return false;
        }
        if (parent->kind == MND_YAML_MAPPING)
        {
            for (i = 0; (i < parent->as.mapping.count) && !found; ++i)
            {
                if (parent->as.mapping.pairs[i].key == child)
                {
                    reversed[count].kind = FORMAT_STEP_KEY;
                    reversed[count].index = i;
                    found = true;
                }
                else if (parent->as.mapping.pairs[i].value == child)
                {
                    reversed[count].kind = FORMAT_STEP_VALUE;
                    reversed[count].index = i;
                    found = true;
                }
            }
        }
        else if (parent->kind == MND_YAML_SEQUENCE)
        {
            for (i = 0; (i < parent->as.sequence.count) && !found; ++i)
            {
                if (parent->as.sequence.items[i] == child)
                {
                    reversed[count].kind = FORMAT_STEP_ITEM;
                    reversed[count].index = i;
                    found = true;
                }
            }
        }
        if (!found)
        {
            return false;
        }
        ++count;
        child = parent;
    }
    path->count = count;
    for (i = 0; i < count; ++i)
    {
        path->steps[i] = reversed[count - 1 - i];
    }

    return true;
}

/* doc の根から path をたどったノード。たどれなければ NULL */
static const mnd_yaml_node_t *format_follow(const mnd_yaml_doc_t *doc, const format_path_t *path)
{
    const mnd_yaml_node_t *node = mnd_yaml_doc_root(doc);
    const format_step_t *step = NULL;
    size_t i;

    for (i = 0; (i < path->count) && (node != NULL); ++i)
    {
        step = &path->steps[i];
        if (step->kind == FORMAT_STEP_ITEM)
        {
            if ((node->kind != MND_YAML_SEQUENCE) || (step->index >= node->as.sequence.count))
            {
                return NULL;
            }
            node = node->as.sequence.items[step->index];
        }
        else
        {
            if ((node->kind != MND_YAML_MAPPING) || (step->index >= node->as.mapping.count))
            {
                return NULL;
            }
            node = (step->kind == FORMAT_STEP_KEY) ? node->as.mapping.pairs[step->index].key
                                                   : node->as.mapping.pairs[step->index].value;
        }
    }

    return node;
}

/* doc の中で span.start が offset のスカラー（なければ NULL）。再帰を使わない */
static const mnd_yaml_node_t *format_scalar_at(const mnd_yaml_doc_t *doc, size_t offset)
{
    const mnd_yaml_node_t *stack[FORMAT_STACK_MAX];
    const mnd_yaml_node_t *node = NULL;
    size_t depth = 0;
    size_t i;

    if (mnd_yaml_doc_root(doc) == NULL)
    {
        return NULL;
    }
    stack[depth++] = mnd_yaml_doc_root(doc);
    while (depth > 0)
    {
        node = stack[--depth];
        if (node->kind == MND_YAML_SCALAR)
        {
            if (node->span.start == offset)
            {
                return node;
            }
        }
        else if (node->kind == MND_YAML_MAPPING)
        {
            for (i = 0; (i < node->as.mapping.count) && ((depth + 2) <= FORMAT_STACK_MAX); ++i)
            {
                stack[depth++] = node->as.mapping.pairs[i].key;
                stack[depth++] = node->as.mapping.pairs[i].value;
            }
        }
        else
        {
            for (i = 0; (i < node->as.sequence.count) && (depth < FORMAT_STACK_MAX); ++i)
            {
                stack[depth++] = node->as.sequence.items[i];
            }
        }
    }

    return NULL;
}

/* data[0..len) 全体を解析した文書（失敗したら NULL。診断は捨てる） */
static mnd_yaml_doc_t *format_parse(const char *data, size_t len)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;

    content.start = 0;
    content.end = len;
    if (!mnd_yaml_parse(data, len, content, NULL, &doc, &diags))
    {
        if (mnd_diag_list_count(&diags) > 0)
        {
            NAOS_NOTE("parse error: line %u: %s", (unsigned)mnd_diag_list_at(&diags, 0)->line,
                      mnd_diag_list_at(&diags, 0)->message);
        }
        doc = NULL;
    }
    mnd_diag_list_free(&diags);

    return doc;
}

/*
 * doc のスカラーを、根から明示的なスタックでたどった順に scalars に入れて数を返す（再帰を使わない）。
 * FORMAT_STACK_MAX を超える分は入れない
 */
static size_t format_collect_scalars(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t **scalars)
{
    const mnd_yaml_node_t *stack[FORMAT_STACK_MAX];
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
            if (count < FORMAT_STACK_MAX)
            {
                scalars[count++] = node;
            }
        }
        else if (node->kind == MND_YAML_MAPPING)
        {
            for (i = 0; (i < node->as.mapping.count) && ((depth + 2) <= FORMAT_STACK_MAX); ++i)
            {
                stack[depth++] = node->as.mapping.pairs[i].key;
                stack[depth++] = node->as.mapping.pairs[i].value;
            }
        }
        else
        {
            for (i = 0; (i < node->as.sequence.count) && (depth < FORMAT_STACK_MAX); ++i)
            {
                stack[depth++] = node->as.sequence.items[i];
            }
        }
    }

    return count;
}

/*
 * 置き換えた範囲の外側が変わらないこと: 元の文書と解析し直した文書で、target と found 以外のスカラーの
 * 数・順・論理値・スタイルが一致する。一致すれば true
 */
static bool format_check_others(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *target,
                                const mnd_yaml_doc_t *again, const mnd_yaml_node_t *found)
{
    const mnd_yaml_node_t *before[FORMAT_STACK_MAX];
    const mnd_yaml_node_t *after[FORMAT_STACK_MAX];
    size_t before_count = format_collect_scalars(doc, before);
    size_t after_count = format_collect_scalars(again, after);
    size_t i;
    bool ok = (before_count == after_count);

    NAOS_CHECK_UINT_EQ(before_count, after_count);
    for (i = 0; ok && (i < before_count); ++i)
    {
        if ((before[i] == target) || (after[i] == found))
        {
            NAOS_CHECK_TRUE((before[i] == target) && (after[i] == found));
            ok = (before[i] == target) && (after[i] == found);
            continue;
        }
        NAOS_CHECK_MEM_EQ(before[i]->as.scalar.value, before[i]->as.scalar.value_len, after[i]->as.scalar.value,
                          after[i]->as.scalar.value_len);
        NAOS_CHECK_INT_EQ(before[i]->as.scalar.style, after[i]->as.scalar.style);
        ok = (before[i]->as.scalar.value_len == after[i]->as.scalar.value_len)
             && (memcmp(before[i]->as.scalar.value, after[i]->as.scalar.value, before[i]->as.scalar.value_len) == 0)
             && (before[i]->as.scalar.style == after[i]->as.scalar.style);
    }

    return ok;
}

/*
 * replaced[0..replaced_len) を解析し直し、木の検査を満たし、path の位置のスカラーの論理値が text[0..len) と
 * 一致し、それ以外のスカラーが元の文書 doc（置き換えたスカラーは target）と同じであることを確かめる。
 * すべて満たせば true
 */
static bool format_check_reparsed(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *target, const char *replaced,
                                  size_t replaced_len, const format_path_t *path, const char *text, size_t len)
{
    mnd_yaml_doc_t *again = format_parse(replaced, replaced_len);
    const mnd_yaml_node_t *found = NULL;
    bool ok = false;

    NAOS_CHECK_TRUE(again != NULL);
    if (again != NULL)
    {
        NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(again));
        found = format_follow(again, path);
        NAOS_CHECK_TRUE((found != NULL) && (found->kind == MND_YAML_SCALAR));
        if ((found != NULL) && (found->kind == MND_YAML_SCALAR))
        {
            NAOS_CHECK_MEM_EQ(text, len, found->as.scalar.value, found->as.scalar.value_len);
            ok = (found->as.scalar.value_len == len) && (memcmp(text, found->as.scalar.value, len) == 0);
            ok = format_check_others(doc, target, again, found) && ok;
        }
    }
    if (!ok)
    {
        NAOS_NOTE("replaced: %s", replaced);
    }
    mnd_yaml_doc_free(again);

    return ok;
}

/*
 * yaml を解析し、offset から始まるスカラーを text[0..len) で整形する。整形が診断なしで成功し、
 * expected が NULL でなければ出力が expected と一致することを確かめる。さらに、元の範囲を出力で
 * 置き換えた内容を解析し直し、木の検査を満たし、同じ位置のスカラーの論理値が text と一致することを確かめる。
 * すべて満たせば true
 */
static bool format_roundtrip(const char *yaml, size_t offset, const char *text, size_t len, const char *expected)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t replaced = MND_BUF_INIT;
    mnd_yaml_doc_t *doc = NULL;
    const mnd_yaml_node_t *scalar = NULL;
    format_path_t path;
    size_t yaml_len = strlen(yaml);
    bool ok = false;

    doc = format_parse(yaml, yaml_len);
    NAOS_CHECK_TRUE(doc != NULL);
    if (doc == NULL)
    {
        return false;
    }
    scalar = format_scalar_at(doc, offset);
    NAOS_CHECK_TRUE((scalar != NULL) && format_path_of(scalar, &path));
    if ((scalar != NULL) && format_path_of(scalar, &path))
    {
        ok = mnd_yaml_format_value(doc, scalar, text, len, "a.md", &out, &diags);
        NAOS_CHECK_TRUE(ok);
        NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
        if (!ok && (mnd_diag_list_count(&diags) > 0))
        {
            NAOS_NOTE("format error: %s", mnd_diag_list_at(&diags, 0)->message);
        }
    }
    if (ok && (expected != NULL))
    {
        NAOS_CHECK_STR_EQ(expected, mnd_buf_cstr(&out));
        ok = (strcmp(expected, mnd_buf_cstr(&out)) == 0);
    }
    if (ok)
    {
        ok = mnd_buf_append(&replaced, yaml, scalar->span.start)
             && mnd_buf_append(&replaced, out.data, out.len)
             && mnd_buf_append(&replaced, yaml + scalar->span.end, yaml_len - scalar->span.end);
        NAOS_CHECK_TRUE(ok);
    }
    if (ok)
    {
        ok = format_check_reparsed(doc, scalar, replaced.data, replaced.len, &path, text, len);
    }
    mnd_yaml_doc_free(doc);
    mnd_buf_free(&replaced);
    mnd_buf_free(&out);
    mnd_diag_list_free(&diags);

    return ok;
}

/* yaml の中の needle の最初の出現位置 + delta */
static size_t format_offset(const char *yaml, const char *needle, size_t delta)
{
    const char *at = strstr(yaml, needle);

    NAOS_REQUIRE_TRUE(at != NULL);

    return (size_t)(at - yaml) + delta;
}

/* needle の位置から始まるスカラーを text で整形すると expected になり、往復することを確かめる */
static void check_format(const char *yaml, const char *needle, const char *text, const char *expected)
{
    NAOS_TRACE_PUSH("yaml=[%s] text=[%s]", yaml, text);
    (void)format_roundtrip(yaml, format_offset(yaml, needle, 0), text, strlen(text), expected);
    NAOS_TRACE_POP();
}

/* プレーンの訳文はプレーンのまま（5.2）。途中の # と : と非 ASCII はプレーンを妨げない（5.7） */
NAOS_TEST(format_plain_kept)
{
    check_format("title: Hello\n", "Hello", "Bonjour le monde", "Bonjour le monde");
    check_format("title: Hello\n", "Hello", "C#", "C#");
    check_format("title: Hello\n", "Hello", "a#b", "a#b");
    check_format("title: Hello\n", "Hello", "a:b", "a:b");
    check_format("title: Hello\n", "Hello", "a - b", "a - b");
    check_format("title: Hello\n", "Hello", "caf\xc3\xa9", "caf\xc3\xa9");
    check_format("title: Hello\n", "Hello", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    check_format("title: Hello # c\n", "Hello", "Bonjour", "Bonjour");
    check_format("- Hello\n", "Hello", "Bonjour", "Bonjour");
    check_format("Hello\n", "Hello", "Bonjour", "Bonjour");
    /* Core Schema と YAML 1.1 の非文字列の形でないもの */
    check_format("title: Hello\n", "Hello", "true1", "true1");
    check_format("title: Hello\n", "Hello", "0x", "0x");
    check_format("title: Hello\n", "Hello", "0xG", "0xG");
    check_format("title: Hello\n", "Hello", ".in", ".in");
    check_format("title: Hello\n", "Hello", "+", "+");
    check_format("title: Hello\n", "Hello", "nul", "nul");
    check_format("title: Hello\n", "Hello", "1e", "1e");
    /* フロー形式の外の ? はプレーンを妨げない */
    check_format("title: Hello\n", "Hello", "What?", "What?");
}

/* プレーンを維持できない訳文はダブルクオートに切り替える（5.4） */
NAOS_TEST(format_plain_switches_to_double_quoted)
{
    check_format("title: Hello\n", "Hello", "a: b", "\"a: b\"");
    check_format("title: Hello\n", "Hello", "a #b", "\"a #b\"");
    check_format("title: Hello\n", "Hello", "#x", "\"#x\"");
    check_format("title: Hello\n", "Hello", "-x", "\"-x\"");
    check_format("title: Hello\n", "Hello", "- x", "\"- x\"");
    check_format("title: Hello\n", "Hello", "a\nb", "\"a\\nb\"");
    check_format("title: Hello\n", "Hello", "true", "\"true\"");
    check_format("title: Hello\n", "Hello", "2024", "\"2024\"");
    check_format("title: Hello\n", "Hello", "", "\"\"");
    check_format("title: Hello\n", "Hello", " lead", "\" lead\"");
    check_format("title: Hello\n", "Hello", "trail ", "\"trail \"");
    check_format("title: Hello\n", "Hello", "\tlead", "\"\\tlead\"");
    check_format("title: Hello\n", "Hello", "trail\t", "\"trail\\t\"");
    check_format("title: Hello\n", "Hello", "trail:", "\"trail:\"");
    check_format("title: Hello\n", "Hello", "a\t#b", "\"a\\t#b\"");
    check_format("title: Hello\n", "Hello", "a:\tb", "\"a:\\tb\"");
    check_format("title: Hello\n", "Hello", "---", "\"---\"");
    check_format("title: Hello\n", "Hello", "...", "\"...\"");
    check_format("title: Hello\n", "Hello", "... x", "\"... x\"");
    check_format("title: Hello\n", "Hello", "a\rb", "\"a\\rb\"");
    check_format("title: Hello\n", "Hello", "a\x01" "b", "\"a\\x01b\"");
    check_format("title: Hello\n", "Hello", "a\x7f" "b", "\"a\\x7Fb\"");
    check_format("title: Hello\n", "Hello", "a\xc2\x85" "b", "\"a\\x85b\"");
}

/*
 * YAML 1.1 の非文字列の形（真偽値、null、整数、小数、日付と時刻、<<、=）はダブルクオートに切り替える（5.9）。
 * 判定に迷う形（1.2.3、.、....）も切り替える側に倒す
 */
NAOS_TEST(format_plain_yaml11_forms_switch)
{
    static const char *const forms[] = {
        /* 真偽値 */
        "y", "Y", "yes", "Yes", "YES", "n", "N", "no", "No", "NO", "on", "On", "ON", "off", "Off", "OFF",
        "yEs", "oN",
        /* null */
        "nULL",
        /* 整数 */
        "1_000", "-1_000", "+0", "0b101", "-0b1_0", "012", "0_7", "0x_1F", "0xFF_FF", "1:30", "-1:30:59",
        "190:20:30", "1,000",
        /* 小数 */
        "1_000.5", "1.", "1._5", "+.5", ".5e+3", "1.5e-3", "1_0e5", "1:30.5", "1:30.", "-.inf", ".nAn",
        ".", "+.", "1.2.3", "....",
        /* 日付と時刻 */
        "2024-01-01", "2024-1-2", "2024-01-02T03:04:05Z", "2024-01-02t03:04:05", "2024-01-02 03:04:05",
        "2024-01-02  3:04:05.123", "2024-1-2 3:04:05 +09:00", "2024-01-02T03:04:05-5", "2024-01-02T03:04:05+0900",
        "2024-01-02T03:04:05.5 Z",
        /* merge と value */
        "<<", "="
    };
    char expected[64];
    size_t i;
    size_t n;

    for (i = 0; i < (sizeof(forms) / sizeof(forms[0])); ++i)
    {
        n = strlen(forms[i]);
        expected[0] = '"';
        memcpy(expected + 1, forms[i], n);
        expected[n + 1] = '"';
        expected[n + 2] = '\0';
        check_format("title: Hello\n", "Hello", forms[i], expected);
    }
    check_format("title: Hello\n", "Hello", "2024-01-02\t03:04:05Z", "\"2024-01-02\\t03:04:05Z\"");
}

/* YAML 1.1 の非文字列の形に似ているが一致しない訳文はプレーンのまま（5.2、5.9） */
NAOS_TEST(format_plain_yaml11_lookalikes_kept)
{
    static const char *const forms[] = {
        "Yesterday", "yes!", "Noon", "onion", "y2k", "nil", "2024-01-01 release", "2024-01", "2024-01-01T",
        "2024-01-02T03:04", "2024-01-02T03:04:05Zulu", "20240-01-01", "12a", "1:3x", "1:60", "0b", "0b2",
        "0xG1", "1e", "1e+", "=x", "x=", "<", "<<<", "<<a", "a.b", "v1.2", "1-2", "inf", "nan", ".infinity"
    };
    size_t i;

    for (i = 0; i < (sizeof(forms) / sizeof(forms[0])); ++i)
    {
        check_format("title: Hello\n", "Hello", forms[i], forms[i]);
    }
}

/*
 * プレーンの中のタブ、フロー形式の中の ?、U+0085・U+2028・U+2029 はダブルクオートに切り替える（5.9）。
 * シングルクオートも U+0085・U+2028・U+2029 では切り替える。ダブルクオートでは U+2028・U+2029 を
 * UTF-8 のまま出力する（5.7）
 */
NAOS_TEST(format_yaml11_characters_switch)
{
    check_format("title: Hello\n", "Hello", "a\tb", "\"a\\tb\"");
    check_format("tags: [a, b]\n", "b]", "What?", "\"What?\"");
    check_format("tags: [a, b]\n", "b]", "a?b", "\"a?b\"");
    check_format("m: {k: v}\n", "v}", "What?", "\"What?\"");
    check_format("m: {k: v}\n", "k:", "a?", "\"a?\"");
    check_format("title: Hello\n", "Hello", "a\xe2\x80\xa8" "b", "\"a\xe2\x80\xa8" "b\"");
    check_format("title: Hello\n", "Hello", "a\xe2\x80\xa9" "b", "\"a\xe2\x80\xa9" "b\"");
    check_format("title: 'Hello'\n", "'Hello'", "a\xe2\x80\xa8" "b", "\"a\xe2\x80\xa8" "b\"");
    check_format("title: 'Hello'\n", "'Hello'", "a\xe2\x80\xa9" "b", "\"a\xe2\x80\xa9" "b\"");
    check_format("title: 'Hello'\n", "'Hello'", "a\xc2\x85" "b", "\"a\\x85b\"");
    /* 似ているが別の文字（U+2027、U+202A）はそのまま */
    check_format("title: Hello\n", "Hello", "a\xe2\x80\xa7" "b", "a\xe2\x80\xa7" "b");
    check_format("title: 'Hello'\n", "'Hello'", "a\xe2\x80\xaa" "b", "'a\xe2\x80\xaa" "b'");
    /* シングルクオートのタブと YAML 1.1 の非文字列の形はシングルクオートのまま */
    check_format("title: 'Hello'\n", "'Hello'", "yes", "'yes'");
    check_format("title: [a, 'Hello']\n", "'Hello'", "What?", "'What?'");
}

/* 先頭の指示文字（-?:,[]{}#&*!|>'"%@ と `）ではダブルクオートに切り替える（5.4） */
NAOS_TEST(format_plain_leading_indicators_switch)
{
    static const char indicators[] = "-?:,[]{}#&*!|>'\"%@`";
    char text[3];
    char expected[8];
    size_t i;

    for (i = 0; i < (sizeof(indicators) - 1); ++i)
    {
        text[0] = indicators[i];
        text[1] = 'x';
        text[2] = '\0';
        expected[0] = '"';
        if (indicators[i] == '"')
        {
            expected[1] = '\\';
            expected[2] = '"';
            expected[3] = 'x';
            expected[4] = '"';
            expected[5] = '\0';
        }
        else
        {
            expected[1] = indicators[i];
            expected[2] = 'x';
            expected[3] = '"';
            expected[4] = '\0';
        }
        check_format("title: Hello\n", "Hello", text, expected);
    }
}

/* Core Schema の非文字列の形（null、真偽値、整数、小数、.inf、.nan）はダブルクオートに切り替える（5.4） */
NAOS_TEST(format_plain_core_schema_forms_switch)
{
    static const char *const forms[] = {
        "null", "Null", "NULL", "~", "true", "True", "TRUE", "false", "False", "FALSE",
        "0", "2024", "+12", "007", "0o17", "0x1F", "0xab", "1.5", "+1.5", ".5", "1.", "1e3", "1.5E-3", "+2e+10",
        ".inf", ".Inf", ".INF", "+.inf", ".nan", ".NaN", ".NAN"
    };
    char expected[32];
    size_t i;
    size_t n;

    for (i = 0; i < (sizeof(forms) / sizeof(forms[0])); ++i)
    {
        n = strlen(forms[i]);
        expected[0] = '"';
        memcpy(expected + 1, forms[i], n);
        expected[n + 1] = '"';
        expected[n + 2] = '\0';
        check_format("title: Hello\n", "Hello", forms[i], expected);
    }
}

/* シングルクオートは '' にして維持し、改行・CR・制御文字ではダブルクオートに切り替える（5.2、5.4） */
NAOS_TEST(format_single_quoted)
{
    check_format("title: 'Hello'\n", "'Hello'", "it's", "'it''s'");
    check_format("title: 'Hello'\n", "'Hello'", "''", "''''''");
    check_format("title: 'Hello'\n", "'Hello'", "true", "'true'");
    check_format("title: 'Hello'\n", "'Hello'", "a: b #c", "'a: b #c'");
    check_format("title: 'Hello'\n", "'Hello'", " both ", "' both '");
    check_format("title: 'Hello'\n", "'Hello'", "", "''");
    check_format("title: 'Hello'\n", "'Hello'", "a\tb", "'a\tb'");
    check_format("title: 'Hello'\n", "'Hello'", "caf\xc3\xa9", "'caf\xc3\xa9'");
    check_format("title: 'Hello'\n", "'Hello'", "it's\nnew", "\"it's\\nnew\"");
    check_format("title: 'Hello'\n", "'Hello'", "a\rb", "\"a\\rb\"");
    check_format("title: 'Hello'\n", "'Hello'", "a\x1b" "b", "\"a\\x1Bb\"");
    check_format("title: [a, 'Hello']\n", "'Hello'", "x, y", "'x, y'");
}

/* ダブルクオートのエスケープ（5.2）と、非 ASCII をエスケープしないこと（5.7） */
NAOS_TEST(format_double_quoted_escapes)
{
    check_format("title: \"Hello\"\n", "\"Hello\"", "plain", "\"plain\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "a\\b\"c", "\"a\\\\b\\\"c\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "l1\nl2\n", "\"l1\\nl2\\n\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "t\tr\r", "\"t\\tr\\r\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "\x01\x1f\x7f", "\"\\x01\\x1F\\x7F\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "\xc2\x80\xc2\x9f", "\"\\x80\\x9F\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "caf\xc3\xa9", "\"caf\xc3\xa9\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "\xc2\xa0\xe2\x80\xa8\xf0\x9f\x98\x80",
                 "\"\xc2\xa0\xe2\x80\xa8\xf0\x9f\x98\x80\"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "  ", "\"  \"");
    check_format("title: \"Hello\"\n", "\"Hello\"", "", "\"\"");
}

/* フロー形式の中のプレーンは , [ ] { } でダブルクオートに切り替える（5.5）。ブロック形式ではプレーンのまま */
NAOS_TEST(format_flow_context_symbols)
{
    check_format("tags: [a, b]\n", "b]", "c, d", "\"c, d\"");
    check_format("tags: [a, b]\n", "b]", "[x", "\"[x\"");
    check_format("tags: [a, b]\n", "b]", "x]", "\"x]\"");
    check_format("tags: [a, b]\n", "b]", "x{y", "\"x{y\"");
    check_format("tags: [a, b]\n", "b]", "x}", "\"x}\"");
    check_format("tags: [a, b]\n", "b]", "c d", "c d");
    check_format("tags: [a, b]\n", "b]", "a:b", "a:b");
    check_format("m: {k: v}\n", "v}", "x, y", "\"x, y\"");
    check_format("m: {k: v}\n", "k:", "x}", "\"x}\"");
    check_format("title: Hello\n", "Hello", "c, d", "c, d");
    check_format("title: Hello\n", "Hello", "x]", "x]");
    check_format("title: Hello\n", "Hello", "x{y}", "x{y}");
}

/* 空の範囲（値のないキー）では先頭に 1 つの空白を付ける（5.8）。{k} では値の指示文字も付ける */
NAOS_TEST(format_empty_value_gets_leading_space)
{
    static const char block[] = "key:\nnext: 1\n";
    static const char item[] = "-\n- b\n";
    static const char flow_colon[] = "m: {k: }\n";
    static const char flow_bare[] = "m: {k}\n";

    NAOS_TRACE_PUSH("empty values");
    (void)format_roundtrip(block, format_offset(block, ":", 1), "v", 1, " v");
    (void)format_roundtrip(block, format_offset(block, ":", 1), "a: b", 4, " \"a: b\"");
    (void)format_roundtrip(block, format_offset(block, ":", 1), "", 0, " \"\"");
    (void)format_roundtrip(item, 1, "v", 1, " v");
    (void)format_roundtrip(flow_colon, format_offset(flow_colon, "k:", 2), "v", 1, " v");
    (void)format_roundtrip(flow_colon, format_offset(flow_colon, "k:", 2), "x, y", 4, " \"x, y\"");
    (void)format_roundtrip(flow_bare, format_offset(flow_bare, "k}", 1), "v", 1, ": v");
    NAOS_TRACE_POP();
}

/* マッピングのキーも同じ規則で整形する（キー、インデント、後ろの内容は出力に含めない: 5.8） */
NAOS_TEST(format_mapping_keys)
{
    check_format("title: Hello\n", "title", "name", "name");
    check_format("title: Hello\n", "title", "a: b", "\"a: b\"");
    check_format("'title': Hello\n", "'title'", "it's", "'it''s'");
    check_format("m: {k: v}\n", "k:", "a b", "a b");
}

/* 結果は out の後ろに追加する */
NAOS_TEST(format_appends_to_out)
{
    static const char yaml[] = "title: Hello\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_yaml_doc_t *doc = format_parse(yaml, strlen(yaml));
    const mnd_yaml_node_t *scalar = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    scalar = format_scalar_at(doc, 7);
    NAOS_REQUIRE_TRUE(scalar != NULL);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "pre:"));
    NAOS_CHECK_TRUE(mnd_yaml_format_value(doc, scalar, "a: b", 4, NULL, &out, &diags));
    NAOS_CHECK_STR_EQ("pre:\"a: b\"", mnd_buf_cstr(&out));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_buf_free(&out);
    mnd_diag_list_free(&diags);
    mnd_yaml_doc_free(doc);
}

/*
 * text[0..len) の整形が false を返し、message のエラーの診断（行 0、パス a.md）が 1 つだけ追加され、
 * out の内容が変わらないことを確かめる
 */
static void check_format_error(const char *text, size_t len, const char *message)
{
    static const char yaml[] = "title: Hello\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_yaml_doc_t *doc = format_parse(yaml, strlen(yaml));
    const mnd_yaml_node_t *scalar = NULL;
    const mnd_diag_t *d = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    scalar = format_scalar_at(doc, 7);
    NAOS_REQUIRE_TRUE(scalar != NULL);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "pre"));
    NAOS_CHECK_FALSE(mnd_yaml_format_value(doc, scalar, text, len, "a.md", &out, &diags));
    NAOS_CHECK_STR_EQ("pre", mnd_buf_cstr(&out));
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
    if (mnd_diag_list_count(&diags) == 1)
    {
        d = mnd_diag_list_at(&diags, 0);
        NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
        NAOS_CHECK_STR_EQ(message, d->message);
        NAOS_CHECK_STR_EQ("a.md", d->path);
        NAOS_CHECK_UINT_EQ(0, d->line);
    }
    mnd_buf_free(&out);
    mnd_diag_list_free(&diags);
    mnd_yaml_doc_free(doc);
}

/* 訳文の NUL と不正な UTF-8 はエラー（設計のメッセージ） */
NAOS_TEST(format_nul_and_invalid_utf8_errors)
{
    check_format_error("a\0b", 3, "NUL in value");
    check_format_error("\0", 1, "NUL in value");
    check_format_error("a\xff", 2, "invalid UTF-8 sequence");
    check_format_error("\xc3", 1, "invalid UTF-8 sequence");
    check_format_error("\xc0\xaf", 2, "invalid UTF-8 sequence");
    check_format_error("\xed\xa0\x80", 3, "invalid UTF-8 sequence");
}

/* ブロックスカラーの訳文も往復する（維持できない訳文はダブルクオートに切り替わる） */
NAOS_TEST(format_block_scalar_roundtrips)
{
    check_format("text: |\n  Hello\n", "|", "Bonjour", NULL);
    check_format("text: |\n  Hello\n", "|", "a: b\n#c\n", NULL);
    check_format("text: >-\n  Hello\nnext: 1\n", ">", "x\ny", NULL);
    check_format("text: | # c\n  Hello\n\nnext: 1\n", "|", "", NULL);
}

/*
 * リテラルは各チョンプで、末尾の改行の数が合えば維持する（5.3）: ヘッダを原文のまま、内容の各行を
 * content_indent 個の空白で始める。空行は空白を付けない。keep では末尾の改行を空行にする
 */
NAOS_TEST(format_literal_kept_each_chomp)
{
    check_format("k: |\n  v\nn: 1\n", "|", "a\nb\n", "|\n  a\n  b");
    check_format("k: |\n  v\nn: 1\n", "|", "a\n\nb\n", "|\n  a\n\n  b");
    check_format("k: |\n  v\nn: 1\n", "|", "\na\n", "|\n\n  a");
    check_format("k: |\n  v\nn: 1\n", "|", "a\n  b\n\tc\n", "|\n  a\n    b\n  \tc");
    check_format("k: |\n  v\nn: 1\n", "|", "a: b # c\n- [x]\n", "|\n  a: b # c\n  - [x]");
    check_format("k: |\n  v\n\n\nn: 1\n", "|", "caf\xc3\xa9\n", "|\n  caf\xc3\xa9");
    check_format("k: |-\n  v\nn: 1\n", "|", "a\nb", "|-\n  a\n  b");
    check_format("k: |-\n  v\n\nn: 1\n", "|", "x", "|-\n  x");
    check_format("k: |+\n  v\nn: 1\n", "|", "a\n", "|+\n  a");
    check_format("k: |+\n  v\n\nn: 1\n", "|", "a\n\n\n", "|+\n  a\n\n");
    check_format("k: |+\n  v\n\n\n", "|", "a\nb\n\n", "|+\n  a\n  b\n");
    /* 内容の行がない元のスカラー（範囲は指示子の直後まで） */
    check_format("k: |\nn: 1\n", "|", "a\n", "|\n a");
    check_format("k: |-\n\nn: 1\n", "|", "a", "|-\n a");
    check_format("k: |+\n\n\nn: 1\n", "|", "a\n\n", "|+\n a\n");
    /* 最後の内容の行に行末記号がない文書: clip の末尾の改行は 0 個 */
    check_format("k: |\n  v", "|", "a\nb", "|\n  a\n  b");
    check_format("k: |+\n  v", "|", "a\n\n", "|+\n  a\n\n");
    check_format("k: |+\n  v", "|", "a", "|+\n  a");
}

/*
 * 維持できない訳文はダブルクオートに切り替える（5.4）: 末尾の改行の数がチョンプと合わない、CR、
 * 空白だけの行、指示子がないのに最初の空でない行が空白で始まる、タブ以外の制御文字、内容の行がない
 */
NAOS_TEST(format_block_scalar_switches_to_double_quoted)
{
    check_format("k: |\n  v\nn: 1\n", "|", "a", "\"a\"");
    check_format("k: |\n  v\nn: 1\n", "|", "a\n\n", "\"a\\n\\n\"");
    check_format("k: |-\n  v\nn: 1\n", "|", "a\n", "\"a\\n\"");
    check_format("k: |+\n  v\nn: 1\n", "|", "a", "\"a\"");
    check_format("k: >\n  v\nn: 1\n", ">", "a", "\"a\"");
    check_format("k: >-\n  v\nn: 1\n", ">", "a\n", "\"a\\n\"");
    check_format("k: >+\n  v\nn: 1\n", ">", "a", "\"a\"");
    check_format("k: |\n  v\nn: 1\n", "|", "a\r\nb\n", "\"a\\r\\nb\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "a\rb\n", "\"a\\rb\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "a\n  \nb\n", "\"a\\n  \\nb\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "a\n\t\nb\n", "\"a\\n\\t\\nb\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", " a\n", "\" a\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "\ta\n", "\"\\ta\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "\n\n a\n", "\"\\n\\n a\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "a\x01\n", "\"a\\x01\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "nel\xc2\x85\n", "\"nel\\x85\\n\"");
    check_format("k: |\n  v\nn: 1\n", "|", "\n", "\"\\n\"");
    check_format("k: |-\n  v\nn: 1\n", "|", "", "\"\"");
    check_format("k: |+\n  v\nn: 1\n", "|", "\n\n", "\"\\n\\n\"");
    /* 最後の内容の行に行末記号がない文書: clip の末尾の改行は 0 個でなければならない */
    check_format("k: |\n  v", "|", "a\n", "\"a\\n\"");
    /* 内容の行がなく、ヘッダの行の指示子の後ろに範囲の外の空白やコメントがある */
    check_format("k: | # c\nn: 1\n", "|", "a\n", "\"a\\n\"");
    check_format("k: |  \nn: 1\n", "|", "a\n", "\"a\\n\"");
}

/*
 * ヘッダの行のコメントは範囲の中にあるため、維持するときはヘッダとともに原文のまま出し、
 * ダブルクオートに切り替えるときは値の後ろに残す（5.8）
 */
NAOS_TEST(format_block_scalar_header_comment)
{
    check_format("k: | # c\n  v\nn: 1\n", "|", "a\n", "| # c\n  a");
    check_format("k: >- \t# c: d\n  v\nn: 1\n", ">", "a\nb", ">- \t# c: d\n  a\n\n  b");
    check_format("k: | # c\n  v\nn: 1\n", "|", "a", "\"a\" # c");
    check_format("k: |  \n  v\nn: 1\n", "|", "a\n", "|  \n  a");
    check_format("k: |  \n  v\nn: 1\n", "|", "a", "\"a\"");
}

/*
 * インデント指示子はヘッダとともに維持し、内容の行を content_indent（親のインデント + 指示子）で始める。
 * 指示子があれば、最初の空でない行が空白で始まってもリテラルを維持できる（5.3）
 */
NAOS_TEST(format_block_scalar_indent_indicator_kept)
{
    check_format("k: |2\n    v\nn: 1\n", "|", " a\nb\n", "|2\n   a\n  b");
    check_format("k: |1-\n  v\nn: 1\n", "|", " a", "|1-\n  a");
    check_format("k: |-1\n v\nn: 1\n", "|", "\n\n  a", "|-1\n\n\n   a");
    check_format("k: >2+\n  v\n\nn: 1\n", ">", "a\nb\n\n", ">2+\n  a\n\n  b\n");
    check_format("- |2\n    v\n- w\n", "|", " a\n", "|2\n   a");
    check_format("a:\n  b: |3\n       v\n  c: 1\n", "|", "  x\n", "|3\n       x");
    /* 指示子があっても、折り返しでは空白で始まる行を維持できない */
    check_format("k: >2\n    v\nn: 1\n", ">", " a\n", "\" a\\n\"");
}

/*
 * 折り返しは訳文の各行を内容の行とし、訳文の改行 n 個の並びを n 個の空行にする（5.3）。
 * 先頭の改行は空行にする。空白で始まる行（より深いインデント）を含む訳文は切り替える（5.4）
 */
NAOS_TEST(format_folded_blank_lines)
{
    check_format("k: >\n  v\nn: 1\n", ">", "a b\n", ">\n  a b");
    check_format("k: >\n  v\nn: 1\n", ">", "a\nb\n", ">\n  a\n\n  b");
    check_format("k: >\n  v\nn: 1\n", ">", "a\n\nb\n", ">\n  a\n\n\n  b");
    check_format("k: >\n  v\nn: 1\n", ">", "a\n\n\nb\nc\n", ">\n  a\n\n\n\n  b\n\n  c");
    check_format("k: >\n  v\nn: 1\n", ">", "\na\n", ">\n\n  a");
    check_format("k: >\n  v\nn: 1\n", ">", "\n\na\nb\n", ">\n\n\n  a\n\n  b");
    check_format("k: >-\n  v\nn: 1\n", ">", "a\nb", ">-\n  a\n\n  b");
    check_format("k: >+\n  v\n\nn: 1\n", ">", "a\nb\n\n", ">+\n  a\n\n  b\n");
    check_format("k: >\n  v\nn: 1\n", ">", "a  \n", ">\n  a  ");
    check_format("k: >\n  v\nn: 1\n", ">", "a\n b\n", "\"a\\n b\\n\"");
    check_format("k: >\n  v\nn: 1\n", ">", "a\n\tb\n", "\"a\\n\\tb\\n\"");
    check_format("k: >\n  v\nn: 1\n", ">", " a\n", "\" a\\n\"");
}

/* CRLF の文書では、複数行の出力の行末記号も CRLF（5.6） */
NAOS_TEST(format_block_scalar_crlf)
{
    check_format("k: |\r\n  v\r\nn: 1\r\n", "|", "a\nb\n", "|\r\n  a\r\n  b");
    check_format("k: |\r\n  v\r\nn: 1\r\n", "|", "\na\n\nb\n", "|\r\n\r\n  a\r\n\r\n  b");
    check_format("k: |+\r\n  v\r\n\r\nn: 1\r\n", "|", "a\n\n", "|+\r\n  a\r\n");
    check_format("k: >\r\n  v\r\nn: 1\r\n", ">", "a\n\nb\n", ">\r\n  a\r\n\r\n\r\n  b");
    check_format("k: | # c\r\n  v\r\nn: 1\r\n", "|", "a\n", "| # c\r\n  a");
    check_format("k: | # c\r\n  v\r\nn: 1\r\n", "|", "a", "\"a\" # c");
    check_format("- >-\r\n  v\r\n- w\r\n", ">", "a\nb", ">-\r\n  a\r\n\r\n  b");
    /* LF の方が多い文書では LF（文書の行末記号） */
    check_format("k: |\n  v\r\nn: 1\n", "|", "a\nb\n", "|\n  a\n  b");
}

/* 入れ子の文脈（マッピングの値、シーケンスの要素、入れ子のシーケンスの中のマッピング） */
NAOS_TEST(format_block_scalar_nested)
{
    check_format("- |\n  v\n- w\n", "|", "a\nb\n", "|\n  a\n  b");
    check_format("a:\n  b: |-\n    v\n  c: 1\n", "|", "x\ny", "|-\n    x\n    y");
    check_format("a:\n  - >\n    v\n  - w\n", ">", "x\ny\n", ">\n    x\n\n    y");
    check_format("a:\n  b:\n    - c: |+\n        v\n\n      d: w\n", "|", "x\n\n", "|+\n        x\n");
    check_format("- - |\n    v\n  - w\n", "|", "x\n", "|\n    x");
}

/*
 * 文脈と訳文の組み合わせ: 各文脈の needle の位置のスカラーを各訳文で整形して置き換え、
 * 解析し直すと論理値が訳文と一致する（5.1、5.8）
 */
NAOS_TEST(format_roundtrip_matrix)
{
    /* { 文脈, スカラーの開始位置の目印（空の範囲では範囲の位置にある文字） } */
    static const char *const contexts[][2] = {
        { "k: v\n", "v" },
        { "k: 'v'\n", "'v'" },
        { "k: \"v\"\n", "\"v\"" },
        { "k: v # c\nn: 1\n", "v" },
        { "v: 1\n", "v" },
        { "- v\n- w\n", "v" },
        { "k:\n  - v\n  - w\n", "v" },
        { "a:\n  b:\n    - c: v\n      d: w\n", "v" },
        { "k: [v, w]\n", "v" },
        { "k: [w, v]\n", "v]" },
        { "k: ['v', w]\n", "'v'" },
        { "k: [\"v\"]\n", "\"v\"" },
        { "k: {a: v, b: w}\n", "v," },
        { "k: {v: 1}\n", "v:" },
        { "k: [[v]]\n", "v" },
        { "k:\nn: 1\n", "\nn" },
        { "-\n", "\n" },
        { "k: {a}\n", "}" },
        { "k: {a: }\n", " }" },
        { "v\n", "v" },
        { "k: |\n  v\n", "|" },
        { "k: >-\n  v\nn: 1\n", ">" },
        { "k: v\r\nn: 1\r\n", "v" },
        { "k: {a: 'v'}\n", "'v'" },
        { "k: |-\n  v\nn: 1\n", "|" },
        { "k: |+\n  v\n\nn: 1\n", "|" },
        { "k: |\n  v\n\n\nn: 1\n", "|" },
        { "k: >\n  v\n", ">" },
        { "k: >+\n  v\n\n", ">" },
        { "k: |2\n   v\nn: 1\n", "|" },
        { "k: >1-\n v\nn: 1\n", ">" },
        { "k: | # c\n  v\nn: 1\n", "|" },
        { "k: >+2 # c\n  v\n\nn: 1\n", ">" },
        { "k: |\nn: 1\n", "|" },
        { "k: | # c\nn: 1\n", "|" },
        { "k: |+\n\n\nn: 1\n", "|" },
        { "k: |\n  v", "|" },
        { "k: |+\n  v", "|" },
        { "k: |\r\n  v\r\nn: 1\r\n", "|" },
        { "k: >+\r\n  v\r\n\r\n", ">" },
        { "k: |-2 # c\r\n   v\r\n\r\nn: 1\r\n", "|" },
        { "- |\n  v\n- w\n", "|" },
        { "- >-\n  v\n", ">" },
        { "- - |+\n    v\n\n  - w\n", "|" },
        { "a:\n  b: |\n    v\n  c: 1\n", "|" },
        { "a:\n  - |+\n    v\n\n  - w\n", "|" },
        { "a:\n  b:\n    - c: >2\n        v\n      d: w\n", ">" },
        { "a:\r\n  - >\r\n    v\r\n  - w\r\n", ">" }
    };
    static const char *const texts[] = {
        "plain", "two words", "a: b", "a #b", "a#b", "a:b", "#x", "-x", "- x", "?x", "x?", ":x",
        "a\nb", "a\n\nb", "\n", "end\n", " lead", "trail ", "\ttab", "a\tb", "a\t#b", "a:\tb", "trail:",
        "true", "False", "null", "~", "2024", "-12", "+1", "1.5", ".5", "1e3", "0x1F", "0o17", ".inf",
        "-.Inf", ".NaN", "---", "...", "... x", "it's", "'q'", "\"dq\"", "back\\slash", "a, b", "[x]",
        "{y}", "x]", "}", "caf\xc3\xa9", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "", " ", "`x", "@x", "%x",
        "&a", "*a", "!t", "|", ">", "\x01" "ctl", "del\x7f", "nel\xc2\x85", "\xc2\xa0" "nbsp", "a\r\nb", "\r",
        "x # y: z", "a\\nb", "x\"y'z", "a  b", "%", "a: ", "- ", "?", "<<", "a,b", "a:b:c", "http://x.y/z?a=b#c",
        "\t", "a\tb\tc", "\xe2\x80\xa8" "sep", "\xef\xbb\xbf" "bom",
        "yes", "No", "y", "2024-01-01", "2024-01-02T03:04:05Z", "1_000", "0b101", "012", "1:30", "1:30.5", "=",
        "What?", "a?b", "a\xe2\x80\xa9" "b", "1.2.3", ".",
        "a\n", "a\nb\n", "a\n\n", "a\n\n\n", "\na\n", "\n\na", "a\n\nb\n", "a\n\n\nb\nc\n", " a\n", "  a",
        "a\n b\n", "a\n\tb\n", "a\n \nb\n", "a\r\n", "\n\n", "a \n", "- a\n# b\n", "---\n...\n", "a: |\n  b\n",
        "#\n", "\xe6\x97\xa5\n\xe6\x9c\xac\n"
    };
    size_t c;
    size_t t;
    const char *yaml = NULL;

    for (c = 0; c < (sizeof(contexts) / sizeof(contexts[0])); ++c)
    {
        yaml = contexts[c][0];
        for (t = 0; t < (sizeof(texts) / sizeof(texts[0])); ++t)
        {
            NAOS_TRACE_PUSH("context=%u text=%u yaml=[%s] text=[%s]", (unsigned)c, (unsigned)t, yaml, texts[t]);
            (void)format_roundtrip(yaml, format_offset(yaml, contexts[c][1], 0), texts[t], strlen(texts[t]),
                                   NULL);
            NAOS_TRACE_POP();
        }
    }
}

NAOS_SUITE(format,
    NAOS_ADD_TEST(format_scaffold_styles_and_chomps),
    NAOS_ADD_TEST(format_plain_kept),
    NAOS_ADD_TEST(format_plain_switches_to_double_quoted),
    NAOS_ADD_TEST(format_plain_leading_indicators_switch),
    NAOS_ADD_TEST(format_plain_core_schema_forms_switch),
    NAOS_ADD_TEST(format_plain_yaml11_forms_switch),
    NAOS_ADD_TEST(format_plain_yaml11_lookalikes_kept),
    NAOS_ADD_TEST(format_yaml11_characters_switch),
    NAOS_ADD_TEST(format_single_quoted),
    NAOS_ADD_TEST(format_double_quoted_escapes),
    NAOS_ADD_TEST(format_flow_context_symbols),
    NAOS_ADD_TEST(format_empty_value_gets_leading_space),
    NAOS_ADD_TEST(format_mapping_keys),
    NAOS_ADD_TEST(format_appends_to_out),
    NAOS_ADD_TEST(format_nul_and_invalid_utf8_errors),
    NAOS_ADD_TEST(format_block_scalar_roundtrips),
    NAOS_ADD_TEST(format_literal_kept_each_chomp),
    NAOS_ADD_TEST(format_block_scalar_switches_to_double_quoted),
    NAOS_ADD_TEST(format_block_scalar_header_comment),
    NAOS_ADD_TEST(format_block_scalar_indent_indicator_kept),
    NAOS_ADD_TEST(format_folded_blank_lines),
    NAOS_ADD_TEST(format_block_scalar_crlf),
    NAOS_ADD_TEST(format_block_scalar_nested),
    NAOS_ADD_TEST(format_roundtrip_matrix)
)
