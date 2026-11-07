/*
 * 起動テスト用の補助実行ファイル cli_spec_split（Naos を使わない）。
 * spec.txt の例を 1 つずつファイルに展開する（10.1、10.3〜10.5）。
 *
 *   cli_spec_split <spec.txt> <展開先のディレクトリ>
 *
 * 例ごとに <dir>/<番号>.md（原文）と <dir>/<番号>.crlf.md（LF を CRLF に変換したもの）を書く。
 * 番号は 1 起点で、例の数の桁数（最小 3 桁）に 0 を詰める。展開先のディレクトリは呼び出し側が作る。
 * spec.txt は読むだけで、改変もコピーもしない。成功すれば 0、失敗すれば診断を標準エラーに出して 1 を返す
 */

#include "../markdown/spec_reader.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ファイル名の番号の最小の桁数 */
#define SPEC_SPLIT_MIN_DIGITS 3

static void print_diags(const mnd_diag_list_t *diags)
{
    size_t i;

    for (i = 0; i < mnd_diag_list_count(diags); ++i)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        fprintf(stderr, "cli_spec_split: %s: %s: %s\n",
                (diag->severity == MND_SEVERITY_ERROR) ? "error" : "warning",
                (diag->path != NULL) ? diag->path : "(none)", diag->message);
    }
    if (diags->dropped)
    {
        fprintf(stderr, "cli_spec_split: some diagnostics were dropped (out of memory)\n");
    }
}

/* count を 10 進で書いたときの桁数（最小 SPEC_SPLIT_MIN_DIGITS） */
static int number_width(size_t count)
{
    int digits = 1;

    while (count >= 10u)
    {
        count /= 10u;
        digits++;
    }

    return (digits < SPEC_SPLIT_MIN_DIGITS) ? SPEC_SPLIT_MIN_DIGITS : digits;
}

/* markdown の LF を CRLF に変換して out に入れる（spec.txt に CR はない） */
static bool to_crlf(const mnd_buf_t *markdown, mnd_buf_t *out)
{
    const char *data = mnd_buf_cstr(markdown);
    size_t i;

    mnd_buf_clear(out);
    for (i = 0; i < markdown->len; i++)
    {
        if ((data[i] == '\n') && !mnd_buf_append_byte(out, (unsigned char)'\r'))
        {
            return false;
        }
        if (!mnd_buf_append_byte(out, (unsigned char)data[i]))
        {
            return false;
        }
    }

    return true;
}

/* <dir>/<番号><suffix> のパスを path に作る */
static bool make_path(mnd_buf_t *path, const char *dir, int width, size_t number,
                      const char *suffix)
{
    char name[64];
    int n = snprintf(name, sizeof(name), "%0*lu%s", width, (unsigned long)number, suffix);

    if ((n < 0) || ((size_t)n >= sizeof(name)))
    {
        return false;
    }

    mnd_buf_clear(path);
    return mnd_buf_append_str(path, dir) && mnd_buf_append_byte(path, (unsigned char)'/')
           && mnd_buf_append_str(path, name);
}

/* 1 つの例を原文と CRLF の 2 つのファイルに書く */
static bool write_example(const mnd_md_spec_example_t *ex, const char *dir, int width,
                          mnd_buf_t *path, mnd_buf_t *crlf, mnd_diag_list_t *diags)
{
    if (!make_path(path, dir, width, ex->number, ".md"))
    {
        fprintf(stderr, "cli_spec_split: failed to build the path of example %lu\n",
                (unsigned long)ex->number);
        return false;
    }
    if (!mnd_file_write(mnd_buf_cstr(path), mnd_buf_cstr(&ex->markdown), ex->markdown.len, false,
                        diags))
    {
        return false;
    }

    if (!to_crlf(&ex->markdown, crlf) || !make_path(path, dir, width, ex->number, ".crlf.md"))
    {
        fprintf(stderr, "cli_spec_split: failed to prepare the CRLF version of example %lu\n",
                (unsigned long)ex->number);
        return false;
    }

    return mnd_file_write(mnd_buf_cstr(path), mnd_buf_cstr(crlf), crlf->len, false, diags);
}

int main(int argc, char **argv)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_buf_t crlf = MND_BUF_INIT;
    const char *spec_path;
    const char *dir;
    bool ok;
    size_t i;
    int width;

    if (argc != 3)
    {
        fprintf(stderr, "usage: cli_spec_split <spec.txt> <dir>\n");
        return EXIT_FAILURE;
    }
    spec_path = argv[1];
    dir = argv[2];

    ok = mnd_md_spec_load(spec_path, &spec, &diags);
    if (ok && (spec.count == 0))
    {
        fprintf(stderr, "cli_spec_split: no examples in %s\n", spec_path);
        ok = false;
    }

    width = number_width(spec.count);
    for (i = 0; ok && (i < spec.count); i++)
    {
        ok = write_example(&spec.examples[i], dir, width, &path, &crlf, &diags);
    }

    print_diags(&diags);
    if (ok)
    {
        printf("cli_spec_split: wrote %lu examples to %s\n", (unsigned long)spec.count, dir);
    }
    else
    {
        fprintf(stderr, "cli_spec_split: failed to split %s\n", spec_path);
    }

    mnd_buf_free(&crlf);
    mnd_buf_free(&path);
    mnd_md_spec_free(&spec);
    mnd_diag_list_free(&diags);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
