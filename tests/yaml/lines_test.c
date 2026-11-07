/* スイート lines: 物理的な行の表（2.5、6.3、7.1、7.2） */

#include <naos/naos.h>

#include "yaml/yaml_lines.h"

#include <stddef.h>

/* 雛形: 行の型が CRLF の行末記号の長さ 2 と内容の範囲を表せる */
NAOS_TEST(lines_scaffold_line_record_shape)
{
    mnd_yaml_line_t line;

    line.line = 2;
    line.start = 4;
    line.indent = 2;
    line.content.start = 4;
    line.content.end = 10;
    line.eol_len = 2;
    line.blank = false;
    line.comment = false;
    line.tab_in_indent = false;
    NAOS_CHECK_UINT_EQ(2, line.line);
    NAOS_CHECK_UINT_EQ(6, line.content.end - line.content.start);
    NAOS_CHECK_UINT_EQ(12, line.content.end + line.eol_len);
    NAOS_CHECK_FALSE(line.blank || line.comment || line.tab_in_indent);
}

NAOS_SUITE(lines,
    NAOS_ADD_TEST(lines_scaffold_line_record_shape)
)
