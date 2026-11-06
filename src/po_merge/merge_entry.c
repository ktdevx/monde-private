/* Builder: エントリの複製と、完全一致・あいまい一致・新規・廃止エントリの組み立て */

#include "po_merge/merge_internal.h"

#include <string.h>

/* fuzzy のフラグの名前 */
static const char merge_entry_fuzzy_flag[] = "fuzzy";

/* src の文字列をすべて順に dst の末尾に複製する */
static bool merge_entry_copy_strings(mnd_po_strings_t *dst, const mnd_po_strings_t *src)
{
    size_t i;

    for (i = 0; i < src->count; i++)
    {
        if (!mnd_po_strings_push(dst, src->items[i].data, src->items[i].len))
        {
            return false;
        }
    }

    return true;
}

/* src の参照をすべて順に dst の末尾に複製する */
static bool merge_entry_copy_refs(mnd_po_refs_t *dst, const mnd_po_refs_t *src)
{
    size_t i;

    for (i = 0; i < src->count; i++)
    {
        if (!mnd_po_refs_push(dst, mnd_buf_cstr(&src->items[i].file), src->items[i].line))
        {
            return false;
        }
    }

    return true;
}

/* src の内容を dst（空）に複製する */
static bool merge_entry_copy_buf(mnd_buf_t *dst, const mnd_buf_t *src)
{
    return mnd_buf_append(dst, src->data, src->len);
}

/*
 * 省略可能な文字列を複製する。src_has が偽なら dst は空のまま *dst_has を偽にする
 * （has_* が偽のときの内容は無視されるので複製しない）
 */
static bool merge_entry_copy_optional(bool *dst_has, mnd_buf_t *dst, bool src_has,
                                      const mnd_buf_t *src)
{
    *dst_has = src_has;
    if (!src_has)
    {
        return true;
    }

    return merge_entry_copy_buf(dst, src);
}

/* msgid_plural の有無と値が異なれば真（複数形の食い違い） */
static bool merge_entry_plural_differs(const mnd_po_entry_t *a, const mnd_po_entry_t *b)
{
    if (a->has_msgid_plural != b->has_msgid_plural)
    {
        return true;
    }

    if (!a->has_msgid_plural)
    {
        return false;
    }

    return (a->msgid_plural.len != b->msgid_plural.len)
        || ((a->msgid_plural.len > 0)
            && (memcmp(a->msgid_plural.data, b->msgid_plural.data, a->msgid_plural.len) != 0));
}

/*
 * src の全要素を dst（初期状態）に複製する。copy_msgstr が偽なら msgstr だけは複製しない。
 * 失敗しても dst の解放は呼び出し側が行う
 */
static bool merge_entry_copy_all(mnd_po_entry_t *dst, const mnd_po_entry_t *src, bool copy_msgstr)
{
    if (!merge_entry_copy_strings(&dst->translator_comments, &src->translator_comments)
        || !merge_entry_copy_strings(&dst->extracted_comments, &src->extracted_comments)
        || !merge_entry_copy_refs(&dst->refs, &src->refs)
        || !merge_entry_copy_strings(&dst->flags, &src->flags)
        || !merge_entry_copy_optional(&dst->has_prev_msgctxt, &dst->prev_msgctxt,
                                      src->has_prev_msgctxt, &src->prev_msgctxt)
        || !merge_entry_copy_optional(&dst->has_prev_msgid, &dst->prev_msgid,
                                      src->has_prev_msgid, &src->prev_msgid)
        || !merge_entry_copy_optional(&dst->has_prev_msgid_plural, &dst->prev_msgid_plural,
                                      src->has_prev_msgid_plural, &src->prev_msgid_plural)
        || !merge_entry_copy_optional(&dst->has_msgctxt, &dst->msgctxt,
                                      src->has_msgctxt, &src->msgctxt)
        || !merge_entry_copy_buf(&dst->msgid, &src->msgid)
        || !merge_entry_copy_optional(&dst->has_msgid_plural, &dst->msgid_plural,
                                      src->has_msgid_plural, &src->msgid_plural)
        || (copy_msgstr && !merge_entry_copy_strings(&dst->msgstr, &src->msgstr)))
    {
        return false;
    }

    dst->obsolete = src->obsolete;
    dst->line = src->line;

    return true;
}

/*
 * combine の要素を dst（初期状態）に組み立てる。失敗しても dst の解放は呼び出し側が行う。
 * msgstr は単数形・複数形が同じときだけ d のものを順に複製する（食い違うときは複数形の後処理で作る）
 */
static bool merge_entry_combine_elements(mnd_po_entry_t *dst, const mnd_po_entry_t *old_entry,
                                         const mnd_po_entry_t *template_entry, bool force_fuzzy)
{
    bool old_fuzzy = mnd_po_flag_has(old_entry, merge_entry_fuzzy_flag);
    bool fuzzy = old_fuzzy || force_fuzzy || merge_entry_plural_differs(old_entry, template_entry);
    size_t i;

    /* msgctxt、msgid、msgid_plural は r から */
    if (!merge_entry_copy_optional(&dst->has_msgctxt, &dst->msgctxt,
                                   template_entry->has_msgctxt, &template_entry->msgctxt)
        || !merge_entry_copy_buf(&dst->msgid, &template_entry->msgid)
        || !merge_entry_copy_optional(&dst->has_msgid_plural, &dst->msgid_plural,
                                      template_entry->has_msgid_plural,
                                      &template_entry->msgid_plural))
    {
        return false;
    }

    /*
     * msgstr は d から（単数形・複数形が食い違えば、ここでは複製せず複数形の後処理で作る）。
     * 翻訳者コメントは d から、抽出コメントと参照は r から
     */
    if (template_entry->has_msgid_plural == old_entry->has_msgid_plural)
    {
        if (!merge_entry_copy_strings(&dst->msgstr, &old_entry->msgstr))
        {
            return false;
        }
    }

    if (!merge_entry_copy_strings(&dst->translator_comments, &old_entry->translator_comments)
        || !merge_entry_copy_strings(&dst->extracted_comments, &template_entry->extracted_comments)
        || !merge_entry_copy_refs(&dst->refs, &template_entry->refs))
    {
        return false;
    }

    /* フラグ: fuzzy なら先頭に fuzzy、その後に r のフラグから fuzzy を除いたものを r の順に */
    if (fuzzy)
    {
        if (!mnd_po_strings_push(&dst->flags, merge_entry_fuzzy_flag,
                                 sizeof(merge_entry_fuzzy_flag) - 1))
        {
            return false;
        }
    }

    for (i = 0; i < template_entry->flags.count; i++)
    {
        const mnd_buf_t *flag = &template_entry->flags.items[i];

        if ((flag->len == (sizeof(merge_entry_fuzzy_flag) - 1))
            && (memcmp(flag->data, merge_entry_fuzzy_flag, flag->len) == 0))
        {
            continue;
        }

        if (!mnd_po_strings_push(&dst->flags, flag->data, flag->len))
        {
            return false;
        }
    }

    /*
     * 前回の msgctxt・msgid・msgid_plural: d が fuzzy なら d の前回の値、
     * そうでなければ d の msgctxt・msgid・msgid_plural（ないものはない）
     */
    if (old_fuzzy)
    {
        if (!merge_entry_copy_optional(&dst->has_prev_msgctxt, &dst->prev_msgctxt,
                                       old_entry->has_prev_msgctxt, &old_entry->prev_msgctxt)
            || !merge_entry_copy_optional(&dst->has_prev_msgid, &dst->prev_msgid,
                                          old_entry->has_prev_msgid, &old_entry->prev_msgid)
            || !merge_entry_copy_optional(&dst->has_prev_msgid_plural, &dst->prev_msgid_plural,
                                          old_entry->has_prev_msgid_plural,
                                          &old_entry->prev_msgid_plural))
        {
            return false;
        }
    }
    else
    {
        if (!merge_entry_copy_optional(&dst->has_prev_msgctxt, &dst->prev_msgctxt,
                                       old_entry->has_msgctxt, &old_entry->msgctxt)
            || !merge_entry_copy_optional(&dst->has_prev_msgid, &dst->prev_msgid,
                                          true, &old_entry->msgid)
            || !merge_entry_copy_optional(&dst->has_prev_msgid_plural, &dst->prev_msgid_plural,
                                          old_entry->has_msgid_plural, &old_entry->msgid_plural))
        {
            return false;
        }
    }

    /* 廃止エントリではなく、入力から作られていないので行番号は 0 */
    dst->obsolete = false;
    dst->line = 0;

    return true;
}

/*
 * list の末尾に data（len バイト）の複製を count 個加える。配列の拡大は po.h の
 * mnd_po_strings_push に任せ、確保のオーバーフローの検査もその中で行われる
 * （count に使う nplurals は Header が 100 以下に制限している）
 */
static bool merge_entry_push_copies(mnd_po_strings_t *list, const char *data, size_t len,
                                    size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (!mnd_po_strings_push(list, data, len))
        {
            return false;
        }
    }

    return true;
}

/*
 * 複数形の後処理。r と d の単数形・複数形が同じなら、msgstr は組み立てで d から複製済みなので
 * 何もしない。食い違えば組み立ては msgstr を複製していないので、r が複数形（d が単数形）なら
 * d の最初の msgstr（なければ空文字列）の複製を nplurals 個、r が単数形（d が複数形）なら
 * d の最初の msgstr（なければ空文字列）を 1 個加える。どちらの場合も複数形の食い違いとして
 * 組み立てで fuzzy になっている
 */
static bool merge_entry_adjust_plural(mnd_po_entry_t *dst, const mnd_po_entry_t *old_entry,
                                      const mnd_po_entry_t *template_entry, size_t nplurals)
{
    const char *first = "";
    size_t first_len = 0;
    size_t count;

    if (template_entry->has_msgid_plural == old_entry->has_msgid_plural)
    {
        return true;
    }

    count = template_entry->has_msgid_plural ? nplurals : 1;
    if (old_entry->msgstr.count >= 1)
    {
        first = old_entry->msgstr.items[0].data;
        first_len = old_entry->msgstr.items[0].len;
    }

    return merge_entry_push_copies(&dst->msgstr, first, first_len, count);
}

/* msgstr が 1 つも空でない文字列を含まなければ真（msgstr が 1 個もない場合を含む） */
static bool merge_entry_all_msgstr_empty(const mnd_po_entry_t *entry)
{
    size_t i;

    for (i = 0; i < entry->msgstr.count; i++)
    {
        if (entry->msgstr.items[i].len > 0)
        {
            return false;
        }
    }

    return true;
}

/* has と buf の前回の値を消す */
static void merge_entry_drop_optional(bool *has, mnd_buf_t *buf)
{
    *has = false;
    mnd_buf_free(buf);
}

bool mnd_merge_entry_copy(mnd_po_entry_t *dst, const mnd_po_entry_t *src)
{
    if (!merge_entry_copy_all(dst, src, true))
    {
        mnd_po_entry_free(dst);
        return false;
    }

    return true;
}

bool mnd_merge_combine(mnd_po_entry_t *dst, const mnd_po_entry_t *old_entry,
                       const mnd_po_entry_t *template_entry, bool force_fuzzy, size_t nplurals)
{
    if (!merge_entry_combine_elements(dst, old_entry, template_entry, force_fuzzy)
        || !merge_entry_adjust_plural(dst, old_entry, template_entry, nplurals))
    {
        mnd_po_entry_free(dst);
        return false;
    }

    return true;
}

bool mnd_merge_has_translation(const mnd_po_entry_t *entry)
{
    return (entry->msgstr.count >= 1) && (entry->msgstr.items[0].len > 0);
}

bool mnd_merge_fresh(mnd_po_entry_t *dst, const mnd_po_entry_t *template_entry, size_t nplurals)
{
    /*
     * 複数形で msgstr がすべて空なら、msgstr を空文字列 nplurals 個にする。msgmerge と同じく
     * 「空でない msgstr がない」で判定するので、msgstr が 1 個もない場合も含む
     */
    bool fill_empty = template_entry->has_msgid_plural
                      && merge_entry_all_msgstr_empty(template_entry);

    if (!merge_entry_copy_all(dst, template_entry, !fill_empty)
        || (fill_empty && !merge_entry_push_copies(&dst->msgstr, "", 0, nplurals)))
    {
        mnd_po_entry_free(dst);
        return false;
    }

    /* 廃止エントリではなく、入力から作られていないので行番号は 0 */
    dst->obsolete = false;
    dst->line = 0;

    return true;
}

bool mnd_merge_obsolete(mnd_po_entry_t *dst, const mnd_po_entry_t *old_entry)
{
    mnd_po_entry_t source;

    /*
     * 抽出コメントと参照だけを除いた浅い写しを複製する（写しは old_entry の領域を指すだけで、
     * 解放しない）
     */
    source = *old_entry;
    source.extracted_comments.items = NULL;
    source.extracted_comments.count = 0;
    source.extracted_comments.cap = 0;
    source.refs.items = NULL;
    source.refs.count = 0;
    source.refs.cap = 0;

    if (!merge_entry_copy_all(dst, &source, true))
    {
        mnd_po_entry_free(dst);
        return false;
    }

    /* 廃止エントリとして出力し、入力から作られていないので行番号は 0 */
    dst->obsolete = true;
    dst->line = 0;

    return true;
}

void mnd_merge_tidy_previous(mnd_po_entry_t *entry)
{
    if (mnd_po_flag_has(entry, merge_entry_fuzzy_flag) && mnd_merge_has_translation(entry))
    {
        return;
    }

    merge_entry_drop_optional(&entry->has_prev_msgctxt, &entry->prev_msgctxt);
    merge_entry_drop_optional(&entry->has_prev_msgid, &entry->prev_msgid);
    merge_entry_drop_optional(&entry->has_prev_msgid_plural, &entry->prev_msgid_plural);
}
