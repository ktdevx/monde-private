/* parser_fixture: 入口を使わずに解析の作業状態を組み立て、片付ける */

#include "parser_fixture.h"

#include <stddef.h>

/*
 * 組み立ての本体はタスク 2.1 が実装する。それまでは何も確保せず、diags だけを初期化して false を返す
 */
bool mnd_yaml_fixture_init(mnd_yaml_fixture_t *fx, const char *data, size_t len, mnd_span_t content,
                           const mnd_yaml_allocator_t *allocator)
{
    mnd_diag_list_init(&fx->diags);
    fx->parser.data = data;
    fx->parser.len = len;
    fx->parser.content = content;
    fx->parser.path = NULL;
    fx->parser.allocator = mnd_yaml_allocator_or_default(allocator);
    fx->parser.doc = NULL;
    fx->parser.lines.items = NULL;
    fx->parser.lines.count = 0;
    fx->parser.lines.cap = 0;
    fx->parser.lines.range = content;
    fx->parser.lines.first_line = 1;
    fx->parser.diags = &fx->diags;
    fx->parser.failed = false;

    return false;
}

void mnd_yaml_fixture_cleanup(mnd_yaml_fixture_t *fx)
{
    mnd_diag_list_free(&fx->diags);
}

mnd_yaml_cursor_t mnd_yaml_fixture_cursor(const mnd_yaml_fixture_t *fx, size_t offset)
{
    mnd_yaml_cursor_t cursor;

    cursor.line_index = mnd_yaml_lines_index_of(&fx->parser.lines, offset);
    cursor.offset = offset;

    return cursor;
}
