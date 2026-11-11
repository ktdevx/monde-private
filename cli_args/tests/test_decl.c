#include "test_decl.h"

#include <stdbool.h>
#include <stddef.h>

static const clopts_option_t test_tool_build_options[] = {
    { .long_name = "output", .short_name = 'o', .kind = CLOPTS_VALUE, .value_name = "FILE",
      .help = "Write the result to FILE", .required = true },
    { .long_name = "keys", .short_name = 'k', .kind = CLOPTS_LIST, .value_name = "KEY",
      .help = "Keys to include", .default_value = "a,b" },
    { .long_name = "level", .kind = CLOPTS_VALUE, .value_name = "N", .help = "Processing level",
      .default_value = "1" },
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_FLAG, .help = "Print more details" },
    { .long_name = "quiet", .short_name = 'q', .kind = CLOPTS_FLAG, .help = "Print only errors" }
};

static const clopts_positional_t test_tool_build_positionals[] = {
    { "SOURCE", "Source file to build" }
};

static const clopts_positional_t test_tool_copy_positionals[] = {
    { "SRC", "File to copy" },
    { "DEST", "Destination path" }
};

static const clopts_positional_t test_tool_cat_positionals[] = {
    { "FILE", "Files to print" }
};

#define TEST_TOOL_COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const clopts_command_t test_tool_commands[] = {
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
        .max_positionals = CLOPTS_UNLIMITED
    }
};

static const clopts_command_t test_tool_root = {
    .description = "A sample tool for testing.",
    .commands = test_tool_commands,
    .command_count = TEST_TOOL_COUNT_OF(test_tool_commands)
};

const clopts_program_t test_tool_program = {
    .name = "tool",
    .version = "1.2.3",
    .root = &test_tool_root
};

const clopts_command_t *const test_tool_build = &test_tool_commands[0];
const clopts_command_t *const test_tool_copy = &test_tool_commands[1];
const clopts_command_t *const test_tool_cat = &test_tool_commands[2];

static const clopts_option_t test_single_options[] = {
    { .long_name = "output", .short_name = 'o', .kind = CLOPTS_VALUE, .value_name = "FILE",
      .help = "Write the result to FILE" },
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_FLAG, .help = "Print more details" },
    { .long_name = "keys", .short_name = 'k', .kind = CLOPTS_LIST, .value_name = "KEY",
      .help = "Keys to include" }
};

static const clopts_positional_t test_single_positionals[] = {
    { "FILE", "Files to process" }
};

static const clopts_command_t test_single_root = {
    .description = "A sample tool without subcommands.",
    .options = test_single_options,
    .option_count = TEST_TOOL_COUNT_OF(test_single_options),
    .positionals = test_single_positionals,
    .positional_count = TEST_TOOL_COUNT_OF(test_single_positionals),
    .min_positionals = 0,
    .max_positionals = CLOPTS_UNLIMITED
};

const clopts_program_t test_single_program = {
    .name = "single",
    .version = "2.0",
    .root = &test_single_root
};

static const clopts_option_t test_nest_root_options[] = {
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_FLAG, .help = "Print more details",
      .inherited = true },
    { .long_name = "config", .short_name = 'c', .kind = CLOPTS_VALUE, .value_name = "FILE",
      .help = "Read settings from FILE", .default_value = "nest.cfg", .inherited = true },
    { .long_name = "dry-run", .kind = CLOPTS_FLAG, .help = "Show what would be done" }
};

static const clopts_option_t test_nest_remote_options[] = {
    { .long_name = "token", .short_name = 't', .kind = CLOPTS_VALUE, .value_name = "TOKEN",
      .help = "Access token", .required = true, .inherited = true },
    { .long_name = "tags", .kind = CLOPTS_LIST, .value_name = "TAG", .help = "Tags to attach",
      .inherited = true }
};

static const clopts_option_t test_nest_remote_add_options[] = {
    { .long_name = "force", .short_name = 'f', .kind = CLOPTS_FLAG,
      .help = "Overwrite an existing remote" }
};

static const clopts_positional_t test_nest_remote_add_positionals[] = {
    { "NAME", "Name of the remote" },
    { "URL", "URL of the remote" }
};

static const clopts_positional_t test_nest_remote_remove_positionals[] = {
    { "NAME", "Name of the remote" }
};

static const clopts_command_t test_nest_remote_commands[] = {
    {
        .name = "add",
        .summary = "Add a remote",
        .options = test_nest_remote_add_options,
        .option_count = TEST_TOOL_COUNT_OF(test_nest_remote_add_options),
        .positionals = test_nest_remote_add_positionals,
        .positional_count = TEST_TOOL_COUNT_OF(test_nest_remote_add_positionals),
        .min_positionals = 2,
        .max_positionals = 2
    },
    {
        .name = "remove",
        .summary = "Remove a remote",
        .positionals = test_nest_remote_remove_positionals,
        .positional_count = TEST_TOOL_COUNT_OF(test_nest_remote_remove_positionals),
        .min_positionals = 1,
        .max_positionals = 1
    }
};

static const clopts_command_t test_nest_commands[] = {
    {
        .name = "remote",
        .summary = "Manage remotes",
        .options = test_nest_remote_options,
        .option_count = TEST_TOOL_COUNT_OF(test_nest_remote_options),
        .commands = test_nest_remote_commands,
        .command_count = TEST_TOOL_COUNT_OF(test_nest_remote_commands)
    },
    {
        .name = "status",
        .summary = "Show the status"
    }
};

static const clopts_command_t test_nest_root = {
    .description = "A sample tool with nested commands.",
    .options = test_nest_root_options,
    .option_count = TEST_TOOL_COUNT_OF(test_nest_root_options),
    .commands = test_nest_commands,
    .command_count = TEST_TOOL_COUNT_OF(test_nest_commands)
};

const clopts_program_t test_nest_program = {
    .name = "nest",
    .version = "3.0",
    .root = &test_nest_root
};

const clopts_command_t *const test_nest_remote = &test_nest_commands[0];
const clopts_command_t *const test_nest_remote_add = &test_nest_remote_commands[0];
const clopts_command_t *const test_nest_remote_remove = &test_nest_remote_commands[1];
const clopts_command_t *const test_nest_status = &test_nest_commands[1];
