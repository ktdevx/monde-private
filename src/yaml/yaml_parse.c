/* Parse: 入口（UTF-8 の検証、行の表の作成、改行コードの検出）とブロックのコレクションの解析 */

#include "yaml/yaml_parse.h"
#include "yaml/yaml_flow.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_scalar.h"

#include "foundation/eol.h"
#include "foundation/utf8.h"

#include <stddef.h>

/* 行の走査の補助 */

static bool parse_is_white(char c)
{
    return (c == ' ') || (c == '\t');
}

/* data[start..end) に CR があれば true（行の内容の CR は、行末記号でない単独の CR） */
static bool parse_has_cr(const char *data, size_t start, size_t end)
{
    size_t i;

    for (i = start; i < end; i++)
    {
        if (data[i] == '\r')
        {
            return true;
        }
    }

    return false;
}

/* 行の表の index の行の先頭のカーソル（index == count なら範囲の終わり） */
static mnd_yaml_cursor_t parse_cursor_at_line(const mnd_yaml_parser_t *p, size_t index)
{
    mnd_yaml_cursor_t cursor;

    cursor.line_index = index;
    if (index < p->lines.count)
    {
        cursor.offset = p->lines.items[index].start;
    }
    else
    {
        cursor.offset = p->content.end;
    }

    return cursor;
}

/*
 * *index の行から、構造の解析で読み飛ばす行（空白だけの行とコメント行）を読み飛ばし、*index を
 * 最初のそれ以外の行（なければ lines.count）にする。コメント行の単独の CR はエラー（6.2）
 */
static bool parse_skip_ignorable_lines(mnd_yaml_parser_t *p, size_t *index)
{
    const mnd_yaml_line_t *line = NULL;

    while (*index < p->lines.count)
    {
        line = &p->lines.items[*index];
        if (line->comment)
        {
            if (parse_has_cr(p->data, line->content.start, line->content.end))
            {
                mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
                return false;
            }
        }
        else if (!line->blank)
        {
            break;
        }
        ++*index;
    }

    return true;
}

/*
 * ノードの後ろの、同じ行の残り（at から行の内容の終わりまで）を検査し、*at を次の行の先頭に進める。
 * 残りは空白と、空白の後ろのコメントだけを認める。それ以外は "unexpected content after value"（6.2）、
 * 単独の CR は "carriage return without line feed"
 */
static bool parse_finish_line(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at)
{
    const mnd_yaml_line_t *line = NULL;
    size_t i;
    size_t end;

    if (at->line_index >= p->lines.count)
    {
        return true;
    }
    line = &p->lines.items[at->line_index];
    end = line->content.end;
    i = at->offset;
    while ((i < end) && parse_is_white(p->data[i]))
    {
        ++i;
    }
    if (i < end)
    {
        if ((p->data[i] == '#') && (i > at->offset))
        {
            if (parse_has_cr(p->data, i, end))
            {
                mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
                return false;
            }
        }
        else if (p->data[i] == '\r')
        {
            mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
            return false;
        }
        else
        {
            mnd_yaml_parser_error(p, line->line, "unexpected content after value");
            return false;
        }
    }
    *at = parse_cursor_at_line(p, at->line_index + 1);

    return true;
}

/* ノードの解析 */

/*
 * at（空白でない最初の文字）から、ブロック文脈の値のノードを 1 つ読む。
 * フロー形式のコレクション（[ と {）は Flow に、スカラー（ブロックスカラーを含む）は Scalar に委ねる。
 * parent_indent は親のインデント（ブロックスカラーと継続行の判定に使う）。
 * ブロックスカラーでは *at が消費した最後の行の次の行の先頭に進む。それ以外では *at はノードの直後
 * （同じ行の残りは呼び出し側が parse_finish_line で検査する）
 */
static bool parse_value_node(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent,
                             mnd_yaml_node_t **out)
{
    char c = p->data[at->offset];

    if ((c == '[') || (c == '{'))
    {
        return mnd_yaml_flow_parse(p, at, out);
    }

    return mnd_yaml_scalar_parse(p, at, parent_indent, false, out);
}

/* node がブロックスカラー（| か >）か */
static bool parse_is_block_scalar(const mnd_yaml_node_t *node)
{
    return (node->kind == MND_YAML_SCALAR)
        && ((node->as.scalar.style == MND_YAML_LITERAL) || (node->as.scalar.style == MND_YAML_FOLDED));
}

/*
 * 範囲の全体を 1 つのドキュメントとして解析し、根を文書に設定する。
 * 根は最初の空でない行のインデントで始まるノード 1 つ。空でない行がなければ空のドキュメント（2.7）。
 * 根の後ろには、空白だけの行とコメント行だけを認める
 */
static bool parse_document(mnd_yaml_parser_t *p)
{
    const mnd_yaml_line_t *line = NULL;
    mnd_yaml_node_t *root = NULL;
    mnd_yaml_cursor_t at;
    size_t index = 0;

    if (!parse_skip_ignorable_lines(p, &index))
    {
        return false;
    }
    if (index >= p->lines.count)
    {
        mnd_yaml_doc_set_root(p->doc, NULL);
        return true;
    }

    line = &p->lines.items[index];
    if (line->tab_in_indent)
    {
        mnd_yaml_parser_error(p, line->line, "tab character in indentation");
        return false;
    }
    at.line_index = index;
    at.offset = line->start + line->indent;

    /*
     * 根の親のインデントは 0 とする（ルートのブロックスカラーと継続行は、インデント 1 以上の行に限られる）
     */
    if (!parse_value_node(p, &at, 0, &root))
    {
        return false;
    }
    if (!parse_is_block_scalar(root) && !parse_finish_line(p, &at))
    {
        return false;
    }

    index = at.line_index;
    if (!parse_skip_ignorable_lines(p, &index))
    {
        return false;
    }
    if (index < p->lines.count)
    {
        line = &p->lines.items[index];
        if (line->tab_in_indent)
        {
            mnd_yaml_parser_error(p, line->line, "tab character in indentation");
        }
        else if (parse_has_cr(p->data, line->content.start, line->content.end))
        {
            mnd_yaml_parser_error(p, line->line, "carriage return without line feed");
        }
        else
        {
            mnd_yaml_parser_error(p, line->line, "unexpected content after value");
        }
        return false;
    }

    root->parent = NULL;
    mnd_yaml_doc_set_root(p->doc, root);

    return true;
}

/* 入口 */

/* content の範囲の行末記号（MND_EOL_LF か MND_EOL_CRLF。改行がなければ LF） */
static mnd_eol_t parse_detect_eol(const char *data, mnd_span_t content)
{
    mnd_eol_info_t info;

    if (content.end > content.start)
    {
        info = mnd_eol_detect(data + content.start, content.end - content.start);
        if (info.dominant == MND_EOL_CRLF)
        {
            return MND_EOL_CRLF;
        }
    }

    return MND_EOL_LF;
}

bool mnd_yaml_parse(const char *data, size_t len, mnd_span_t content, const mnd_yaml_options_t *options,
                    mnd_yaml_doc_t **out, mnd_diag_list_t *diags)
{
    mnd_yaml_parser_t p;
    const char *path = NULL;
    size_t invalid_offset = 0;
    size_t line = 0;
    bool ok = false;

    *out = NULL;
    if (options != NULL)
    {
        path = options->path;
    }

    /* 範囲の検証（呼び出し側のプログラムの誤り） */
    if ((content.start > content.end) || (content.end > len) || ((data == NULL) && (len > 0)))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "content span out of range");
        return false;
    }

    /* 範囲の UTF-8 の検証（6.6）。行番号は文書全体の先頭から数える（7.1） */
    if ((content.end > content.start)
        && !mnd_utf8_validate(data + content.start, content.end - content.start, &invalid_offset))
    {
        if (!mnd_source_line_of(data, len, content.start + invalid_offset, &line))
        {
            line = 0;
        }
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, line, "invalid UTF-8 sequence");
        return false;
    }

    p.data = data;
    p.len = len;
    p.content = content;
    p.path = path;
    p.allocator = mnd_yaml_allocator_or_default((options != NULL) ? options->allocator : NULL);
    p.doc = NULL;
    p.lines.items = NULL;
    p.lines.count = 0;
    p.lines.cap = 0;
    p.lines.range = content;
    p.lines.first_line = 1;
    p.diags = diags;
    p.failed = false;

    p.doc = mnd_yaml_doc_new(data, len, content, parse_detect_eol(data, content), p.allocator);
    if (p.doc == NULL)
    {
        mnd_yaml_parser_oom(&p);
    }
    else if (!mnd_yaml_lines_build(data, len, content, p.allocator, &p.lines))
    {
        mnd_yaml_parser_oom(&p);
    }
    else
    {
        ok = parse_document(&p);
    }

    mnd_yaml_lines_free(p.allocator, &p.lines);
    if (!ok || p.failed)
    {
        mnd_yaml_doc_free(p.doc);
        return false;
    }
    *out = p.doc;

    return true;
}
