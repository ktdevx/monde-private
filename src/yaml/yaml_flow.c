/* Flow: フロー形式のシーケンスとマッピング（明示的なスタック） */

#include "yaml/yaml_flow.h"
#include "yaml/yaml_scalar.h"

#include <stddef.h>
#include <string.h>

/* 作業用の配列の最初の容量（要素数） */
#define FLOW_INITIAL_CAP 4

/*
 * スタックの段の状態。
 * ENTRY: 項目（シーケンス）またはキー（マッピング）か閉じ括弧を待つ（開き括弧と , の直後）。
 * COLON: キーの後ろの :、,、閉じ括弧を待つ（マッピングのみ）。
 * VALUE: : の後ろの値、,、閉じ括弧を待つ（マッピングのみ）。
 * SEPARATOR: 項目の後ろの , か閉じ括弧を待つ
 */
typedef enum flow_state
{
    FLOW_STATE_ENTRY,
    FLOW_STATE_COLON,
    FLOW_STATE_VALUE,
    FLOW_STATE_SEPARATOR
} flow_state_t;

/*
 * スタックの段: 作りかけのコレクションのノードと、作業用の項目の配列（確保関数から直接確保し、
 * 閉じるときにアリーナへ複製する）
 */
typedef struct flow_frame
{
    mnd_yaml_node_t *node;      /* 作りかけのノード（kind と span.start を設定済み） */
    size_t open_line;           /* 開き括弧の行 */
    flow_state_t state;
    mnd_yaml_node_t **items;    /* シーケンスの項目 */
    mnd_yaml_pair_t *pairs;     /* マッピングの項目 */
    size_t count;
    size_t cap;
    mnd_yaml_node_t *key;       /* 値を待っているキー（COLON と VALUE） */
    size_t colon_end;           /* VALUE での : の直後のオフセット */
} flow_frame_t;

typedef struct flow_stack
{
    flow_frame_t *frames;
    size_t count;
    size_t cap;
} flow_stack_t;

static bool flow_is_white(char c)
{
    return (c == ' ') || (c == '\t');
}

static bool flow_is_indicator(char c)
{
    return (c == ',') || (c == '[') || (c == ']') || (c == '{') || (c == '}');
}

static char flow_closer(const mnd_yaml_node_t *node)
{
    return (node->kind == MND_YAML_SEQUENCE) ? ']' : '}';
}

static bool flow_fail(mnd_yaml_parser_t *p, size_t line, const char *message)
{
    mnd_yaml_parser_error(p, line, message);

    return false;
}

static void flow_free(mnd_yaml_parser_t *p, void *ptr)
{
    p->allocator->free(p->allocator->ctx, ptr);
}

/*
 * 要素の大きさが elem の配列 old（count 個の要素、容量 *cap）を 2 倍の容量（空なら FLOW_INITIAL_CAP）の
 * 新しい配列に移し、old を解放して新しい配列を返す。*cap を更新する。
 * メモリ不足または大きさのオーバーフローなら診断を追加して NULL（old と *cap は変更しない）
 */
static void *flow_grow(mnd_yaml_parser_t *p, void *old, size_t count, size_t *cap, size_t elem)
{
    size_t new_cap = FLOW_INITIAL_CAP;
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
    flow_free(p, old);
    *cap = new_cap;

    return fresh;
}

/* スタックのすべての段の作業用の配列と、スタックの配列を解放する */
static void flow_stack_free(mnd_yaml_parser_t *p, flow_stack_t *stack)
{
    size_t i;

    for (i = 0; i < stack->count; ++i)
    {
        flow_free(p, stack->frames[i].items);
        flow_free(p, stack->frames[i].pairs);
    }
    flow_free(p, stack->frames);
    stack->frames = NULL;
    stack->count = 0;
    stack->cap = 0;
}

/*
 * at（[ または {）で始まるコレクションの段を積む。ノードをアリーナに作る。
 * メモリ不足なら診断を追加して false
 */
static bool flow_push(mnd_yaml_parser_t *p, flow_stack_t *stack, mnd_yaml_cursor_t at)
{
    flow_frame_t *frame = NULL;
    flow_frame_t *frames = NULL;
    mnd_yaml_node_t *node = NULL;
    mnd_yaml_kind_t kind = (p->data[at.offset] == '[') ? MND_YAML_SEQUENCE : MND_YAML_MAPPING;

    if (stack->count == stack->cap)
    {
        frames = (flow_frame_t *)flow_grow(p, stack->frames, stack->count, &stack->cap, sizeof(flow_frame_t));
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
    node->span.start = at.offset;

    frame = &stack->frames[stack->count];
    frame->node = node;
    frame->open_line = p->lines.items[at.line_index].line;
    frame->state = FLOW_STATE_ENTRY;
    frame->items = NULL;
    frame->pairs = NULL;
    frame->count = 0;
    frame->cap = 0;
    frame->key = NULL;
    frame->colon_end = 0;
    ++stack->count;

    return true;
}

/* シーケンスの段に項目を加える。メモリ不足なら診断を追加して false */
static bool flow_add_item(mnd_yaml_parser_t *p, flow_frame_t *frame, mnd_yaml_node_t *item)
{
    mnd_yaml_node_t **items = NULL;

    if (frame->count == frame->cap)
    {
        items = (mnd_yaml_node_t **)flow_grow(p, frame->items, frame->count, &frame->cap,
                                               sizeof(mnd_yaml_node_t *));
        if (items == NULL)
        {
            return false;
        }
        frame->items = items;
    }
    frame->items[frame->count] = item;
    ++frame->count;
    frame->state = FLOW_STATE_SEPARATOR;

    return true;
}

/* マッピングの段に、待っているキーと value の組を加える。メモリ不足なら診断を追加して false */
static bool flow_add_pair(mnd_yaml_parser_t *p, flow_frame_t *frame, mnd_yaml_node_t *value)
{
    mnd_yaml_pair_t *pairs = NULL;

    if (frame->count == frame->cap)
    {
        pairs = (mnd_yaml_pair_t *)flow_grow(p, frame->pairs, frame->count, &frame->cap, sizeof(mnd_yaml_pair_t));
        if (pairs == NULL)
        {
            return false;
        }
        frame->pairs = pairs;
    }
    frame->pairs[frame->count].key = frame->key;
    frame->pairs[frame->count].value = value;
    ++frame->count;
    frame->key = NULL;
    frame->state = FLOW_STATE_SEPARATOR;

    return true;
}

/*
 * 値のないキーの値として、offset の位置の空のプレーンスカラーを作り、組を加える
 * （{k} ではキーの直後、{k: } では : の直後）。メモリ不足なら診断を追加して false
 */
static bool flow_add_empty_value(mnd_yaml_parser_t *p, flow_frame_t *frame, size_t offset)
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
    node->as.scalar.flow = true;
    span.start = offset;
    span.end = offset;
    mnd_yaml_parser_set_span(p, node, span);

    return flow_add_pair(p, frame, node);
}

/*
 * 段のキー key が、同じマッピングのそれまでのキーと論理値で一致すれば、後のキーの行で
 * 同じキーのエラーを報告して false
 */
static bool flow_check_duplicate(mnd_yaml_parser_t *p, const flow_frame_t *frame, const mnd_yaml_node_t *key)
{
    size_t i;

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

    return true;
}

/*
 * 一番上の段を、end（閉じ括弧の直後）で閉じる: 作業用の配列をアリーナへ複製し、範囲と子の親を設定し、
 * 作業用の配列を解放して段を降ろす。メモリ不足なら診断を追加して false（段は残す）
 */
static bool flow_close(mnd_yaml_parser_t *p, flow_stack_t *stack, size_t end)
{
    flow_frame_t *frame = &stack->frames[stack->count - 1];
    mnd_yaml_node_t *node = frame->node;
    mnd_yaml_node_t **items = NULL;
    mnd_yaml_pair_t *pairs = NULL;
    mnd_span_t span;
    size_t bytes;
    size_t i;

    if (frame->count > 0)
    {
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
        }
    }
    if (node->kind == MND_YAML_SEQUENCE)
    {
        node->as.sequence.items = items;
        node->as.sequence.count = frame->count;
    }
    else
    {
        node->as.mapping.pairs = pairs;
        node->as.mapping.count = frame->count;
    }
    span.start = node->span.start;
    span.end = end;
    mnd_yaml_parser_set_span(p, node, span);

    flow_free(p, frame->items);
    flow_free(p, frame->pairs);
    frame->items = NULL;
    frame->pairs = NULL;
    --stack->count;

    return true;
}

/*
 * 空白、タブ、コメント、行の境目を読み飛ばし、*cur を次の文字（または範囲の終わり: line_index == lines.count）に
 * 進める。# は行の先頭か空白の直後でコメントとする。単独の CR ならエラーを報告して false
 */
static bool flow_skip(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *cur)
{
    const char *data = p->data;
    const mnd_yaml_line_t *line = NULL;
    char c;

    while (cur->line_index < p->lines.count)
    {
        line = &p->lines.items[cur->line_index];
        if (cur->offset >= line->content.end)
        {
            ++cur->line_index;
            if (cur->line_index < p->lines.count)
            {
                cur->offset = p->lines.items[cur->line_index].content.start;
            }
            continue;
        }
        c = data[cur->offset];
        if (flow_is_white(c))
        {
            ++cur->offset;
        }
        else if ((c == '#')
                 && ((cur->offset == line->content.start) || flow_is_white(data[cur->offset - 1])))
        {
            cur->offset = line->content.end;
        }
        else if (c == '\r')
        {
            return flow_fail(p, line->line, "carriage return without line feed");
        }
        else
        {
            return true;
        }
    }

    return true;
}

/*
 * 本体: スタックの段を積み降ろししながら 1 文字（または 1 つのスカラー）ずつ読む。
 * 失敗したら診断を追加して false（スタックは呼び出し側が解放する）
 */
static bool flow_run(mnd_yaml_parser_t *p, flow_stack_t *stack, mnd_yaml_cursor_t *cur, mnd_yaml_node_t **out)
{
    const char *data = p->data;
    const mnd_yaml_line_t *line = NULL;
    flow_frame_t *frame = NULL;
    mnd_yaml_node_t *node = NULL;
    size_t line_no;
    char c;

    if (!flow_push(p, stack, *cur))
    {
        return false;
    }
    ++cur->offset;

    for (;;)
    {
        if (!flow_skip(p, cur))
        {
            return false;
        }
        frame = &stack->frames[stack->count - 1];
        if (cur->line_index >= p->lines.count)
        {
            return flow_fail(p, frame->open_line, "unterminated flow collection");
        }
        line = &p->lines.items[cur->line_index];
        line_no = line->line;
        c = data[cur->offset];

        /*
         * 閉じ括弧: 項目・キー・値・区切りを待つどの状態でも閉じられる（末尾の , と値のないキーを含む）
         */
        if (c == flow_closer(frame->node))
        {
            if (frame->state == FLOW_STATE_COLON)
            {
                if (!flow_add_empty_value(p, frame, frame->key->span.end))
                {
                    return false;
                }
            }
            else if (frame->state == FLOW_STATE_VALUE)
            {
                if (!flow_add_empty_value(p, frame, frame->colon_end))
                {
                    return false;
                }
            }
            node = frame->node;
            if (!flow_close(p, stack, cur->offset + 1))
            {
                return false;
            }
            ++cur->offset;
            if (stack->count == 0)
            {
                *out = node;
                return true;
            }

            /* 閉じたコレクションを、下の段の項目または値にする */
            frame = &stack->frames[stack->count - 1];
            if (frame->state == FLOW_STATE_VALUE)
            {
                if (!flow_add_pair(p, frame, node))
                {
                    return false;
                }
            }
            else if (!flow_add_item(p, frame, node))
            {
                return false;
            }
            continue;
        }

        switch (frame->state)
        {
        case FLOW_STATE_SEPARATOR:
            if (c != ',')
            {
                return flow_fail(p, line_no, "invalid flow collection entry");
            }
            ++cur->offset;
            frame->state = FLOW_STATE_ENTRY;
            break;

        case FLOW_STATE_COLON:
            if ((c == ':')
                && (((cur->offset + 1) >= line->content.end) || flow_is_white(data[cur->offset + 1])
                    || flow_is_indicator(data[cur->offset + 1]) || (frame->key->as.scalar.style != MND_YAML_PLAIN)))
            {
                ++cur->offset;
                frame->colon_end = cur->offset;
                frame->state = FLOW_STATE_VALUE;
            }
            else if (c == ',')
            {
                if (!flow_add_empty_value(p, frame, frame->key->span.end))
                {
                    return false;
                }
            }
            else
            {
                return flow_fail(p, line_no, "invalid flow collection entry");
            }
            break;

        case FLOW_STATE_VALUE:
            if (c == ',')
            {
                if (!flow_add_empty_value(p, frame, frame->colon_end))
                {
                    return false;
                }
            }
            else if ((c == '[') || (c == '{'))
            {
                if (!flow_push(p, stack, *cur))
                {
                    return false;
                }
                ++cur->offset;
            }
            else
            {
                if (!mnd_yaml_scalar_parse(p, cur, 0, true, &node) || !flow_add_pair(p, frame, node))
                {
                    return false;
                }
            }
            break;

        case FLOW_STATE_ENTRY:
        default:
            if ((frame->node->kind == MND_YAML_SEQUENCE) && ((c == '[') || (c == '{')))
            {
                if (!flow_push(p, stack, *cur))
                {
                    return false;
                }
                ++cur->offset;
            }
            else if (frame->node->kind == MND_YAML_SEQUENCE)
            {
                if (!mnd_yaml_scalar_parse(p, cur, 0, true, &node) || !flow_add_item(p, frame, node))
                {
                    return false;
                }
            }
            else
            {
                /*
                 * マッピングのキーはスカラーだけ（[ と { はスカラーの走査が不正な項目として報告する）
                 */
                if (!mnd_yaml_scalar_parse(p, cur, 0, true, &node) || !flow_check_duplicate(p, frame, node))
                {
                    return false;
                }
                frame->key = node;
                frame->state = FLOW_STATE_COLON;
            }
            break;
        }
    }
}

bool mnd_yaml_flow_parse(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, mnd_yaml_node_t **out)
{
    flow_stack_t stack;
    mnd_yaml_cursor_t cur = *at;
    mnd_yaml_node_t *node = NULL;
    const mnd_yaml_line_t *line = NULL;
    bool ok;

    if (cur.line_index >= p->lines.count)
    {
        return flow_fail(p, mnd_yaml_parser_line_of(p, cur.offset), "invalid flow collection entry");
    }
    line = &p->lines.items[cur.line_index];
    if ((cur.offset >= line->content.end) || ((p->data[cur.offset] != '[') && (p->data[cur.offset] != '{')))
    {
        return flow_fail(p, line->line, "invalid flow collection entry");
    }

    stack.frames = NULL;
    stack.count = 0;
    stack.cap = 0;
    ok = flow_run(p, &stack, &cur, &node);
    flow_stack_free(p, &stack);
    if (!ok)
    {
        return false;
    }
    *out = node;
    *at = cur;

    return true;
}
