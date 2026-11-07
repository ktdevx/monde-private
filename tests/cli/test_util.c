/* cli のテスト用の補助関数（テストコードのみ fopen を直接使う。すべてバイナリモード） */

#include "test_util.h"

#include <stdio.h>
#include <string.h>

/* test_env_getenv が引く表（getenv の差し替えは状態を引数で受け取れないため、ファイル内に持つ） */
static const test_env_var_t *g_env_vars = NULL;
static size_t g_env_count = 0;

/* test_clock_now が返す固定時刻 */
static time_t g_clock_now = (time_t)0;

const char *test_work_dir(void)
{
    return MONDE_CLI_TEST_WORK_DIR;
}

bool test_work_path(const char *name, mnd_buf_t *path)
{
    mnd_buf_clear(path);
    return (mnd_buf_append_str(path, test_work_dir())
            && mnd_buf_append_byte(path, (unsigned char)'/')
            && mnd_buf_append_str(path, name));
}

/* path に bytes の len バイトを書く */
static bool write_path(const char *path, const void *bytes, size_t len)
{
    FILE *fp = fopen(path, "wb");
    bool ok;

    if (fp == NULL)
    {
        return false;
    }

    ok = ((len == 0) || (fwrite(bytes, 1, len, fp) == len));
    if (fclose(fp) != 0)
    {
        ok = false;
    }

    return ok;
}

/* path の内容を読み、out の内容を置き換える */
static bool read_path(const char *path, mnd_buf_t *out)
{
    char chunk[4096];
    FILE *fp = fopen(path, "rb");
    bool ok = true;
    size_t n;

    if (fp == NULL)
    {
        return false;
    }

    mnd_buf_clear(out);
    do
    {
        n = fread(chunk, 1, sizeof chunk, fp);
        if (!mnd_buf_append(out, chunk, n))
        {
            ok = false;
            break;
        }
    } while (n == sizeof chunk);

    if (ferror(fp))
    {
        ok = false;
    }

    fclose(fp);

    return ok;
}

bool test_write_file(const char *name, const void *bytes, size_t len)
{
    mnd_buf_t path = MND_BUF_INIT;
    bool ok = (test_work_path(name, &path) && write_path(mnd_buf_cstr(&path), bytes, len));

    mnd_buf_free(&path);

    return ok;
}

bool test_read_file(const char *name, mnd_buf_t *out)
{
    mnd_buf_t path = MND_BUF_INIT;
    bool ok = (test_work_path(name, &path) && read_path(mnd_buf_cstr(&path), out));

    mnd_buf_free(&path);

    return ok;
}

bool test_file_exists(const char *name)
{
    mnd_buf_t path = MND_BUF_INIT;
    bool exists = false;
    FILE *fp;

    if (test_work_path(name, &path))
    {
        fp = fopen(mnd_buf_cstr(&path), "rb");
        if (fp != NULL)
        {
            exists = true;
            fclose(fp);
        }
    }

    mnd_buf_free(&path);

    return exists;
}

bool test_remove_file(const char *name)
{
    mnd_buf_t path = MND_BUF_INIT;
    bool ok = false;

    if (test_work_path(name, &path))
    {
        ok = ((remove(mnd_buf_cstr(&path)) == 0) || !test_file_exists(name));
    }

    mnd_buf_free(&path);

    return ok;
}

void test_cli_result_free(test_cli_result_t *result)
{
    mnd_buf_free(&result->out);
    mnd_buf_free(&result->err);
    result->exit_code = -1;
}

/* 作業ディレクトリの <name><suffix> のパスを path に書く */
static bool stream_path(const char *name, const char *suffix, mnd_buf_t *path)
{
    return (test_work_path(name, path) && mnd_buf_append_str(path, suffix));
}

/* fp を閉じ、失敗なら ok を偽にする。fp == NULL なら何もしない */
static void close_stream(FILE *fp, bool *ok)
{
    if ((fp != NULL) && (fclose(fp) != 0))
    {
        *ok = false;
    }
}

bool test_cli_run(const char *name, const void *stdin_bytes, size_t stdin_len,
                  int argc, char *const *argv, test_cli_result_t *result)
{
    mnd_buf_t in_path = MND_BUF_INIT;
    mnd_buf_t out_path = MND_BUF_INIT;
    mnd_buf_t err_path = MND_BUF_INIT;
    mnd_cli_env_t env;
    FILE *in = NULL;
    FILE *out = NULL;
    FILE *err = NULL;
    bool ok;

    result->exit_code = -1;
    mnd_buf_clear(&result->out);
    mnd_buf_clear(&result->err);

    ok = (stream_path(name, ".in", &in_path)
          && stream_path(name, ".out", &out_path)
          && stream_path(name, ".err", &err_path)
          && write_path(mnd_buf_cstr(&in_path), stdin_bytes, (stdin_bytes != NULL) ? stdin_len : 0));

    if (ok)
    {
        in = fopen(mnd_buf_cstr(&in_path), "rb");
        out = fopen(mnd_buf_cstr(&out_path), "wb");
        err = fopen(mnd_buf_cstr(&err_path), "wb");
        ok = ((in != NULL) && (out != NULL) && (err != NULL));
    }

    if (ok)
    {
        env.in = in;
        env.out = out;
        env.err = err;
        env.version = TEST_CLI_VERSION;
        env.getenv = test_env_getenv;
        env.now = test_clock_now;
        result->exit_code = mnd_cli_run(&env, argc, argv);
    }

    close_stream(in, &ok);
    close_stream(out, &ok);
    close_stream(err, &ok);

    if (ok)
    {
        ok = (read_path(mnd_buf_cstr(&out_path), &result->out)
              && read_path(mnd_buf_cstr(&err_path), &result->err));
    }

    mnd_buf_free(&in_path);
    mnd_buf_free(&out_path);
    mnd_buf_free(&err_path);

    return ok;
}

void test_env_set(const test_env_var_t *vars, size_t count)
{
    if ((vars == NULL) || (count == 0))
    {
        g_env_vars = NULL;
        g_env_count = 0;
    }
    else
    {
        g_env_vars = vars;
        g_env_count = count;
    }
}

const char *test_env_getenv(const char *name)
{
    size_t i;

    for (i = 0; i < g_env_count; i++)
    {
        if (strcmp(g_env_vars[i].name, name) == 0)
        {
            return g_env_vars[i].value;
        }
    }

    return NULL;
}

void test_clock_set(time_t now)
{
    g_clock_now = now;
}

time_t test_clock_now(void)
{
    return g_clock_now;
}
