#include <naos/naos.h>

#include <string.h>

#include "foundation/file_io.h"

NAOS_TEST(work_dir_is_defined)
{
    NAOS_CHECK_UINT_GT(strlen(MONDE_TEST_WORK_DIR), 0);
}

NAOS_SUITE(file_io,
    NAOS_ADD_TEST(work_dir_is_defined)
)
