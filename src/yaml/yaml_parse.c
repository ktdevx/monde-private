/* Parse: 入口（UTF-8 の検証、行の表の作成、改行コードの検出）とブロックのコレクションの解析 */

#include "yaml/yaml_parse.h"
#include "yaml/yaml_flow.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_scalar.h"

#include "foundation/eol.h"
#include "foundation/utf8.h"

#include <stddef.h>
#include <string.h>

/* 行の走査の補助 */

static bool parse_is_white(char c)
{
    return (c == ' ') || (c == '\t');
}

/* data[start..end) に CR があれば true（行の内容の CR は、行末記号でない単独の CR） */
static bool parse_has_cr(const char *data, size_t start, size_t end)
{
    size_t i;

    for (i = start; i < end; i++)
    {
        if (data[i] == '\r')
        {
            return true;
        }
    }

    return false;
}

/* 行の表の index の行の先頭のカーソル（index == count なら範囲の終わり） */
static mnd_yaml_cursor_t parse_cursor_at_line(const mnd_yaml_parser_t *p, size_t index)
{
    mnd_yaml_cursor_t cursor;

    cursor.line_index = index;
    if (index < p->lines.count)
    {
        cursor.offset = p->lines.items[index].start;
    }
    else
    {
        cursor.offset = p->content.end;
    }

    return cursor;
}

/*
 * *index の行から、構造の解析で読み飛ばす行（空白だけの行とコメント行）を読み飛ばし、*index を
 * 最初のそれ以外の行（なければ lines.count）にする。コメント行の単独の CR はエラー（6.2）
 */
static bool parse_skip_ignorable_lines(mnd_yaml_parser_t *p, size_t *index)
{
    const mnd_yaml_line_t *line = NULL;

    while (*index < p->lines.count)
    {
        line = &p->lines.items[*index];
        if (line->comment)
        {
            if (parse_has_cr(p->data, line->content.start, line->content.end))
            {
                mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
                return false;
            }
        }
        else if (!line->blank)
        {
            break;
        }
        ++*index;
    }

    return true;
}

/*
 * ノードの後ろの、同じ行の残り（at から行の内容の終わりまで）を検査し、*at を次の行の先頭に進める。
 * 残りは空白と、空白の後ろのコメントだけを認める。それ以外は "unexpected content after value"（6.2）、
 * 単独の CR は "carriage return without line feed"
 */
static bool parse_finish_line(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at)
{
    const mnd_yaml_line_t *line = NULL;
    size_t i;
    size_t end;

    if (at->line_index >= p->lines.count)
    {
        return true;
    }
    line = &p->lines.items[at->line_index];
    end = line->content.end;
    i = at->offset;
    while ((i < end) && parse_is_white(p->data[i]))
    {
        ++i;
    }
    if (i < end)
    {
        if ((p->data[i] == '#') && (i > at->offset))
        {
            if (parse_has_cr(p->data, i, end))
            {
                mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
                return false;
            }
        }
        else if (p->data[i] == '\r')
        {
            mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
            return false;
        }
        else
        {
            mnd_yaml_parser_error(p, line->line, "unexpected content after value");
            return false;
        }
    }
    *at = parse_cursor_at_line(p, at->line_index + 1);

    return true;
}

/* ノードの解析 */

/*
 * at（空白でない最初の文字）から、ブロック文脈の値のノードを 1 つ読む。
 * フロー形式のコレクション（[ と {）は Flow に、スカラー（ブロックスカラーを含む）は Scalar に委ねる。
 * parent_indent は親のインデント（ブロックスカラーと継続行の判定に使う）。
 * ブロックスカラーでは *at が消費した最後の行の次の行の先頭に進む。それ以外では *at はノードの直後
 * （同じ行の残りは呼び出し側が parse_finish_line で検査する）
 */
static bool parse_value_node(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent,
                             mnd_yaml_node_t **out)
{
    char c = p->data[at->offset];

    if ((c == '[') || (c == '{'))
    {
        return mnd_yaml_flow_parse(p, at, out);
    }

    return mnd_yaml_scalar_parse(p, at, parent_indent, false, out);
}

/* node がブロックスカラー（| か >）か */
static bool parse_is_block_scalar(const mnd_yaml_node_t *node)
{
    return (node->kind == MND_YAML_SCALAR)
        && ((node->as.scalar.style == MND_YAML_LITERAL) || (node->as.scalar.style == MND_YAML_FOLDED));
}

/* ブロックのコレクション（明示的なスタック。再帰を使わない: 6.9） */

/* 作業用の配列の最初の容量（要素数） */
#define BLOCK_INITIAL_CAP 4

/*
 * スタックの段: 開いているコレクションのノードと、作業用の項目の配列（確保関数から直接確保し、
 * 閉じるときにアリーナへ複製する）。最後の項目の値が決まっていなければ pending が true で、
 * 値がないときは empty_offset の位置の空のプレーンスカラーにする。
 * 一番上でない段は、常に一番上の段に向かう子のコレクションを値として待っている
 */
typedef struct block_frame
{
    mnd_yaml_node_t *node;      /* 作りかけのノード（kind と span.start を設定済み） */
    size_t indent;              /* 項目の桁（マッピングはキー、シーケンスは - の桁） */
    mnd_yaml_pair_t *pairs;     /* マッピングの項目 */
    mnd_yaml_node_t **items;    /* シーケンスの項目 */
    size_t count;
    size_t cap;
    bool pending;               /* 最後の項目の値を待っている */
    size_t empty_offset;        /* 値がないときの空のスカラーの位置（: または - の直後） */
} block_frame_t;

typedef struct block_stack
{
    block_frame_t *frames;
    size_t count;
    size_t cap;
} block_stack_t;

static bool block_fail(mnd_yaml_parser_t *p, size_t line, const char *message)
{
    mnd_yaml_parser_error(p, line, message);

    return false;
}

static void block_free(mnd_yaml_parser_t *p, void *ptr)
{
    p->allocator->free(p->allocator->ctx, ptr);
}

/*
 * 要素の大きさが elem の配列 old（count 個の要素、容量 *cap）を 2 倍の容量（空なら BLOCK_INITIAL_CAP）の
 * 新しい配列に移し、old を解放して新しい配列を返す。*cap を更新する。
 * メモリ不足または大きさのオーバーフローなら診断を追加して NULL（old と *cap は変更しない）
 */
static void *block_grow(mnd_yaml_parser_t *p, void *old, size_t count, size_t *cap, size_t elem)
{
    size_t new_cap = BLOCK_INITIAL_CAP;
    size_t bytes;
    void *fresh = NULL;

    if (*cap > 0)
    {
        if (*cap > (((size_t)-1) / 2))
        {
            mnd_yaml_parser_oom(p);
            return NULL;
        }
        new_cap = *cap * 2;
    }
    if (!mnd_yaml_size_mul(new_cap, elem, &bytes))
    {
        mnd_yaml_parser_oom(p);
        return NULL;
    }
    fresh = p->allocator->alloc(p->allocator->ctx, bytes);
    if (fresh == NULL)
    {
        mnd_yaml_parser_oom(p);
        return NULL;
    }
    if (count > 0)
    {
        memcpy(fresh, old, count * elem);
    }
    block_free(p, old);
    *cap = new_cap;

    return fresh;
}

/* スタックのすべての段の作業用の配列と、スタックの配列を解放する */
static void block_stack_free(mnd_yaml_parser_t *p, block_stack_t *stack)
{
    size_t i;

    for (i = 0; i < stack->count; ++i)
    {
        block_free(p, stack->frames[i].items);
        block_free(p, stack->frames[i].pairs);
    }
    block_free(p, stack->frames);
    stack->frames = NULL;
    stack->count = 0;
    stack->cap = 0;
}

static block_frame_t *block_top(block_stack_t *stack)
{
    return &stack->frames[stack->count - 1];
}

/*
 * kind のコレクションの段を、項目の桁 indent、範囲の開始 start で積む。ノードをアリーナに作る。
 * メモリ不足なら診断を追加して false
 */
static bool block_push(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_kind_t kind, size_t indent,
                       size_t start)
{
    block_frame_t *frame = NULL;
    block_frame_t *frames = NULL;
    mnd_yaml_node_t *node = NULL;

    if (stack->count == stack->cap)
    {
        frames = (block_frame_t *)block_grow(p, stack->frames, stack->count, &stack->cap, sizeof(block_frame_t));
        if (frames == NULL)
        {
            return false;
        }
        stack->frames = frames;
    }
    node = mnd_yaml_node_new(p->doc, kind);
    if (node == NULL)
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    node->span.start = start;

    frame = &stack->frames[stack->count];
    frame->node = node;
    frame->indent = indent;
    frame->pairs = NULL;
    frame->items = NULL;
    frame->count = 0;
    frame->cap = 0;
    frame->pending = false;
    frame->empty_offset = 0;
    ++stack->count;

    return true;
}

/*
 * 段に、値を待つ項目を加える（マッピングでは key と値の組、シーケンスでは要素）。
 * 値がないときの空のスカラーの位置を empty_offset にする。メモリ不足なら診断を追加して false
 */
static bool block_add_entry(mnd_yaml_parser_t *p, block_frame_t *frame, mnd_yaml_node_t *key, size_t empty_offset)
{
    mnd_yaml_pair_t *pairs = NULL;
    mnd_yaml_node_t **items = NULL;

    if (frame->node->kind == MND_YAML_MAPPING)
    {
        if (frame->count == frame->cap)
        {
            pairs = (mnd_yaml_pair_t *)block_grow(p, frame->pairs, frame->count, &frame->cap,
                                                  sizeof(mnd_yaml_pair_t));
            if (pairs == NULL)
            {
                return false;
            }
            frame->pairs = pairs;
        }
        frame->pairs[frame->count].key = key;
        frame->pairs[frame->count].value = NULL;
    }
    else
    {
        if (frame->count == frame->cap)
        {
            items = (mnd_yaml_node_t **)block_grow(p, frame->items, frame->count, &frame->cap,
                                                    sizeof(mnd_yaml_node_t *));
            if (items == NULL)
            {
                return false;
            }
            frame->items = items;
        }
        frame->items[frame->count] = NULL;
    }
    ++frame->count;
    frame->pending = true;
    frame->empty_offset = empty_offset;

    return true;
}

/*
 * 決まった値 node を、一番上の段の待っている項目に入れる。段がなければ根にする（*root）
 */
static void block_fill(block_stack_t *stack, mnd_yaml_node_t *node, mnd_yaml_node_t **root)
{
    block_frame_t *frame = NULL;

    if (stack->count == 0)
    {
        *root = node;
        return;
    }
    frame = block_top(stack);
    if (frame->node->kind == MND_YAML_MAPPING)
    {
        frame->pairs[frame->count - 1].value = node;
    }
    else
    {
        frame->items[frame->count - 1] = node;
    }
    frame->pending = false;
}

/*
 * 一番上の段の待っている項目の値を、empty_offset の位置の空のプレーンスカラーにする（2.8）。
 * メモリ不足なら診断を追加して false
 */
static bool block_fill_empty(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_node_t **root)
{
    mnd_yaml_node_t *node = NULL;
    char *value = NULL;
    mnd_span_t span;

    node = mnd_yaml_node_new(p->doc, MND_YAML_SCALAR);
    if (node == NULL)
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    value = mnd_yaml_arena_strdup(p->doc, NULL, 0);
    if (value == NULL)
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    node->as.scalar.style = MND_YAML_PLAIN;
    node->as.scalar.value = value;
    node->as.scalar.value_len = 0;
    span.start = block_top(stack)->empty_offset;
    span.end = span.start;
    mnd_yaml_parser_set_span(p, node, span);
    block_fill(stack, node, root);

    return true;
}

/*
 * 一番上の段（値を待っていない）を閉じる: 作業用の配列をアリーナへ複製し、子の親と範囲（最初の項目の
 * 先頭から最後の値の終わりまで）を設定し、段を降ろして、下の段の待っている項目（なければ根）に入れる。
 * メモリ不足なら診断を追加して false（段は残す）
 */
static bool block_close(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_node_t **root)
{
    block_frame_t *frame = block_top(stack);
    mnd_yaml_node_t *node = frame->node;
    mnd_yaml_node_t **items = NULL;
    mnd_yaml_pair_t *pairs = NULL;
    mnd_span_t span;
    size_t bytes;
    size_t i;

    span.start = node->span.start;
    if (!mnd_yaml_size_mul(frame->count,
                           (node->kind == MND_YAML_SEQUENCE) ? sizeof(mnd_yaml_node_t *) : sizeof(mnd_yaml_pair_t),
                           &bytes))
    {
        mnd_yaml_parser_oom(p);
        return false;
    }
    if (node->kind == MND_YAML_SEQUENCE)
    {
        items = (mnd_yaml_node_t **)mnd_yaml_arena_alloc(p->doc, bytes);
        if (items == NULL)
        {
            mnd_yaml_parser_oom(p);
            return false;
        }
        memcpy(items, frame->items, bytes);
        for (i = 0; i < frame->count; ++i)
        {
            items[i]->parent = node;
        }
        node->as.sequence.items = items;
        node->as.sequence.count = frame->count;
        span.end = items[frame->count - 1]->span.end;
    }
    else
    {
        pairs = (mnd_yaml_pair_t *)mnd_yaml_arena_alloc(p->doc, bytes);
        if (pairs == NULL)
        {
            mnd_yaml_parser_oom(p);
            return false;
        }
        memcpy(pairs, frame->pairs, bytes);
        for (i = 0; i < frame->count; ++i)
        {
            pairs[i].key->parent = node;
            pairs[i].value->parent = node;
        }
        node->as.mapping.pairs = pairs;
        node->as.mapping.count = frame->count;
        span.end = pairs[frame->count - 1].value->span.end;
    }
    mnd_yaml_parser_set_span(p, node, span);

    block_free(p, frame->items);
    block_free(p, frame->pairs);
    frame->items = NULL;
    frame->pairs = NULL;
    --stack->count;
    block_fill(stack, node, root);

    return true;
}

/* line の pos が、シーケンスの項目の指示文字（- と、その後ろの空白か行の終わり）か */
static bool block_is_dash(const mnd_yaml_parser_t *p, const mnd_yaml_line_t *line, size_t pos)
{
    const size_t end = line->content.end;

    return (pos < end) && (p->data[pos] == '-') && (((pos + 1) >= end) || parse_is_white(p->data[pos + 1]));
}

/* line の pos が、複雑なキーの指示文字（? と、その後ろの空白か行の終わり）か */
static bool block_is_complex_key(const mnd_yaml_parser_t *p, const mnd_yaml_line_t *line, size_t pos)
{
    const size_t end = line->content.end;

    return (pos < end) && (p->data[pos] == '?') && (((pos + 1) >= end) || parse_is_white(p->data[pos + 1]));
}

/*
 * line の pos から始まる内容が、マッピングの項目（1 行のキーと、その後ろの :。: の後ろは空白か行の終わり）か。
 * キーは、同じ行で閉じる引用符付きのスカラー、またはプレーン（: と区切りか、空白の後ろの # の前まで。
 * 空でない）。そうなら *colon に : の位置を入れて true
 */
static bool block_find_key(const mnd_yaml_parser_t *p, const mnd_yaml_line_t *line, size_t pos, size_t *colon)
{
    const char *data = p->data;
    const size_t end = line->content.end;
    size_t i = pos + 1;
    char c;

    if (pos >= end)
    {
        return false;
    }
    c = data[pos];
    if ((c == '\'') || (c == '"'))
    {
        for (;;)
        {
            if (i >= end)
            {
                return false;
            }
            if ((c == '"') && (data[i] == '\\'))
            {
                i += 2;
                continue;
            }
            if (data[i] == c)
            {
                if ((c == '\'') && ((i + 1) < end) && (data[i + 1] == '\''))
                {
                    i += 2;
                    continue;
                }
                break;
            }
            ++i;
        }
        ++i;
        while ((i < end) && parse_is_white(data[i]))
        {
            ++i;
        }
        if ((i < end) && (data[i] == ':') && (((i + 1) >= end) || parse_is_white(data[i + 1])))
        {
            *colon = i;
            return true;
        }
        return false;
    }
    if ((c == '[') || (c == '{') || (c == '|') || (c == '>') || (c == '#'))
    {
        return false;
    }
    for (i = pos; i < end; ++i)
    {
        c = data[i];
        if (c == '\r')
        {
            return false;
        }
        if ((c == ':') && (((i + 1) >= end) || parse_is_white(data[i + 1])))
        {
            if (i == pos)
            {
                return false;
            }
            *colon = i;
            return true;
        }
        if ((c == '#') && (i > pos) && parse_is_white(data[i - 1]))
        {
            return false;
        }
    }

    return false;
}

/*
 * 構造の行（項目を始める行）の検査: インデントのタブ（6.3）、単独の CR（6.2）、行頭の % のディレクティブ（6.1）
 */
static bool block_check_line(mnd_yaml_parser_t *p, const mnd_yaml_line_t *line)
{
    if (line->tab_in_indent)
    {
        return block_fail(p, line->line, "tab character in indentation");
    }
    if (parse_has_cr(p->data, line->content.start, line->content.end))
    {
        return block_fail(p, line->line, "carriage return without line feed");
    }
    if ((line->indent == 0) && (p->data[line->content.start] == '%'))
    {
        return block_fail(p, line->line, "unsupported YAML syntax: directive");
    }

    return true;
}

/*
 * at から、スカラーまたはフロー形式のコレクションの値を読み、一番上の段の待っている項目（なければ根）に入れる。
 * 親のインデントは一番上の段の桁（なければ 0）。ブロックスカラーでなければ同じ行の残りを検査する。
 * *at は次の行の先頭に進む
 */
static bool block_value(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_cursor_t *at, mnd_yaml_node_t **root)
{
    mnd_yaml_node_t *node = NULL;
    size_t parent_indent = (stack->count > 0) ? block_top(stack)->indent : 0;

    if (!parse_value_node(p, at, parent_indent, &node))
    {
        return false;
    }
    if (!parse_is_block_scalar(node) && !parse_finish_line(p, at))
    {
        return false;
    }
    block_fill(stack, node, root);

    return true;
}

/*
 * 一番上の段（マッピング）に、at のキーと colon の : で始まる項目を加える: キーを読み、同じキーを検査し（6.4）、
 * 同じ行に値があればそれを読む（値がなければ値を待つ）。*at は次の行の先頭に進む
 */
static bool block_map_entry(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_cursor_t *at, size_t colon,
                            mnd_yaml_node_t **root)
{
    block_frame_t *frame = block_top(stack);
    const mnd_yaml_line_t *line = &p->lines.items[at->line_index];
    mnd_yaml_node_t *key = NULL;
    mnd_yaml_cursor_t key_at = *at;
    size_t i;

    if (!mnd_yaml_scalar_parse(p, &key_at, frame->indent, false, &key))
    {
        return false;
    }
    for (i = 0; i < frame->count; ++i)
    {
        if (mnd_yaml_key_equal(frame->pairs[i].key, key))
        {
            if (mnd_yaml_parser_claim_error(p))
            {
                mnd_diag_add(p->diags, MND_SEVERITY_ERROR, p->path, key->start_line, "duplicate mapping key \"%s\"",
                             key->as.scalar.value);
            }
            return false;
        }
    }
    if (!block_add_entry(p, frame, key, colon + 1))
    {
        return false;
    }

    at->offset = colon + 1;
    i = at->offset;
    while ((i < line->content.end) && parse_is_white(p->data[i]))
    {
        ++i;
    }
    if ((i >= line->content.end) || (p->data[i] == '#'))
    {
        return parse_finish_line(p, at);
    }
    at->offset = i;

    return block_value(p, stack, at, root);
}

/*
 * 一番上の段（シーケンス）に、at の - で始まる項目を加える。同じ行の内容が入れ子のコレクション
 * （- か key:）なら、*nested を true にして *at をその内容の先頭にする（呼び出し側がそこから続ける）。
 * それ以外の内容は値として読み、内容がなければ値を待つ。そのとき *at は次の行の先頭に進む
 */
static bool block_seq_entry(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_cursor_t *at,
                            mnd_yaml_node_t **root, bool *nested)
{
    const mnd_yaml_line_t *line = &p->lines.items[at->line_index];
    size_t i = at->offset + 1;
    size_t colon;
    bool tab = false;

    *nested = false;
    if (!block_add_entry(p, block_top(stack), NULL, at->offset + 1))
    {
        return false;
    }
    while ((i < line->content.end) && parse_is_white(p->data[i]))
    {
        if (p->data[i] == '\t')
        {
            tab = true;
        }
        ++i;
    }
    if ((i >= line->content.end) || (p->data[i] == '#'))
    {
        at->offset += 1;
        return parse_finish_line(p, at);
    }
    at->offset = i;
    if (block_is_dash(p, line, i) || block_find_key(p, line, i, &colon))
    {
        if (tab)
        {
            return block_fail(p, line->line, "tab character in indentation");
        }
        *nested = true;
        return true;
    }

    return block_value(p, stack, at, root);
}

/*
 * at（行の内容の中）から始まるブロックのノードを読む: - ならシーケンス、key: ならマッピングの段を積んで
 * 最初の項目を読み（- の後ろの入れ子は同じ行で続ける）、それ以外はスカラーまたはフロー形式の値として
 * 一番上の段の待っている項目（なければ根）に入れる。*at は次の行の先頭に進む
 */
static bool block_start_node(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_cursor_t *at,
                             mnd_yaml_node_t **root)
{
    const mnd_yaml_line_t *line = &p->lines.items[at->line_index];
    size_t colon;
    bool nested = true;

    while (nested)
    {
        if (block_is_dash(p, line, at->offset))
        {
            if (!block_push(p, stack, MND_YAML_SEQUENCE, at->offset - line->start, at->offset)
                || !block_seq_entry(p, stack, at, root, &nested))
            {
                return false;
            }
        }
        else if (block_is_complex_key(p, line, at->offset))
        {
            return block_fail(p, line->line, "unsupported YAML syntax: complex mapping key");
        }
        else if (block_find_key(p, line, at->offset, &colon))
        {
            return block_push(p, stack, MND_YAML_MAPPING, at->offset - line->start, at->offset)
                && block_map_entry(p, stack, at, colon, root);
        }
        else
        {
            return block_value(p, stack, at, root);
        }
    }

    return true;
}

/*
 * 一番上の段の待っている値を決める: 次の構造の行が段の桁より深い（マッピングでは、同じ桁の - も）なら、
 * そこから始まるノード。そうでなければ空のプレーンスカラー（2.8）
 */
static bool block_resolve_pending(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_cursor_t *at,
                                  mnd_yaml_node_t **root)
{
    const block_frame_t *frame = block_top(stack);
    const mnd_yaml_line_t *line = NULL;
    size_t index = at->line_index;
    size_t pos;

    if (!parse_skip_ignorable_lines(p, &index))
    {
        return false;
    }
    if (index < p->lines.count)
    {
        line = &p->lines.items[index];
        if (line->tab_in_indent)
        {
            return block_fail(p, line->line, "tab character in indentation");
        }
        pos = line->start + line->indent;
        if ((line->indent > frame->indent)
            || ((frame->node->kind == MND_YAML_MAPPING) && (line->indent == frame->indent)
                && block_is_dash(p, line, pos)))
        {
            if (!block_check_line(p, line))
            {
                return false;
            }
            at->line_index = index;
            at->offset = pos;
            return block_start_node(p, stack, at, root);
        }
    }

    return block_fill_empty(p, stack, root);
}

/*
 * ルートのコレクションを読み始めたあとの残りの行を読む: 行ごとに、インデントで段を閉じ、一番上の段の
 * 項目（シーケンスは -、マッピングは key:）を読む。範囲の終わりですべての段を閉じ、根を *root にする
 */
static bool block_run(mnd_yaml_parser_t *p, block_stack_t *stack, mnd_yaml_cursor_t *at, mnd_yaml_node_t **root)
{
    const mnd_yaml_line_t *line = NULL;
    block_frame_t *frame = NULL;
    size_t index;
    size_t colon;
    bool nested;

    while (stack->count > 0)
    {
        if (block_top(stack)->pending)
        {
            if (!block_resolve_pending(p, stack, at, root))
            {
                return false;
            }
            continue;
        }

        index = at->line_index;
        if (!parse_skip_ignorable_lines(p, &index))
        {
            return false;
        }
        if (index >= p->lines.count)
        {
            while (stack->count > 0)
            {
                if (!block_close(p, stack, root))
                {
                    return false;
                }
            }
            break;
        }
        line = &p->lines.items[index];
        if (!block_check_line(p, line))
        {
            return false;
        }
        while ((stack->count > 0) && (block_top(stack)->indent > line->indent))
        {
            if (!block_close(p, stack, root))
            {
                return false;
            }
        }
        if ((stack->count == 0) || (block_top(stack)->indent < line->indent))
        {
            return block_fail(p, line->line, "inconsistent indentation");
        }

        at->line_index = index;
        at->offset = line->start + line->indent;
        frame = block_top(stack);
        if (frame->node->kind == MND_YAML_SEQUENCE)
        {
            if (block_is_dash(p, line, at->offset))
            {
                if (!block_seq_entry(p, stack, at, root, &nested))
                {
                    return false;
                }
                if (nested && !block_start_node(p, stack, at, root))
                {
                    return false;
                }
                continue;
            }

            /* キーと同じ桁のシーケンス（key:\n- a）の後ろの、同じマッピングの次の項目 */
            if ((stack->count < 2) || (stack->frames[stack->count - 2].node->kind != MND_YAML_MAPPING)
                || (stack->frames[stack->count - 2].indent != line->indent))
            {
                return block_fail(p, line->line, "expected a mapping entry or a sequence item");
            }
            if (!block_close(p, stack, root))
            {
                return false;
            }
        }

        if (block_is_complex_key(p, line, at->offset))
        {
            return block_fail(p, line->line, "unsupported YAML syntax: complex mapping key");
        }
        if (!block_find_key(p, line, at->offset, &colon))
        {
            return block_fail(p, line->line, "expected a mapping entry or a sequence item");
        }
        if (!block_map_entry(p, stack, at, colon, root))
        {
            return false;
        }
    }

    return true;
}

/*
 * 範囲の全体を 1 つのドキュメントとして解析し、根を文書に設定する。
 * 根は最初の空でない行のインデントで始まるノード 1 つ（マッピング、シーケンス、スカラー）。
 * 空でない行がなければ空のドキュメント（2.7）。根がスカラーなら、その後ろには空白だけの行と
 * コメント行だけを認める
 */
static bool parse_document(mnd_yaml_parser_t *p)
{
    const mnd_yaml_line_t *line = NULL;
    mnd_yaml_node_t *root = NULL;
    block_stack_t stack;
    mnd_yaml_cursor_t at;
    size_t index = 0;
    bool ok;

    if (!parse_skip_ignorable_lines(p, &index))
    {
        return false;
    }
    if (index >= p->lines.count)
    {
        mnd_yaml_doc_set_root(p->doc, NULL);
        return true;
    }

    line = &p->lines.items[index];
    if (!block_check_line(p, line))
    {
        return false;
    }
    at.line_index = index;
    at.offset = line->start + line->indent;

    stack.frames = NULL;
    stack.count = 0;
    stack.cap = 0;

    /*
     * 根の親のインデントは 0 とする（ルートのブロックスカラーと継続行は、インデント 1 以上の行に限られる）
     */
    ok = block_start_node(p, &stack, &at, &root);
    if (ok && (stack.count > 0))
    {
        ok = block_run(p, &stack, &at, &root);
    }
    else if (ok)
    {
        index = at.line_index;
        ok = parse_skip_ignorable_lines(p, &index);
        if (ok && (index < p->lines.count))
        {
            line = &p->lines.items[index];
            if (line->tab_in_indent)
            {
                mnd_yaml_parser_error(p, line->line, "tab character in indentation");
            }
            else if (parse_has_cr(p->data, line->content.start, line->content.end))
            {
                mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
            }
            else
            {
                mnd_yaml_parser_error(p, line->line, "unexpected content after value");
            }
            ok = false;
        }
    }
    block_stack_free(p, &stack);
    if (!ok)
    {
        return false;
    }

    root->parent = NULL;
    mnd_yaml_doc_set_root(p->doc, root);

    return true;
}

/* 入口 */

/* content の範囲の行末記号（MND_EOL_LF か MND_EOL_CRLF。改行がなければ LF） */
static mnd_eol_t parse_detect_eol(const char *data, mnd_span_t content)
{
    mnd_eol_info_t info;

    if (content.end > content.start)
    {
        info = mnd_eol_detect(data + content.start, content.end - content.start);
        if (info.dominant == MND_EOL_CRLF)
        {
            return MND_EOL_CRLF;
        }
    }

    return MND_EOL_LF;
}

bool mnd_yaml_parse(const char *data, size_t len, mnd_span_t content, const mnd_yaml_options_t *options,
                    mnd_yaml_doc_t **out, mnd_diag_list_t *diags)
{
    mnd_yaml_parser_t p;
    const char *path = NULL;
    size_t invalid_offset = 0;
    size_t line = 0;
    bool ok = false;

    *out = NULL;
    if (options != NULL)
    {
        path = options->path;
    }

    /* 範囲の検証（呼び出し側のプログラムの誤り） */
    if ((content.start > content.end) || (content.end > len) || ((data == NULL) && (len > 0)))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "content span out of range");
        return false;
    }

    /* 範囲の UTF-8 の検証（6.6）。行番号は文書全体の先頭から数える（7.1） */
    if ((content.end > content.start)
        && !mnd_utf8_validate(data + content.start, content.end - content.start, &invalid_offset))
    {
        if (!mnd_source_line_of(data, len, content.start + invalid_offset, &line))
        {
            line = 0;
        }
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, line, "invalid UTF-8 sequence");
        return false;
    }

    p.data = data;
    p.len = len;
    p.content = content;
    p.path = path;
    p.allocator = mnd_yaml_allocator_or_default((options != NULL) ? options->allocator : NULL);
    p.doc = NULL;
    p.lines.items = NULL;
    p.lines.count = 0;
    p.lines.cap = 0;
    p.lines.range = content;
    p.lines.first_line = 1;
    p.diags = diags;
    p.failed = false;

    p.doc = mnd_yaml_doc_new(data, len, content, parse_detect_eol(data, content), p.allocator);
    if (p.doc == NULL)
    {
        mnd_yaml_parser_oom(&p);
    }
    else if (!mnd_yaml_lines_build(data, len, content, p.allocator, &p.lines))
    {
        mnd_yaml_parser_oom(&p);
    }
    else
    {
        ok = parse_document(&p);
    }

    mnd_yaml_lines_free(p.allocator, &p.lines);
    if (!ok || p.failed)
    {
        mnd_yaml_doc_free(p.doc);
        return false;
    }
    *out = p.doc;

    return true;
}
