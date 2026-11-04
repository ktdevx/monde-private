/* msgmerge 0.21 の出力である正解ファイルとの比較 */

#include <naos/naos.h>

#include <string.h>

NAOS_TEST(golden_scaffold_data_dir_is_defined)
{
    const char *dir = MND_MERGE_TEST_DATA_DIR;
    const char *suffix = "/data";
    size_t len = strlen(dir);

    /* 正解ファイルのディレクトリはテストのソースの data/ */
    NAOS_CHECK_UINT_GE(len, strlen(suffix));
    NAOS_CHECK_STR_EQ(dir + (len - strlen(suffix)), suffix);
}

NAOS_SUITE(golden,
    NAOS_ADD_TEST(golden_scaffold_data_dir_is_defined)
)
