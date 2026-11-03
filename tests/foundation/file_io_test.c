#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"

/* 作業ディレクトリ内のファイル名からパスを組み立てる */
#define WORK_PATH(name) MONDE_TEST_WORK_DIR "/" name

/* 文字列リテラルの内容（終端の NUL を除く）をファイルに書く */
#define WRITE_LITERAL(path, literal) \
    write_bytes((path), (literal), sizeof(literal) - 1)

/* 読み込みテストの入力ファイルをバイナリモードで作る（テストコードのみ fopen を直接使う） */
static bool write_bytes(const char *path, const char *bytes, size_t len)
{
    FILE *fp = fopen(path, "wb");
    bool ok;

    if (fp == NULL) {
        return false;
    }
    ok = (len == 0 || fwrite(bytes, 1, len, fp) == len);
    if (fclose(fp) != 0) {
        ok = false;
    }
    return ok;
}

NAOS_TEST(read_keeps_crlf_mixed_lf_and_nul_bytes)
{
    static const char path[] = WORK_PATH("read_bytes.txt");
    static const char bytes[] = "a\r\nb\nc\0d\r\n\re";
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, bytes));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_UINT_EQ(sizeof(bytes) - 1, content.len);
    NAOS_CHECK_MEM_EQ(bytes, sizeof(bytes) - 1, content.data, content.len);
    NAOS_CHECK_INT_EQ('\0', content.data[content.len]);

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_strips_bom_and_reports_it)
{
    static const char path[] = WORK_PATH("read_bom.txt");
    static const char bytes[] = "\xEF\xBB\xBF" "abc\r\n";
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = false;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, bytes));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_TRUE(had_bom);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_UINT_EQ(5, content.len);
    NAOS_CHECK_MEM_EQ("abc\r\n", 5, content.data, content.len);

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_bom_only_file_is_empty_with_bom)
{
    static const char path[] = WORK_PATH("read_bom_only.txt");
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = false;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, "\xEF\xBB\xBF"));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_TRUE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_without_bom_keeps_content)
{
    static const char path[] = WORK_PATH("read_no_bom.txt");
    /* 先頭以外にある EF BB BF（U+FEFF）は BOM ではないので残す */
    static const char bytes[] = "a\xEF\xBB\xBF" "b";
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, "plain\n"));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_REQUIRE_UINT_EQ(6, content.len);
    NAOS_CHECK_MEM_EQ("plain\n", 6, content.data, content.len);

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, bytes));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_MEM_EQ(bytes, sizeof(bytes) - 1, content.data, content.len);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_empty_file_succeeds_with_length_zero)
{
    static const char path[] = WORK_PATH("read_empty.txt");
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(write_bytes(path, NULL, 0));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&content));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_replaces_existing_content)
{
    static const char path[] = WORK_PATH("read_replace.txt");
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = false;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&content, "previous content"));
    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, "new"));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_REQUIRE_UINT_EQ(3, content.len);
    NAOS_CHECK_STR_EQ("new", mnd_buf_cstr(&content));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_file_larger_than_chunk)
{
    static const char path[] = WORK_PATH("read_large.txt");
    /* 読み込みのチャンク（数 KiB）を複数回またぐ大きさ */
    static char bytes[70000];
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;
    size_t i;

    for (i = 0; i < sizeof(bytes); i++) {
        bytes[i] = (i % 61 == 60) ? '\n' : (char)('A' + (int)(i % 26));
    }
    NAOS_REQUIRE_TRUE(write_bytes(path, bytes, sizeof(bytes)));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_REQUIRE_UINT_EQ(sizeof(bytes), content.len);
    NAOS_CHECK_MEM_EQ(bytes, sizeof(bytes), content.data, content.len);
    NAOS_CHECK_INT_EQ('\0', content.data[content.len]);

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_invalid_utf8_reports_path_offset_and_line)
{
    static const char path[] = WORK_PATH("read_invalid.txt");
    /* 2 行目の "cd" の後（オフセット 6）に不正なバイト FF を置く */
    static const char bytes[] = "ab\r\ncd\xFF" "ef\n";
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag *d;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&content, "stale"));
    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, bytes));
    NAOS_CHECK_FALSE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));

    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
    NAOS_CHECK_UINT_EQ(2, d->line);
    NAOS_CHECK_STR_EQ("invalid UTF-8 byte sequence at byte offset 6", d->message);
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_invalid_utf8_offset_is_after_bom)
{
    static const char path[] = WORK_PATH("read_invalid_bom.txt");
    /* BOM 除去後の内容で "x\n" の後（オフセット 2、2 行目）に単独の 80 を置く */
    static const char bytes[] = "\xEF\xBB\xBF" "x\n\x80";
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag *d;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, bytes));
    NAOS_CHECK_FALSE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));

    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_UINT_EQ(2, d->line);
    NAOS_CHECK_STR_EQ("invalid UTF-8 byte sequence at byte offset 2", d->message);

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_missing_file_reports_target_path)
{
    static const char path[] = WORK_PATH("read_does_not_exist.txt");
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag *d;
    bool had_bom = true;

    remove(path);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&content, "stale"));
    NAOS_CHECK_FALSE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&content));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));

    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_NOT_NULL(strstr(d->message, "cannot open file for reading: "));
    /* 理由の文字列が付いている */
    NAOS_CHECK_UINT_GT(strlen(d->message), strlen("cannot open file for reading: "));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_directory_fails_with_one_diagnostic)
{
    /* 環境により open（Windows）または read（POSIX）で失敗する */
    static const char path[] = MONDE_TEST_WORK_DIR;
    mnd_buf content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag *d;
    bool had_bom = true;

    NAOS_CHECK_FALSE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));

    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
    NAOS_CHECK_TRUE(strstr(d->message, "cannot open file for reading: ") != NULL
                    || strstr(d->message, "cannot read file: ") != NULL);

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(file_io,
    NAOS_ADD_TEST(read_keeps_crlf_mixed_lf_and_nul_bytes),
    NAOS_ADD_TEST(read_strips_bom_and_reports_it),
    NAOS_ADD_TEST(read_bom_only_file_is_empty_with_bom),
    NAOS_ADD_TEST(read_without_bom_keeps_content),
    NAOS_ADD_TEST(read_empty_file_succeeds_with_length_zero),
    NAOS_ADD_TEST(read_replaces_existing_content),
    NAOS_ADD_TEST(read_file_larger_than_chunk),
    NAOS_ADD_TEST(read_invalid_utf8_reports_path_offset_and_line),
    NAOS_ADD_TEST(read_invalid_utf8_offset_is_after_bom),
    NAOS_ADD_TEST(read_missing_file_reports_target_path),
    NAOS_ADD_TEST(read_directory_fails_with_one_diagnostic)
)
