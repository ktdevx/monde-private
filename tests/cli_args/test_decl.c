#include "test_decl.h"

#include <stdbool.h>
#include <stddef.h>

static const mnd_args_option_t test_tool_build_options[] = {
    { "output", 'o', MND_ARGS_VALUE, "FILE", "Write the result to FILE", true, NULL },
    { "keys", 'k', MND_ARGS_LIST, "KEY", "Keys to include", false, "a,b" },
    { "level", '\0', MND_ARGS_VALUE, "N", "Processing level", false, "1" },
    { "verbose", 'v', MND_ARGS_FLAG, NULL, "Print more details", false, NULL },
    { "quiet", 'q', MND_ARGS_FLAG, NULL, "Print only errors", false, NULL }
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
        "build",
        "Build an output from a source",
        "Build an output file from SOURCE.",
        test_tool_build_options,
        TEST_TOOL_COUNT_OF(test_tool_build_options),
        test_tool_build_positionals,
        TEST_TOOL_COUNT_OF(test_tool_build_positionals),
        1,
        1
    },
    {
        "copy",
        "Copy a file",
        NULL,
        NULL,
        0,
        test_tool_copy_positionals,
        TEST_TOOL_COUNT_OF(test_tool_copy_positionals),
        2,
        2
    },
    {
        "cat",
        "Print files",
        NULL,
        NULL,
        0,
        test_tool_cat_positionals,
        TEST_TOOL_COUNT_OF(test_tool_cat_positionals),
        0,
        MND_ARGS_UNLIMITED
    }
};

const mnd_args_program_t test_tool_program = {
    "tool",
    "A sample tool for testing.",
    "1.2.3",
    test_tool_commands,
    TEST_TOOL_COUNT_OF(test_tool_commands)
};

const mnd_args_command_t *const test_tool_build = &test_tool_commands[0];
const mnd_args_command_t *const test_tool_copy = &test_tool_commands[1];
const mnd_args_command_t *const test_tool_cat = &test_tool_commands[2];
