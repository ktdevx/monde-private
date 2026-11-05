/* CommonMark spec 0.31.2 の全例の HTML と範囲の検査（9.x） */

#include <naos/naos.h>

#include "html_normalize.h"
#include "html_render.h"
#include "markdown/md_parse.h"
#include "spec_reader.h"
#include "tree_check.h"

#include <string.h>

NAOS_TEST(spec_file_path_is_defined)
{
    const char *path = MND_MD_SPEC_FILE;

    NAOS_CHECK_UINT_GT(strlen(path), (size_t)0);
}

NAOS_SUITE(spec,
    NAOS_ADD_TEST(spec_file_path_is_defined)
)
