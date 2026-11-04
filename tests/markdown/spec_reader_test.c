/* spec_reader: spec.txt からの例の取り出し */

#include <naos/naos.h>

#include "spec_reader.h"

#include <string.h>

NAOS_TEST(spec_reader_spec_file_path_is_defined)
{
    const char *path = MND_MD_SPEC_FILE;
    const char *suffix = "tests/data/commonmark/spec.txt";
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);

    NAOS_REQUIRE_TRUE(path_len >= suffix_len);
    NAOS_CHECK_STR_EQ(path + (path_len - suffix_len), suffix);
}

NAOS_SUITE(spec_reader,
    NAOS_ADD_TEST(spec_reader_spec_file_path_is_defined)
)
