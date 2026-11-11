#include "test_decl.h"

#include <stdbool.h>
#include <stddef.h>

static const mnd_args_option_t test_tool_build_options[] = {
    { .long_name = "output", .short_name = 'o', .kind = MND_ARGS_VALUE, .value_name = "FILE",
      .help = "Write the result to FILE", .required = true },
    { .long_name = "keys", .short_name = 'k', .kind = MND_ARGS_LIST, .value_name = "KEY",
      .help = "Keys to include", .default_value = "a,b" },
    { .long_name = "level", .kind = MND_ARGS_VALUE, .value_name = "N", .help = "Processing level",
      .default_value = "1" },
    { .long_name = "verbose", .short_name = 'v', .kind = MND_ARGS_FLAG, .help = "Print more details" },
    { .long_name = "quiet", .short_name = 'q', .kind = MND_ARGS_FLAG, .help = "Print only errors" }
};

static const mnd_args_positional_t test_tool_build_positionals[] = {
    { "SOURCE", "Source file to build" }
};

static const mnd_args_positional_t test_tool_copy_positionals[] = {
    { "SRC", "File to copy" },
    { "DEST", "Destination path" }
};

static const mnd_args_positional_t test_tool_cat_positionals[] = {
    { "FILE", "Files to print" }
};

#define TEST_TOOL_COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const mnd_args_command_t test_tool_commands[] = {
    {
        .name = "build",
        .summary = "Build an output from a source",
        .description = "Build an output file from SOURCE.",
        .options = test_tool_build_options,
        .option_count = TEST_TOOL_COUNT_OF(test_tool_build_options),
        .positionals = test_tool_build_positionals,
        .positional_count = TEST_TOOL_COUNT_OF(test_tool_build_positionals),
        .min_positionals = 1,
        .max_positionals = 1
    },
    {
        .name = "copy",
        .summary = "Copy a file",
        .positionals = test_tool_copy_positionals,
        .positional_count = TEST_TOOL_COUNT_OF(test_tool_copy_positionals),
        .min_positionals = 2,
        .max_positionals = 2
    },
    {
        .name = "cat",
        .summary = "Print files",
        .positionals = test_tool_cat_positionals,
        .positional_count = TEST_TOOL_COUNT_OF(test_tool_cat_positionals),
        .min_positionals = 0,
        .max_positionals = MND_ARGS_UNLIMITED
    }
};

static const mnd_args_command_t test_tool_root = {
    .description = "A sample tool for testing.",
    .commands = test_tool_commands,
    .command_count = TEST_TOOL_COUNT_OF(test_tool_commands)
};

const mnd_args_program_t test_tool_program = {
    .name = "tool",
    .version = "1.2.3",
    .root = &test_tool_root
};

const mnd_args_command_t *const test_tool_build = &test_tool_commands[0];
const mnd_args_command_t *const test_tool_copy = &test_tool_commands[1];
const mnd_args_command_t *const test_tool_cat = &test_tool_commands[2];

static const mnd_args_option_t test_single_options[] = {
    { .long_name = "output", .short_name = 'o', .kind = MND_ARGS_VALUE, .value_name = "FILE",
      .help = "Write the result to FILE" },
    { .long_name = "verbose", .short_name = 'v', .kind = MND_ARGS_FLAG, .help = "Print more details" },
    { .long_name = "keys", .short_name = 'k', .kind = MND_ARGS_LIST, .value_name = "KEY",
      .help = "Keys to include" }
};

static const mnd_args_positional_t test_single_positionals[] = {
    { "FILE", "Files to process" }
};

static const mnd_args_command_t test_single_root = {
    .description = "A sample tool without subcommands.",
    .options = test_single_options,
    .option_count = TEST_TOOL_COUNT_OF(test_single_options),
    .positionals = test_single_positionals,
    .positional_count = TEST_TOOL_COUNT_OF(test_single_positionals),
    .min_positionals = 0,
    .max_positionals = MND_ARGS_UNLIMITED
};

const mnd_args_program_t test_single_program = {
    .name = "single",
    .version = "2.0",
    .root = &test_single_root
};
