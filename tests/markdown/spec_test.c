/* CommonMark spec 0.31.2 の全例の HTML と範囲の検査（9.2〜9.4、7.5） */

#include <naos/naos.h>

#include "html_normalize.h"
#include "html_render.h"
#include "spec_reader.h"
#include "tree_check.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"
#include "markdown/md.h"
#include "markdown/md_dialect.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* spec.txt（タグ 0.31.2）の大きさと例の数 */
#define SPEC_FILE_SIZE 205025u
#define SPEC_EXAMPLE_COUNT 652u

/* 何も追加しない方言（既定の方言 mnd_md_commonmark とは別に作る。後処理も持たない） */
static const mnd_md_dialect_t empty_dialect = {
    "empty", NULL, 0, NULL, 0, NULL
};

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

/* 例の Markdown を dialect（NULL 可）で解析する。失敗したら NULL */
static mnd_md_doc_t *parse_example(const mnd_md_spec_example_t *ex, const mnd_md_dialect_t *dialect)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;

    options.dialect = dialect;
    if (!mnd_md_parse(mnd_buf_cstr(&ex->markdown), ex->markdown.len, &options, &doc, &diags))
    {
        doc = NULL;
    }
    mnd_diag_list_free(&diags);

    return doc;
}

/* 一致しなかった例を、番号・行・節とともにメモに出して失敗にする */
static void fail_example(const mnd_md_spec_example_t *ex, const char *what)
{
    NAOS_NOTE("example %zu (line %zu, section \"%s\"): %s", ex->number, ex->start_line,
              mnd_buf_cstr(&ex->section), what);
    NAOS_CHECK_TRUE(false);
}

/*
 * 節ごとの結果をメモに出す。例は節ごとに続けて並ぶので、同じ節の連なりごとに数える。
 * 一致しない例がある節だけを、その数とともに失敗として出す
 */
static void note_sections(const mnd_md_spec_t *spec, const bool *failed)
{
    size_t i = 0;

    while (i < spec->count)
    {
        const mnd_buf_t *section = &spec->examples[i].section;
        size_t total = 0;
        size_t bad = 0;

        while ((i < spec->count) && (spec->examples[i].section.len == section->len)
               && (memcmp(mnd_buf_cstr(&spec->examples[i].section), mnd_buf_cstr(section), section->len) == 0))
        {
            total++;
            if (failed[i])
            {
                bad++;
            }
            i++;
        }

        if (bad > 0)
        {
            NAOS_NOTE("section \"%s\": %zu of %zu examples failed", mnd_buf_cstr(section), bad, total);
            NAOS_CHECK_UINT_EQ(0, bad);
        }
    }
}

/* 一致しなかった例の数をメモに出し、1 つでもあれば失敗にする */
static void note_summary(const char *what, size_t failures)
{
    NAOS_NOTE("%s: %zu of %u examples failed", what, failures, SPEC_EXAMPLE_COUNT);
    NAOS_CHECK_UINT_EQ(0, failures);
}

/* 9.x の前提: spec.txt の大きさと、改行に CR が含まれないこと（取り違えと改行の変換の検出） */
NAOS_TEST(file)
{
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;
    bool ok = mnd_file_read(MND_MD_SPEC_FILE, &content, &had_bom, &diags);

    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(SPEC_FILE_SIZE, content.len);
    NAOS_CHECK_NULL(memchr(mnd_buf_cstr(&content), '\r', content.len));
    mnd_buf_free(&content);
}

/* 9.2、9.3: 全例の HTML 出力が期待 HTML と完全に、または正規化したうえで一致する */
NAOS_TEST(html)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    mnd_buf_t actual = MND_BUF_INIT;
    mnd_buf_t norm_actual = MND_BUF_INIT;
    mnd_buf_t norm_expected = MND_BUF_INIT;
    bool *failed = NULL;
    size_t failures = 0;
    size_t not_exact = 0;
    size_t i = 0;

    load_spec(&spec);
    failed = calloc(spec.count, sizeof(*failed));
    NAOS_REQUIRE_NOT_NULL(failed);

    for (i = 0; i < spec.count; i++)
    {
        const mnd_md_spec_example_t *ex = &spec.examples[i];
        const mnd_buf_t *expected = &ex->html;
        mnd_md_doc_t *doc = parse_example(ex, NULL);

        if (doc == NULL)
        {
            failed[i] = true;
            failures++;
            fail_example(ex, "parse failed");
            continue;
        }

        mnd_buf_clear(&actual);
        NAOS_REQUIRE_TRUE(mnd_md_html_render(doc, &actual));
        mnd_md_doc_free(doc);

        if ((actual.len == expected->len)
            && (memcmp(mnd_buf_cstr(&actual), mnd_buf_cstr(expected), actual.len) == 0))
        {
            continue;
        }

        /* 完全一致しなければ、両方を normalize.py と同じ規則で正規化して比べる */
        not_exact++;
        mnd_buf_clear(&norm_actual);
        mnd_buf_clear(&norm_expected);
        NAOS_REQUIRE_TRUE(mnd_md_html_normalize(mnd_buf_cstr(&actual), actual.len, &norm_actual));
        NAOS_REQUIRE_TRUE(mnd_md_html_normalize(mnd_buf_cstr(expected), expected->len, &norm_expected));
        if ((norm_actual.len != norm_expected.len)
            || (memcmp(mnd_buf_cstr(&norm_actual), mnd_buf_cstr(&norm_expected), norm_actual.len) != 0))
        {
            failed[i] = true;
            failures++;
            fail_example(ex, "HTML differs");
        }
    }

    note_sections(&spec, failed);
    /* 完全一致しなかった例の数（正規化で一致したものを含む）も、全体の数のメモに出す */
    NAOS_NOTE("html: %zu of %u examples failed, %zu not exactly equal", failures, SPEC_EXAMPLE_COUNT,
              not_exact);
    NAOS_CHECK_UINT_EQ(0, failures);

    free(failed);
    mnd_buf_free(&actual);
    mnd_buf_free(&norm_actual);
    mnd_buf_free(&norm_expected);
    mnd_md_spec_free(&spec);
}

/* 9.4: 全例の構文木が範囲の不変条件（3.1〜3.3、4.1〜4.3）を満たす */
NAOS_TEST(ranges)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    bool *failed = NULL;
    size_t failures = 0;
    size_t i = 0;

    load_spec(&spec);
    failed = calloc(spec.count, sizeof(*failed));
    NAOS_REQUIRE_NOT_NULL(failed);

    for (i = 0; i < spec.count; i++)
    {
        const mnd_md_spec_example_t *ex = &spec.examples[i];
        mnd_md_doc_t *doc = parse_example(ex, NULL);
        mnd_md_tree_check_result_t result;

        if (doc == NULL)
        {
            failed[i] = true;
            failures++;
            fail_example(ex, "parse failed");
            continue;
        }

        if (!mnd_md_tree_check(doc, &result))
        {
            failed[i] = true;
            failures++;
            fail_example(ex, result.message);
        }
        mnd_md_doc_free(doc);
    }

    note_sections(&spec, failed);
    note_summary("ranges", failures);

    free(failed);
    mnd_md_spec_free(&spec);
}

/* 2 つの範囲が同じか */
static bool same_span(mnd_span_t a, mnd_span_t b)
{
    return (a.start == b.start) && (a.end == b.end);
}

/* 2 つのノードの行の配列が同じか */
static bool same_lines(const mnd_md_node_t *a, const mnd_md_node_t *b)
{
    size_t i = 0;

    if (((a->lines == NULL) != (b->lines == NULL)) || (a->line_count != b->line_count))
    {
        return false;
    }

    for (i = 0; i < a->line_count; i++)
    {
        const mnd_md_line_t *x = &a->lines[i];
        const mnd_md_line_t *y = &b->lines[i];

        if ((x->line != y->line) || !same_span(x->prefix, y->prefix) || !same_span(x->content, y->content)
            || (x->pad != y->pad) || (x->eol_len != y->eol_len) || (x->lazy != y->lazy))
        {
            return false;
        }
    }

    return true;
}

/* 2 つのリンク・画像の参照先の定義が同じか（別の文書のノードなので、範囲で比べる） */
static bool same_def(const mnd_md_node_t *a, const mnd_md_node_t *b)
{
    if ((a == NULL) || (b == NULL))
    {
        return (a == NULL) && (b == NULL);
    }

    return (a->kind == b->kind) && same_span(a->span, b->span);
}

/* 2 つのノードの種類ごとの値が同じか（種類は同じであること） */
static bool same_fields(const mnd_md_node_t *a, const mnd_md_node_t *b)
{
    switch (a->kind)
    {
    case MND_MD_HEADING:
        return (a->as.heading.level == b->as.heading.level) && (a->as.heading.setext == b->as.heading.setext)
               && same_span(a->as.heading.content, b->as.heading.content);
    case MND_MD_LIST:
        return (a->as.list.ordered == b->as.list.ordered) && (a->as.list.marker == b->as.list.marker)
               && (a->as.list.start == b->as.list.start) && (a->as.list.tight == b->as.list.tight);
    case MND_MD_ITEM:
        return same_span(a->as.item.marker, b->as.item.marker)
               && (a->as.item.content_indent == b->as.item.content_indent);
    case MND_MD_CODE_BLOCK:
        return (a->as.code_block.fenced == b->as.code_block.fenced)
               && (a->as.code_block.fence_char == b->as.code_block.fence_char)
               && (a->as.code_block.fence_len == b->as.code_block.fence_len)
               && (a->as.code_block.fence_indent == b->as.code_block.fence_indent)
               && (a->as.code_block.has_info == b->as.code_block.has_info)
               && same_span(a->as.code_block.info, b->as.code_block.info)
               && (a->as.code_block.closed == b->as.code_block.closed);
    case MND_MD_HTML_BLOCK:
        return a->as.html_block.type == b->as.html_block.type;
    case MND_MD_LINK_REF_DEF:
        return same_span(a->as.link_ref_def.label, b->as.link_ref_def.label)
               && same_span(a->as.link_ref_def.dest, b->as.link_ref_def.dest)
               && (a->as.link_ref_def.dest_angle == b->as.link_ref_def.dest_angle)
               && (a->as.link_ref_def.has_title == b->as.link_ref_def.has_title)
               && same_span(a->as.link_ref_def.title, b->as.link_ref_def.title)
               && (a->as.link_ref_def.title_delim == b->as.link_ref_def.title_delim)
               && ((a->as.link_ref_def.norm_label == NULL) == (b->as.link_ref_def.norm_label == NULL))
               && ((a->as.link_ref_def.norm_label == NULL)
                   || (strcmp(a->as.link_ref_def.norm_label, b->as.link_ref_def.norm_label) == 0));
    case MND_MD_LINK:
    case MND_MD_IMAGE:
        return (a->as.link.ref_kind == b->as.link.ref_kind) && same_span(a->as.link.text, b->as.link.text)
               && same_span(a->as.link.label, b->as.link.label) && same_def(a->as.link.def, b->as.link.def)
               && same_span(a->as.link.dest, b->as.link.dest)
               && (a->as.link.dest_angle == b->as.link.dest_angle)
               && (a->as.link.has_title == b->as.link.has_title)
               && same_span(a->as.link.title, b->as.link.title)
               && (a->as.link.title_delim == b->as.link.title_delim);
    case MND_MD_CODE_SPAN:
        return same_span(a->as.code_span.content, b->as.code_span.content)
               && (a->as.code_span.ticks == b->as.code_span.ticks);
    case MND_MD_EMPH:
    case MND_MD_STRONG:
        return (a->as.emph.delim == b->as.emph.delim) && same_span(a->as.emph.open, b->as.emph.open)
               && same_span(a->as.emph.close, b->as.emph.close);
    case MND_MD_AUTOLINK:
        return (a->as.autolink.email == b->as.autolink.email)
               && same_span(a->as.autolink.dest, b->as.autolink.dest);
    case MND_MD_ESCAPE:
        return a->as.escape.ch == b->as.escape.ch;
    case MND_MD_CHAR_REF:
        return (a->as.char_ref.numeric == b->as.char_ref.numeric) && (a->as.char_ref.len == b->as.char_ref.len)
               && (a->as.char_ref.len <= sizeof(a->as.char_ref.text))
               && (memcmp(a->as.char_ref.text, b->as.char_ref.text, a->as.char_ref.len) == 0);
    case MND_MD_HARD_BREAK:
        return a->as.hard_break.backslash == b->as.hard_break.backslash;
    default:
        /* 値を持たない種類。何も追加しない方言では、方言の種類のノードは現れない */
        return a->kind < MND_MD_KIND_DIALECT;
    }
}

/*
 * 2 つの文書の木が同じか: 前順に並べてたどり（再帰なし）、各ノードの種類・範囲・行番号・行・種類ごとの値と、
 * 子と次の兄弟の有無を比べる（前順の並びとこの 2 つの有無が同じなら、木の形も同じ）。
 * 異なれば、最初に異なったノードを *what に説明する
 */
static bool same_tree(const mnd_md_doc_t *x, const mnd_md_doc_t *y, const char **what)
{
    const mnd_md_node_t *rx = mnd_md_doc_root(x);
    const mnd_md_node_t *ry = mnd_md_doc_root(y);
    const mnd_md_node_t *a = rx;
    const mnd_md_node_t *b = ry;

    while ((a != NULL) && (b != NULL))
    {
        if ((a->kind != b->kind) || ((a->first_child == NULL) != (b->first_child == NULL))
            || ((a->next == NULL) != (b->next == NULL)))
        {
            *what = "tree shape or kinds differ";
            return false;
        }
        if (!same_span(a->span, b->span) || (a->start_line != b->start_line) || (a->end_line != b->end_line))
        {
            *what = "node spans or line numbers differ";
            return false;
        }
        if (!same_lines(a, b))
        {
            *what = "node lines differ";
            return false;
        }
        if (!same_fields(a, b))
        {
            *what = "node fields differ";
            return false;
        }

        a = mnd_md_node_next(a, rx);
        b = mnd_md_node_next(b, ry);
    }

    if ((a != NULL) || (b != NULL))
    {
        *what = "node counts differ";
        return false;
    }

    return true;
}

/* 7.1、7.5: 何も追加しない方言の解析で、全例の HTML とすべての範囲が既定の方言の解析と一致する */
NAOS_TEST(empty_dialect)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    mnd_buf_t html_default = MND_BUF_INIT;
    mnd_buf_t html_empty = MND_BUF_INIT;
    bool *failed = NULL;
    size_t failures = 0;
    size_t i = 0;

    NAOS_REQUIRE_TRUE(&empty_dialect != &mnd_md_commonmark);
    load_spec(&spec);
    failed = calloc(spec.count, sizeof(*failed));
    NAOS_REQUIRE_NOT_NULL(failed);

    for (i = 0; i < spec.count; i++)
    {
        const mnd_md_spec_example_t *ex = &spec.examples[i];
        mnd_md_doc_t *with_default = parse_example(ex, NULL);
        mnd_md_doc_t *with_empty = parse_example(ex, &empty_dialect);
        const char *what = NULL;

        if ((with_default == NULL) || (with_empty == NULL))
        {
            what = "parse failed";
        }
        else if (!same_tree(with_default, with_empty, &what))
        {
            /* what は same_tree が設定した */
        }
        else
        {
            mnd_buf_clear(&html_default);
            mnd_buf_clear(&html_empty);
            NAOS_REQUIRE_TRUE(mnd_md_html_render(with_default, &html_default));
            NAOS_REQUIRE_TRUE(mnd_md_html_render(with_empty, &html_empty));
            if ((html_default.len != html_empty.len)
                || (memcmp(mnd_buf_cstr(&html_default), mnd_buf_cstr(&html_empty), html_default.len) != 0))
            {
                what = "HTML differs";
            }
        }

        if (what != NULL)
        {
            failed[i] = true;
            failures++;
            fail_example(ex, what);
        }
        mnd_md_doc_free(with_default);
        mnd_md_doc_free(with_empty);
    }

    note_sections(&spec, failed);
    note_summary("empty_dialect", failures);

    free(failed);
    mnd_buf_free(&html_default);
    mnd_buf_free(&html_empty);
    mnd_md_spec_free(&spec);
}

NAOS_SUITE(spec,
    NAOS_ADD_TEST(file),
    NAOS_ADD_TEST(html),
    NAOS_ADD_TEST(ranges),
    NAOS_ADD_TEST(empty_dialect)
)
