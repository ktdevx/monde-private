/* テスト用の方言による拡張点（7.x） */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_dialect.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* メモリ不足のテストで、失敗させる位置を変える回数の上限（成功するまでの確保の回数より十分に大きい） */
#define OOM_MAX_STEPS 1000

/* テスト用の方言の種類 */
#define KIND_LINE ((mnd_md_kind_t)MND_MD_KIND_DIALECT)          /* `%%% ` で始まる 1 行のリーフ */
#define KIND_TAKE ((mnd_md_kind_t)(MND_MD_KIND_DIALECT + 1))    /* 段落の最後の行を引き取る `@@@` のリーフ */
#define KIND_BOX ((mnd_md_kind_t)(MND_MD_KIND_DIALECT + 2))     /* `|` のコンテナ */
#define KIND_INLINE ((mnd_md_kind_t)(MND_MD_KIND_DIALECT + 3))  /* インラインの規則（検査のテスト用） */

/* finalize が呼ばれた回数 */
static size_t line_finalize_calls;

/* 行の残りの最初の空白でない文字から text で始まり、インデントが 3 桁以下か */
static bool rest_starts_with(const mnd_md_line_view_t *view, const char *text)
{
    size_t n = strlen(text);

    if (view->blank || (view->indent >= 4) || ((view->end - view->first_nonspace) < n))
    {
        return false;
    }

    return (memcmp(view->data + view->first_nonspace, text, n) == 0);
}

/* `%%% ` で始まる行: 記号の位置からブロックを開き、記号を使う */
static bool line_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    if (!rest_starts_with(&view, "%%% "))
    {
        return false;
    }

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    if (mnd_md_block_ctx_open(ctx, KIND_LINE) == NULL)
    {
        return false;
    }

    mnd_md_block_ctx_advance(ctx, 4);

    return true;
}

/* 1 行で終わる */
static bool line_try_continue(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node)
{
    (void)ctx;
    (void)node;

    return false;
}

/* 閉じるときに、行の数をアリーナに確保したデータとして付ける */
static bool line_finalize(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node)
{
    size_t *data = (size_t *)mnd_md_block_ctx_alloc(ctx, sizeof(*data));

    line_finalize_calls++;
    if (data == NULL)
    {
        return false;
    }

    *data = node->line_count;
    node->as.dialect_data = data;

    return true;
}

/* `@@@` の行: 中断しようとしている段落の最後の行を引き取ってブロックを開く */
static bool take_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    if ((mnd_md_block_ctx_paragraph(ctx) == NULL) || !rest_starts_with(&view, "@@@"))
    {
        return false;
    }

    if (!mnd_md_block_ctx_take_paragraph_lines(ctx, 1))
    {
        return false;
    }

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    if (mnd_md_block_ctx_open(ctx, KIND_TAKE) == NULL)
    {
        return false;
    }

    mnd_md_block_ctx_advance(ctx, 3);

    return true;
}

/* `@@@` の行が続く間は続く */
static bool take_try_continue(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    (void)node;
    if (!rest_starts_with(&view, "@@@"))
    {
        return false;
    }

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    mnd_md_block_ctx_advance(ctx, 3);

    return true;
}

/* `|` とその後ろの空白 1 桁分を使う */
static void box_consume_marker(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view;

    mnd_md_block_ctx_advance(ctx, 1);
    view = mnd_md_block_ctx_line(ctx);
    if (view.indent >= 1)
    {
        mnd_md_block_ctx_advance_columns(ctx, 1);
    }
}

static bool box_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    if (!rest_starts_with(&view, "|"))
    {
        return false;
    }

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    if (mnd_md_block_ctx_open(ctx, KIND_BOX) == NULL)
    {
        return false;
    }

    box_consume_marker(ctx);

    return true;
}

static bool box_try_continue(mnd_md_block_ctx_t *ctx, mnd_md_node_t *node)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    (void)node;
    if (!rest_starts_with(&view, "|"))
    {
        return false;
    }

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    box_consume_marker(ctx);

    return true;
}

/* インデントの桁数を問わず、最初の空白でない文字が `^^^` の行: 記号の位置からブロックを開き、記号を使う */
static bool indented_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    if (view.blank || ((view.end - view.first_nonspace) < 3)
        || (memcmp(view.data + view.first_nonspace, "^^^", 3) != 0))
    {
        return false;
    }

    mnd_md_block_ctx_advance_columns(ctx, view.indent);
    if (mnd_md_block_ctx_open(ctx, KIND_LINE) == NULL)
    {
        return false;
    }

    mnd_md_block_ctx_advance(ctx, 3);

    return true;
}

/* 不正な結果: 開かずに true を返す */
static bool no_open_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    return rest_starts_with(&view, "!!!");
}

/* 不正な結果: 行末を超えて使う */
static bool overrun_try_start(mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view = mnd_md_block_ctx_line(ctx);

    if (!rest_starts_with(&view, "???"))
    {
        return false;
    }

    if (mnd_md_block_ctx_open(ctx, KIND_TAKE) == NULL)
    {
        return false;
    }

    mnd_md_block_ctx_advance(ctx, (view.end - view.pos) + 1);

    return true;
}

static bool inline_try_match(mnd_md_inline_ctx_t *ctx)
{
    (void)ctx;

    return false;
}

static bool inline_match_delims(const mnd_md_delim_run_t *opener, const mnd_md_delim_run_t *closer,
                                size_t *use)
{
    (void)opener;
    (void)closer;
    (void)use;

    return false;
}

/* kind、container、inline_content、can_interrupt_paragraph、try_start、try_continue、finalize */
static const mnd_md_block_rule_t line_rule = {
    KIND_LINE, false, true, true, line_try_start, line_try_continue, line_finalize
};

static const mnd_md_block_rule_t test_block_rules[3] = {
    { KIND_LINE, false, true, true, line_try_start, line_try_continue, line_finalize },
    { KIND_TAKE, false, false, true, take_try_start, take_try_continue, NULL },
    { KIND_BOX, true, false, true, box_try_start, box_try_continue, NULL }
};

static const mnd_md_dialect_t test_dialect = {
    "test", test_block_rules, 3, NULL, 0, NULL
};

static const mnd_md_dialect_t empty_dialect = {
    "empty", NULL, 0, NULL, 0, NULL
};

/* 期待する行の内容（prefix は [prefix_start, content_start)） */
static void check_line(const mnd_md_line_t *line, size_t line_no, size_t prefix_start,
                       size_t content_start, size_t content_end, unsigned char eol_len)
{
    NAOS_CHECK_UINT_EQ(line_no, line->line);
    NAOS_CHECK_UINT_EQ(prefix_start, line->prefix.start);
    NAOS_CHECK_UINT_EQ(content_start, line->prefix.end);
    NAOS_CHECK_UINT_EQ(content_start, line->content.start);
    NAOS_CHECK_UINT_EQ(content_end, line->content.end);
    NAOS_CHECK_UINT_EQ(0, line->pad);
    NAOS_CHECK_UINT_EQ(eol_len, line->eol_len);
    NAOS_CHECK_FALSE(line->lazy);
}

static void check_node(const mnd_md_node_t *node, mnd_md_kind_t kind, size_t start, size_t end,
                       size_t start_line, size_t end_line)
{
    NAOS_CHECK_INT_EQ((int)kind, (int)node->kind);
    NAOS_CHECK_UINT_EQ(start, node->span.start);
    NAOS_CHECK_UINT_EQ(end, node->span.end);
    NAOS_CHECK_UINT_EQ(start_line, node->start_line);
    NAOS_CHECK_UINT_EQ(end_line, node->end_line);
}

/* dialect で解析が成功し、診断が増えず、tree_check を満たすことを確かめる。失敗なら NULL */
static mnd_md_doc_t *parse_with(const char *data, size_t len, const mnd_md_dialect_t *dialect)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    bool ok = false;

    options.dialect = dialect;
    ok = mnd_md_parse(data, len, &options, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        return NULL;
    }

    NAOS_CHECK_NOT_NULL(doc);
    if (doc != NULL)
    {
        NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    }

    return doc;
}

/* dialect で解析が失敗し、*out が NULL になり、1 つのエラーの診断が message と line で追加されることを確かめる */
static void check_dialect_error(const char *data, const mnd_md_dialect_t *dialect, const char *message,
                                size_t line)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = (mnd_md_doc_t *)&options; /* NULL に書き換えられることを確かめる */
    const mnd_diag_t *diag = NULL;
    mnd_md_test_alloc_t ta;

    mnd_md_test_alloc_init(&ta, 0);
    options.dialect = dialect;
    options.path = "doc.md";
    options.allocator = &ta.allocator;
    NAOS_CHECK_FALSE(mnd_md_parse(data, strlen(data), &options, &doc, &diags));
    NAOS_CHECK_NULL(doc);
    NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(&ta));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)diag->severity);
    NAOS_CHECK_STR_EQ(message, diag->message);
    NAOS_REQUIRE_NOT_NULL(diag->path);
    NAOS_CHECK_STR_EQ("doc.md", diag->path);
    NAOS_CHECK_UINT_EQ(line, diag->line);
    mnd_diag_list_free(&diags);
}

/* 2 つの行の配列が同じか */
static bool same_lines(const mnd_md_node_t *a, const mnd_md_node_t *b)
{
    size_t i = 0;

    if (a->line_count != b->line_count)
    {
        return false;
    }

    for (i = 0; i < a->line_count; i++)
    {
        const mnd_md_line_t *x = &a->lines[i];
        const mnd_md_line_t *y = &b->lines[i];

        if ((x->line != y->line) || (x->prefix.start != y->prefix.start) || (x->prefix.end != y->prefix.end)
            || (x->content.start != y->content.start) || (x->content.end != y->content.end)
            || (x->pad != y->pad) || (x->eol_len != y->eol_len) || (x->lazy != y->lazy))
        {
            return false;
        }
    }

    return true;
}

/* 2 つの木が、形・種類・範囲・行番号・行の内容で同じか（前順に並べてたどる。再帰なし） */
static bool same_tree(const mnd_md_doc_t *x, const mnd_md_doc_t *y)
{
    const mnd_md_node_t *rx = mnd_md_doc_root(x);
    const mnd_md_node_t *ry = mnd_md_doc_root(y);
    const mnd_md_node_t *a = rx;
    const mnd_md_node_t *b = ry;

    while ((a != NULL) && (b != NULL))
    {
        if ((a->kind != b->kind) || (a->span.start != b->span.start) || (a->span.end != b->span.end)
            || (a->start_line != b->start_line) || (a->end_line != b->end_line)
            || ((a->first_child == NULL) != (b->first_child == NULL))
            || ((a->next == NULL) != (b->next == NULL)) || !same_lines(a, b))
        {
            NAOS_NOTE("trees differ at kind %d span [%zu, %zu)", (int)a->kind, a->span.start, a->span.end);
            return false;
        }

        a = mnd_md_node_next(a, rx);
        b = mnd_md_node_next(b, ry);
    }

    return ((a == NULL) && (b == NULL));
}

/* 7.1、7.5: 方言を指定しない解析、既定の方言、何も追加しない方言の解析で、同じ木になる */
NAOS_TEST(dialect_default_and_empty_dialects_give_same_tree)
{
    static const char *const inputs[] = {
        "",
        "hello *world*\nnext `code` line\n",
        "# Title\n\n> quote\n> - item 1\n>   continued\n> - item 2\n\n```c\ncode\n```\n",
        "[foo]: /url \"title\"\n\n[foo] and ![img](/a.png) <http://x.y>\n    indented\n",
        "<div>\nhtml\n</div>\n\nSetext\n===\n\n***\n1. a\n2. b\n\n   lazy\n",
        "%%% not a rule\n@@@ neither\n| nor this\n"
    };
    size_t i = 0;

    for (i = 0; i < (sizeof(inputs) / sizeof(inputs[0])); i++)
    {
        size_t len = strlen(inputs[i]);
        mnd_md_doc_t *plain = parse_with(inputs[i], len, NULL);
        mnd_md_doc_t *commonmark = parse_with(inputs[i], len, &mnd_md_commonmark);
        mnd_md_doc_t *empty = parse_with(inputs[i], len, &empty_dialect);

        NAOS_REQUIRE_NOT_NULL(plain);
        NAOS_REQUIRE_NOT_NULL(commonmark);
        NAOS_REQUIRE_NOT_NULL(empty);
        NAOS_CHECK_TRUE(same_tree(plain, commonmark));
        NAOS_CHECK_TRUE(same_tree(plain, empty));
        mnd_md_doc_free(plain);
        mnd_md_doc_free(commonmark);
        mnd_md_doc_free(empty);
    }
}

/* 既定の方言は何も追加しない */
NAOS_TEST(dialect_commonmark_has_no_rules)
{
    NAOS_CHECK_NOT_NULL(mnd_md_commonmark.name);
    NAOS_CHECK_UINT_EQ(0, mnd_md_commonmark.block_rule_count);
    NAOS_CHECK_UINT_EQ(0, mnd_md_commonmark.inline_rule_count);
    NAOS_CHECK_TRUE(mnd_md_commonmark.postprocess == NULL);
}

/* 7.2、7.3: `%%% ` の 1 行のリーフが段落を中断し、種類・範囲・行・インラインの子を持つ */
NAOS_TEST(dialect_line_rule_interrupts_paragraph)
{
    static const char data[] = "para\n%%% *hi* there\nafter\n";
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *node = NULL;
    const mnd_md_node_t *after = NULL;
    const mnd_md_node_t *emph = NULL;
    const mnd_md_node_t *text = NULL;

    line_finalize_calls = 0;
    doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    para = root->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 0, 4, 1, 1);
    NAOS_CHECK_UINT_EQ(1, para->line_count);

    node = para->next;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_LINE, 5, 19, 2, 2);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 2, 5, 9, 19, 1);
    NAOS_CHECK_UINT_EQ(1, line_finalize_calls);
    NAOS_REQUIRE_NOT_NULL(node->as.dialect_data);
    NAOS_CHECK_UINT_EQ(1, *(const size_t *)node->as.dialect_data);

    /* 内容 `*hi* there` はインラインとして解析される */
    emph = node->first_child;
    NAOS_REQUIRE_NOT_NULL(emph);
    check_node(emph, MND_MD_EMPH, 9, 13, 2, 2);
    text = emph->next;
    NAOS_REQUIRE_NOT_NULL(text);
    check_node(text, MND_MD_TEXT, 13, 19, 2, 2);
    NAOS_CHECK_NULL(text->next);

    after = node->next;
    NAOS_REQUIRE_NOT_NULL(after);
    check_node(after, MND_MD_PARAGRAPH, 20, 25, 3, 3);
    NAOS_CHECK_NULL(after->next);
    mnd_md_doc_free(doc);
}

/* 方言の 1 行のリーフはコンテナの中でも、インデントがあっても始まり、次の行は別の段落になる */
NAOS_TEST(dialect_line_rule_in_block_quote)
{
    static const char data[] = ">  %%% x\n> y\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    const mnd_md_node_t *quote = NULL;
    const mnd_md_node_t *node = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(quote);
    check_node(quote, MND_MD_BLOCK_QUOTE, 0, 12, 1, 2);
    node = quote->first_child;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_LINE, 3, 8, 1, 1);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 1, 0, 7, 8, 1);
    para = node->next;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 11, 12, 2, 2);
    mnd_md_doc_free(doc);
}

/* 段落の最後の行を引き取る規則: ノードの範囲は引き取った行の内容の開始から始まる */
NAOS_TEST(dialect_take_paragraph_lines_range)
{
    static const char data[] = "a\nb\n@@@ x\n@@@y\nc\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *node = NULL;
    const mnd_md_node_t *last = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 0, 1, 1, 1);
    NAOS_REQUIRE_UINT_EQ(1, para->line_count);
    check_line(&para->lines[0], 1, 0, 0, 1, 1);

    node = para->next;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_TAKE, 2, 14, 2, 4);
    NAOS_REQUIRE_UINT_EQ(3, node->line_count);
    check_line(&node->lines[0], 2, 2, 2, 3, 1);
    check_line(&node->lines[1], 3, 4, 7, 9, 1);
    check_line(&node->lines[2], 4, 10, 13, 14, 1);
    /* inline_content が偽なので子を持たない */
    NAOS_CHECK_NULL(node->first_child);

    last = node->next;
    NAOS_REQUIRE_NOT_NULL(last);
    check_node(last, MND_MD_PARAGRAPH, 15, 16, 5, 5);
    mnd_md_doc_free(doc);
}

/* 段落の行をすべて引き取ると、段落は木から取り除かれる */
NAOS_TEST(dialect_take_all_paragraph_lines_removes_paragraph)
{
    static const char data[] = "b\n@@@\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    const mnd_md_node_t *node = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    node = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_TAKE, 0, 5, 1, 2);
    NAOS_CHECK_NULL(node->next);
    NAOS_REQUIRE_UINT_EQ(2, node->line_count);
    check_line(&node->lines[0], 1, 0, 0, 1, 1);
    check_line(&node->lines[1], 2, 2, 5, 5, 1);
    mnd_md_doc_free(doc);
}

/* 段落がなければ、引き取る規則は始まらない */
NAOS_TEST(dialect_take_rule_needs_paragraph)
{
    static const char data[] = "@@@ x\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 0, 5, 1, 1);
    mnd_md_doc_free(doc);
}

/* 方言のコンテナ: try_continue で続き、子のブロックを持ち、照合できない行で閉じる */
NAOS_TEST(dialect_container_rule_continues_and_closes)
{
    static const char data[] = "| # h\n| text\n|\nafter\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    const mnd_md_node_t *box = NULL;
    const mnd_md_node_t *heading = NULL;
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *after = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    box = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(box);
    /* 範囲は記号が現れた最後の行（3 行目の `|`）まで */
    check_node(box, KIND_BOX, 0, 14, 1, 3);
    heading = box->first_child;
    NAOS_REQUIRE_NOT_NULL(heading);
    check_node(heading, MND_MD_HEADING, 2, 5, 1, 1);
    para = heading->next;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 8, 12, 2, 2);
    NAOS_REQUIRE_UINT_EQ(1, para->line_count);
    check_line(&para->lines[0], 2, 6, 8, 12, 1);
    NAOS_CHECK_NULL(para->next);

    after = box->next;
    NAOS_REQUIRE_NOT_NULL(after);
    check_node(after, MND_MD_PARAGRAPH, 15, 20, 4, 4);
    mnd_md_doc_free(doc);
}

/* 方言の規則が試されるのは CommonMark の規則より前（`|` の後ろの `%%% ` も方言のリーフ） */
NAOS_TEST(dialect_rules_nest_inside_dialect_container)
{
    static const char data[] = "| %%% x\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &test_dialect);
    const mnd_md_node_t *box = NULL;
    const mnd_md_node_t *node = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    box = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(box);
    check_node(box, KIND_BOX, 0, 7, 1, 1);
    node = box->first_child;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_LINE, 2, 7, 1, 1);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 1, 0, 6, 7, 1);
    mnd_md_doc_free(doc);
}

/* 段落を中断できない規則は、段落の中では試されない */
NAOS_TEST(dialect_rule_without_interrupt_does_not_interrupt)
{
    static const mnd_md_block_rule_t rules[1] = {
        { KIND_LINE, false, true, false, line_try_start, line_try_continue, NULL }
    };
    static const mnd_md_dialect_t dialect = { "nointerrupt", rules, 1, NULL, 0, NULL };
    static const char data[] = "para\n%%% x\n\n%%% y\n";
    mnd_md_doc_t *doc = parse_with(data, sizeof(data) - 1, &dialect);
    const mnd_md_node_t *para = NULL;
    const mnd_md_node_t *node = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 0, 10, 1, 2);
    node = para->next;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_LINE, 12, 17, 4, 4);
    mnd_md_doc_free(doc);
}

/*
 * インデント 4 以上の行: 段落の中（遅延継続行の候補を含む）では方言の規則を試し、段落の中でなければ
 * インデントコードブロックになる（設計の「ブロックの段階」の 3）
 */
NAOS_TEST(dialect_rules_tried_on_indented_line_in_paragraph)
{
    static const mnd_md_block_rule_t rules[1] = {
        { KIND_LINE, false, false, true, indented_try_start, line_try_continue, NULL }
    };
    static const mnd_md_dialect_t dialect = { "indented", rules, 1, NULL, 0, NULL };
    static const char in_para[] = "para\n    ^^^ x\n";
    static const char lazy[] = "> para\n    ^^^ x\n";
    static const char no_para[] = "    ^^^ x\n";
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *first = NULL;
    const mnd_md_node_t *node = NULL;

    doc = parse_with(in_para, sizeof(in_para) - 1, &dialect);
    NAOS_REQUIRE_NOT_NULL(doc);
    first = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(first);
    check_node(first, MND_MD_PARAGRAPH, 0, 4, 1, 1);
    node = first->next;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_LINE, 9, 14, 2, 2);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 2, 5, 12, 14, 1);
    NAOS_CHECK_NULL(node->next);
    mnd_md_doc_free(doc);

    /* 遅延継続行の候補: 引用を閉じて、文書の子として開く */
    doc = parse_with(lazy, sizeof(lazy) - 1, &dialect);
    NAOS_REQUIRE_NOT_NULL(doc);
    first = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(first);
    check_node(first, MND_MD_BLOCK_QUOTE, 0, 6, 1, 1);
    node = first->next;
    NAOS_REQUIRE_NOT_NULL(node);
    check_node(node, KIND_LINE, 11, 16, 2, 2);
    mnd_md_doc_free(doc);

    /* 段落の中でなければインデントコードブロック */
    doc = parse_with(no_para, sizeof(no_para) - 1, &dialect);
    NAOS_REQUIRE_NOT_NULL(doc);
    first = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(first);
    check_node(first, MND_MD_CODE_BLOCK, 4, 9, 1, 1);
    NAOS_CHECK_FALSE(first->as.code_block.fenced);
    NAOS_CHECK_NULL(first->next);
    mnd_md_doc_free(doc);
}

/* 不正な規則は、設計のメッセージの診断になる（7.x、1.4 と同じ扱い） */
NAOS_TEST(dialect_invalid_rules_are_diagnosed)
{
    static const mnd_md_block_rule_t low_kind[1] = {
        { MND_MD_PARAGRAPH, false, true, true, line_try_start, line_try_continue, NULL }
    };
    static const mnd_md_block_rule_t no_start[2] = {
        { KIND_LINE, false, true, true, line_try_start, line_try_continue, NULL },
        { KIND_TAKE, false, true, true, NULL, line_try_continue, NULL }
    };
    static const mnd_md_block_rule_t no_continue[1] = {
        { KIND_LINE, false, true, true, line_try_start, NULL, NULL }
    };
    static const mnd_md_block_rule_t duplicate[2] = {
        { KIND_LINE, false, true, true, line_try_start, line_try_continue, NULL },
        { KIND_LINE, false, false, true, take_try_start, take_try_continue, NULL }
    };
    static const mnd_md_inline_rule_t star[1] = {
        { KIND_INLINE, '*', false, inline_try_match, NULL }
    };
    static const mnd_md_inline_rule_t newline[1] = {
        { KIND_INLINE, '\n', false, inline_try_match, NULL }
    };
    static const mnd_md_inline_rule_t inline_low[1] = {
        { MND_MD_TEXT, '{', false, inline_try_match, NULL }
    };
    static const mnd_md_inline_rule_t no_match[2] = {
        { KIND_INLINE, '{', false, inline_try_match, NULL },
        { (mnd_md_kind_t)(MND_MD_KIND_DIALECT + 4), '=', true, inline_try_match, NULL }
    };
    static const mnd_md_inline_rule_t no_try[1] = {
        { KIND_INLINE, '{', false, NULL, inline_match_delims }
    };
    static const mnd_md_inline_rule_t same_as_block[1] = {
        { KIND_LINE, '{', false, inline_try_match, NULL }
    };
    static const mnd_md_dialect_t d_low_kind = { "bad", low_kind, 1, NULL, 0, NULL };
    static const mnd_md_dialect_t d_no_start = { "bad", no_start, 2, NULL, 0, NULL };
    static const mnd_md_dialect_t d_no_continue = { "bad", no_continue, 1, NULL, 0, NULL };
    static const mnd_md_dialect_t d_duplicate = { "bad", duplicate, 2, NULL, 0, NULL };
    static const mnd_md_dialect_t d_missing_array = { "bad", NULL, 1, NULL, 0, NULL };
    static const mnd_md_dialect_t d_star = { "bad", NULL, 0, star, 1, NULL };
    static const mnd_md_dialect_t d_newline = { "bad", NULL, 0, newline, 1, NULL };
    static const mnd_md_dialect_t d_inline_low = { "bad", NULL, 0, inline_low, 1, NULL };
    static const mnd_md_dialect_t d_no_match = { "bad", NULL, 0, no_match, 2, NULL };
    static const mnd_md_dialect_t d_no_try = { "bad", NULL, 0, no_try, 1, NULL };
    static const mnd_md_dialect_t d_same_as_block = { "bad", &line_rule, 1, same_as_block, 1, NULL };
    static const mnd_md_dialect_t d_unnamed = { NULL, low_kind, 1, NULL, 0, NULL };

    check_dialect_error("x\n", &d_low_kind, "dialect \"bad\": invalid block rule 0", 0);
    check_dialect_error("x\n", &d_no_start, "dialect \"bad\": invalid block rule 1", 0);
    check_dialect_error("x\n", &d_no_continue, "dialect \"bad\": invalid block rule 0", 0);
    check_dialect_error("x\n", &d_duplicate, "dialect \"bad\": invalid block rule 1", 0);
    check_dialect_error("x\n", &d_missing_array, "dialect \"bad\": invalid block rule 0", 0);
    check_dialect_error("x\n", &d_star, "dialect \"bad\": invalid inline rule 0", 0);
    check_dialect_error("x\n", &d_newline, "dialect \"bad\": invalid inline rule 0", 0);
    check_dialect_error("x\n", &d_inline_low, "dialect \"bad\": invalid inline rule 0", 0);
    check_dialect_error("x\n", &d_no_match, "dialect \"bad\": invalid inline rule 1", 0);
    check_dialect_error("x\n", &d_no_try, "dialect \"bad\": invalid inline rule 0", 0);
    check_dialect_error("x\n", &d_same_as_block, "dialect \"bad\": invalid inline rule 0", 0);
    check_dialect_error("x\n", &d_unnamed, "dialect \"\": invalid block rule 0", 0);
    /* 方言の検査は UTF-8 の検査より前 */
    check_dialect_error("\xff\n", &d_low_kind, "dialect \"bad\": invalid block rule 0", 0);
}

/* 規則が不正な結果を返したら、その行の診断になる */
NAOS_TEST(dialect_invalid_rule_results_are_diagnosed)
{
    static const mnd_md_block_rule_t no_open[2] = {
        { KIND_LINE, false, true, true, line_try_start, line_try_continue, NULL },
        { KIND_TAKE, false, false, true, no_open_try_start, take_try_continue, NULL }
    };
    static const mnd_md_block_rule_t overrun[1] = {
        { KIND_TAKE, false, false, true, overrun_try_start, take_try_continue, NULL }
    };
    static const mnd_md_dialect_t d_no_open = { "bad", no_open, 2, NULL, 0, NULL };
    static const mnd_md_dialect_t d_overrun = { "bad", overrun, 1, NULL, 0, NULL };

    check_dialect_error("a\n\n!!!\n", &d_no_open, "dialect \"bad\": rule 1 returned an invalid result", 3);
    check_dialect_error("???\n", &d_overrun, "dialect \"bad\": rule 0 returned an invalid result", 1);
}

/* 8.2: 方言の規則を使う解析でも、メモリ不足は `out of memory` になり、確保したものをすべて解放する */
NAOS_TEST(dialect_out_of_memory_frees_everything)
{
    static const char data[] = "para\n%%% *hi*\n| a\n| b\nc\n@@@ x\n";
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
        options.dialect = &test_dialect;
        if (mnd_md_parse(data, sizeof(data) - 1, &options, &doc, &diags))
        {
            succeeded = true;
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_REQUIRE_NOT_NULL(doc);
            NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
            mnd_md_doc_free(doc);
        }
        else
        {
            failures++;
            NAOS_CHECK_NULL(doc);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
            NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_at(&diags, 0)->line);
        }

        mnd_diag_list_free(&diags);
        NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(&ta));
    }

    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_TRUE(failures > 0);
}

NAOS_SUITE(dialect,
    NAOS_ADD_TEST(dialect_default_and_empty_dialects_give_same_tree),
    NAOS_ADD_TEST(dialect_commonmark_has_no_rules),
    NAOS_ADD_TEST(dialect_line_rule_interrupts_paragraph),
    NAOS_ADD_TEST(dialect_line_rule_in_block_quote),
    NAOS_ADD_TEST(dialect_take_paragraph_lines_range),
    NAOS_ADD_TEST(dialect_take_all_paragraph_lines_removes_paragraph),
    NAOS_ADD_TEST(dialect_take_rule_needs_paragraph),
    NAOS_ADD_TEST(dialect_container_rule_continues_and_closes),
    NAOS_ADD_TEST(dialect_rules_nest_inside_dialect_container),
    NAOS_ADD_TEST(dialect_rule_without_interrupt_does_not_interrupt),
    NAOS_ADD_TEST(dialect_rules_tried_on_indented_line_in_paragraph),
    NAOS_ADD_TEST(dialect_invalid_rules_are_diagnosed),
    NAOS_ADD_TEST(dialect_invalid_rule_results_are_diagnosed),
    NAOS_ADD_TEST(dialect_out_of_memory_frees_everything)
)
