/* parser_fixture: 入口を使わずに解析の作業状態を組み立て、片付ける */

#include "parser_fixture.h"

#include "foundation/eol.h"
#include "yaml/yaml_lines.h"

#include <stddef.h>

bool mnd_yaml_fixture_init(mnd_yaml_fixture_t *fx, const char *data, size_t len, mnd_span_t content,
                           const mnd_yaml_allocator_t *allocator)
{
    mnd_eol_info_t info;
    mnd_eol_t eol = MND_EOL_LF;

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

    /*
     * 行末記号は content の範囲で決める（改行がなければ LF）
     */
    if (content.end > content.start)
    {
        info = mnd_eol_detect(data + content.start, content.end - content.start);
        if (info.dominant == MND_EOL_CRLF)
        {
            eol = MND_EOL_CRLF;
        }
    }

    fx->parser.doc = mnd_yaml_doc_new(data, len, content, eol, fx->parser.allocator);
    if (fx->parser.doc == NULL)
    {
        return false;
    }
    if (!mnd_yaml_lines_build(data, len, content, fx->parser.allocator, &fx->parser.lines))
    {
        mnd_yaml_doc_free(fx->parser.doc);
        fx->parser.doc = NULL;
        return false;
    }

    return true;
}

void mnd_yaml_fixture_cleanup(mnd_yaml_fixture_t *fx)
{
    mnd_yaml_lines_free(fx->parser.allocator, &fx->parser.lines);
    if (fx->parser.doc != NULL)
    {
        mnd_yaml_doc_free(fx->parser.doc);
        fx->parser.doc = NULL;
    }
    mnd_diag_list_free(&fx->diags);
}

mnd_yaml_cursor_t mnd_yaml_fixture_cursor(const mnd_yaml_fixture_t *fx, size_t offset)
{
    mnd_yaml_cursor_t cursor;

    cursor.line_index = mnd_yaml_lines_index_of(&fx->parser.lines, offset);
    cursor.offset = offset;

    return cursor;
}
