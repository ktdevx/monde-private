/*
 * 互換性テスト用の実行ファイル（Naos を使わない）。
 * Markdown を走査し、テンプレート用ヘッダを付けて抽出した .pot を書き出す（3.9）。
 *
 *   tu_extract_pot <入力の Markdown> <出力の .pot> <#: に書くファイル名> [<フロントマターのキーパス>...]
 *
 * 手順は monde-cli と同じく、mnd_po_header_template でヘッダを先に加えてから mnd_tu_extract を呼び、
 * mnd_po_write で書き出す。成功すれば 0、失敗すれば診断を標準エラーに出して 1 を返す
 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"
#include "po/po.h"
#include "po/po_write.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* 出力を再現できるように作成日時は固定する */
#define EXTRACT_POT_DATE "2026-11-07 12:00+0900"

static void print_diags(const mnd_diag_list_t *diags)
{
    size_t i;

    for (i = 0; i < mnd_diag_list_count(diags); ++i)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        fprintf(stderr, "tu_extract_pot: %s: %s:%lu: %s\n",
                (diag->severity == MND_SEVERITY_ERROR) ? "error" : "warning",
                (diag->path != NULL) ? diag->path : "(none)", (unsigned long)diag->line,
                diag->message);
    }
    if (diags->dropped)
    {
        fprintf(stderr, "tu_extract_pot: some diagnostics were dropped (out of memory)\n");
    }
}

/* ヘッダを加えてから doc を抽出し、catalog に入れる */
static bool build_catalog(const mnd_tu_doc_t *doc, const char *ref_file,
                          mnd_po_catalog_t *catalog, mnd_diag_list_t *diags)
{
    mnd_po_entry_t *header = mnd_po_catalog_append(catalog);

    if ((header == NULL) || !mnd_po_header_template(header, NULL, EXTRACT_POT_DATE))
    {
        fprintf(stderr, "tu_extract_pot: failed to build the header (out of memory)\n");
        return false;
    }

    return mnd_tu_extract(doc, ref_file, catalog, diags);
}

int main(int argc, char **argv)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t content = MND_BUF_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_tu_doc_t *doc = NULL;
    const char *input;
    const char *output;
    const char *ref_file;
    bool had_bom = false;
    bool ok;

    if (argc < 4)
    {
        fprintf(stderr,
                "usage: tu_extract_pot <input.md> <output.pot> <ref-file> [<front-matter-key>...]\n");
        return EXIT_FAILURE;
    }
    input = argv[1];
    output = argv[2];
    ref_file = argv[3];

    options.path = input;
    options.front_matter_keys = (const char *const *)&argv[4];
    options.front_matter_key_count = (size_t)(argc - 4);

    ok = mnd_file_read(input, &content, &had_bom, &diags)
         && mnd_tu_scan(mnd_buf_cstr(&content), content.len, &options, &doc, &diags)
         && build_catalog(doc, ref_file, &catalog, &diags)
         && mnd_po_write(&catalog, NULL, &out, &diags)
         && mnd_file_write(output, mnd_buf_cstr(&out), out.len, false, &diags);
    print_diags(&diags);
    if (ok)
    {
        /* ヘッダを除いたエントリの数を表示する */
        printf("tu_extract_pot: wrote %s (%lu messages)\n", output,
               (unsigned long)(catalog.count - 1));
    }
    else
    {
        fprintf(stderr, "tu_extract_pot: failed to extract %s\n", input);
    }

    mnd_tu_doc_free(doc);
    mnd_po_catalog_free(&catalog);
    mnd_buf_free(&out);
    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
