#include "foundation/file_io.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "foundation/source.h"
#include "foundation/utf8.h"

/* 1 回の fread で読むバイト数 */
#define MND_FILE_IO_CHUNK_SIZE 4096

static const char mnd_file_io_bom[3] = { '\xEF', '\xBB', '\xBF' };

/* path をバイナリモードなどの mode で開く。ファイルを開くのはこの関数だけ */
static FILE *mnd_file_open(const char *path, const char *mode)
{
    return fopen(path, mode);
}

/* errno の値の説明を返す。errno が設定されていなければ固定の説明を返す */
static const char *mnd_file_io_reason(int err)
{
    return err != 0 ? strerror(err) : "unknown error";
}

/* 失敗時の出力（内容を空、BOM なし）にそろえる */
static bool mnd_file_read_fail(mnd_buf *content, bool *had_bom)
{
    mnd_buf_clear(content);
    *had_bom = false;
    return false;
}

bool mnd_file_read(const char *path, mnd_buf *content, bool *had_bom,
                   mnd_diag_list *diags)
{
    char chunk[MND_FILE_IO_CHUNK_SIZE];
    bool first = true;
    bool bom = false;
    size_t invalid_offset = 0;
    size_t line = 0;
    FILE *fp;

    errno = 0;
    fp = mnd_file_open(path, "rb");
    if (fp == NULL) {
        int err = errno;
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0,
                     "cannot open file for reading: %s", mnd_file_io_reason(err));
        return mnd_file_read_fail(content, had_bom);
    }

    mnd_buf_clear(content);
    for (;;) {
        size_t n;
        size_t skip = 0;

        errno = 0;
        n = fread(chunk, 1, sizeof chunk, fp);
        if (n < sizeof chunk && ferror(fp)) {
            int err = errno;
            fclose(fp);
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0,
                         "cannot read file: %s", mnd_file_io_reason(err));
            return mnd_file_read_fail(content, had_bom);
        }
        /* fread は EOF かエラーでしか要求より少なく返さないので、BOM は最初のチャンクで判定できる */
        if (first) {
            first = false;
            if (n >= sizeof mnd_file_io_bom
                && memcmp(chunk, mnd_file_io_bom, sizeof mnd_file_io_bom) == 0) {
                bom = true;
                skip = sizeof mnd_file_io_bom;
            }
        }
        if (!mnd_buf_append(content, chunk + skip, n - skip)) {
            fclose(fp);
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0,
                         "out of memory while reading file");
            return mnd_file_read_fail(content, had_bom);
        }
        if (n < sizeof chunk) {
            break;
        }
    }
    fclose(fp);

    if (!mnd_utf8_validate(content->data, content->len, &invalid_offset)) {
        /* invalid_offset <= len なので行番号は必ず求まる */
        (void)mnd_source_line_of(content->data, content->len, invalid_offset, &line);
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, line,
                     "invalid UTF-8 byte sequence at byte offset %zu", invalid_offset);
        return mnd_file_read_fail(content, had_bom);
    }

    *had_bom = bom;
    return true;
}

bool mnd_file_write(const char *path, const char *data, size_t len, bool add_bom,
                    mnd_diag_list *diags)
{
    bool ok = true;
    int err = 0;
    FILE *fp;

    errno = 0;
    fp = mnd_file_open(path, "wb");
    if (fp == NULL) {
        err = errno;
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0,
                     "cannot open file for writing: %s", mnd_file_io_reason(err));
        return false;
    }

    errno = 0;
    if (add_bom
        && fwrite(mnd_file_io_bom, 1, sizeof mnd_file_io_bom, fp) != sizeof mnd_file_io_bom) {
        ok = false;
        err = errno;
    }
    if (ok && len > 0 && fwrite(data, 1, len, fp) != len) {
        ok = false;
        err = errno;
    }
    /* 書き込みに失敗しても閉じる。バッファされた内容の書き出しの失敗は close で分かる */
    errno = 0;
    if (fclose(fp) != 0 && ok) {
        ok = false;
        err = errno;
    }
    if (!ok) {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0,
                     "cannot write file: %s", mnd_file_io_reason(err));
    }
    return ok;
}
