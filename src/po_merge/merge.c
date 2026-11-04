/* Merge: 入力の検査、各段階の呼び出し、出力の順序、統計、失敗時の後始末 */

#include "po_merge/merge.h"
#include "po_merge/merge_internal.h"

#include <stdlib.h>

/* 診断に表示する値（msgctxt、msgid）の最大のバイト数（... を除く） */
#define MERGE_SHOW_MAX 40

/* マージの 1 回分の状態 */
typedef struct merge_state
{
    const mnd_po_catalog_t *template_catalog;  /* T */
    const mnd_po_catalog_t *old_catalog;       /* O */
    mnd_po_catalog_t *out;
    mnd_diag_list_t *diags;
    mnd_merge_key_index_t old_index;  /* O のキー → O の添字（通常のエントリが優先） */
    bool has_old_index;               /* old_index を初期化した（解放が必要） */
    bool *used;                       /* O の添字ごとの「使われた」（O が空なら NULL） */
    size_t nplurals;                  /* O のヘッダの Plural-Forms から求めた nplurals */
    mnd_merge_stats_t stats;          /* 成功時に呼び出し側へ返す統計 */
} merge_state_t;

/* UTF-8 の先頭のバイトから文字のバイト数を返す（入力は正しい UTF-8） */
static size_t merge_char_len(unsigned char c)
{
    if (c < 0x80)
    {
        return 1;
    }

    if (c >= 0xF0)
    {
        return 4;
    }

    if (c >= 0xE0)
    {
        return 3;
    }

    if (c >= 0xC0)
    {
        return 2;
    }

    return 1;
}

/*
 * value を診断に表示する形にして show（空）に書く。制御文字（C0、DEL、C1）を ? に置き換え、
 * 置き換えた後で 40 バイトを超える前の文字の境界で切り、切った場合は ... を付ける。
 * false はメモリ不足
 */
static bool merge_show(const mnd_buf_t *value, mnd_buf_t *show)
{
    const char *s = mnd_buf_cstr(value);
    size_t len = value->len;
    size_t i = 0;
    size_t n = 0;

    while (i < len)
    {
        unsigned char c = (unsigned char)s[i];
        size_t char_len = merge_char_len(c);
        bool control = (c < 0x20) || (c == 0x7F)
                       || ((c == 0xC2) && ((i + 1) < len) && ((unsigned char)s[i + 1] < 0xA0));
        size_t shown_len;
        bool ok;

        if (char_len > (len - i))
        {
            char_len = len - i;
        }

        shown_len = control ? 1 : char_len;
        if ((n + shown_len) > MERGE_SHOW_MAX)
        {
            break;
        }

        if (control)
        {
            ok = mnd_buf_append_byte(show, '?');
        }
        else
        {
            ok = mnd_buf_append(show, s + i, char_len);
        }

        if (!ok)
        {
            return false;
        }

        n += shown_len;
        i += char_len;
    }

    if ((i < len) && (!mnd_buf_append_str(show, "...")))
    {
        return false;
    }

    return true;
}

/*
 * catalog の later のエントリが first のエントリと同じキーであることを報告する。
 * 値は常に %s の引数として渡す。先のエントリの行番号がわかれば括弧の部分を続ける。
 * false はメモリ不足（診断は追加しない）
 */
static bool merge_report_duplicate(const mnd_po_catalog_t *catalog, const char *path,
                                   size_t first, size_t later, mnd_diag_list_t *diags)
{
    const mnd_po_entry_t *entry = &catalog->entries[later];
    size_t first_line = catalog->entries[first].line;
    mnd_buf_t context = MND_BUF_INIT;
    mnd_buf_t id = MND_BUF_INIT;
    bool ok = true;

    if (entry->has_msgctxt)
    {
        ok = merge_show(&entry->msgctxt, &context);
    }

    ok = ok && merge_show(&entry->msgid, &id);
    if (ok)
    {
        if (entry->has_msgctxt && (first_line > 0))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, entry->line,
                         "duplicate message definition: msgctxt \"%s\", msgid \"%s\""
                         " (first defined at line %zu)",
                         mnd_buf_cstr(&context), mnd_buf_cstr(&id), first_line);
        }
        else if (entry->has_msgctxt)
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, entry->line,
                         "duplicate message definition: msgctxt \"%s\", msgid \"%s\"",
                         mnd_buf_cstr(&context), mnd_buf_cstr(&id));
        }
        else if (first_line > 0)
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, entry->line,
                         "duplicate message definition: msgid \"%s\" (first defined at line %zu)",
                         mnd_buf_cstr(&id), first_line);
        }
        else
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, entry->line,
                         "duplicate message definition: msgid \"%s\"", mnd_buf_cstr(&id));
        }
    }

    mnd_buf_free(&id);
    mnd_buf_free(&context);

    return ok;
}

/*
 * 手順 1: catalog の廃止エントリでないエントリのキーの重複をすべて、後のエントリについて報告する。
 * 重複があれば *found を真にする。false はメモリ不足
 */
static bool merge_check_duplicates(const mnd_po_catalog_t *catalog, const char *path,
                                   mnd_diag_list_t *diags, bool *found)
{
    mnd_merge_key_index_t index;
    size_t i;
    bool ok;

    ok = mnd_merge_key_index_init(&index, catalog);
    for (i = 0; ok && (i < catalog->count); i++)
    {
        bool added = false;
        size_t existing = 0;

        if (catalog->entries[i].obsolete)
        {
            continue;
        }

        ok = mnd_merge_key_index_add(&index, i, &added, &existing);
        if (ok && (!added))
        {
            *found = true;
            ok = merge_report_duplicate(catalog, path, existing, i, diags);
        }
    }

    mnd_merge_key_index_free(&index);

    return ok;
}

/*
 * 手順 2: O の索引に、ヘッダ以外の廃止エントリでないエントリを先に、廃止エントリを後に登録する。
 * 登録済みのキーは上書きしない。「使われた」の配列を用意し、nplurals を求める。false はメモリ不足
 */
static bool merge_index_old(merge_state_t *state)
{
    const mnd_po_catalog_t *old_catalog = state->old_catalog;
    const mnd_po_entry_t *header = NULL;
    size_t header_index = 0;
    bool has_header = mnd_po_catalog_header(old_catalog, &header_index);
    size_t pass;
    size_t i;

    if (has_header)
    {
        header = &old_catalog->entries[header_index];
    }

    state->nplurals = mnd_merge_nplurals(header);

    if (old_catalog->count > 0)
    {
        state->used = calloc(old_catalog->count, sizeof(*state->used));
        if (state->used == NULL)
        {
            return false;
        }
    }

    if (!mnd_merge_key_index_init(&state->old_index, old_catalog))
    {
        mnd_merge_key_index_free(&state->old_index);

        return false;
    }

    state->has_old_index = true;

    /* pass 0 で廃止エントリでないもの、pass 1 で廃止エントリを登録する */
    for (pass = 0; pass < 2; pass++)
    {
        bool want_obsolete = (pass == 1);

        for (i = 0; i < old_catalog->count; i++)
        {
            bool added = false;
            size_t existing = 0;

            if (old_catalog->entries[i].obsolete != want_obsolete)
            {
                continue;
            }

            if (has_header && (i == header_index))
            {
                continue;
            }

            if (!mnd_merge_key_index_add(&state->old_index, i, &added, &existing))
            {
                return false;
            }
        }
    }

    return true;
}

/*
 * 手順 3: O にヘッダがあれば、O のヘッダと T のヘッダ（なければ NULL）から結果のヘッダを作って
 * 出力の先頭に置き、O のヘッダを「使われた」とする。O にヘッダがなければ何もしない。
 * false はメモリ不足
 */
static bool merge_place_header(merge_state_t *state)
{
    const mnd_po_entry_t *template_header = NULL;
    size_t old_header_index = 0;
    size_t template_header_index = 0;
    mnd_po_entry_t *dst;

    if (!mnd_po_catalog_header(state->old_catalog, &old_header_index))
    {
        return true;
    }

    if (mnd_po_catalog_header(state->template_catalog, &template_header_index))
    {
        template_header = &state->template_catalog->entries[template_header_index];
    }

    dst = mnd_po_catalog_append(state->out);
    if (dst == NULL)
    {
        return false;
    }

    if (!mnd_merge_header(dst, &state->old_catalog->entries[old_header_index], template_header))
    {
        return false;
    }

    state->used[old_header_index] = true;

    return true;
}

/* 統計を 0 にする（NULL 可） */
static void merge_stats_clear(mnd_merge_stats_t *stats)
{
    if (stats != NULL)
    {
        stats->matched = 0;
        stats->fuzzy = 0;
        stats->added = 0;
        stats->obsoleted = 0;
    }
}

/* 手順 2 以降（索引、ヘッダ）を順に行う。false はメモリ不足 */
static bool merge_run(merge_state_t *state)
{
    if (!merge_index_old(state))
    {
        return false;
    }

    if (!merge_place_header(state))
    {
        return false;
    }

    return true;
}

bool mnd_merge_catalogs(const mnd_po_catalog_t *template_catalog, const char *template_path,
                        const mnd_po_catalog_t *old_catalog, const char *old_path,
                        const mnd_merge_options_t *options,
                        mnd_po_catalog_t *out, mnd_merge_stats_t *stats,
                        mnd_diag_list_t *diags)
{
    merge_state_t state;
    bool found = false;
    bool ok;

    (void)options;

    state.template_catalog = template_catalog;
    state.old_catalog = old_catalog;
    state.out = out;
    state.diags = diags;
    state.has_old_index = false;
    state.used = NULL;
    state.nplurals = MND_MERGE_NPLURALS_DEFAULT;
    merge_stats_clear(&state.stats);

    /* 手順 1: 両方の入力の重複をすべて報告するため、T に重複があっても O も調べる */
    ok = merge_check_duplicates(template_catalog, template_path, diags, &found)
         && merge_check_duplicates(old_catalog, old_path, diags, &found);
    if (ok && (!found))
    {
        ok = merge_run(&state);
    }

    if (!ok)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");
    }

    if (state.has_old_index)
    {
        mnd_merge_key_index_free(&state.old_index);
    }

    free(state.used);

    if ((!ok) || found)
    {
        mnd_po_catalog_free(out);
        mnd_po_catalog_init(out);
        merge_stats_clear(stats);

        return false;
    }

    if (stats != NULL)
    {
        *stats = state.stats;
    }

    return true;
}
