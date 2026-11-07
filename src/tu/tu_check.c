/* StructureCheck: 構造を変え得る訳文の検査 */

#include "tu/tu_internal.h"

/* 現在は検査項目を判定せず、常に MND_TU_CHECK_OK を返す（*line_index は変えない） */
mnd_tu_check_result_t mnd_tu_check_structure(const char *text, size_t len, bool is_atx_heading,
                                             size_t *line_index)
{
    (void)text;
    (void)len;
    (void)is_atx_heading;
    (void)line_index;
    return MND_TU_CHECK_OK;
}
