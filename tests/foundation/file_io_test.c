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

/* tests/foundation/CMakeLists.txt が作業ディレクトリの下に作る「日本語ディレクトリ」（UTF-8） */
#define NON_ASCII_DIR_NAME \
    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" \
    "\xE3\x83\x87\xE3\x82\xA3\xE3\x83\xAC\xE3\x82\xAF\xE3\x83\x88\xE3\x83\xAA"

/* 非 ASCII のファイル名「翻訳.md」（UTF-8） */
#define NON_ASCII_FILE_NAME "\xE7\xBF\xBB\xE8\xA8\xB3" ".md"

/* 読み込みテストの入力ファイルをバイナリモードで作る（テストコードのみ fopen を直接使う） */
static bool write_bytes(const char *path, const char *bytes, size_t len)
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

/* ファイルの生のバイト列をバイナリモードで読む（テストコードのみ fopen を直接使う） */
static bool read_raw(const char *path, mnd_buf_t *out)
{
    char chunk[512];
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

NAOS_TEST(read_keeps_crlf_mixed_lf_and_nul_bytes)
{
    static const char path[] = WORK_PATH("read_bytes.txt");
    static const char bytes[] = "a\r\nb\nc\0d\r\n\re";
    mnd_buf_t content = MND_BUF_INIT;
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
    mnd_buf_t content = MND_BUF_INIT;
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
    mnd_buf_t content = MND_BUF_INIT;
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
    mnd_buf_t content = MND_BUF_INIT;
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
    mnd_buf_t content = MND_BUF_INIT;
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
    mnd_buf_t content = MND_BUF_INIT;
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
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;
    size_t i;

    for (i = 0; i < sizeof(bytes); i++)
    {
        bytes[i] = ((i % 61) == 60) ? '\n' : (char)('A' + (int)(i % 26));
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
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;
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
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;
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
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;
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
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;
    bool had_bom = true;

    NAOS_CHECK_FALSE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));

    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
    NAOS_CHECK_TRUE((strstr(d->message, "cannot open file for reading: ") != NULL)
                    || (strstr(d->message, "cannot read file: ") != NULL));

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_with_bom_prefixes_ef_bb_bf)
{
    static const char path[] = WORK_PATH("write_bom.txt");
    static const char expected[] = "\xEF\xBB\xBF" "a\r\nb";
    mnd_buf_t raw = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_TRUE(mnd_file_write(path, "a\r\nb", 4, true, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_TRUE(read_raw(path, &raw));
    NAOS_REQUIRE_UINT_EQ(sizeof(expected) - 1, raw.len);
    NAOS_CHECK_MEM_EQ("\xEF\xBB\xBF", 3, raw.data, 3);
    NAOS_CHECK_MEM_EQ(expected, sizeof(expected) - 1, raw.data, raw.len);

    mnd_buf_free(&raw);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_without_bom_writes_content_only)
{
    static const char path[] = WORK_PATH("write_no_bom.txt");
    mnd_buf_t raw = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_TRUE(mnd_file_write(path, "abc\n", 4, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_TRUE(read_raw(path, &raw));
    NAOS_REQUIRE_UINT_EQ(4, raw.len);
    NAOS_CHECK_MEM_EQ("abc\n", 4, raw.data, raw.len);

    mnd_buf_free(&raw);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_keeps_crlf_mixed_lf_and_nul_bytes)
{
    static const char path[] = WORK_PATH("write_bytes.txt");
    static const char bytes[] = "a\r\nb\nc\0d\r\n\re";
    mnd_buf_t raw = MND_BUF_INIT;
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_CHECK_TRUE(mnd_file_write(path, bytes, sizeof(bytes) - 1, false, &diags));
    NAOS_REQUIRE_TRUE(read_raw(path, &raw));
    NAOS_CHECK_MEM_EQ(bytes, sizeof(bytes) - 1, raw.data, raw.len);

    /* 書いた内容を読み戻すとバイト単位で一致する */
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_MEM_EQ(bytes, sizeof(bytes) - 1, content.data, content.len);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_buf_free(&content);
    mnd_buf_free(&raw);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_round_trip_without_bom_is_byte_identical)
{
    static const char original_path[] = WORK_PATH("round_trip_original.txt");
    static const char copy_path[] = WORK_PATH("round_trip_copy.txt");
    /* LF・CRLF・単独 CR の混在、NUL、多バイト文字（U+3042）を含む */
    static const char bytes[] = "line1\r\nline2\n\xE3\x81\x82" "\r\0end\r";
    mnd_buf_t content = MND_BUF_INIT;
    mnd_buf_t original = MND_BUF_INIT;
    mnd_buf_t copy = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(original_path, bytes));
    NAOS_REQUIRE_TRUE(mnd_file_read(original_path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_TRUE(mnd_file_write(copy_path, content.data, content.len, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    NAOS_REQUIRE_TRUE(read_raw(original_path, &original));
    NAOS_REQUIRE_TRUE(read_raw(copy_path, &copy));
    NAOS_CHECK_UINT_EQ(sizeof(bytes) - 1, original.len);
    NAOS_CHECK_MEM_EQ(original.data, original.len, copy.data, copy.len);

    mnd_buf_free(&copy);
    mnd_buf_free(&original);
    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_round_trip_with_bom_restores_original)
{
    static const char original_path[] = WORK_PATH("round_trip_bom_original.txt");
    static const char copy_path[] = WORK_PATH("round_trip_bom_copy.txt");
    static const char bytes[] = "\xEF\xBB\xBF" "x\r\ny\n";
    mnd_buf_t content = MND_BUF_INIT;
    mnd_buf_t original = MND_BUF_INIT;
    mnd_buf_t copy = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = false;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(original_path, bytes));
    NAOS_REQUIRE_TRUE(mnd_file_read(original_path, &content, &had_bom, &diags));
    NAOS_CHECK_TRUE(had_bom);
    NAOS_CHECK_TRUE(mnd_file_write(copy_path, content.data, content.len, had_bom, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    NAOS_REQUIRE_TRUE(read_raw(original_path, &original));
    NAOS_REQUIRE_TRUE(read_raw(copy_path, &copy));
    NAOS_CHECK_MEM_EQ(original.data, original.len, copy.data, copy.len);

    mnd_buf_free(&copy);
    mnd_buf_free(&original);
    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_truncates_existing_longer_file)
{
    static const char path[] = WORK_PATH("write_truncate.txt");
    mnd_buf_t raw = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, "a much longer previous content\n"));
    NAOS_CHECK_TRUE(mnd_file_write(path, "new", 3, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_TRUE(read_raw(path, &raw));
    NAOS_REQUIRE_UINT_EQ(3, raw.len);
    NAOS_CHECK_MEM_EQ("new", 3, raw.data, raw.len);

    mnd_buf_free(&raw);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_empty_content_creates_empty_or_bom_only_file)
{
    static const char path[] = WORK_PATH("write_empty.txt");
    static const char bom_path[] = WORK_PATH("write_empty_bom.txt");
    mnd_buf_t raw = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(WRITE_LITERAL(path, "stale"));
    NAOS_CHECK_TRUE(mnd_file_write(path, NULL, 0, false, &diags));
    NAOS_REQUIRE_TRUE(read_raw(path, &raw));
    NAOS_CHECK_UINT_EQ(0, raw.len);

    NAOS_CHECK_TRUE(mnd_file_write(bom_path, NULL, 0, true, &diags));
    NAOS_REQUIRE_TRUE(read_raw(bom_path, &raw));
    NAOS_CHECK_MEM_EQ("\xEF\xBB\xBF", 3, raw.data, raw.len);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_buf_free(&raw);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(write_into_missing_directory_reports_target_path)
{
    static const char path[] = WORK_PATH("write_no_such_dir/out.txt");
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;

    NAOS_CHECK_FALSE(mnd_file_write(path, "abc", 3, false, &diags));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));

    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_NOT_NULL(strstr(d->message, "cannot open file for writing: "));
    /* 理由の文字列が付いている */
    NAOS_CHECK_UINT_GT(strlen(d->message), strlen("cannot open file for writing: "));
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));

    mnd_diag_list_free(&diags);
}

NAOS_TEST(non_ascii_directory_and_file_name_round_trip)
{
    static const char path[] = WORK_PATH(NON_ASCII_DIR_NAME "/" NON_ASCII_FILE_NAME);
    /* 内容にも多バイト文字（U+3042）と CRLF を含める */
    static const char bytes[] = "# \xE3\x81\x82\r\nbody\n";
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_CHECK_TRUE(mnd_file_write(path, bytes, sizeof(bytes) - 1, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_TRUE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_UINT_EQ(sizeof(bytes) - 1, content.len);
    NAOS_CHECK_MEM_EQ(bytes, sizeof(bytes) - 1, content.data, content.len);

    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(invalid_utf8_path_fails_with_one_diagnostic)
{
    /* FF は UTF-8 に現れないバイト。直後の 'b' が 16 進数として続かないようにリテラルを分ける */
    static const char path[] = WORK_PATH("\xFF" "bad.txt");
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;
    bool had_bom = true;

    /* POSIX ではこのファイルが存在しないので、どの環境でも読み込みは失敗する */
    NAOS_CHECK_FALSE(mnd_file_read(path, &content, &had_bom, &diags));
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, content.len);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
#ifdef _WIN32
    NAOS_CHECK_STR_EQ("cannot open file for reading: file path is not valid UTF-8",
                      d->message);

    /* Windows では UTF-16 に変換できないパスへの書き込みも open の段階で失敗する */
    NAOS_CHECK_FALSE(mnd_file_write(path, "abc", 3, false, &diags));
    NAOS_REQUIRE_UINT_EQ(2, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 1);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ(path, d->path);
    NAOS_CHECK_STR_EQ("cannot open file for writing: file path is not valid UTF-8",
                      d->message);
#endif

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
    NAOS_ADD_TEST(read_directory_fails_with_one_diagnostic),
    NAOS_ADD_TEST(write_with_bom_prefixes_ef_bb_bf),
    NAOS_ADD_TEST(write_without_bom_writes_content_only),
    NAOS_ADD_TEST(write_keeps_crlf_mixed_lf_and_nul_bytes),
    NAOS_ADD_TEST(write_round_trip_without_bom_is_byte_identical),
    NAOS_ADD_TEST(write_round_trip_with_bom_restores_original),
    NAOS_ADD_TEST(write_truncates_existing_longer_file),
    NAOS_ADD_TEST(write_empty_content_creates_empty_or_bom_only_file),
    NAOS_ADD_TEST(write_into_missing_directory_reports_target_path),
    NAOS_ADD_TEST(non_ascii_directory_and_file_name_round_trip),
    NAOS_ADD_TEST(invalid_utf8_path_fails_with_one_diagnostic)
)
