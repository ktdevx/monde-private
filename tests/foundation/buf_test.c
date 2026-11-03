#include <naos/naos.h>

#include <stdint.h>
#include <string.h>

#include "foundation/buf.h"

NAOS_TEST(init_macro_is_empty)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&buf));
}

NAOS_TEST(init_resets_to_empty)
{
    char dummy = 'x';
    mnd_buf_t buf;

    buf.data = &dummy;
    buf.len = 5;
    buf.cap = 7;
    mnd_buf_init(&buf);

    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
}

NAOS_TEST(append_to_empty)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append(&buf, "abc", 3));
    NAOS_REQUIRE_NOT_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(3, buf.len);
    NAOS_CHECK_UINT_GT(buf.cap, buf.len);
    NAOS_CHECK_INT_EQ('\0', buf.data[buf.len]);
    NAOS_CHECK_STR_EQ("abc", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(append_zero_length_accepts_null)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_CHECK_TRUE(mnd_buf_append(&buf, NULL, 0));
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&buf));

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "ab"));
    NAOS_CHECK_TRUE(mnd_buf_append(&buf, NULL, 0));
    NAOS_CHECK_UINT_EQ(2, buf.len);
    NAOS_CHECK_STR_EQ("ab", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(append_variants_concatenate)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "foo"));
    NAOS_REQUIRE_TRUE(mnd_buf_append(&buf, "bar", 3));
    NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&buf, (unsigned char)'!'));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, ""));

    NAOS_CHECK_UINT_EQ(7, buf.len);
    NAOS_CHECK_STR_EQ("foobar!", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(appends_beyond_capacity_concatenate)
{
    mnd_buf_t buf = MND_BUF_INIT;
    char expected[1003];
    size_t initial_cap = 0;
    size_t i;

    for (i = 0; i < 1000; ++i)
    {
        expected[i] = (char)('a' + (int)(i % 26));
        NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&buf, (unsigned char)expected[i]));
        if (i == 0)
        {
            initial_cap = buf.cap;
        }
    }
    memcpy(expected + 1000, "xyz", 3);
    NAOS_REQUIRE_TRUE(mnd_buf_append(&buf, "xyz", 3));

    NAOS_REQUIRE_NOT_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(1003, buf.len);
    NAOS_CHECK_UINT_GT(buf.cap, buf.len);
    NAOS_CHECK_UINT_GT(buf.cap, initial_cap);
    NAOS_CHECK_MEM_EQ(expected, sizeof expected, buf.data, buf.len);
    NAOS_CHECK_INT_EQ('\0', buf.data[buf.len]);

    mnd_buf_free(&buf);
}

NAOS_TEST(holds_embedded_nul)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append(&buf, "a\0b", 3));
    NAOS_REQUIRE_NOT_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(3, buf.len);
    NAOS_CHECK_MEM_EQ("a\0b", 3, buf.data, buf.len);
    NAOS_CHECK_INT_EQ('\0', buf.data[buf.len]);

    NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&buf, 0));
    NAOS_REQUIRE_TRUE(mnd_buf_append(&buf, "c", 1));
    NAOS_CHECK_UINT_EQ(5, buf.len);
    NAOS_CHECK_MEM_EQ("a\0b\0c", 5, buf.data, buf.len);
    NAOS_CHECK_INT_EQ('\0', buf.data[buf.len]);
    NAOS_CHECK_PTR_EQ(buf.data, mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(reserve_grows_capacity)
{
    mnd_buf_t buf = MND_BUF_INIT;
    char *data;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "ab"));
    NAOS_REQUIRE_TRUE(mnd_buf_reserve(&buf, 100));
    NAOS_CHECK_UINT_GE(buf.cap, buf.len + 100 + 1);
    NAOS_CHECK_STR_EQ("ab", mnd_buf_cstr(&buf));

    data = buf.data;
    NAOS_REQUIRE_TRUE(mnd_buf_append(&buf, "0123456789", 10));
    NAOS_CHECK_PTR_EQ(data, buf.data);
    NAOS_CHECK_STR_EQ("ab0123456789", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(huge_reserve_fails_and_keeps_content)
{
    mnd_buf_t buf = MND_BUF_INIT;
    char *data;
    size_t len;
    size_t cap;

    NAOS_CHECK_FALSE(mnd_buf_reserve(&buf, SIZE_MAX));
    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "abc"));
    data = buf.data;
    len = buf.len;
    cap = buf.cap;

    NAOS_CHECK_FALSE(mnd_buf_reserve(&buf, SIZE_MAX));
    NAOS_CHECK_FALSE(mnd_buf_reserve(&buf, SIZE_MAX - buf.len));
    NAOS_CHECK_PTR_EQ(data, buf.data);
    NAOS_CHECK_UINT_EQ(len, buf.len);
    NAOS_CHECK_UINT_EQ(cap, buf.cap);
    NAOS_CHECK_STR_EQ("abc", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(overflowing_append_fails_and_keeps_content)
{
    mnd_buf_t buf = MND_BUF_INIT;
    char *data;
    size_t len;
    size_t cap;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "abc"));
    data = buf.data;
    len = buf.len;
    cap = buf.cap;

    /* 長さの計算（len + 追加分 + 終端）がオーバーフローするので、bytes は読まれない */
    NAOS_CHECK_FALSE(mnd_buf_append(&buf, "x", SIZE_MAX - buf.len));
    NAOS_CHECK_FALSE(mnd_buf_append(&buf, "x", SIZE_MAX));
    NAOS_CHECK_PTR_EQ(data, buf.data);
    NAOS_CHECK_UINT_EQ(len, buf.len);
    NAOS_CHECK_UINT_EQ(cap, buf.cap);
    NAOS_CHECK_STR_EQ("abc", mnd_buf_cstr(&buf));

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "d"));
    NAOS_CHECK_STR_EQ("abcd", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(clear_keeps_capacity_for_reuse)
{
    mnd_buf_t buf = MND_BUF_INIT;
    char *data;
    size_t cap;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "hello, world"));
    data = buf.data;
    cap = buf.cap;

    mnd_buf_clear(&buf);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(cap, buf.cap);
    NAOS_CHECK_PTR_EQ(data, buf.data);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&buf));

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "xy"));
    NAOS_CHECK_PTR_EQ(data, buf.data);
    NAOS_CHECK_UINT_EQ(cap, buf.cap);
    NAOS_CHECK_UINT_EQ(2, buf.len);
    NAOS_CHECK_STR_EQ("xy", mnd_buf_cstr(&buf));

    mnd_buf_free(&buf);
}

NAOS_TEST(clear_on_empty_is_safe)
{
    mnd_buf_t buf = MND_BUF_INIT;

    mnd_buf_clear(&buf);
    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&buf));
}

NAOS_TEST(free_twice_is_safe)
{
    mnd_buf_t buf = MND_BUF_INIT;

    mnd_buf_free(&buf);
    NAOS_CHECK_NULL(buf.data);

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "abc"));
    mnd_buf_free(&buf);
    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);

    mnd_buf_free(&buf);
    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&buf));

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&buf, "again"));
    NAOS_CHECK_STR_EQ("again", mnd_buf_cstr(&buf));
    mnd_buf_free(&buf);
}

NAOS_SUITE(buf,
    NAOS_ADD_TEST(init_macro_is_empty),
    NAOS_ADD_TEST(init_resets_to_empty),
    NAOS_ADD_TEST(append_to_empty),
    NAOS_ADD_TEST(append_zero_length_accepts_null),
    NAOS_ADD_TEST(append_variants_concatenate),
    NAOS_ADD_TEST(appends_beyond_capacity_concatenate),
    NAOS_ADD_TEST(holds_embedded_nul),
    NAOS_ADD_TEST(reserve_grows_capacity),
    NAOS_ADD_TEST(huge_reserve_fails_and_keeps_content),
    NAOS_ADD_TEST(overflowing_append_fails_and_keeps_content),
    NAOS_ADD_TEST(clear_keeps_capacity_for_reuse),
    NAOS_ADD_TEST(clear_on_empty_is_safe),
    NAOS_ADD_TEST(free_twice_is_safe)
)
