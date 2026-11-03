#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "cli_args/args_internal.h"
#include "test_decl.h"

/*
 * ResultStore の書き込み関数を直接使うテスト。
 * test_tool_build のオプションの添字: 0 output、1 keys、2 level、3 verbose、4 quiet
 */
#define VALUES_OUTPUT 0
#define VALUES_KEYS 1
#define VALUES_LEVEL 2
#define VALUES_VERBOSE 3

static void begin_build(mnd_args_result_t *result)
{
    mnd_args_result_init(result);
    NAOS_REQUIRE_TRUE(mnd_args_result_begin(result, test_tool_build));
}

NAOS_TEST(init_is_empty_and_free_is_repeatable)
{
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;

    mnd_args_result_free(&result);
    mnd_args_result_free(&result);
    NAOS_CHECK_TRUE(result.status == MND_ARGS_NO_COMMAND);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_NULL(result.values);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&result));
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "output"));
    NAOS_CHECK_NULL(mnd_args_value(&result, "output"));

    begin_build(&result);
    NAOS_CHECK_PTR_EQ(test_tool_build, result.command);
    NAOS_CHECK_NOT_NULL(result.values);
    NAOS_CHECK_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "x,y", true) == MND_ARGS_OK);
    NAOS_CHECK_TRUE(mnd_args_result_add_positional(&result, "src"));
    mnd_args_result_free(&result);
    NAOS_CHECK_TRUE(result.status == MND_ARGS_NO_COMMAND);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_NULL(result.values);
    NAOS_CHECK_NULL(result.positionals);
    NAOS_CHECK_UINT_EQ(0, result.positional_count);
    NAOS_CHECK_UINT_EQ(0, result.positional_cap);
    mnd_args_result_free(&result);
    NAOS_CHECK_NULL(result.command);
}

NAOS_TEST(begin_without_options_succeeds)
{
    mnd_args_result_t result;

    mnd_args_result_init(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_begin(&result, test_tool_cat));
    NAOS_CHECK_PTR_EQ(test_tool_cat, result.command);
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "output"));
    NAOS_CHECK_UINT_EQ(0, mnd_args_list_count(&result, "keys"));
    mnd_args_result_free(&result);
}

NAOS_TEST(list_splits_on_comma)
{
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "a,b", true) == MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ("a", mnd_args_list_at(&result, "keys", 0));
    NAOS_CHECK_STR_EQ("b", mnd_args_list_at(&result, "keys", 1));
    NAOS_CHECK_NULL(mnd_args_list_at(&result, "keys", 2));
    NAOS_CHECK_TRUE(mnd_args_is_set(&result, "keys"));
    mnd_args_result_free(&result);
}

NAOS_TEST(list_keeps_whitespace_in_items)
{
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, " a , b", true) == MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ(" a ", mnd_args_list_at(&result, "keys", 0));
    NAOS_CHECK_STR_EQ(" b", mnd_args_list_at(&result, "keys", 1));
    mnd_args_result_free(&result);
}

NAOS_TEST(list_rejects_empty_items_without_adding)
{
    static const char *const bad[] = { "", ",a", "a,", "a,,b", "," };
    mnd_args_result_t result;
    size_t i;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "x", true) == MND_ARGS_OK);
    for (i = 0; i < (sizeof(bad) / sizeof(bad[0])); i++)
    {
        NAOS_CHECK_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, bad[i], true)
                        == MND_ARGS_USAGE_ERROR);
        NAOS_CHECK_UINT_EQ(1, mnd_args_list_count(&result, "keys"));
    }

    NAOS_CHECK_STR_EQ("x", mnd_args_list_at(&result, "keys", 0));
    mnd_args_result_free(&result);

    begin_build(&result);
    NAOS_CHECK_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "a,,b", true)
                    == MND_ARGS_USAGE_ERROR);
    NAOS_CHECK_UINT_EQ(0, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "keys"));
    mnd_args_result_free(&result);
}

NAOS_TEST(two_appends_concatenate)
{
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "a,b", true) == MND_ARGS_OK);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "c", true) == MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ("a", mnd_args_list_at(&result, "keys", 0));
    NAOS_CHECK_STR_EQ("b", mnd_args_list_at(&result, "keys", 1));
    NAOS_CHECK_STR_EQ("c", mnd_args_list_at(&result, "keys", 2));
    mnd_args_result_free(&result);
}

NAOS_TEST(list_grows_past_initial_capacity)
{
    mnd_args_result_t result;
    size_t i;

    begin_build(&result);
    for (i = 0; i < 40; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "p,q", true) == MND_ARGS_OK);
    }

    NAOS_REQUIRE_UINT_EQ(80, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ("p", mnd_args_list_at(&result, "keys", 78));
    NAOS_CHECK_STR_EQ("q", mnd_args_list_at(&result, "keys", 79));
    mnd_args_result_free(&result);
}

NAOS_TEST(value_later_wins_and_points_at_argument)
{
    char first[] = "one";
    char second[] = "two";
    mnd_args_result_t result;

    begin_build(&result);
    mnd_args_result_set_value(&result, VALUES_LEVEL, first);
    mnd_args_result_set_value(&result, VALUES_LEVEL, second);
    NAOS_CHECK_PTR_EQ(second, mnd_args_value(&result, "level"));
    NAOS_CHECK_STR_EQ("two", mnd_args_value(&result, "level"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&result, "level"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "output"));
    NAOS_CHECK_NULL(mnd_args_value(&result, "output"));
    mnd_args_result_free(&result);
}

NAOS_TEST(flag_is_recorded)
{
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_CHECK_FALSE(mnd_args_flag(&result, "verbose"));
    mnd_args_result_set_flag(&result, VALUES_VERBOSE);
    mnd_args_result_set_flag(&result, VALUES_VERBOSE);
    NAOS_CHECK_TRUE(mnd_args_flag(&result, "verbose"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&result, "verbose"));
    NAOS_CHECK_FALSE(mnd_args_flag(&result, "quiet"));
    mnd_args_result_free(&result);
}

NAOS_TEST(defaults_apply_when_not_set)
{
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_apply_defaults(&result) == MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ("a", mnd_args_list_at(&result, "keys", 0));
    NAOS_CHECK_STR_EQ("b", mnd_args_list_at(&result, "keys", 1));
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "keys"));
    NAOS_CHECK_STR_EQ("1", mnd_args_value(&result, "level"));
    NAOS_CHECK_PTR_EQ(test_tool_build->options[VALUES_LEVEL].default_value,
                      mnd_args_value(&result, "level"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "level"));
    NAOS_CHECK_NULL(mnd_args_value(&result, "output"));
    NAOS_CHECK_FALSE(mnd_args_flag(&result, "verbose"));
    mnd_args_result_free(&result);
}

NAOS_TEST(explicit_values_suppress_defaults)
{
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, "z", true) == MND_ARGS_OK);
    mnd_args_result_set_value(&result, VALUES_LEVEL, "3");
    NAOS_REQUIRE_TRUE(mnd_args_result_apply_defaults(&result) == MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ("z", mnd_args_list_at(&result, "keys", 0));
    NAOS_CHECK_TRUE(mnd_args_is_set(&result, "keys"));
    NAOS_CHECK_STR_EQ("3", mnd_args_value(&result, "level"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&result, "level"));
    mnd_args_result_free(&result);
}

NAOS_TEST(positionals_point_at_arguments)
{
    char first[] = "in.txt";
    char second[] = "out.txt";
    mnd_args_result_t result;

    mnd_args_result_init(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_begin(&result, test_tool_cat));
    NAOS_REQUIRE_TRUE(mnd_args_result_add_positional(&result, first));
    NAOS_REQUIRE_TRUE(mnd_args_result_add_positional(&result, second));
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_positional_count(&result));
    NAOS_CHECK_PTR_EQ(first, mnd_args_positional_at(&result, 0));
    NAOS_CHECK_PTR_EQ(second, mnd_args_positional_at(&result, 1));
    NAOS_CHECK_NULL(mnd_args_positional_at(&result, 2));
    mnd_args_result_free(&result);
}

NAOS_TEST(undeclared_names_return_empty)
{
    mnd_args_result_t result;

    begin_build(&result);
    mnd_args_result_set_flag(&result, VALUES_VERBOSE);
    NAOS_REQUIRE_TRUE(mnd_args_result_apply_defaults(&result) == MND_ARGS_OK);
    NAOS_CHECK_FALSE(mnd_args_is_set(&result, "missing"));
    NAOS_CHECK_FALSE(mnd_args_flag(&result, "missing"));
    NAOS_CHECK_NULL(mnd_args_value(&result, "missing"));
    NAOS_CHECK_UINT_EQ(0, mnd_args_list_count(&result, "missing"));
    NAOS_CHECK_NULL(mnd_args_list_at(&result, "missing", 0));
    NAOS_CHECK_FALSE(mnd_args_flag(&result, "verb"));
    NAOS_CHECK_FALSE(mnd_args_flag(&result, "Verbose"));
    mnd_args_result_free(&result);
}

NAOS_TEST(inputs_are_not_modified_and_bytes_are_kept)
{
    char list_arg[] = "\xC3\xA9t\xC3\xA9,\xE6\x97\xA5\xE6\x9C\xAC";
    char value_arg[] = "\xE5\x80\xA4";
    char bad_arg[] = "a,,b";
    mnd_args_result_t result;

    begin_build(&result);
    NAOS_REQUIRE_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, list_arg, true) == MND_ARGS_OK);
    mnd_args_result_set_value(&result, VALUES_OUTPUT, value_arg);
    NAOS_CHECK_TRUE(mnd_args_result_append_list(&result, VALUES_KEYS, bad_arg, true)
                    == MND_ARGS_USAGE_ERROR);
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_list_count(&result, "keys"));
    NAOS_CHECK_STR_EQ("\xC3\xA9t\xC3\xA9", mnd_args_list_at(&result, "keys", 0));
    NAOS_CHECK_STR_EQ("\xE6\x97\xA5\xE6\x9C\xAC", mnd_args_list_at(&result, "keys", 1));
    NAOS_CHECK_STR_EQ("\xE5\x80\xA4", mnd_args_value(&result, "output"));
    mnd_args_result_free(&result);

    NAOS_CHECK_STR_EQ("\xC3\xA9t\xC3\xA9,\xE6\x97\xA5\xE6\x9C\xAC", list_arg);
    NAOS_CHECK_STR_EQ("\xE5\x80\xA4", value_arg);
    NAOS_CHECK_STR_EQ("a,,b", bad_arg);
}

NAOS_SUITE(values,
    NAOS_ADD_TEST(init_is_empty_and_free_is_repeatable),
    NAOS_ADD_TEST(begin_without_options_succeeds),
    NAOS_ADD_TEST(list_splits_on_comma),
    NAOS_ADD_TEST(list_keeps_whitespace_in_items),
    NAOS_ADD_TEST(list_rejects_empty_items_without_adding),
    NAOS_ADD_TEST(two_appends_concatenate),
    NAOS_ADD_TEST(list_grows_past_initial_capacity),
    NAOS_ADD_TEST(value_later_wins_and_points_at_argument),
    NAOS_ADD_TEST(flag_is_recorded),
    NAOS_ADD_TEST(defaults_apply_when_not_set),
    NAOS_ADD_TEST(explicit_values_suppress_defaults),
    NAOS_ADD_TEST(positionals_point_at_arguments),
    NAOS_ADD_TEST(undeclared_names_return_empty),
    NAOS_ADD_TEST(inputs_are_not_modified_and_bytes_are_kept)
)
