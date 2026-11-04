/*
 * 互換性テスト用の実行ファイル（Naos を使わない）。
 * msgfmt --check にかける PO を MND_PO_MSGFMT_OUT_DIR に書く（5.6、9.6）。
 *   template.pot  テンプレート用ヘッダ、日本語の長い msgid、msgctxt、改行を含む msgid、参照を持つテンプレート
 *   init_ja.po    template.pot を mnd_po_init で初期化し、一部を翻訳済み・fuzzy・廃止エントリにしたもの
 *   basic.po      正解ファイル basic.po を読み込んで書き出したもの（複数形と Plural-Forms を含む）
 * 成功すれば 0、失敗すれば診断を標準エラーに出して 1 を返す。
 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"
#include "po/po.h"
#include "po/po_init.h"
#include "po/po_read.h"
#include "po/po_write.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MND_PO_MSGFMT_OUT_DIR
#error "MND_PO_MSGFMT_OUT_DIR must be defined"
#endif
#ifndef MND_PO_TEST_DATA_DIR
#error "MND_PO_TEST_DATA_DIR must be defined"
#endif

#define GEN_DATE "2026-11-04 12:34+0900"

/* 日本語の長い msgid（79 桁を超えて折り返される） */
#define GEN_LONG_JA \
    "ファイル %s を開けませんでした。パスが正しいこと、読み取りの権限があること、" \
    "ほかのプログラムがファイルを使っていないことを確かめてから、もう一度やり直してください。"

/* テンプレートのエントリ（msgctxt が NULL なら msgctxt なし） */
typedef struct gen_message
{
    const char *msgctxt;
    const char *msgid;
    const char *ref_file;
    size_t ref_line;
    const char *flag;  /* NULL ならフラグなし */
} gen_message_t;

static const gen_message_t gen_messages[] = {
    { NULL, GEN_LONG_JA, "src/ファイル.c", 120, "c-format" },
    { "menu", "Open", "src/menu.c", 10, NULL },
    { "verb", "Open", "src/menu.c", 20, NULL },
    { NULL, "First line\nSecond line\n", "src/help.c", 30, NULL },
    { NULL, "Draft message", "src/draft.c", 40, NULL },
    { NULL, "Removed message", "src/old.c", 50, NULL },
    { NULL, "Untranslated message", "src/rest.c", 60, NULL },
};

static void print_diags(const mnd_diag_list_t *diags)
{
    size_t i;

    for (i = 0; i < mnd_diag_list_count(diags); ++i)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        fprintf(stderr, "msgfmt_gen: %s: %s:%lu: %s\n",
                (diag->severity == MND_SEVERITY_ERROR) ? "error" : "warning",
                (diag->path != NULL) ? diag->path : "(none)", (unsigned long)diag->line,
                diag->message);
    }
    if (diags->dropped)
    {
        fprintf(stderr, "msgfmt_gen: some diagnostics were dropped (out of memory)\n");
    }
}

/* catalog を既定の設定で書き出し、MND_PO_MSGFMT_OUT_DIR/name に書く */
static bool write_catalog(const mnd_po_catalog_t *catalog, const char *name)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_buf_t path = MND_BUF_INIT;
    bool ok;

    ok = mnd_po_write(catalog, NULL, &out, &diags)
         && mnd_buf_append_str(&path, MND_PO_MSGFMT_OUT_DIR "/")
         && mnd_buf_append_str(&path, name)
         && mnd_file_write(mnd_buf_cstr(&path), mnd_buf_cstr(&out), out.len, false, &diags);
    if (ok)
    {
        printf("msgfmt_gen: wrote %s\n", mnd_buf_cstr(&path));
    }
    else
    {
        fprintf(stderr, "msgfmt_gen: failed to write %s\n", name);
    }
    print_diags(&diags);
    mnd_buf_free(&path);
    mnd_buf_free(&out);
    mnd_diag_list_free(&diags);

    return ok;
}

static bool set_msgstr(mnd_po_entry_t *entry, const char *msgstr)
{
    mnd_buf_t *item = &entry->msgstr.items[0];

    mnd_buf_clear(item);
    return mnd_buf_append_str(item, msgstr);
}

/* テンプレートのカタログを作る */
static bool build_template(mnd_po_catalog_t *catalog)
{
    mnd_po_entry_t *entry;
    size_t i;

    entry = mnd_po_catalog_append(catalog);
    if ((entry == NULL) || !mnd_po_header_template(entry, "demo 1.0", GEN_DATE)
        || !mnd_po_strings_push(&entry->translator_comments, "SOME DESCRIPTIVE TITLE.",
                                strlen("SOME DESCRIPTIVE TITLE.")))
    {
        return false;
    }

    for (i = 0; i < (sizeof(gen_messages) / sizeof(gen_messages[0])); ++i)
    {
        const gen_message_t *message = &gen_messages[i];

        entry = mnd_po_catalog_append(catalog);
        if ((entry == NULL)
            || !mnd_buf_append_str(&entry->msgid, message->msgid)
            || !mnd_po_strings_push(&entry->msgstr, "", 0)
            || !mnd_po_refs_push(&entry->refs, message->ref_file, message->ref_line)
            || ((message->flag != NULL) && !mnd_po_flag_add(entry, message->flag)))
        {
            return false;
        }
        if (message->msgctxt != NULL)
        {
            entry->has_msgctxt = true;
            if (!mnd_buf_append_str(&entry->msgctxt, message->msgctxt))
            {
                return false;
            }
        }
    }

    return true;
}

/* find で見つけたエントリを返す。見つからなければ NULL */
static mnd_po_entry_t *find_entry(mnd_po_catalog_t *catalog, const char *msgctxt,
                                  const char *msgid)
{
    size_t index;

    if (!mnd_po_catalog_find(catalog, msgctxt, msgid, &index))
    {
        fprintf(stderr, "msgfmt_gen: entry not found: %s\n", msgid);
        return NULL;
    }
    return &catalog->entries[index];
}

/* 初期化したカタログの一部を、翻訳済み・fuzzy・廃止エントリにする */
static bool translate(mnd_po_catalog_t *catalog)
{
    mnd_po_entry_t *entry;

    entry = find_entry(catalog, NULL, GEN_LONG_JA);
    if ((entry == NULL)
        || !set_msgstr(entry,
                       "ファイル %s を開けませんでした。パス、読み取りの権限、"
                       "ほかのプログラムによる使用を確かめてから、もう一度やり直してください。"))
    {
        return false;
    }
    entry = find_entry(catalog, "menu", "Open");
    if ((entry == NULL) || !set_msgstr(entry, "開く"))
    {
        return false;
    }
    entry = find_entry(catalog, NULL, "First line\nSecond line\n");
    if ((entry == NULL) || !set_msgstr(entry, "1 行目\n2 行目\n"))
    {
        return false;
    }
    entry = find_entry(catalog, NULL, "Draft message");
    if ((entry == NULL) || !set_msgstr(entry, "下書きのメッセージ") || !mnd_po_flag_add(entry, "fuzzy"))
    {
        return false;
    }
    entry = find_entry(catalog, NULL, "Removed message");
    if ((entry == NULL) || !set_msgstr(entry, "削除されたメッセージ"))
    {
        return false;
    }
    entry->obsolete = true;

    return true;
}

static bool gen_template_and_init(void)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    ok = build_template(&catalog);
    if (!ok)
    {
        fprintf(stderr, "msgfmt_gen: failed to build the template (out of memory)\n");
    }
    ok = ok && write_catalog(&catalog, "template.pot");
    if (ok)
    {
        ok = mnd_po_init(&catalog, "ja", GEN_DATE, &diags);
        print_diags(&diags);
        if (!ok)
        {
            fprintf(stderr, "msgfmt_gen: mnd_po_init failed\n");
        }
    }
    ok = ok && translate(&catalog) && write_catalog(&catalog, "init_ja.po");
    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&catalog);

    return ok;
}

static bool gen_basic(void)
{
    static const char path[] = MND_PO_TEST_DATA_DIR "/basic.po";
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t content = MND_BUF_INIT;
    bool had_bom;
    bool ok;

    ok = mnd_file_read(path, &content, &had_bom, &diags)
         && mnd_po_read(mnd_buf_cstr(&content), content.len, path, &catalog, &diags);
    print_diags(&diags);
    if (!ok)
    {
        fprintf(stderr, "msgfmt_gen: failed to read %s\n", path);
    }
    ok = ok && write_catalog(&catalog, "basic.po");
    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&catalog);

    return ok;
}

int main(void)
{
    bool ok = gen_template_and_init();

    ok = gen_basic() && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
