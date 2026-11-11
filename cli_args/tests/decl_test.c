#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts_internal.h"
#include "clopts/errors.h"
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
    clopts_option_t run_options[3];
    clopts_positional_t run_positionals[2];
    clopts_positional_t list_positionals[1];
    clopts_command_t commands[2];
    clopts_command_t root;
    clopts_program_t program;
} decl_fixture_t;

static void set_option(clopts_option_t *option, const char *long_name, char short_name,
                       clopts_kind_t kind, const char *value_name, const char *default_value)
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

    set_option(&f->run_options[0], "name", 'n', CLOPTS_VALUE, NULL, NULL);
    set_option(&f->run_options[1], "force", 'f', CLOPTS_FLAG, NULL, NULL);
    set_option(&f->run_options[2], "tags", 't', CLOPTS_LIST, "TAG", "x,y");

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
    f->commands[1].max_positionals = CLOPTS_UNLIMITED;

    f->root.commands = f->commands;
    f->root.command_count = 2;

    f->program.name = "prog";
    f->program.version = "0.1";
    f->program.root = &f->root;
}

/*
 * 入れ子の規則を確かめるための、書き換え可能な入れ子の宣言（プログラム名 "nest"）。
 * nest_init の直後は妥当な宣言で、各テストが一部だけ書き換える。
 *   ルート: --verbose/-v（フラグ、引き継ぐ）、--config/-c <FILE>（値、引き継ぐ、既定値 "nest.cfg"）、
 *           --dry-run/-d（フラグ、引き継がない）
 *   remote（子を持つ）: --token/-t <TOKEN>（値、引き継ぐ）
 *     remote add: --force/-f（フラグ）。位置引数 NAME、URL（2 個ちょうど）
 *     remote remove: オプションなし。位置引数 NAME（1 個ちょうど）
 *   status: オプションなし。位置引数なし
 */
typedef struct nest_fixture
{
    clopts_option_t root_options[3];
    clopts_option_t remote_options[1];
    clopts_option_t add_options[2];
    clopts_positional_t add_positionals[2];
    clopts_positional_t remove_positionals[1];
    clopts_command_t remote_commands[2];
    clopts_command_t commands[2];
    clopts_command_t root;
    clopts_program_t program;
} nest_fixture_t;

static void set_leaf(clopts_command_t *command, const char *name,
                     const clopts_positional_t *positionals, size_t count)
{
    command->name = name;
    command->summary = NULL;
    command->description = NULL;
    command->positionals = positionals;
    command->positional_count = count;
    command->min_positionals = count;
    command->max_positionals = count;
}

static void nest_init(nest_fixture_t *f)
{
    memset(f, 0, sizeof *f);

    set_option(&f->root_options[0], "verbose", 'v', CLOPTS_FLAG, NULL, NULL);
    f->root_options[0].inherited = true;
    set_option(&f->root_options[1], "config", 'c', CLOPTS_VALUE, "FILE", "nest.cfg");
    f->root_options[1].inherited = true;
    set_option(&f->root_options[2], "dry-run", 'd', CLOPTS_FLAG, NULL, NULL);
    set_option(&f->remote_options[0], "token", 't', CLOPTS_VALUE, "TOKEN", NULL);
    f->remote_options[0].inherited = true;
    /* add_options[1] は、テストが 2 つ目のオプションを加えるときに使う（既定では数えない） */
    set_option(&f->add_options[0], "force", 'f', CLOPTS_FLAG, NULL, NULL);
    set_option(&f->add_options[1], "extra", 'x', CLOPTS_VALUE, NULL, NULL);

    f->add_positionals[0].name = "NAME";
    f->add_positionals[1].name = "URL";
    f->remove_positionals[0].name = "NAME";

    set_leaf(&f->remote_commands[0], "add", f->add_positionals, 2);
    f->remote_commands[0].options = f->add_options;
    f->remote_commands[0].option_count = 1;
    set_leaf(&f->remote_commands[1], "remove", f->remove_positionals, 1);

    set_leaf(&f->commands[0], "remote", NULL, 0);
    f->commands[0].options = f->remote_options;
    f->commands[0].option_count = 1;
    f->commands[0].commands = f->remote_commands;
    f->commands[0].command_count = 2;
    set_leaf(&f->commands[1], "status", NULL, 0);

    f->root.options = f->root_options;
    f->root.option_count = 3;
    f->root.commands = f->commands;
    f->root.command_count = 2;

    f->program.name = "nest";
    f->program.version = "3.0";
    f->program.root = &f->root;
}

/* add に 2 つ目のオプション（long_name と short_name を指定）を加える */
static void nest_add_option(nest_fixture_t *f, const char *long_name, char short_name)
{
    f->add_options[1].long_name = long_name;
    f->add_options[1].short_name = short_name;
    f->remote_commands[0].option_count = 2;
}

/* test_tool_program のルートコマンドと command の 2 段の経路を作る */
static void begin_path(clopts_result_t *result, const clopts_command_t *command)
{
    clopts_result_init(result);
    result->program = &test_tool_program;
    NAOS_REQUIRE_TRUE(clopts_result_push(result, test_tool_program.root));
    NAOS_REQUIRE_TRUE(clopts_result_push(result, command));
}

static size_t found_index(bool found, clopts_ref_t ref)
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
static size_t find_long_index(const clopts_command_t *command, const char *name, size_t name_len)
{
    clopts_result_t result;
    clopts_ref_t ref = { 0, 0 };
    size_t index;
    bool found;

    begin_path(&result, command);
    /* 引数の評価順は規定されないので、ref を設定する呼び出しを先に済ませる */
    found = clopts_find_long(&result, name, name_len, &ref);
    index = found_index(found, ref);
    clopts_result_free(&result);
    return index;
}

static size_t find_short_index(const clopts_command_t *command, char short_name)
{
    clopts_result_t result;
    clopts_ref_t ref = { 0, 0 };
    size_t index;
    bool found;

    begin_path(&result, command);
    /* 引数の評価順は規定されないので、ref を設定する呼び出しを先に済ませる */
    found = clopts_find_short(&result, short_name, &ref);
    index = found_index(found, ref);
    clopts_result_free(&result);
    return index;
}

/* 宣言の検査が失敗し、期待するメッセージのエラーがちょうど 1 件だけ追加されることを確かめる */
static void check_single_error(const clopts_program_t *program, const char *expected)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    const char *message;
    bool ok;

    ok = clopts_decl_check(program, &errors);
    NAOS_CHECK_FALSE(ok);
    NAOS_CHECK_UINT_EQ(1, clopts_errors_count(&errors));
    if (clopts_errors_count(&errors) >= 1)
    {
        message = clopts_errors_at(&errors, 0);
        NAOS_CHECK_STR_EQ(expected, message);
    }

    clopts_errors_free(&errors);
}

/* 宣言の検査が成功し、エラーが追加されないことを確かめる */
static void check_valid(const clopts_program_t *program)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    NAOS_CHECK_TRUE(clopts_decl_check(program, &errors));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    clopts_errors_free(&errors);
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
    check_single_error(&f.program, "invalid argument declaration: invalid command name '' in command 'prog'");
}

NAOS_TEST(command_name_null_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = NULL;
    check_single_error(&f.program, "invalid argument declaration: invalid command name '' in command 'prog'");
}

NAOS_TEST(command_name_starting_with_dash_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "-list";
    check_single_error(&f.program, "invalid argument declaration: invalid command name '-list' in command 'prog'");
}

NAOS_TEST(command_named_help_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "help";
    check_single_error(&f.program, "invalid argument declaration: command name 'help' is reserved in command 'prog'");
}

NAOS_TEST(duplicate_command_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "run";
    check_single_error(&f.program, "invalid argument declaration: duplicate command 'run' in command 'prog'");
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
                       "invalid argument declaration: invalid option name '' in command 'prog run'");
}

NAOS_TEST(long_name_empty_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name '' in command 'prog run'");
}

NAOS_TEST(long_name_starting_with_dash_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "-name";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name '-name' in command 'prog run'");
}

NAOS_TEST(long_name_containing_equals_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "na=me";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid option name 'na=me' in command 'prog run'");
}

NAOS_TEST(long_name_help_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "help";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--help' is reserved in command 'prog run'");
}

NAOS_TEST(short_name_h_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].short_name = 'h';
    check_single_error(&f.program,
                       "invalid argument declaration: option '-h' is reserved in command 'prog run'");
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
                       "invalid argument declaration: invalid short option for '--force' in command 'prog run'");
}

NAOS_TEST(short_name_space_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].short_name = ' ';
    check_single_error(&f.program,
                       "invalid argument declaration: invalid short option for '--force' in command 'prog run'");
}

NAOS_TEST(short_name_non_ascii_byte_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].short_name = '\xE6';
    check_single_error(&f.program,
                       "invalid argument declaration: invalid short option for '--force' in command 'prog run'");
}

/* 同じサブコマンド内の重複 */

NAOS_TEST(duplicate_long_name_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].long_name = "name";
    check_single_error(&f.program,
                       "invalid argument declaration: duplicate option '--name' in command 'prog run'");
}

NAOS_TEST(duplicate_short_name_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].short_name = 'n';
    check_single_error(&f.program,
                       "invalid argument declaration: duplicate option '-n' in command 'prog run'");
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
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'prog run'");
}

NAOS_TEST(flag_with_default_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].default_value = "1";
    check_single_error(&f.program,
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'prog run'");
}

NAOS_TEST(required_and_defaulted_value_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].required = true;
    f.run_options[0].default_value = "a";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--name' cannot be both required and defaulted in command 'prog run'");
}

NAOS_TEST(required_and_defaulted_list_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].required = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--tags' cannot be both required and defaulted in command 'prog run'");
}

/* リスト値の既定値の空の項目 */

static void check_list_default_has_empty_item(const char *default_value)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].default_value = default_value;
    check_single_error(&f.program,
                       "invalid argument declaration: default of '--tags' has an empty item in command 'prog run'");
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
                       "invalid argument declaration: invalid positional range in command 'prog run'");
}

NAOS_TEST(bounded_positional_names_fewer_than_max_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[0].max_positionals = 2;
    check_single_error(&f.program,
                       "invalid argument declaration: positional names do not match the range in command 'prog run'");
}

NAOS_TEST(bounded_positional_names_more_than_max_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[0].positional_count = 2;
    check_single_error(&f.program,
                       "invalid argument declaration: positional names do not match the range in command 'prog run'");
}

NAOS_TEST(unlimited_without_positional_names_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].positionals = NULL;
    f.commands[1].positional_count = 0;
    check_single_error(&f.program,
                       "invalid argument declaration: positional names do not match the range in command 'prog list'");
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
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.commands[1].name = "run";
    f.run_options[1].required = true;

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: flag '--force' cannot be required or have a default in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: duplicate command 'run' in command 'prog'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

NAOS_TEST(errors_in_several_commands_are_all_reported)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.program.version = NULL;
    f.run_options[0].short_name = 'h';
    f.commands[1].positional_count = 0;

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_CHECK_UINT_EQ(3, clopts_errors_count(&errors));
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));
    clopts_errors_free(&errors);
}

NAOS_TEST(check_appends_to_existing_diagnostics)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[1].short_name = 'n';
    /* 内部の追加の操作で、あらかじめ 1 件入れておく */
    clopts_errors_add(&errors, "%s", "earlier");
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&errors));

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("earlier", clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: duplicate option '-n' in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

NAOS_TEST(names_with_percent_are_not_format_strings)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.commands[1].name = "-%s%n";
    check_single_error(&f.program, "invalid argument declaration: invalid command name '-%s%n' in command 'prog'");
}

/* ルートコマンド */

NAOS_TEST(root_null_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.program.root = NULL;
    check_single_error(&f.program, "invalid argument declaration: root command is required");
}

/* 入れ子の宣言（コマンドの木） */

NAOS_TEST(nested_declaration_is_valid)
{
    nest_fixture_t f;

    nest_init(&f);
    check_valid(&f.program);
}

NAOS_TEST(same_child_name_under_different_parents_is_valid)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_commands[1].name = "status";
    check_valid(&f.program);
}

NAOS_TEST(same_option_in_sibling_commands_is_valid)
{
    nest_fixture_t f;

    nest_init(&f);
    /* status は remote の子孫ではないので、remote の --token/-t と重複しない */
    f.commands[1].options = f.remote_options;
    f.commands[1].option_count = 1;
    check_valid(&f.program);
}

NAOS_TEST(deep_declaration_is_checked_without_depth_limit)
{
    clopts_command_t chain[40];
    clopts_option_t bad;
    clopts_program_t program;
    size_t i;

    memset(chain, 0, sizeof chain);
    for (i = 0; i < 40; ++i)
    {
        chain[i].name = "c";
        if (i + 1 < 40)
        {
            chain[i].commands = &chain[i + 1];
            chain[i].command_count = 1;
        }
    }

    /* set_option が設定しないメンバ（aliases など）を 0 にしておく */
    memset(&bad, 0, sizeof bad);
    set_option(&bad, "help", '\0', CLOPTS_FLAG, NULL, NULL);
    bad.inherited = false;
    chain[39].options = &bad;
    chain[39].option_count = 1;
    program.name = "deep";
    program.version = "1";
    program.root = &chain[0];

    {
        clopts_errors_t errors = CLOPTS_ERRORS_INIT;
        const char *message;

        NAOS_CHECK_FALSE(clopts_decl_check(&program, &errors));
        NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&errors));
        message = clopts_errors_at(&errors, 0);
        /* 経路は "deep" と 39 個の " c" */
        NAOS_CHECK_TRUE(strstr(message, "option '--help' is reserved in command 'deep c c ") != NULL);
        NAOS_CHECK_UINT_EQ(strlen("invalid argument declaration: option '--help' is reserved in command 'deep'")
                               + 39 * 2,
                           strlen(message));
        clopts_errors_free(&errors);
    }
}

/* 子を持つコマンドの位置引数（1.6） */

NAOS_TEST(command_with_children_and_positional_names_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.commands[0].positionals = f.remove_positionals;
    f.commands[0].positional_count = 1;
    check_single_error(&f.program,
                       "invalid argument declaration: command 'nest remote' cannot have both subcommands and positional arguments");
}

NAOS_TEST(command_with_children_and_min_positionals_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.commands[0].min_positionals = 1;
    check_single_error(&f.program,
                       "invalid argument declaration: command 'nest remote' cannot have both subcommands and positional arguments");
}

NAOS_TEST(command_with_children_and_max_positionals_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.commands[0].max_positionals = CLOPTS_UNLIMITED;
    check_single_error(&f.program,
                       "invalid argument declaration: command 'nest remote' cannot have both subcommands and positional arguments");
}

NAOS_TEST(root_with_children_and_positionals_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.root.positionals = f.list_positionals;
    f.root.positional_count = 1;
    f.root.min_positionals = 0;
    f.root.max_positionals = CLOPTS_UNLIMITED;
    check_single_error(&f.program,
                       "invalid argument declaration: command 'prog' cannot have both subcommands and positional arguments");
}

NAOS_TEST(root_without_children_may_have_positionals)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.root.commands = NULL;
    f.root.command_count = 0;
    f.root.positionals = f.list_positionals;
    f.root.positional_count = 1;
    f.root.max_positionals = CLOPTS_UNLIMITED;
    check_valid(&f.program);
}

/* 子コマンド名（1.7） */

NAOS_TEST(duplicate_child_in_nested_command_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_commands[1].name = "add";
    check_single_error(&f.program,
                       "invalid argument declaration: duplicate command 'add' in command 'nest remote'");
}

NAOS_TEST(child_named_help_in_nested_command_is_reserved)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_commands[1].name = "help";
    check_single_error(&f.program,
                       "invalid argument declaration: command name 'help' is reserved in command 'nest remote'");
}

NAOS_TEST(invalid_child_name_in_nested_command_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_commands[1].name = "-rm";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid command name '-rm' in command 'nest remote'");
}

/* 祖先のオプションとの重複（1.8） */

NAOS_TEST(long_name_of_inherited_ancestor_option_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    nest_add_option(&f, "verbose", '\0');
    check_single_error(&f.program,
                       "invalid argument declaration: option '--verbose' in command 'nest remote add' duplicates an option of command 'nest'");
}

NAOS_TEST(long_name_of_non_inherited_ancestor_option_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    nest_add_option(&f, "dry-run", '\0');
    check_single_error(&f.program,
                       "invalid argument declaration: option '--dry-run' in command 'nest remote add' duplicates an option of command 'nest'");
}

NAOS_TEST(long_name_of_parent_option_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    nest_add_option(&f, "token", '\0');
    check_single_error(&f.program,
                       "invalid argument declaration: option '--token' in command 'nest remote add' duplicates an option of command 'nest remote'");
}

NAOS_TEST(short_name_of_inherited_ancestor_option_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    nest_add_option(&f, "extra", 'v');
    check_single_error(&f.program,
                       "invalid argument declaration: option '-v' in command 'nest remote add' duplicates an option of command 'nest'");
}

NAOS_TEST(short_name_of_non_inherited_ancestor_option_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_options[0].short_name = 'd';
    check_single_error(&f.program,
                       "invalid argument declaration: option '-d' in command 'nest remote' duplicates an option of command 'nest'");
}

NAOS_TEST(long_and_short_of_ancestor_option_are_two_errors)
{
    nest_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    nest_init(&f);
    nest_add_option(&f, "config", 'c');

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: option '--config' in command 'nest remote add' duplicates an option of command 'nest'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: option '-c' in command 'nest remote add' duplicates an option of command 'nest'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

/* --version という長い名前（1.9） */

NAOS_TEST(long_name_version_is_reserved)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "version";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--version' is reserved in command 'prog run'");
}

NAOS_TEST(long_name_version_in_root_is_reserved)
{
    nest_fixture_t f;

    nest_init(&f);
    f.root_options[2].long_name = "version";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--version' is reserved in command 'nest'");
}

/* 2 段目のコマンドの既存の規則の誤り */

NAOS_TEST(error_in_second_level_command_shows_path)
{
    nest_fixture_t f;

    nest_init(&f);
    f.add_options[0].required = true;
    check_single_error(&f.program,
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'nest remote add'");
}

NAOS_TEST(positional_range_error_in_second_level_command_shows_path)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_commands[1].min_positionals = 2;
    check_single_error(&f.program,
                       "invalid argument declaration: invalid positional range in command 'nest remote remove'");
}

NAOS_TEST(option_error_in_nested_parent_shows_path)
{
    nest_fixture_t f;

    nest_init(&f);
    f.remote_options[0].short_name = 'h';
    check_single_error(&f.program,
                       "invalid argument declaration: option '-h' is reserved in command 'nest remote'");
}

NAOS_TEST(two_errors_in_nested_declaration_produce_two_diagnostics)
{
    nest_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    nest_init(&f);
    f.remote_commands[1].name = "help";
    f.commands[1].min_positionals = 1;

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: command name 'help' is reserved in command 'nest remote'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid positional range in command 'nest status'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

/* 自分自身を子孫に持つコマンド */

NAOS_TEST(command_containing_itself_as_child_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    /* remote の子コマンドを、remote 自身と status にする */
    f.commands[0].commands = f.commands;
    f.commands[0].command_count = 2;
    check_single_error(&f.program, "invalid argument declaration: command 'nest remote' contains itself");
}

NAOS_TEST(command_containing_itself_as_descendant_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    /* remove の子コマンドを remote と status にする（remove は子を持つので位置引数をなくす） */
    set_leaf(&f.remote_commands[1], "remove", NULL, 0);
    f.remote_commands[1].commands = f.commands;
    f.remote_commands[1].command_count = 2;
    check_single_error(&f.program, "invalid argument declaration: command 'nest remote' contains itself");
}

NAOS_TEST(root_containing_itself_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.root.name = "self";
    f.root.commands = &f.root;
    f.root.command_count = 1;
    check_single_error(&f.program, "invalid argument declaration: command 'prog' contains itself");
}

/* 検索 */

NAOS_TEST(find_command_matches_exact_bytes)
{
    NAOS_CHECK_PTR_EQ(test_tool_build, clopts_find_child(test_tool_program.root, "build", 5));
    NAOS_CHECK_PTR_EQ(test_tool_copy, clopts_find_child(test_tool_program.root, "copy", 4));
    NAOS_CHECK_PTR_EQ(test_tool_cat, clopts_find_child(test_tool_program.root, "cat", 3));
    /* name_len で区切った先頭部分だけを照合する */
    NAOS_CHECK_PTR_EQ(test_tool_cat, clopts_find_child(test_tool_program.root, "catalog", 3));
}

NAOS_TEST(find_command_is_case_sensitive_and_exact)
{
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "Build", 5));
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "BUILD", 5));
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "bui", 3));
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "builds", 6));
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "bulid", 5));
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "help", 4));
    NAOS_CHECK_NULL(clopts_find_child(test_tool_program.root, "", 0));
}

NAOS_TEST(find_command_in_program_without_commands)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.root.commands = NULL;
    f.root.command_count = 0;
    NAOS_CHECK_NULL(clopts_find_child(f.program.root, "run", 3));
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
    NAOS_CHECK_STR_EQ("FILE", clopts_value_name(&test_tool_build->options[0]));
    NAOS_CHECK_STR_EQ("KEY", clopts_value_name(&test_tool_build->options[1]));
}

NAOS_TEST(value_name_defaults_to_value)
{
    decl_fixture_t f;

    fixture_init(&f);
    NAOS_CHECK_STR_EQ("VALUE", clopts_value_name(&f.run_options[0]));
}

NAOS_TEST(positional_name_by_index)
{
    NAOS_CHECK_STR_EQ("SOURCE", clopts_positional_name(test_tool_build, 0));
    NAOS_CHECK_STR_EQ("SRC", clopts_positional_name(test_tool_copy, 0));
    NAOS_CHECK_STR_EQ("DEST", clopts_positional_name(test_tool_copy, 1));
}

NAOS_TEST(positional_name_repeats_last_name)
{
    decl_fixture_t f;

    NAOS_CHECK_STR_EQ("FILE", clopts_positional_name(test_tool_cat, 0));
    NAOS_CHECK_STR_EQ("FILE", clopts_positional_name(test_tool_cat, 1));
    NAOS_CHECK_STR_EQ("FILE", clopts_positional_name(test_tool_cat, 100));
    NAOS_CHECK_STR_EQ("DEST", clopts_positional_name(test_tool_copy, 2));

    fixture_init(&f);
    f.commands[1].positionals = f.run_positionals;
    f.commands[1].positional_count = 2;
    NAOS_CHECK_STR_EQ("INPUT", clopts_positional_name(&f.commands[1], 0));
    NAOS_CHECK_STR_EQ("OUTPUT", clopts_positional_name(&f.commands[1], 1));
    NAOS_CHECK_STR_EQ("OUTPUT", clopts_positional_name(&f.commands[1], 5));
}

/* 否定できるフラグの宣言（14.10、14.11、14.16） */

NAOS_TEST(neg_declaration_is_valid)
{
    check_valid(&test_neg_program);
}

NAOS_TEST(negatable_flag_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].negatable = true;
    check_valid(&f.program);
}

NAOS_TEST(negatable_flag_enabled_by_default_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].negatable = true;
    f.run_options[1].default_enabled = true;
    check_valid(&f.program);
}

NAOS_TEST(negatable_value_option_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].negatable = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--name' cannot be negatable in command 'prog run'");
}

NAOS_TEST(negatable_list_option_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].negatable = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--tags' cannot be negatable in command 'prog run'");
}

NAOS_TEST(non_negatable_flag_enabled_by_default_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].default_enabled = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--force' must be a negatable flag to be enabled by default in command 'prog run'");
}

NAOS_TEST(value_option_enabled_by_default_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].default_enabled = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--name' must be a negatable flag to be enabled by default in command 'prog run'");
}

NAOS_TEST(list_option_enabled_by_default_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].default_enabled = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--tags' must be a negatable flag to be enabled by default in command 'prog run'");
}

NAOS_TEST(negatable_value_option_enabled_by_default_is_two_errors)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[0].negatable = true;
    f.run_options[0].default_enabled = true;

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: option '--name' cannot be negatable in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: option '--name' must be a negatable flag to be enabled by default in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

NAOS_TEST(negatable_flag_with_default_value_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].negatable = true;
    f.run_options[1].default_value = "on";
    check_single_error(&f.program,
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'prog run'");
}

NAOS_TEST(negated_name_after_negatable_flag_in_same_command_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].negatable = true;
    f.run_options[2].long_name = "no-force";
    check_single_error(&f.program,
                       "invalid argument declaration: option '--no-force' conflicts with negatable flag '--force' in command 'prog run'");
}

NAOS_TEST(negated_name_before_negatable_flag_in_same_command_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].long_name = "no-force";
    f.run_options[1].negatable = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--no-force' conflicts with negatable flag '--force' in command 'prog run'");
}

NAOS_TEST(negated_name_in_descendant_of_inherited_negatable_flag_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.root_options[0].negatable = true;
    nest_add_option(&f, "no-verbose", '\0');
    check_single_error(&f.program,
                       "invalid argument declaration: option '--no-verbose' in command 'nest remote add' conflicts with negatable flag '--verbose' of command 'nest'");
}

NAOS_TEST(negated_name_in_descendant_of_non_inherited_negatable_flag_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.root_options[2].negatable = true;
    nest_add_option(&f, "no-dry-run", '\0');
    check_single_error(&f.program,
                       "invalid argument declaration: option '--no-dry-run' in command 'nest remote add' conflicts with negatable flag '--dry-run' of command 'nest'");
}

NAOS_TEST(negatable_flag_in_descendant_of_negated_name_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.root_options[0].long_name = "no-force";
    f.add_options[0].negatable = true;
    check_single_error(&f.program,
                       "invalid argument declaration: negatable flag '--force' in command 'nest remote add' conflicts with option '--no-force' of command 'nest'");
}

NAOS_TEST(negatable_flag_in_descendant_of_non_inherited_negated_name_is_error)
{
    nest_fixture_t f;

    nest_init(&f);
    f.root_options[2].long_name = "no-token";
    f.remote_options[0].kind = CLOPTS_FLAG;
    f.remote_options[0].value_name = NULL;
    f.remote_options[0].negatable = true;
    check_single_error(&f.program,
                       "invalid argument declaration: negatable flag '--token' in command 'nest remote' conflicts with option '--no-token' of command 'nest'");
}

NAOS_TEST(non_negatable_flag_and_negated_name_are_valid)
{
    decl_fixture_t f;
    nest_fixture_t n;

    fixture_init(&f);
    f.run_options[2].long_name = "no-force";
    check_valid(&f.program);

    nest_init(&n);
    nest_add_option(&n, "no-verbose", '\0');
    check_valid(&n.program);
}

NAOS_TEST(negatable_flag_named_with_no_prefix_alone_is_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].long_name = "no-x";
    f.run_options[1].negatable = true;
    check_valid(&f.program);
}

/* 隠しオプションの宣言（16.1、16.6） */

NAOS_TEST(ext_declaration_passes_decl_check)
{
    check_valid(&test_ext_program);
}

NAOS_TEST(hidden_flag_value_and_list_are_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].hidden = true;
    f.run_options[1].hidden = true;
    f.run_options[2].hidden = true;
    check_valid(&f.program);
}

NAOS_TEST(required_and_hidden_value_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].required = true;
    f.run_options[0].hidden = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--name' cannot be both required and hidden in command 'prog run'");
}

NAOS_TEST(required_and_hidden_list_is_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[2].default_value = NULL;
    f.run_options[2].required = true;
    f.run_options[2].hidden = true;
    check_single_error(&f.program,
                       "invalid argument declaration: option '--tags' cannot be both required and hidden in command 'prog run'");
}

NAOS_TEST(required_and_hidden_flag_is_two_errors)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[1].required = true;
    f.run_options[1].hidden = true;

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: flag '--force' cannot be required or have a default in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: option '--force' cannot be both required and hidden in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

NAOS_TEST(hidden_flag_with_default_is_one_error)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[1].default_value = "1";
    f.run_options[1].hidden = true;
    check_single_error(&f.program,
                       "invalid argument declaration: flag '--force' cannot be required or have a default in command 'prog run'");
}

/* 別名の形と予約（17.13） */

/* run の --name に aliases を付けた宣言が、期待するメッセージの誤り 1 件になることを確かめる */
static void check_alias_error(const char *aliases, const char *expected)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].aliases = aliases;
    check_single_error(&f.program, expected);
}

NAOS_TEST(well_formed_aliases_are_valid)
{
    decl_fixture_t f;

    fixture_init(&f);
    f.run_options[0].aliases = "nm";
    f.run_options[1].aliases = "a-b,Help,VERSION,helper";
    f.run_options[2].aliases = "tag,label";
    check_valid(&f.program);
}

NAOS_TEST(alias_empty_string_is_error)
{
    check_alias_error("",
                      "invalid argument declaration: invalid alias '' of option '--name' in command 'prog run'");
}

NAOS_TEST(alias_double_comma_is_error)
{
    check_alias_error("a,,b",
                      "invalid argument declaration: invalid alias '' of option '--name' in command 'prog run'");
}

NAOS_TEST(alias_leading_comma_is_error)
{
    check_alias_error(",a",
                      "invalid argument declaration: invalid alias '' of option '--name' in command 'prog run'");
}

NAOS_TEST(alias_trailing_comma_is_error)
{
    check_alias_error("a,",
                      "invalid argument declaration: invalid alias '' of option '--name' in command 'prog run'");
}

NAOS_TEST(alias_starting_with_dash_is_error)
{
    check_alias_error("-x",
                      "invalid argument declaration: invalid alias '-x' of option '--name' in command 'prog run'");
}

NAOS_TEST(alias_containing_equals_is_error)
{
    check_alias_error("a=b",
                      "invalid argument declaration: invalid alias 'a=b' of option '--name' in command 'prog run'");
}

NAOS_TEST(invalid_alias_among_valid_ones_shows_only_that_alias)
{
    check_alias_error("nm,-x,alt",
                      "invalid argument declaration: invalid alias '-x' of option '--name' in command 'prog run'");
}

NAOS_TEST(alias_help_is_reserved)
{
    check_alias_error("help",
                      "invalid argument declaration: alias '--help' of option '--name' is reserved in command 'prog run'");
}

NAOS_TEST(alias_version_after_valid_alias_is_reserved)
{
    check_alias_error("x,version",
                      "invalid argument declaration: alias '--version' of option '--name' is reserved in command 'prog run'");
}

NAOS_TEST(alias_errors_are_reported_per_alias)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[0].aliases = "-x,ok,a=b,help,";

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(4, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias '-x' of option '--name' in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias 'a=b' of option '--name' in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    NAOS_CHECK_STR_EQ("invalid argument declaration: alias '--help' of option '--name' is reserved in command 'prog run'",
                      clopts_errors_at(&errors, 2));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias '' of option '--name' in command 'prog run'",
                      clopts_errors_at(&errors, 3));
    clopts_errors_free(&errors);
}

NAOS_TEST(two_invalid_aliases_are_two_errors)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[2].aliases = ",";

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias '' of option '--tags' in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias '' of option '--tags' in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    clopts_errors_free(&errors);
}

NAOS_TEST(aliases_are_checked_when_long_name_is_invalid)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[0].long_name = "";
    f.run_options[0].aliases = "-x,version";

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(3, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid option name '' in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias '-x' of option '--' in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    NAOS_CHECK_STR_EQ("invalid argument declaration: alias '--version' of option '--' is reserved in command 'prog run'",
                      clopts_errors_at(&errors, 2));
    clopts_errors_free(&errors);
}

NAOS_TEST(aliases_are_checked_when_long_name_is_null_or_reserved)
{
    decl_fixture_t f;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    fixture_init(&f);
    f.run_options[0].long_name = NULL;
    f.run_options[0].aliases = "";
    f.run_options[1].long_name = "help";
    f.run_options[1].aliases = "a=b";

    NAOS_CHECK_FALSE(clopts_decl_check(&f.program, &errors));
    NAOS_REQUIRE_UINT_EQ(4, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid option name '' in command 'prog run'",
                      clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias '' of option '--' in command 'prog run'",
                      clopts_errors_at(&errors, 1));
    NAOS_CHECK_STR_EQ("invalid argument declaration: option '--help' is reserved in command 'prog run'",
                      clopts_errors_at(&errors, 2));
    NAOS_CHECK_STR_EQ("invalid argument declaration: invalid alias 'a=b' of option '--help' in command 'prog run'",
                      clopts_errors_at(&errors, 3));
    clopts_errors_free(&errors);
}

NAOS_TEST(alias_error_in_nested_command_shows_path)
{
    nest_fixture_t f;

    nest_init(&f);
    f.add_options[0].aliases = "help";
    check_single_error(&f.program,
                       "invalid argument declaration: alias '--help' of option '--force' is reserved in command 'nest remote add'");

    nest_init(&f);
    f.root_options[1].aliases = "cfg,-c";
    check_single_error(&f.program,
                       "invalid argument declaration: invalid alias '-c' of option '--config' in command 'nest'");
}

NAOS_TEST(alias_with_percent_is_not_a_format_string)
{
    check_alias_error("-%s%n",
                      "invalid argument declaration: invalid alias '-%s%n' of option '--name' in command 'prog run'");
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
    NAOS_ADD_TEST(root_null_is_error),
    NAOS_ADD_TEST(nested_declaration_is_valid),
    NAOS_ADD_TEST(same_child_name_under_different_parents_is_valid),
    NAOS_ADD_TEST(same_option_in_sibling_commands_is_valid),
    NAOS_ADD_TEST(deep_declaration_is_checked_without_depth_limit),
    NAOS_ADD_TEST(command_with_children_and_positional_names_is_error),
    NAOS_ADD_TEST(command_with_children_and_min_positionals_is_error),
    NAOS_ADD_TEST(command_with_children_and_max_positionals_is_error),
    NAOS_ADD_TEST(root_with_children_and_positionals_is_error),
    NAOS_ADD_TEST(root_without_children_may_have_positionals),
    NAOS_ADD_TEST(duplicate_child_in_nested_command_is_error),
    NAOS_ADD_TEST(child_named_help_in_nested_command_is_reserved),
    NAOS_ADD_TEST(invalid_child_name_in_nested_command_is_error),
    NAOS_ADD_TEST(long_name_of_inherited_ancestor_option_is_error),
    NAOS_ADD_TEST(long_name_of_non_inherited_ancestor_option_is_error),
    NAOS_ADD_TEST(long_name_of_parent_option_is_error),
    NAOS_ADD_TEST(short_name_of_inherited_ancestor_option_is_error),
    NAOS_ADD_TEST(short_name_of_non_inherited_ancestor_option_is_error),
    NAOS_ADD_TEST(long_and_short_of_ancestor_option_are_two_errors),
    NAOS_ADD_TEST(long_name_version_is_reserved),
    NAOS_ADD_TEST(long_name_version_in_root_is_reserved),
    NAOS_ADD_TEST(error_in_second_level_command_shows_path),
    NAOS_ADD_TEST(positional_range_error_in_second_level_command_shows_path),
    NAOS_ADD_TEST(option_error_in_nested_parent_shows_path),
    NAOS_ADD_TEST(two_errors_in_nested_declaration_produce_two_diagnostics),
    NAOS_ADD_TEST(command_containing_itself_as_child_is_error),
    NAOS_ADD_TEST(command_containing_itself_as_descendant_is_error),
    NAOS_ADD_TEST(root_containing_itself_is_error),
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
    NAOS_ADD_TEST(positional_name_repeats_last_name),
    NAOS_ADD_TEST(neg_declaration_is_valid),
    NAOS_ADD_TEST(negatable_flag_is_valid),
    NAOS_ADD_TEST(negatable_flag_enabled_by_default_is_valid),
    NAOS_ADD_TEST(negatable_value_option_is_error),
    NAOS_ADD_TEST(negatable_list_option_is_error),
    NAOS_ADD_TEST(non_negatable_flag_enabled_by_default_is_error),
    NAOS_ADD_TEST(value_option_enabled_by_default_is_error),
    NAOS_ADD_TEST(list_option_enabled_by_default_is_error),
    NAOS_ADD_TEST(negatable_value_option_enabled_by_default_is_two_errors),
    NAOS_ADD_TEST(negatable_flag_with_default_value_is_error),
    NAOS_ADD_TEST(negated_name_after_negatable_flag_in_same_command_is_error),
    NAOS_ADD_TEST(negated_name_before_negatable_flag_in_same_command_is_error),
    NAOS_ADD_TEST(negated_name_in_descendant_of_inherited_negatable_flag_is_error),
    NAOS_ADD_TEST(negated_name_in_descendant_of_non_inherited_negatable_flag_is_error),
    NAOS_ADD_TEST(negatable_flag_in_descendant_of_negated_name_is_error),
    NAOS_ADD_TEST(negatable_flag_in_descendant_of_non_inherited_negated_name_is_error),
    NAOS_ADD_TEST(non_negatable_flag_and_negated_name_are_valid),
    NAOS_ADD_TEST(negatable_flag_named_with_no_prefix_alone_is_valid),
    NAOS_ADD_TEST(ext_declaration_passes_decl_check),
    NAOS_ADD_TEST(hidden_flag_value_and_list_are_valid),
    NAOS_ADD_TEST(required_and_hidden_value_is_error),
    NAOS_ADD_TEST(required_and_hidden_list_is_error),
    NAOS_ADD_TEST(required_and_hidden_flag_is_two_errors),
    NAOS_ADD_TEST(hidden_flag_with_default_is_one_error),
    NAOS_ADD_TEST(well_formed_aliases_are_valid),
    NAOS_ADD_TEST(alias_empty_string_is_error),
    NAOS_ADD_TEST(alias_double_comma_is_error),
    NAOS_ADD_TEST(alias_leading_comma_is_error),
    NAOS_ADD_TEST(alias_trailing_comma_is_error),
    NAOS_ADD_TEST(alias_starting_with_dash_is_error),
    NAOS_ADD_TEST(alias_containing_equals_is_error),
    NAOS_ADD_TEST(invalid_alias_among_valid_ones_shows_only_that_alias),
    NAOS_ADD_TEST(alias_help_is_reserved),
    NAOS_ADD_TEST(alias_version_after_valid_alias_is_reserved),
    NAOS_ADD_TEST(alias_errors_are_reported_per_alias),
    NAOS_ADD_TEST(two_invalid_aliases_are_two_errors),
    NAOS_ADD_TEST(aliases_are_checked_when_long_name_is_invalid),
    NAOS_ADD_TEST(aliases_are_checked_when_long_name_is_null_or_reserved),
    NAOS_ADD_TEST(alias_error_in_nested_command_shows_path),
    NAOS_ADD_TEST(alias_with_percent_is_not_a_format_string)
)
