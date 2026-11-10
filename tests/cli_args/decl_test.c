#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "cli_args/args_internal.h"
#include "foundation/diag.h"
#include "test_decl.h"

#define DECL_TEST_NOT_FOUND ((size_t)-1)

/*
 * 規則の違反を 1 つずつ作るための、書き換え可能な宣言。
 * fixture_init の直後は妥当な宣言で、各テストが 1 か所だけ書き換える。
 *   run:  --name/-n <VALUE>（値）、--force/-f（フラグ）、--tags/-t <TAG>（リスト、既定値 "x,y"）。
 *         位置引数 INPUT（1 個ちょうど）
 *   list: オプションなし（options は NULL）。位置引数 ITEM（0 個以上、上限なし）
 * run と list はルートコマンド root（オプションと位置引数なし）の子コマンド
 */
typedef struct decl_fixture
{
    mnd_args_option_t run_options[3];
    mnd_args_positional_t run_positionals[2];
    mnd_args_positional_t list_positionals[1];
    mnd_args_command_t commands[2];
    mnd_args_command_t root;
    mnd_args_program_t program;
} decl_fixture_t;

static void set_option(mnd_args_option_t *option, const char *long_name, char short_name,
                       mnd_args_kind_t kind, const char *value_name, const char *default_value)
{
    option->long_name = long_name;
    option->short_name = short_name;
    option->kind = kind;
    option->value_name = value_name;
    option->help = NULL;
    option->required = false;
    option->default_value = default_value;
}

static void fixture_init(decl_fixture_t *f)
{
    memset(f, 0, sizeof *f);

    set_option(&f->run_options[0], "name", 'n', MND_ARGS_VALUE, NULL, NULL);
    set_option(&f->run_options[1], "force", 'f', MND_ARGS_FLAG, NULL, NULL);
    set_option(&f->run_options[2], "tags", 't', MND_ARGS_LIST, "TAG", "x,y");

    f->run_positionals[0].name = "INPUT";
    f->run_positionals[0].help = "Input file";
    f->run_positionals[1].name = "OUTPUT";
    f->run_positionals[1].help = NULL;
    f->list_positionals[0].name = "ITEM";
    f->list_positionals[0].help = NULL;

    f->commands[0].name = "run";
    f->commands[0].summary = "Run something";
    f->commands[0].description = NULL;
    f->commands[0].options = f->run_options;
    f->commands[0].option_count = 3;
    f->commands[0].positionals = f->run_positionals;
    f->commands[0].positional_count = 1;
    f->commands[0].min_positionals = 1;
    f->commands[0].max_positionals = 1;

    f->commands[1].name = "list";
    f->commands[1].summary = NULL;
    f->commands[1].description = NULL;
    f->commands[1].options = NULL;
    f->commands[1].option_count = 0;
    f->commands[1].positionals = f->list_positionals;
    f->commands[1].positional_count = 1;
    f->commands[1].min_positionals = 0;
    f->commands[1].max_positionals = MND_ARGS_UNLIMITED;

    f->root.commands = f->commands;
    f->root.command_count = 2;

    f->program.name = "prog";
    f->program.version = "0.1";
    f->program.root = &f->root;
}

/* test_tool_program のルートコマンドと command の 2 段の経路を作る */
static void begin_path(mnd_args_result_t *result, const mnd_args_command_t *command)
{
    mnd_args_result_init(result);
    result->program = &test_tool_program;
    NAOS_REQUIRE_TRUE(mnd_args_result_push(result, test_tool_program.root));
    NAOS_REQUIRE_TRUE(mnd_args_result_push(result, command));
}

static size_t found_index(bool found, mnd_args_ref_t ref)
{
    if (!found)
    {
        return DECL_TEST_NOT_FOUND;
    }

    /* ルートコマンドはオプションを持たないので、見つかるのは段 1 だけ */
    NAOS_CHECK_UINT_EQ(1, ref.level);
    return ref.index;
}

/*
 * 2 段の経路で有効なオプションを名前で探す。
 * 見つかれば command の options の添字、なければ DECL_TEST_NOT_FOUND を返す
 */
static size_t find_long_index(const mnd_args_command_t *command, const char *name, size_t name_len)
{
    mnd_args_result_t result;
    mnd_args_ref_t ref = { 0, 0 };
    size_t index;

    begin_path(&result, command);
    index = found_index(mnd_args_find_long(&result, name, name_len, &ref), ref);
    mnd_args_result_free(&result);
    return index;
}

static size_t find_short_index(const mnd_args_command_t *command, char short_name)
{
    mnd_args_result_t result;
    mnd_args_ref_t ref = { 0, 0 };
    size_t index;

    begin_path(&result, command);
    index = found_index(mnd_args_find_short(&result, short_name, &ref), ref);
    mnd_args_result_free(&result);
    return index;
}

/* 宣言の検査が失敗し、期待するメッセージの診断がちょうど 1 件だけ追加されることを確かめる */
static void check_single_error(const mnd_args_program_t *program, const char *expected)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *d;
    bool ok;

    ok = mnd_args_decl_check(program, &diags);
    NAOS_CHECK_FALSE(ok);
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
    if (mnd_diag_list_count(&diags) >= 1)
    {
        d = mnd_diag_list_at(&diags, 0);
        NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
        NAOS_CHECK_NULL(d->path);
        NAOS_CHECK_UINT_EQ(0, d->line);
        NAOS_CHECK_STR_EQ(expected, d->message);
    }

    mnd_diag_list_free(&diags);
}

/* 宣言の検査が成功し、診断が追加されないことを確かめる */
static void check_valid(const mnd_args_program_t *program)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_TRUE(mnd_args_decl_check(program, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

/* 妥当な宣言 */

NAOS_TEST(tool_declaration_is_valid)
{
    check_valid(&test_tool_program);
}

NAOS_TEST(fixture_declaration_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    check_valid(&f.program);
}

NAOS_TEST(program_without_commands_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.root.commands = NULL;
    f.root.command_count = 0;
    check_valid(&f.program);
}

NAOS_TEST(command_without_options_and_positionals_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[0].options = NULL;
    f.commands[0].option_count = 0;
    f.commands[0].positionals = NULL;
    f.commands[0].positional_count = 0;
    f.commands[0].min_positionals = 0;
    f.commands[0].max_positionals = 0;
    check_valid(&f.program);
}

NAOS_TEST(value_option_may_have_empty_default)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].default_value = "";
    check_valid(&f.program);
}

NAOS_TEST(required_value_option_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].required = true;
    check_valid(&f.program);
}

NAOS_TEST(digit_short_name_and_no_short_name_are_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].short_name = '9';
    f.run_options[1].short_name = '\0';
    f.run_options[2].short_name = '\0';
    check_valid(&f.program);
}

/* プログラム名とバージョン */

NAOS_TEST(program_name_null_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.program.name = NULL;
    check_single_error(&f.program,
                       "invalid argument declaration: program name and version are required");
}

NAOS_TEST(program_name_empty_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.program.name = "";
    check_single_error(&f.program,
                       "invalid argument declaration: program name and version are required");
}

NAOS_TEST(program_version_null_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.program.version = NULL;
    check_single_error(&f.program,
                       "invalid argument declaration: program name and version are required");
}

NAOS_TEST(program_version_empty_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.program.version = "";
    check_single_error(&f.program,
                       "invalid argument declaration: program name and version are required");
}

/* サブコマンド名 */

NAOS_TEST(command_name_empty_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "";
    check_single_error(&f.program, "invalid argument declaration: invalid command name ''");
}

NAOS_TEST(command_name_null_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = NULL;
    check_single_error(&f.program, "invalid argument declaration: invalid command name ''");
}

NAOS_TEST(command_name_starting_with_dash_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "-list";
    check_single_error(&f.program, "invalid argument declaration: invalid command name '-list'");
}

NAOS_TEST(command_named_help_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "help";
    check_single_error(&f.program, "invalid argument declaration: command name 'help' is reserved");
}

NAOS_TEST(duplicate_command_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "run";
    check_single_error(&f.program, "invalid argument declaration: duplicate command 'run'");
}

NAOS_TEST(commands_differing_in_case_are_not_duplicates)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "Run";
    check_valid(&f.program);
}

/* 長い名前 */

NAOS_TEST(long_name_null_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = NULL;
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name '' in command 'run'");
}

NAOS_TEST(long_name_empty_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name '' in command 'run'");
}

NAOS_TEST(long_name_starting_with_dash_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "-name";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name '-name' in command 'run'");
}

NAOS_TEST(long_name_containing_equals_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "na=me";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name 'na=me' in command 'run'");
}

NAOS_TEST(long_name_help_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "help";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--help' is reserved in command 'run'");
}

NAOS_TEST(short_name_h_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].short_name = 'h';
    check_single_error(&f.program,
                       "invalid argument declaration: option '-h' is reserved in command 'run'");
}

NAOS_TEST(short_name_upper_h_is_not_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].short_name = 'H';
    check_valid(&f.program);
}

/* 短い名前の文字 */

NAOS_TEST(short_name_punctuation_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].short_name = '-';
    check_single_error(&f.program,
                       "invalid argument declaration: invalid short option for '--force' in command 'run'");
}

NAOS_TEST(short_name_space_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].short_name = ' ';
    check_single_error(&f.program,
                       "invalid argument declaration: invalid short option for '--force' in command 'run'");
}

NAOS_TEST(short_name_non_ascii_byte_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].short_name = '\xE6';
    check_single_error(&f.program,
                       "invalid argument declaration: invalid short option for '--force' in command 'run'");
}

/* 同じサブコマンド内の重複 */

NAOS_TEST(duplicate_long_name_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].long_name = "name";
    check_single_error(&f.program,
                       "invalid argument declaration: duplicate option '--name' in command 'run'");
}

NAOS_TEST(duplicate_short_name_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].short_name = 'n';
    check_single_error(&f.program,
                       "invalid argument declaration: duplicate option '-n' in command 'run'");
}

NAOS_TEST(same_option_in_different_commands_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].options = f.run_options;
    f.commands[1].option_count = 3;
    check_valid(&f.program);
}

NAOS_TEST(long_names_differing_in_case_are_not_duplicates)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].long_name = "Name";
    f.run_options[2].short_name = 'N';
    check_valid(&f.program);
}

/* フラグ、必須、既定値 */

NAOS_TEST(required_flag_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].required = true;
    check_single_error(&f.program,
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'run'");
}

NAOS_TEST(flag_with_default_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].default_value = "1";
    check_single_error(&f.program,
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'run'");
}

NAOS_TEST(required_and_defaulted_value_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].required = true;
    f.run_options[0].default_value = "a";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--name' cannot be both required and defaulted in command 'run'");
}

NAOS_TEST(required_and_defaulted_list_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].required = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--tags' cannot be both required and defaulted in command 'run'");
}

/* リスト値の既定値の空の項目 */

static void check_list_default_has_empty_item(const char *default_value)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].default_value = default_value;
    check_single_error(&f.program,
                       "invalid argument declaration: default of '--tags' has an empty item in command 'run'");
}

NAOS_TEST(list_default_empty_string_is_error)
{
    check_list_default_has_empty_item("");
}

NAOS_TEST(list_default_leading_comma_is_error)
{
    check_list_default_has_empty_item(",x");
}

NAOS_TEST(list_default_trailing_comma_is_error)
{
    check_list_default_has_empty_item("x,");
}

NAOS_TEST(list_default_double_comma_is_error)
{
    check_list_default_has_empty_item("x,,y");
}

NAOS_TEST(list_default_single_comma_is_error)
{
    check_list_default_has_empty_item(",");
}

NAOS_TEST(list_default_with_spaces_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].default_value = " x , y ";
    check_valid(&f.program);
}

NAOS_TEST(list_default_single_item_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].default_value = "x";
    check_valid(&f.program);
}

/* 位置引数 */

NAOS_TEST(min_greater_than_max_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[0].min_positionals = 2;
    check_single_error(&f.program,
                       "invalid argument declaration: invalid positional range in command 'run'");
}

NAOS_TEST(bounded_positional_names_fewer_than_max_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[0].max_positionals = 2;
    check_single_error(&f.program,
                       "invalid argument declaration: positional names do not match the range in command 'run'");
}

NAOS_TEST(bounded_positional_names_more_than_max_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[0].positional_count = 2;
    check_single_error(&f.program,
                       "invalid argument declaration: positional names do not match the range in command 'run'");
}

NAOS_TEST(unlimited_without_positional_names_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].positionals = NULL;
    f.commands[1].positional_count = 0;
    check_single_error(&f.program,
                       "invalid argument declaration: positional names do not match the range in command 'list'");
}

NAOS_TEST(unlimited_with_several_positional_names_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].positionals = f.run_positionals;
    f.commands[1].positional_count = 2;
    f.commands[1].min_positionals = 1;
    check_valid(&f.program);
}

/* 誤りをすべて報告する */

NAOS_TEST(two_errors_produce_two_diagnostics)
{
    decl_fixture_t f;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    fixture_init(&f);
    f.commands[1].name = "run";
    f.run_options[1].required = true;

    NAOS_CHECK_FALSE(mnd_args_decl_check(&f.program, &diags));
    NAOS_REQUIRE_UINT_EQ(2, mnd_diag_list_count(&diags));
    NAOS_CHECK_STR_EQ("invalid argument declaration: flag '--force' cannot be required or have a default in command 'run'",
                      mnd_diag_list_at(&diags, 0)->message);
    NAOS_CHECK_STR_EQ("invalid argument declaration: duplicate command 'run'",
                      mnd_diag_list_at(&diags, 1)->message);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(errors_in_several_commands_are_all_reported)
{
    decl_fixture_t f;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    fixture_init(&f);
    f.program.version = NULL;
    f.run_options[0].short_name = 'h';
    f.commands[1].positional_count = 0;

    NAOS_CHECK_FALSE(mnd_args_decl_check(&f.program, &diags));
    NAOS_CHECK_UINT_EQ(3, mnd_diag_list_count(&diags));
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));
    mnd_diag_list_free(&diags);
}

NAOS_TEST(check_appends_to_existing_diagnostics)
{
    decl_fixture_t f;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    fixture_init(&f);
    f.run_options[1].short_name = 'n';
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, NULL, 0, "%s", "earlier");

    NAOS_CHECK_FALSE(mnd_args_decl_check(&f.program, &diags));
    NAOS_REQUIRE_UINT_EQ(2, mnd_diag_list_count(&diags));
    NAOS_CHECK_STR_EQ("earlier", mnd_diag_list_at(&diags, 0)->message);
    NAOS_CHECK_STR_EQ("invalid argument declaration: duplicate option '-n' in command 'run'",
                      mnd_diag_list_at(&diags, 1)->message);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(names_with_percent_are_not_format_strings)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "-%s%n";
    check_single_error(&f.program, "invalid argument declaration: invalid command name '-%s%n'");
}

/* 検索 */

NAOS_TEST(find_command_matches_exact_bytes)
{
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_find_child(test_tool_program.root, "build", 5));
    NAOS_CHECK_PTR_EQ(test_tool_copy, mnd_args_find_child(test_tool_program.root, "copy", 4));
    NAOS_CHECK_PTR_EQ(test_tool_cat, mnd_args_find_child(test_tool_program.root, "cat", 3));
    /* name_len で区切った先頭部分だけを照合する */
    NAOS_CHECK_PTR_EQ(test_tool_cat, mnd_args_find_child(test_tool_program.root, "catalog", 3));
}

NAOS_TEST(find_command_is_case_sensitive_and_exact)
{
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "Build", 5));
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "BUILD", 5));
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "bui", 3));
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "builds", 6));
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "bulid", 5));
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "help", 4));
    NAOS_CHECK_NULL(mnd_args_find_child(test_tool_program.root, "", 0));
}

NAOS_TEST(find_command_in_program_without_commands)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.root.commands = NULL;
    f.root.command_count = 0;
    NAOS_CHECK_NULL(mnd_args_find_child(f.program.root, "run", 3));
}

NAOS_TEST(find_long_matches_exact_bytes)
{
    NAOS_CHECK_UINT_EQ(0, find_long_index(test_tool_build, "output", 6));
    NAOS_CHECK_UINT_EQ(1, find_long_index(test_tool_build, "keys", 4));
    NAOS_CHECK_UINT_EQ(2, find_long_index(test_tool_build, "level", 5));
    NAOS_CHECK_UINT_EQ(3, find_long_index(test_tool_build, "verbose", 7));
    NAOS_CHECK_UINT_EQ(4, find_long_index(test_tool_build, "quiet", 5));
    /* "--output=x" の '=' より前の部分 */
    NAOS_CHECK_UINT_EQ(0, find_long_index(test_tool_build, "output=x", 6));
}

NAOS_TEST(find_long_is_case_sensitive_and_rejects_prefix)
{
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_long_index(test_tool_build, "out", 3));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_long_index(test_tool_build, "Output", 6));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_long_index(test_tool_build, "outputs", 7));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_long_index(test_tool_build, "help", 4));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_long_index(test_tool_build, "", 0));
}

NAOS_TEST(find_long_in_command_without_options)
{
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_long_index(test_tool_copy, "output", 6));
}

NAOS_TEST(find_short_matches_exact_byte)
{
    NAOS_CHECK_UINT_EQ(0, find_short_index(test_tool_build, 'o'));
    NAOS_CHECK_UINT_EQ(1, find_short_index(test_tool_build, 'k'));
    NAOS_CHECK_UINT_EQ(3, find_short_index(test_tool_build, 'v'));
    NAOS_CHECK_UINT_EQ(4, find_short_index(test_tool_build, 'q'));
}

NAOS_TEST(find_short_is_case_sensitive)
{
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_short_index(test_tool_build, 'O'));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_short_index(test_tool_build, 'x'));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_short_index(test_tool_build, 'h'));
    /* 短い名前を持たないオプション（--level）に '\0' で一致しない */
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_short_index(test_tool_build, '\0'));
    NAOS_CHECK_UINT_EQ(DECL_TEST_NOT_FOUND, find_short_index(test_tool_cat, 'o'));
}

/* 値の表示名と位置引数の名前 */

NAOS_TEST(value_name_uses_declared_name)
{
    NAOS_CHECK_STR_EQ("FILE", mnd_args_value_name(&test_tool_build->options[0]));
    NAOS_CHECK_STR_EQ("KEY", mnd_args_value_name(&test_tool_build->options[1]));
}

NAOS_TEST(value_name_defaults_to_value)
{
    decl_fixture_t f;

    fixture_init(&f);
    NAOS_CHECK_STR_EQ("VALUE", mnd_args_value_name(&f.run_options[0]));
}

NAOS_TEST(positional_name_by_index)
{
    NAOS_CHECK_STR_EQ("SOURCE", mnd_args_positional_name(test_tool_build, 0));
    NAOS_CHECK_STR_EQ("SRC", mnd_args_positional_name(test_tool_copy, 0));
    NAOS_CHECK_STR_EQ("DEST", mnd_args_positional_name(test_tool_copy, 1));
}

NAOS_TEST(positional_name_repeats_last_name)
{
    decl_fixture_t f;

    NAOS_CHECK_STR_EQ("FILE", mnd_args_positional_name(test_tool_cat, 0));
    NAOS_CHECK_STR_EQ("FILE", mnd_args_positional_name(test_tool_cat, 1));
    NAOS_CHECK_STR_EQ("FILE", mnd_args_positional_name(test_tool_cat, 100));
    NAOS_CHECK_STR_EQ("DEST", mnd_args_positional_name(test_tool_copy, 2));

    fixture_init(&f);
    f.commands[1].positionals = f.run_positionals;
    f.commands[1].positional_count = 2;
    NAOS_CHECK_STR_EQ("INPUT", mnd_args_positional_name(&f.commands[1], 0));
    NAOS_CHECK_STR_EQ("OUTPUT", mnd_args_positional_name(&f.commands[1], 1));
    NAOS_CHECK_STR_EQ("OUTPUT", mnd_args_positional_name(&f.commands[1], 5));
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(tool_declaration_is_valid),
    NAOS_ADD_TEST(fixture_declaration_is_valid),
    NAOS_ADD_TEST(program_without_commands_is_valid),
    NAOS_ADD_TEST(command_without_options_and_positionals_is_valid),
    NAOS_ADD_TEST(value_option_may_have_empty_default),
    NAOS_ADD_TEST(required_value_option_is_valid),
    NAOS_ADD_TEST(digit_short_name_and_no_short_name_are_valid),
    NAOS_ADD_TEST(program_name_null_is_error),
    NAOS_ADD_TEST(program_name_empty_is_error),
    NAOS_ADD_TEST(program_version_null_is_error),
    NAOS_ADD_TEST(program_version_empty_is_error),
    NAOS_ADD_TEST(command_name_empty_is_error),
    NAOS_ADD_TEST(command_name_null_is_error),
    NAOS_ADD_TEST(command_name_starting_with_dash_is_error),
    NAOS_ADD_TEST(command_named_help_is_reserved),
    NAOS_ADD_TEST(duplicate_command_is_error),
    NAOS_ADD_TEST(commands_differing_in_case_are_not_duplicates),
    NAOS_ADD_TEST(long_name_null_is_error),
    NAOS_ADD_TEST(long_name_empty_is_error),
    NAOS_ADD_TEST(long_name_starting_with_dash_is_error),
    NAOS_ADD_TEST(long_name_containing_equals_is_error),
    NAOS_ADD_TEST(long_name_help_is_reserved),
    NAOS_ADD_TEST(short_name_h_is_reserved),
    NAOS_ADD_TEST(short_name_upper_h_is_not_reserved),
    NAOS_ADD_TEST(short_name_punctuation_is_error),
    NAOS_ADD_TEST(short_name_space_is_error),
    NAOS_ADD_TEST(short_name_non_ascii_byte_is_error),
    NAOS_ADD_TEST(duplicate_long_name_is_error),
    NAOS_ADD_TEST(duplicate_short_name_is_error),
    NAOS_ADD_TEST(same_option_in_different_commands_is_valid),
    NAOS_ADD_TEST(long_names_differing_in_case_are_not_duplicates),
    NAOS_ADD_TEST(required_flag_is_error),
    NAOS_ADD_TEST(flag_with_default_is_error),
    NAOS_ADD_TEST(required_and_defaulted_value_is_error),
    NAOS_ADD_TEST(required_and_defaulted_list_is_error),
    NAOS_ADD_TEST(list_default_empty_string_is_error),
    NAOS_ADD_TEST(list_default_leading_comma_is_error),
    NAOS_ADD_TEST(list_default_trailing_comma_is_error),
    NAOS_ADD_TEST(list_default_double_comma_is_error),
    NAOS_ADD_TEST(list_default_single_comma_is_error),
    NAOS_ADD_TEST(list_default_with_spaces_is_valid),
    NAOS_ADD_TEST(list_default_single_item_is_valid),
    NAOS_ADD_TEST(min_greater_than_max_is_error),
    NAOS_ADD_TEST(bounded_positional_names_fewer_than_max_is_error),
    NAOS_ADD_TEST(bounded_positional_names_more_than_max_is_error),
    NAOS_ADD_TEST(unlimited_without_positional_names_is_error),
    NAOS_ADD_TEST(unlimited_with_several_positional_names_is_valid),
    NAOS_ADD_TEST(two_errors_produce_two_diagnostics),
    NAOS_ADD_TEST(errors_in_several_commands_are_all_reported),
    NAOS_ADD_TEST(check_appends_to_existing_diagnostics),
    NAOS_ADD_TEST(names_with_percent_are_not_format_strings),
    NAOS_ADD_TEST(find_command_matches_exact_bytes),
    NAOS_ADD_TEST(find_command_is_case_sensitive_and_exact),
    NAOS_ADD_TEST(find_command_in_program_without_commands),
    NAOS_ADD_TEST(find_long_matches_exact_bytes),
    NAOS_ADD_TEST(find_long_is_case_sensitive_and_rejects_prefix),
    NAOS_ADD_TEST(find_long_in_command_without_options),
    NAOS_ADD_TEST(find_short_matches_exact_byte),
    NAOS_ADD_TEST(find_short_is_case_sensitive),
    NAOS_ADD_TEST(value_name_uses_declared_name),
    NAOS_ADD_TEST(value_name_defaults_to_value),
    NAOS_ADD_TEST(positional_name_by_index),
    NAOS_ADD_TEST(positional_name_repeats_last_name)
)
