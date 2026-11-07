/* Lines: 物理的な行の表（インデント、内容、行末記号、空行・コメント行の判定） */

#include "yaml/yaml_lines.h"

#include <stddef.h>

static void lines_reset(mnd_yaml_lines_t *lines)
{
    lines->items = NULL;
    lines->count = 0;
    lines->cap = 0;
    lines->range.start = 0;
    lines->range.end = 0;
    lines->first_line = 1;
}

/*
 * 以下の関数の本体はタスク 1.3 が実装する。それまでは確保せず、表を空にして失敗（false、0）を返す
 */

bool mnd_yaml_lines_build(const char *data, size_t len, mnd_span_t content,
                          const mnd_yaml_allocator_t *allocator, mnd_yaml_lines_t *out)
{
    (void)data;
    (void)len;
    (void)content;
    (void)allocator;
    lines_reset(out);

    return false;
}

void mnd_yaml_lines_free(const mnd_yaml_allocator_t *allocator, mnd_yaml_lines_t *lines)
{
    (void)allocator;
    lines_reset(lines);
}

size_t mnd_yaml_lines_index_of(const mnd_yaml_lines_t *lines, size_t offset)
{
    (void)lines;
    (void)offset;

    return 0;
}

size_t mnd_yaml_lines_line_of(const mnd_yaml_lines_t *lines, size_t offset)
{
    (void)offset;

    return lines->first_line;
}
