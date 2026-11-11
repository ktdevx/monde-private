/* extract ハンドラ: .md を読み、ヘッダ付きの翻訳テンプレートを書く */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "cli/cli_io.h"
#include "cli/cli_report.h"
#include "cli/cli_time.h"

#include "clopts/clopts.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_write.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* メモリ不足の診断 */
#define EXTRACT_OUT_OF_MEMORY_MESSAGE "out of memory"

/*
 * --front-matter-keys の項目を、解析結果の文字列を指す配列として *keys に作る（count == 0 なら NULL）。
 * 配列は呼び出し側が free する。メモリ不足なら診断を追加して false
 */
static bool collect_keys(const mnd_args_result_t *result, const char ***keys, size_t *count,
                         mnd_diag_list_t *diags)
{
    size_t n = mnd_args_list_count(result, MND_CLI_OPT_FRONT_MATTER_KEYS);
    const char **items;
    size_t i;

    *keys = NULL;
    *count = 0;
    if (n == 0)
    {
        return true;
    }

    items = (const char **)malloc(n * sizeof(*items));
    if (items == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, EXTRACT_OUT_OF_MEMORY_MESSAGE);
        return false;
    }
    for (i = 0; i < n; i++)
    {
        items[i] = mnd_args_list_at(result, MND_CLI_OPT_FRONT_MATTER_KEYS, i);
    }

    *keys = items;
    *count = n;
    return true;
}

/*
 * catalog の先頭にテンプレート用ヘッダ（作成日時は現在時刻、Project-Id-Version は
 * --project-id-version または既定、--msgid-bugs-address があれば Report-Msgid-Bugs-To）を加える。
 * 失敗なら診断を追加して false
 */
static bool add_header(const mnd_cli_ctx_t *ctx, mnd_po_catalog_t *catalog)
{
    const char *project_id_version = mnd_args_value(ctx->result, MND_CLI_OPT_PROJECT_ID_VERSION);
    const char *bugs_address = mnd_args_value(ctx->result, MND_CLI_OPT_MSGID_BUGS_ADDRESS);
    char now[MND_PO_DATE_SIZE];
    mnd_po_entry_t *header;

    if (!mnd_cli_format_now(ctx->env, now, ctx->diags))
    {
        return false;
    }

    header = mnd_po_catalog_append(catalog);
    if ((header == NULL)
        || !mnd_po_header_template(header, project_id_version, now)
        || ((bugs_address != NULL) && !mnd_po_header_set(header, "Report-Msgid-Bugs-To", bugs_address)))
    {
        mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, EXTRACT_OUT_OF_MEMORY_MESSAGE);
        return false;
    }

    return true;
}

bool mnd_cli_cmd_extract(mnd_cli_ctx_t *ctx)
{
    const char *input_path = mnd_args_positional_at(ctx->result, 0);
    const char *output_path = mnd_args_value(ctx->result, MND_CLI_OPT_OUTPUT);
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_po_write_options_t write_options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t output = MND_BUF_INIT;
    mnd_tu_doc_t *doc = NULL;
    const char **keys = NULL;
    size_t key_count = 0;
    bool ok;

    ok = mnd_cli_read_input(ctx->env, input_path, &input, ctx->diags)
         && collect_keys(ctx->result, &keys, &key_count, ctx->diags);

    if (ok)
    {
        /* 診断のパスと参照のファイル名は指定されたパスのまま（標準入力は "-"） */
        options.path = input.path;
        options.front_matter_keys = keys;
        options.front_matter_key_count = key_count;
        write_options.no_wrap = mnd_args_flag(ctx->result, MND_CLI_OPT_NO_WRAP);
        write_options.eol = mnd_cli_choose_eol(&input.eol);

        ok = mnd_tu_scan(mnd_buf_cstr(&input.content), input.content.len, &options, &doc, ctx->diags)
             && add_header(ctx, &catalog)
             && mnd_tu_extract(doc, input.path, &catalog, ctx->diags)
             && mnd_po_write(&catalog, &write_options, &output, ctx->diags)
             && mnd_cli_write_output(ctx->env, output_path, mnd_buf_cstr(&output), output.len, false,
                                     ctx->diags);
    }

    if (ok)
    {
        /* ヘッダエントリを除いた件数 */
        mnd_cli_report_extract(ctx->env->err, ctx->quiet, catalog.count - 1);
    }

    mnd_buf_free(&output);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
    free(keys);
    mnd_cli_input_free(&input);

    return ok;
}
