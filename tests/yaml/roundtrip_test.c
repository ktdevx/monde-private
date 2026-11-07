/* スイート roundtrip: 代表的なフロントマターによる往復（5.1、5.8、8.1、8.2、8.3） */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/source.h"
#include "tree_check.h"
#include "yaml/yaml.h"
#include "yaml/yaml_format.h"
#include "yaml/yaml_front_matter.h"
#include "yaml/yaml_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* 雛形: 範囲が指す元のバイト列を取り出して並べると原文に戻る（往復の検証の基本の操作） */
NAOS_TEST(roundtrip_scaffold_span_text_concatenation)
{
    static const char data[] = "---\ntitle: x\n---\nbody\n";
    size_t len = sizeof(data) - 1;
    mnd_span_t head = { 0, 4 };
    mnd_span_t rest = { 4, len };
    mnd_slice_t a;
    mnd_slice_t b;

    NAOS_REQUIRE_TRUE(mnd_span_text(data, len, head, &a));
    NAOS_REQUIRE_TRUE(mnd_span_text(data, len, rest, &b));
    NAOS_REQUIRE_UINT_EQ(len, a.len + b.len);
    NAOS_CHECK_MEM_EQ(data, a.len, a.data, a.len);
    NAOS_CHECK_MEM_EQ(data + a.len, b.len, b.data, b.len);
}

/*
 * テストデータ: Jekyll、Hugo、Astro を模したフロントマターと Markdown の本文（内容はすべて架空）。
 * 全スタイルのスカラー、入れ子、フロー形式、コメント、空の値、CRLF の文書、`...` の閉じ、
 * 行末記号のない閉じの行を含む
 */

typedef struct roundtrip_doc
{
    const char *name;
    const char *data;
    size_t len;
} roundtrip_doc_t;

#define ROUNDTRIP_DOC(name, text) { name, text, sizeof(text) - 1 }

static const roundtrip_doc_t roundtrip_docs[] = {
    ROUNDTRIP_DOC("jekyll_post",
        "---\n"
        "layout: post\n"
        "title: \"Welcome to the \\\"Garden\\\" blog\"\n"
        "date: 2025-03-14 09:30:00 +0900\n"
        "categories: [gardening, notes]\n"
        "tags:\n"
        "  - spring\n"
        "  - 'seed starting'\n"
        "author:\n"
        "  name: Hanako Example\n"
        "  url: https://example.invalid/hanako\n"
        "excerpt: >\n"
        "  A short note about\n"
        "  starting seeds indoors.\n"
        "\n"
        "  Second paragraph.\n"
        "permalink: /blog/:year/:title/\n"
        "published: true\n"
        "comments: # left empty on purpose\n"
        "summary: Plain text that\n"
        "  continues on the next line\n"
        "---\n"
        "# Welcome\n"
        "\n"
        "Body text with --- inside and a: colon.\n"),
    ROUNDTRIP_DOC("hugo_page",
        "---\n"
        "title: 'Release notes: v2'\n"
        "draft: false\n"
        "weight: 10\n"
        "description: |\n"
        "  First line of the description.\n"
        "    An indented line.\n"
        "  Last line.\n"
        "menu:\n"
        "  main:\n"
        "    parent: docs\n"
        "    weight: 20\n"
        "params:\n"
        "  hero: {image: /img/hero.png, alt: \"A hill at dawn\", sizes: [480, 960]}\n"
        "  authors:\n"
        "    - name: Taro\n"
        "      role: writer\n"
        "    - name: 'Jiro'\n"
        "      role: editor\n"
        "aliases: [\"/old/path\", /older/path]\n"
        "outputs:\n"
        "- html\n"
        "- rss\n"
        "notes: |-\n"
        "  Kept without a trailing newline\n"
        "keep: |+\n"
        "  Trailing blank lines are kept\n"
        "\n"
        "---\n"
        "Body of the page.\n"),
    ROUNDTRIP_DOC("astro_entry",
        "---\n"
        "# Astro content collection entry\n"
        "title: \"Building islands\n"
        "  with partial hydration\"\n"
        "pubDate: 2024-11-02\n"
        "heroImage: '/images/islands.webp'\n"
        "tags: ['astro', \"islands\", ssr]   # every quoting style\n"
        "seo:\n"
        "  title: Islands, explained\n"
        "  'og:description': >-\n"
        "    Folded text that\n"
        "    ends without a newline.\n"
        "  \"twitter:card\": summary_large_image\n"
        "code: |2\n"
        "    indented by indicator\n"
        "  back to two\n"
        "quote: 'It''s a\n"
        "  multi-line single quote'\n"
        "empty_value:\n"
        "list_in_flow: {a: [1, 2, {b: c}], d: e,}\n"
        "---\n"
        "<Layout title=\"x\" />\n"),
    ROUNDTRIP_DOC("jekyll_crlf",
        "---\r\n"
        "title: CRLF post\r\n"
        "description: |\r\n"
        "  line one\r\n"
        "  line two\r\n"
        "tags: [a, 'b c']\r\n"
        "nested:\r\n"
        "  - x: \"y\\tz\"\r\n"
        "    z: 'w'\r\n"
        "  - plain item\r\n"
        "folded: >\r\n"
        "  one\r\n"
        "  two\r\n"
        "\r\n"
        "  three\r\n"
        "keep: |+\r\n"
        "  kept\r\n"
        "\r\n"
        "plain: multi\r\n"
        "  line plain\r\n"
        "---\r\n"
        "Body\r\n"),
    ROUNDTRIP_DOC("hugo_dots",
        "---\n"
        "\"title\": Taxonomy overview   # quoted key\n"
        "series:\n"
        "  - name: Basics\n"
        "    parts: [1, 2, 3]\n"
        "    meta: {level: beginner}\n"
        "  - name: 'Advanced'\n"
        "    parts: []\n"
        "    meta: {}\n"
        "resources:\n"
        "  - src: images/cover.jpg\n"
        "    title: \"Cover \\u00e9 #1\"\n"
        "    params:\n"
        "      credits: >+\n"
        "        Photo by nobody\n"
        "\n"
        "summary: >1\n"
        "  leading space kept\n"
        " back\n"
        "...\n"
        "## Overview\n"),
    ROUNDTRIP_DOC("astro_no_body",
        "---\n"
        "layout: ../../layouts/Doc.astro\n"
        "title: \"\xe6\x97\xa5\xe6\x9c\xac\" \n"
        "sidebar: {label: Start, order: 1, badge: {text: New, variant: tip}}\n"
        "flow_keys: {k}\n"
        "items:\n"
        "  -\n"
        "  - 'last'\n"
        "note: |-2\n"
        "    two extra spaces\n"
        "---"),
};

#define ROUNDTRIP_DOC_COUNT (sizeof(roundtrip_docs) / sizeof(roundtrip_docs[0]))

/*
 * 訳文の集合: ASCII、記号、引用符、改行、先頭と末尾の空白、Core Schema と YAML 1.1 の非文字列の形、
 * フロー形式の中の ?、U+2028、日本語。
 * 日本語は UTF-8 のバイト列を \x で書く（日本語のタイトル / 見出し：説明\n二行目です / 「引用」と、句読点。）
 */
typedef struct roundtrip_text
{
    const char *text;
    size_t len;
} roundtrip_text_t;

#define ROUNDTRIP_TEXT(text) { text, sizeof(text) - 1 }

static const roundtrip_text_t roundtrip_texts[] = {
    ROUNDTRIP_TEXT("Hello world"),
    ROUNDTRIP_TEXT("key: value"),
    ROUNDTRIP_TEXT("C# and F#"),
    ROUNDTRIP_TEXT("note #1 # not a comment"),
    ROUNDTRIP_TEXT("- dash first"),
    ROUNDTRIP_TEXT("[bracketed]"),
    ROUNDTRIP_TEXT("a ] b [ c"),
    ROUNDTRIP_TEXT("one, two, three"),
    ROUNDTRIP_TEXT("say \"hi\""),
    ROUNDTRIP_TEXT("it's"),
    ROUNDTRIP_TEXT("'quoted'"),
    ROUNDTRIP_TEXT("mix: # - [ ] , \" '"),
    ROUNDTRIP_TEXT("back\\slash"),
    ROUNDTRIP_TEXT("line one\nline two"),
    ROUNDTRIP_TEXT("line one\nline two\n"),
    ROUNDTRIP_TEXT("para one\n\npara two\n"),
    ROUNDTRIP_TEXT("ends with two\n\n"),
    ROUNDTRIP_TEXT("  leading spaces"),
    ROUNDTRIP_TEXT("trailing spaces  "),
    ROUNDTRIP_TEXT(" both "),
    ROUNDTRIP_TEXT("tab\tinside"),
    ROUNDTRIP_TEXT("true"),
    ROUNDTRIP_TEXT("123"),
    ROUNDTRIP_TEXT("null"),
    ROUNDTRIP_TEXT("yes"),
    ROUNDTRIP_TEXT("No"),
    ROUNDTRIP_TEXT("2024-01-01"),
    ROUNDTRIP_TEXT("2024-01-02T03:04:05Z"),
    ROUNDTRIP_TEXT("1_000"),
    ROUNDTRIP_TEXT("1:30"),
    ROUNDTRIP_TEXT("<<"),
    ROUNDTRIP_TEXT("What?"),
    ROUNDTRIP_TEXT("line\xe2\x80\xa8sep"),
    ROUNDTRIP_TEXT("line\xe2\x80\xa8sep\n"),
    ROUNDTRIP_TEXT("para\xe2\x80\xa9one\nline two\n"),
    ROUNDTRIP_TEXT("---"),
    ROUNDTRIP_TEXT(""),
    ROUNDTRIP_TEXT("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x82\xbf\xe3\x82\xa4\xe3\x83\x88\xe3\x83\xab"),
    ROUNDTRIP_TEXT("\xe8\xa6\x8b\xe5\x87\xba\xe3\x81\x97\xef\xbc\x9a\xe8\xaa\xac\xe6\x98\x8e\n"
                   "\xe4\xba\x8c\xe8\xa1\x8c\xe7\x9b\xae\xe3\x81\xa7\xe3\x81\x99"),
    ROUNDTRIP_TEXT("\xe3\x80\x8c\xe5\xbc\x95\xe7\x94\xa8\xe3\x80\x8d\xe3\x81\xa8\xe3\x80\x81"
                   "\xe5\x8f\xa5\xe8\xaa\xad\xe7\x82\xb9\xe3\x80\x82\n"),
};

#define ROUNDTRIP_TEXT_COUNT (sizeof(roundtrip_texts) / sizeof(roundtrip_texts[0]))

/* 根からノードまでの道筋（解析し直した文書で同じ位置のノードを探すため） */

#define ROUNDTRIP_PATH_MAX 32
#define ROUNDTRIP_SCALAR_MAX 256

typedef enum roundtrip_step_kind
{
    ROUNDTRIP_STEP_KEY,    /* マッピングの index 番目の組のキー */
    ROUNDTRIP_STEP_VALUE,  /* マッピングの index 番目の組の値 */
    ROUNDTRIP_STEP_ITEM    /* シーケンスの index 番目の要素 */
} roundtrip_step_kind_t;

typedef struct roundtrip_step
{
    roundtrip_step_kind_t kind;
    size_t index;
} roundtrip_step_t;

typedef struct roundtrip_path
{
    roundtrip_step_t steps[ROUNDTRIP_PATH_MAX];
    size_t count;
} roundtrip_path_t;

/* 根から node までの道筋を *path に入れる。深すぎるか親子の関係が壊れていれば false */
static bool roundtrip_path_of(const mnd_yaml_node_t *node, roundtrip_path_t *path)
{
    const mnd_yaml_node_t *child = node;
    const mnd_yaml_node_t *parent = NULL;
    roundtrip_step_t reversed[ROUNDTRIP_PATH_MAX];
    size_t count = 0;
    size_t i;
    bool found;

    while (child->parent != NULL)
    {
        parent = child->parent;
        found = false;
        if (count >= ROUNDTRIP_PATH_MAX)
        {
            return false;
        }
        if (parent->kind == MND_YAML_MAPPING)
        {
            for (i = 0; (i < parent->as.mapping.count) && !found; ++i)
            {
                if (parent->as.mapping.pairs[i].key == child)
                {
                    reversed[count].kind = ROUNDTRIP_STEP_KEY;
                    reversed[count].index = i;
                    found = true;
                }
                else if (parent->as.mapping.pairs[i].value == child)
                {
                    reversed[count].kind = ROUNDTRIP_STEP_VALUE;
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
                    reversed[count].kind = ROUNDTRIP_STEP_ITEM;
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

/* 2 つのノードの根からの道筋が等しければ true */
static bool roundtrip_same_path(const mnd_yaml_node_t *a, const mnd_yaml_node_t *b)
{
    roundtrip_path_t pa;
    roundtrip_path_t pb;
    size_t i;

    if (!roundtrip_path_of(a, &pa) || !roundtrip_path_of(b, &pb) || (pa.count != pb.count))
    {
        return false;
    }
    for (i = 0; i < pa.count; ++i)
    {
        if ((pa.steps[i].kind != pb.steps[i].kind) || (pa.steps[i].index != pb.steps[i].index))
        {
            return false;
        }
    }

    return true;
}

/*
 * doc の全スカラー（キーを含む）を原文の順（span.start の昇順）に scalars に入れて数を返す。
 * 再帰を使わない。ROUNDTRIP_SCALAR_MAX を超えたら NAOS の失敗にする
 */
static size_t roundtrip_collect(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t **scalars)
{
    const mnd_yaml_node_t *stack[ROUNDTRIP_SCALAR_MAX];
    const mnd_yaml_node_t *node = NULL;
    const mnd_yaml_node_t *tmp = NULL;
    size_t depth = 0;
    size_t count = 0;
    size_t i;
    size_t j;

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
            NAOS_REQUIRE_TRUE(count < ROUNDTRIP_SCALAR_MAX);
            scalars[count++] = node;
        }
        else if (node->kind == MND_YAML_MAPPING)
        {
            for (i = 0; i < node->as.mapping.count; ++i)
            {
                NAOS_REQUIRE_TRUE((depth + 2) <= ROUNDTRIP_SCALAR_MAX);
                stack[depth++] = node->as.mapping.pairs[i].key;
                stack[depth++] = node->as.mapping.pairs[i].value;
            }
        }
        else
        {
            for (i = 0; i < node->as.sequence.count; ++i)
            {
                NAOS_REQUIRE_TRUE(depth < ROUNDTRIP_SCALAR_MAX);
                stack[depth++] = node->as.sequence.items[i];
            }
        }
    }
    /* 挿入ソート（範囲は重ならないので span.start で全順序になる） */
    for (i = 1; i < count; ++i)
    {
        tmp = scalars[i];
        j = i;
        while ((j > 0) && (scalars[j - 1]->span.start > tmp->span.start))
        {
            scalars[j] = scalars[j - 1];
            --j;
        }
        scalars[j] = tmp;
    }

    return count;
}

/* node がマッピングのキーなら true */
static bool roundtrip_is_key(const mnd_yaml_node_t *node)
{
    const mnd_yaml_node_t *parent = node->parent;
    size_t i;

    if ((parent == NULL) || (parent->kind != MND_YAML_MAPPING))
    {
        return false;
    }
    for (i = 0; i < parent->as.mapping.count; ++i)
    {
        if (parent->as.mapping.pairs[i].key == node)
        {
            return true;
        }
    }

    return false;
}

/* 原文の解析の結果（検出の結果、文書、原文の順のスカラー） */
typedef struct roundtrip_parsed
{
    mnd_yaml_front_matter_t fm;
    mnd_yaml_doc_t *doc;
    const mnd_yaml_node_t *scalars[ROUNDTRIP_SCALAR_MAX];
    size_t count;
} roundtrip_parsed_t;

/*
 * data[0..len) を検出し、フロントマターの内容を解析する。成功し、診断が増えず、木の検査を満たせば true。
 * 失敗したら out->doc は NULL
 */
static bool roundtrip_load(const char *data, size_t len, roundtrip_parsed_t *out)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    out->doc = NULL;
    out->count = 0;
    ok = mnd_yaml_front_matter_detect(data, len, "a.md", &out->fm, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_TRUE(out->fm.present);
    ok = ok && out->fm.present;
    if (ok)
    {
        ok = mnd_yaml_parse(data, len, out->fm.content, NULL, &out->doc, &diags);
        NAOS_CHECK_TRUE(ok);
    }
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    if (mnd_diag_list_count(&diags) > 0)
    {
        NAOS_NOTE("error: line %u: %s", (unsigned)mnd_diag_list_at(&diags, 0)->line,
                  mnd_diag_list_at(&diags, 0)->message);
    }
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        mnd_yaml_doc_free(out->doc);
        out->doc = NULL;
        return false;
    }
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(out->doc));
    out->count = roundtrip_collect(out->doc, out->scalars);

    return true;
}

/* 1 つの置き換え: 原文の範囲 span を bytes[0..len) にする */
typedef struct roundtrip_edit
{
    mnd_span_t span;
    const char *bytes;
    size_t len;
} roundtrip_edit_t;

/*
 * data[0..len) の edits（原文の順で重ならない）を、オフセットの降順に置き換えた結果を out に入れる
 * （translation-units が後ろから置き換える手順と同じ。前の範囲のオフセットがずれない）
 */
static void roundtrip_apply(const char *data, size_t len, const roundtrip_edit_t *edits, size_t count,
                            mnd_buf_t *out)
{
    mnd_buf_t next = MND_BUF_INIT;
    size_t i;

    mnd_buf_clear(out);
    NAOS_REQUIRE_TRUE(mnd_buf_append(out, data, len));
    for (i = count; i > 0; --i)
    {
        const roundtrip_edit_t *e = &edits[i - 1];

        NAOS_REQUIRE_TRUE((e->span.start <= e->span.end) && (e->span.end <= out->len));
        if (i < count)
        {
            NAOS_REQUIRE_TRUE(e->span.end <= edits[i].span.start);
        }
        mnd_buf_clear(&next);
        NAOS_REQUIRE_TRUE(mnd_buf_append(&next, out->data, e->span.start)
                          && mnd_buf_append(&next, e->bytes, e->len)
                          && mnd_buf_append(&next, out->data + e->span.end, out->len - e->span.end));
        mnd_buf_clear(out);
        NAOS_REQUIRE_TRUE(mnd_buf_append(out, next.data, next.len));
    }
    mnd_buf_free(&next);
}

/*
 * edits[0..count) を適用した後の原文のオフセット pos（どの範囲の内側でもない位置）の新しい位置。
 * pos より前（範囲の終わりが pos 以下）の置き換えの長さの差を足す。
 * 置き換えた範囲自身の新しい開始位置は、count をその置き換えの番号にして求める
 */
static size_t roundtrip_shift(const roundtrip_edit_t *edits, size_t count, size_t pos)
{
    size_t result = pos;
    size_t i;

    for (i = 0; i < count; ++i)
    {
        if (edits[i].span.end <= pos)
        {
            result = (result - (edits[i].span.end - edits[i].span.start)) + edits[i].len;
        }
    }

    return result;
}

/*
 * 範囲の終わり pos の新しい位置。roundtrip_shift と同じだが、pos にある空の置き換え（pos で終わる範囲の
 * 後ろに挿入される）は数えない
 */
static size_t roundtrip_shift_end(const roundtrip_edit_t *edits, size_t count, size_t pos)
{
    size_t result = pos;
    size_t i;

    for (i = 0; i < count; ++i)
    {
        if ((edits[i].span.end <= pos) && (edits[i].span.start < pos))
        {
            result = (result - (edits[i].span.end - edits[i].span.start)) + edits[i].len;
        }
    }

    return result;
}

/*
 * 原文 base と、edits を適用した結果 data[0..len) を比べる。
 *  - 検出し直すとフロントマターが同じ位置で見つかり、内容の終わりと本文の開始位置が置き換えの分だけずれ、
 *    本文が原文と一致する
 *  - 置き換えた範囲の外側のバイト列が原文と一致する
 *  - 解析し直すと診断なしで成功し、木の検査を満たし、スカラーの数・道筋が一致する
 *  - 置き換えたスカラー（expected[i] が NULL でない）の論理値が expected[i] と一致し、
 *    置き換えていないスカラーの論理値・スタイル・範囲（ずれを除く）が原文と一致する
 * kept_style が NULL でなければ、置き換えたスカラーごとに元のスタイルが保たれたかを入れる。
 * すべて満たせば true
 */
static bool roundtrip_verify(const char *base_data, size_t base_len, const roundtrip_parsed_t *base,
                             const roundtrip_edit_t *edits, size_t edit_count,
                             const roundtrip_text_t *const *expected, const char *data, size_t len,
                             bool *kept_style)
{
    roundtrip_parsed_t again_storage;
    roundtrip_parsed_t *again = &again_storage;
    size_t prev_end = 0;
    size_t new_pos = 0;
    size_t gap;
    size_t i;
    size_t e = 0;
    size_t new_start;
    bool ok = true;

    /* 範囲の外側（置き換えの間のすき間）が原文と一致する */
    for (i = 0; i <= edit_count; ++i)
    {
        size_t gap_end = (i < edit_count) ? edits[i].span.start : base_len;

        gap = gap_end - prev_end;
        NAOS_CHECK_TRUE((new_pos + gap) <= len);
        if ((new_pos + gap) > len)
        {
            return false;
        }
        NAOS_CHECK_MEM_EQ(base_data + prev_end, gap, data + new_pos, gap);
        ok = ok && (memcmp(base_data + prev_end, data + new_pos, gap) == 0);
        new_pos += gap;
        if (i < edit_count)
        {
            new_pos += edits[i].len;
            prev_end = edits[i].span.end;
        }
    }
    NAOS_CHECK_UINT_EQ(len, new_pos);
    ok = ok && (len == new_pos);

    if (!roundtrip_load(data, len, again))
    {
        NAOS_NOTE("replaced: %s", data);
        return false;
    }
    /* 検出し直したフロントマターが同じ位置にあり、本文が変わらない */
    NAOS_CHECK_UINT_EQ(0, again->fm.span.start);
    NAOS_CHECK_UINT_EQ(base->fm.content.start, again->fm.content.start);
    NAOS_CHECK_UINT_EQ(roundtrip_shift(edits, edit_count, base->fm.content.end), again->fm.content.end);
    NAOS_CHECK_UINT_EQ(roundtrip_shift_end(edits, edit_count, base->fm.span.end), again->fm.span.end);
    NAOS_CHECK_UINT_EQ(roundtrip_shift(edits, edit_count, base->fm.body_start), again->fm.body_start);
    ok = ok && (again->fm.span.start == 0) && (again->fm.content.start == base->fm.content.start)
         && (again->fm.content.end == roundtrip_shift(edits, edit_count, base->fm.content.end))
         && (again->fm.body_start == roundtrip_shift(edits, edit_count, base->fm.body_start));
    if (ok)
    {
        NAOS_CHECK_MEM_EQ(base_data + base->fm.body_start, base_len - base->fm.body_start,
                          data + again->fm.body_start, len - again->fm.body_start);
        ok = ((base_len - base->fm.body_start) == (len - again->fm.body_start))
             && (memcmp(base_data + base->fm.body_start, data + again->fm.body_start,
                        base_len - base->fm.body_start) == 0);
    }
    NAOS_CHECK_INT_EQ(mnd_yaml_doc_eol(base->doc), mnd_yaml_doc_eol(again->doc));

    /* スカラーの対応 */
    NAOS_CHECK_UINT_EQ(base->count, again->count);
    ok = ok && (base->count == again->count);
    for (i = 0; ok && (i < base->count); ++i)
    {
        const mnd_yaml_node_t *before = base->scalars[i];
        const mnd_yaml_node_t *after = again->scalars[i];
        bool replaced = (e < edit_count) && (edits[e].span.start == before->span.start)
                        && (edits[e].span.end == before->span.end);

        NAOS_CHECK_TRUE(roundtrip_same_path(before, after));
        ok = roundtrip_same_path(before, after);
        if (!ok)
        {
            break;
        }
        if (replaced && (expected[e] != NULL))
        {
            const roundtrip_text_t *t = expected[e];

            NAOS_CHECK_MEM_EQ(t->text, t->len, after->as.scalar.value, after->as.scalar.value_len);
            ok = (t->len == after->as.scalar.value_len) && (memcmp(t->text, after->as.scalar.value, t->len) == 0);
            /*
             * 置き換えた範囲の新しい位置は、それより前の置き換えだけでずれる。解析し直した範囲は出力の終わりで
             * 終わり、元の範囲が空でなければ出力の先頭から始まる（空の範囲では先頭に付けた区切りの後ろから）
             */
            new_start = roundtrip_shift(edits, e, before->span.start);
            NAOS_CHECK_UINT_EQ(new_start + edits[e].len, after->span.end);
            if (before->span.start < before->span.end)
            {
                NAOS_CHECK_UINT_EQ(new_start, after->span.start);
                ok = ok && (after->span.start == new_start);
            }
            else
            {
                NAOS_CHECK_TRUE((after->span.start >= new_start) && (after->span.start <= after->span.end));
                ok = ok && (after->span.start >= new_start) && (after->span.start <= after->span.end);
            }
            ok = ok && (after->span.end == (new_start + edits[e].len));
            if (kept_style != NULL)
            {
                kept_style[e] = (after->as.scalar.style == before->as.scalar.style);
            }
        }
        else
        {
            NAOS_CHECK_MEM_EQ(before->as.scalar.value, before->as.scalar.value_len, after->as.scalar.value,
                              after->as.scalar.value_len);
            NAOS_CHECK_INT_EQ(before->as.scalar.style, after->as.scalar.style);
            NAOS_CHECK_UINT_EQ(roundtrip_shift(edits, edit_count, before->span.start), after->span.start);
            NAOS_CHECK_UINT_EQ(roundtrip_shift_end(edits, edit_count, before->span.end), after->span.end);
            ok = (before->as.scalar.value_len == after->as.scalar.value_len)
                 && (memcmp(before->as.scalar.value, after->as.scalar.value, before->as.scalar.value_len) == 0)
                 && (before->as.scalar.style == after->as.scalar.style)
                 && (after->span.start == roundtrip_shift(edits, edit_count, before->span.start))
                 && (after->span.end == roundtrip_shift_end(edits, edit_count, before->span.end));
        }
        if (replaced)
        {
            ++e;
        }
    }
    NAOS_CHECK_UINT_EQ(edit_count, e);
    ok = ok && (edit_count == e);
    if (!ok)
    {
        NAOS_NOTE("replaced: %s", data);
    }
    mnd_yaml_doc_free(again->doc);
    again->doc = NULL;

    return ok;
}

/*
 * 原文 data の scalar を text で整形し、出力を *out に入れる。診断なしで成功すれば true
 */
static bool roundtrip_format(const roundtrip_parsed_t *base, const mnd_yaml_node_t *scalar,
                             const roundtrip_text_t *text, mnd_buf_t *out)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    mnd_buf_clear(out);
    ok = mnd_yaml_format_value(base->doc, scalar, text->text, text->len, "a.md", out, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    if (mnd_diag_list_count(&diags) > 0)
    {
        NAOS_NOTE("format error: %s", mnd_diag_list_at(&diags, 0)->message);
    }
    ok = ok && (mnd_diag_list_count(&diags) == 0);
    mnd_diag_list_free(&diags);

    return ok;
}

/* 各テストで読み込んだテストデータ（スカラーの配列が大きいため静的に置く） */
static roundtrip_parsed_t roundtrip_base;

/* テストデータが要求どおりの範囲を網羅し、すべて検出・解析でき、木の検査を満たす（8.1） */
NAOS_TEST(roundtrip_fixtures_cover_all_styles)
{
    size_t styles[5] = { 0, 0, 0, 0, 0 };
    size_t chomps[3] = { 0, 0, 0 };
    size_t flow = 0;
    size_t crlf = 0;
    size_t nested = 0;
    size_t empty = 0;
    size_t indicators = 0;
    size_t comments = 0;
    size_t no_body = 0;
    size_t d;
    size_t i;

    NAOS_CHECK_TRUE(ROUNDTRIP_DOC_COUNT >= 5);
    for (d = 0; d < ROUNDTRIP_DOC_COUNT; ++d)
    {
        const roundtrip_doc_t *doc = &roundtrip_docs[d];

        NAOS_TRACE_PUSH("doc=%s", doc->name);
        NAOS_REQUIRE_TRUE(roundtrip_load(doc->data, doc->len, &roundtrip_base));
        NAOS_CHECK_TRUE(roundtrip_base.count >= 8);
        if (mnd_yaml_doc_eol(roundtrip_base.doc) == MND_EOL_CRLF)
        {
            ++crlf;
        }
        if (roundtrip_base.fm.body_start == doc->len)
        {
            ++no_body;
        }
        if ((strstr(doc->data, " # ") != NULL) || (strstr(doc->data, "\n# ") != NULL))
        {
            ++comments;
        }
        for (i = 0; i < roundtrip_base.count; ++i)
        {
            const mnd_yaml_node_t *s = roundtrip_base.scalars[i];

            ++styles[s->as.scalar.style];
            if ((s->as.scalar.style == MND_YAML_LITERAL) || (s->as.scalar.style == MND_YAML_FOLDED))
            {
                ++chomps[s->as.scalar.chomp];
                if (s->as.scalar.has_indent_indicator)
                {
                    ++indicators;
                }
            }
            if (s->as.scalar.flow)
            {
                ++flow;
            }
            if (s->span.start == s->span.end)
            {
                ++empty;
            }
            if ((s->parent != NULL) && (s->parent->parent != NULL) && (s->parent->parent->parent != NULL))
            {
                ++nested;
            }
        }
        mnd_yaml_doc_free(roundtrip_base.doc);
        roundtrip_base.doc = NULL;
        NAOS_TRACE_POP();
    }
    for (i = 0; i < 5; ++i)
    {
        NAOS_TRACE_PUSH("style=%u", (unsigned)i);
        NAOS_CHECK_TRUE(styles[i] >= 2);
        NAOS_TRACE_POP();
    }
    for (i = 0; i < 3; ++i)
    {
        NAOS_TRACE_PUSH("chomp=%u", (unsigned)i);
        NAOS_CHECK_TRUE(chomps[i] >= 2);
        NAOS_TRACE_POP();
    }
    NAOS_CHECK_TRUE(flow >= 10);
    NAOS_CHECK_TRUE(crlf >= 1);
    NAOS_CHECK_TRUE(nested >= 4);
    NAOS_CHECK_TRUE(empty >= 3);
    NAOS_CHECK_TRUE(indicators >= 3);
    NAOS_CHECK_TRUE(comments >= 3);
    NAOS_CHECK_TRUE(no_body >= 1);
}

/*
 * 整形せずに全スカラーの範囲を元のバイト列で置き換えると、文書全体が原文とバイト単位で一致し、
 * 解析し直した結果も同じになる（8.3）
 */
NAOS_TEST(roundtrip_original_bytes_identical)
{
    static roundtrip_edit_t edits[ROUNDTRIP_SCALAR_MAX];
    const roundtrip_text_t *expected[ROUNDTRIP_SCALAR_MAX];
    mnd_buf_t out = MND_BUF_INIT;
    size_t d;
    size_t i;

    for (d = 0; d < ROUNDTRIP_DOC_COUNT; ++d)
    {
        const roundtrip_doc_t *doc = &roundtrip_docs[d];

        NAOS_TRACE_PUSH("doc=%s", doc->name);
        NAOS_REQUIRE_TRUE(roundtrip_load(doc->data, doc->len, &roundtrip_base));
        for (i = 0; i < roundtrip_base.count; ++i)
        {
            mnd_slice_t text;

            NAOS_REQUIRE_TRUE(mnd_span_text(doc->data, doc->len, roundtrip_base.scalars[i]->span, &text));
            edits[i].span = roundtrip_base.scalars[i]->span;
            edits[i].bytes = text.data;
            edits[i].len = text.len;
            expected[i] = NULL;
        }
        roundtrip_apply(doc->data, doc->len, edits, roundtrip_base.count, &out);
        NAOS_CHECK_MEM_EQ(doc->data, doc->len, out.data, out.len);
        NAOS_CHECK_TRUE(roundtrip_verify(doc->data, doc->len, &roundtrip_base, edits, roundtrip_base.count,
                                         expected, out.data, out.len, NULL));
        mnd_yaml_doc_free(roundtrip_base.doc);
        roundtrip_base.doc = NULL;
        NAOS_TRACE_POP();
    }
    mnd_buf_free(&out);
}

/*
 * 各スカラー（キーを含む）に各訳文を整形して置き換えると、解析し直した論理値が訳文と一致し、
 * 範囲の外側と他のスカラーと本文が原文と同じになる（5.1、5.8、8.2）。
 * 全スタイルで元のスタイルを維持する整形が少なくとも 1 回起きることも確かめる
 */
NAOS_TEST(roundtrip_each_scalar_each_text)
{
    mnd_buf_t formatted = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    size_t kept[5] = { 0, 0, 0, 0, 0 };
    size_t switched = 0;
    size_t total = 0;
    size_t d;
    size_t i;
    size_t t;

    for (d = 0; d < ROUNDTRIP_DOC_COUNT; ++d)
    {
        const roundtrip_doc_t *doc = &roundtrip_docs[d];

        NAOS_REQUIRE_TRUE(roundtrip_load(doc->data, doc->len, &roundtrip_base));
        for (i = 0; i < roundtrip_base.count; ++i)
        {
            const mnd_yaml_node_t *s = roundtrip_base.scalars[i];

            for (t = 0; t < ROUNDTRIP_TEXT_COUNT; ++t)
            {
                const roundtrip_text_t *text = &roundtrip_texts[t];
                const roundtrip_text_t *expected[1];
                roundtrip_edit_t edit;
                bool kept_style = false;
                bool ok;

                NAOS_TRACE_PUSH("doc=%s scalar@%u text#%u=[%s]", doc->name, (unsigned)s->span.start,
                                (unsigned)t, text->text);
                ok = roundtrip_format(&roundtrip_base, s, text, &formatted);
                if (ok)
                {
                    edit.span = s->span;
                    edit.bytes = formatted.data;
                    edit.len = formatted.len;
                    expected[0] = text;
                    roundtrip_apply(doc->data, doc->len, &edit, 1, &out);
                    ok = roundtrip_verify(doc->data, doc->len, &roundtrip_base, &edit, 1, expected, out.data,
                                          out.len, &kept_style);
                    NAOS_CHECK_TRUE(ok);
                    if (!ok)
                    {
                        NAOS_NOTE("formatted: [%s]", mnd_buf_cstr(&formatted));
                    }
                }
                if (ok)
                {
                    ++total;
                    if (kept_style)
                    {
                        ++kept[s->as.scalar.style];
                    }
                    else
                    {
                        ++switched;
                    }
                }
                NAOS_TRACE_POP();
            }
        }
        mnd_yaml_doc_free(roundtrip_base.doc);
        roundtrip_base.doc = NULL;
    }
    NAOS_CHECK_TRUE(total >= 1000);
    NAOS_CHECK_TRUE(switched > 0);
    for (i = 0; i < 5; ++i)
    {
        NAOS_TRACE_PUSH("kept style=%u", (unsigned)i);
        NAOS_CHECK_TRUE(kept[i] > 0);
        NAOS_TRACE_POP();
    }
    mnd_buf_free(&formatted);
    mnd_buf_free(&out);
}

/*
 * 全スカラーを一度に整形し、オフセットの降順に置き換える（translation-units の手順）。
 * 値には訳文の集合を順にずらして割り当て、キーは同じキーにならないよう番号を付けた訳文にする。
 * 解析し直すと全スカラーの論理値が割り当てた訳文と一致し、本文が原文と同じになる（5.1、5.8、8.2）
 */
NAOS_TEST(roundtrip_all_scalars_at_once)
{
    static roundtrip_edit_t edits[ROUNDTRIP_SCALAR_MAX];
    static mnd_buf_t formatted[ROUNDTRIP_SCALAR_MAX];
    static char key_texts[ROUNDTRIP_SCALAR_MAX][128];
    static roundtrip_text_t texts[ROUNDTRIP_SCALAR_MAX];
    const roundtrip_text_t *expected[ROUNDTRIP_SCALAR_MAX];
    mnd_buf_t out = MND_BUF_INIT;
    size_t d;
    size_t i;
    size_t r;

    for (i = 0; i < ROUNDTRIP_SCALAR_MAX; ++i)
    {
        mnd_buf_init(&formatted[i]);
    }
    for (d = 0; d < ROUNDTRIP_DOC_COUNT; ++d)
    {
        const roundtrip_doc_t *doc = &roundtrip_docs[d];

        NAOS_REQUIRE_TRUE(roundtrip_load(doc->data, doc->len, &roundtrip_base));
        for (r = 0; r < ROUNDTRIP_TEXT_COUNT; ++r)
        {
            bool ok = true;

            NAOS_TRACE_PUSH("doc=%s rotation=%u", doc->name, (unsigned)r);
            for (i = 0; ok && (i < roundtrip_base.count); ++i)
            {
                const mnd_yaml_node_t *s = roundtrip_base.scalars[i];
                const roundtrip_text_t *base_text = &roundtrip_texts[(i + r) % ROUNDTRIP_TEXT_COUNT];

                if (roundtrip_is_key(s))
                {
                    /* 1 行の訳文に番号を付けて同じマッピングの中で重ならないようにする */
                    int n = snprintf(key_texts[i], sizeof(key_texts[i]), "%.*s k%u",
                                     (int)((base_text->len < 64) ? base_text->len : 64), base_text->text,
                                     (unsigned)i);

                    NAOS_REQUIRE_TRUE((n > 0) && ((size_t)n < sizeof(key_texts[i])));
                    texts[i].text = key_texts[i];
                    texts[i].len = (size_t)n;
                }
                else
                {
                    texts[i] = *base_text;
                }
                ok = roundtrip_format(&roundtrip_base, s, &texts[i], &formatted[i]);
                edits[i].span = s->span;
                edits[i].bytes = formatted[i].data;
                edits[i].len = formatted[i].len;
                expected[i] = &texts[i];
            }
            if (ok)
            {
                roundtrip_apply(doc->data, doc->len, edits, roundtrip_base.count, &out);
                ok = roundtrip_verify(doc->data, doc->len, &roundtrip_base, edits, roundtrip_base.count, expected,
                                      out.data, out.len, NULL);
                NAOS_CHECK_TRUE(ok);
            }
            NAOS_TRACE_POP();
        }
        mnd_yaml_doc_free(roundtrip_base.doc);
        roundtrip_base.doc = NULL;
    }
    for (i = 0; i < ROUNDTRIP_SCALAR_MAX; ++i)
    {
        mnd_buf_free(&formatted[i]);
    }
    mnd_buf_free(&out);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_scaffold_span_text_concatenation),
    NAOS_ADD_TEST(roundtrip_fixtures_cover_all_styles),
    NAOS_ADD_TEST(roundtrip_original_bytes_identical),
    NAOS_ADD_TEST(roundtrip_each_scalar_each_text),
    NAOS_ADD_TEST(roundtrip_all_scalars_at_once)
)
