/* Delims: 区切り記号の連なりの判定と、強調の処理（方言の区切り記号の規則を含む） */

#include "markdown/md.h"
#include "markdown/md_delim.h"
#include "markdown/md_internal.h"
#include "markdown/md_unicode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 置換文字 U+FFFD */
#define REPLACEMENT_CP 0xFFFDU

/*
 * 探索の下限の組の数（区切り記号の文字 2 種類 × 閉じが開きにもなれるか 2 × 元の長さを 3 で割った余り 3）。
 * 方言の区切り記号の文字（5.2）は、bottom_index とともに組を加える
 */
#define BOTTOM_CHARS 2
#define BOTTOM_COUNT (BOTTOM_CHARS * 2 * 3)

/* 文字の分類 */

/*
 * s[pos..len) の先頭の UTF-8 の文字のコードポイント（入力は検査済みの UTF-8）。spec のとおり U+0000 は
 * U+FFFD（句読点の So）として扱う
 */
static uint32_t decode_at(const unsigned char *s, size_t len, size_t pos)
{
    uint32_t cp = s[pos];
    size_t extra = 0;
    size_t i = 0;

    if (cp == 0U)
    {
        return REPLACEMENT_CP;
    }
    if (cp < 0x80U)
    {
        return cp;
    }
    if (cp >= 0xF0U)
    {
        cp &= 0x07U;
        extra = 3;
    }
    else if (cp >= 0xE0U)
    {
        cp &= 0x0FU;
        extra = 2;
    }
    else
    {
        cp &= 0x1FU;
        extra = 1;
    }

    for (i = 1; (i <= extra) && ((pos + i) < len); i++)
    {
        cp = (cp << 6) | (uint32_t)(s[pos + i] & 0x3FU);
    }

    return cp;
}

/* s[0..pos) の最後の UTF-8 の文字のコードポイント（pos > 0） */
static uint32_t decode_before(const unsigned char *s, size_t len, size_t pos)
{
    size_t start = pos - 1;

    /* 継続バイト（10xxxxxx）を最大 3 つさかのぼる */
    while ((start > 0) && ((pos - start) < 4) && ((s[start] & 0xC0U) == 0x80U))
    {
        start--;
    }

    return decode_at(s, len, start);
}

/* CommonMark の Unicode の空白（Zs と、タブ・LF・FF・CR） */
static bool is_whitespace(uint32_t cp)
{
    return ((cp == '\t') || (cp == '\n') || (cp == '\f') || (cp == '\r') || mnd_md_unicode_is_space(cp));
}

void mnd_md_delim_classify(const char *s, size_t len, size_t start, size_t end, bool intraword,
                           bool *can_open, bool *can_close)
{
    const unsigned char *u = (const unsigned char *)s;
    uint32_t before = ' ';
    uint32_t after = ' ';
    bool before_space = false;
    bool after_space = false;
    bool before_punct = false;
    bool after_punct = false;
    bool left = false;
    bool right = false;

    /* 行の先頭と終わりは空白とみなす（行の境目は text の '\n' で空白になる） */
    if (start > 0)
    {
        before = decode_before(u, len, start);
    }

    if (end < len)
    {
        after = decode_at(u, len, end);
    }

    before_space = is_whitespace(before);
    after_space = is_whitespace(after);
    before_punct = mnd_md_unicode_is_punct(before);
    after_punct = mnd_md_unicode_is_punct(after);

    left = (!after_space && (!after_punct || before_space || before_punct));
    right = (!before_space && (!before_punct || after_space || after_punct));

    if (intraword)
    {
        *can_open = (left && (!right || before_punct));
        *can_close = (right && (!left || after_punct));
    }
    else
    {
        *can_open = left;
        *can_close = right;
    }
}

/* スタック */

bool mnd_md_delim_push(mnd_md_array_t *stack, mnd_md_node_t *node, unsigned char ch, size_t length,
                       bool can_open, bool can_close)
{
    size_t index = stack->count;
    mnd_md_delim_t *delim = (mnd_md_delim_t *)mnd_md_array_push(stack);
    mnd_md_delim_t *items = NULL;

    if (delim == NULL)
    {
        return false;
    }

    delim->node = node;
    delim->ch = ch;
    delim->run.length = length;
    delim->run.original_length = length;
    delim->run.can_open = can_open;
    delim->run.can_close = can_close;
    delim->next = MND_MD_DELIM_NONE;
    delim->prev = MND_MD_DELIM_NONE;

    if (index > 0)
    {
        /* 処理の外では、配列の要素はすべて連結されている */
        items = (mnd_md_delim_t *)stack->items;
        delim->prev = index - 1;
        items[index - 1].next = index;
    }

    return true;
}

/* 強調の処理 */

/* items[index] を連結から外す */
static void unlink_delim(mnd_md_delim_t *items, size_t index)
{
    size_t prev = items[index].prev;
    size_t next = items[index].next;

    if (prev != MND_MD_DELIM_NONE)
    {
        items[prev].next = next;
    }

    if (next != MND_MD_DELIM_NONE)
    {
        items[next].prev = prev;
    }

    items[index].prev = MND_MD_DELIM_NONE;
    items[index].next = MND_MD_DELIM_NONE;
}

/* 区切り記号の文字の探索の下限の組の番号 */
static size_t bottom_index(const mnd_md_delim_t *closer)
{
    size_t ch = (closer->ch == '_') ? 1 : 0;

    return (((ch * 2) + (closer->run.can_open ? 1 : 0)) * 3) + (closer->run.original_length % 3);
}

/* 開きと閉じが 3 の倍数の規則で対にならないか */
static bool odd_match(const mnd_md_delim_t *opener, const mnd_md_delim_t *closer)
{
    return ((closer->run.can_open || opener->run.can_close)
            && ((closer->run.original_length % 3) != 0)
            && (((opener->run.original_length + closer->run.original_length) % 3) == 0));
}

/*
 * 開き items[o] と閉じ items[c] から use 個の記号を使い、その間の兄弟を子にする強調のノードを作って
 * 開きのテキストの直後に置く。メモリ不足なら NULL
 */
static mnd_md_node_t *make_emph(mnd_md_parser_t *parser, mnd_md_delim_t *opener, mnd_md_delim_t *closer,
                                size_t use)
{
    mnd_md_node_t *open_text = opener->node;
    mnd_md_node_t *close_text = closer->node;
    mnd_md_node_t *emph = NULL;
    mnd_md_node_t *child = NULL;
    mnd_md_node_t *next = NULL;
    mnd_span_t span;

    span.start = open_text->span.end - use;
    span.end = close_text->span.start + use;
    emph = mnd_md_node_new(parser->doc, (use == 2) ? MND_MD_STRONG : MND_MD_EMPH, span);
    if (emph == NULL)
    {
        return NULL;
    }

    emph->as.emph.delim = (char)opener->ch;
    emph->as.emph.open.start = span.start;
    emph->as.emph.open.end = open_text->span.end;
    emph->as.emph.close.start = close_text->span.start;
    emph->as.emph.close.end = span.end;
    mnd_md_parser_set_node_lines(parser, emph);

    /* 開きは後ろから、閉じは前から使う */
    open_text->span.end -= use;
    close_text->span.start += use;
    opener->run.length -= use;
    closer->run.length -= use;

    for (child = open_text->next; (child != NULL) && (child != close_text); child = next)
    {
        next = child->next;
        mnd_md_node_append_child(emph, child);
    }

    mnd_md_node_insert_after(open_text, emph);

    return emph;
}

bool mnd_md_delim_process(mnd_md_parser_t *parser, mnd_md_array_t *stack, size_t stack_bottom)
{
    mnd_md_delim_t *items = (mnd_md_delim_t *)stack->items;
    size_t openers_bottom[BOTTOM_COUNT];
    size_t closer = MND_MD_DELIM_NONE;
    size_t opener = MND_MD_DELIM_NONE;
    size_t old_closer = MND_MD_DELIM_NONE;
    size_t next = MND_MD_DELIM_NONE;
    size_t bottom = 0;
    size_t use = 0;
    size_t i = 0;
    bool found = false;

    if (stack_bottom >= stack->count)
    {
        return true;
    }

    /* 探索の下限は「この添字以上の開きだけを探す」値で持つ（spec の openers_bottom は排他の下限の要素） */
    for (i = 0; i < BOTTOM_COUNT; i++)
    {
        openers_bottom[i] = stack_bottom;
    }

    /* 処理の範囲の要素を、下の要素から切り離す */
    items[stack_bottom].prev = MND_MD_DELIM_NONE;

    closer = stack_bottom;
    while (closer != MND_MD_DELIM_NONE)
    {
        if (!items[closer].run.can_close)
        {
            closer = items[closer].next;
            continue;
        }

        bottom = bottom_index(&items[closer]);
        found = false;
        for (opener = items[closer].prev;
             (opener != MND_MD_DELIM_NONE) && (opener >= openers_bottom[bottom]);
             opener = items[opener].prev)
        {
            if ((items[opener].ch == items[closer].ch) && items[opener].run.can_open
                && !odd_match(&items[opener], &items[closer]))
            {
                found = true;
                break;
            }
        }

        old_closer = closer;
        if (!found)
        {
            /* 以後、この組の閉じは old_closer より前の開きを探さない */
            openers_bottom[bottom] = old_closer;
            closer = items[closer].next;

            if (!items[old_closer].run.can_open)
            {
                unlink_delim(items, old_closer);
            }

            continue;
        }

        use = ((items[closer].run.length >= 2) && (items[opener].run.length >= 2)) ? 2 : 1;
        if (make_emph(parser, &items[opener], &items[closer], use) == NULL)
        {
            stack->count = stack_bottom;
            return false;
        }

        /* 開きと閉じの間の要素を取り除く */
        while ((items[opener].next != MND_MD_DELIM_NONE) && (items[opener].next != closer))
        {
            unlink_delim(items, items[opener].next);
        }

        if (items[opener].run.length == 0)
        {
            mnd_md_node_unlink(items[opener].node);
            unlink_delim(items, opener);
        }

        if (items[closer].run.length == 0)
        {
            mnd_md_node_unlink(items[closer].node);
            next = items[closer].next;
            unlink_delim(items, closer);
            closer = next;
        }
    }

    /* 下限より上の要素をすべて取り除く */
    stack->count = stack_bottom;

    return true;
}
