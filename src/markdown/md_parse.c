/* Parse: mnd_md_parse（オプションと UTF-8 の検査、2 段階と後処理の実行、失敗時の後始末） */

#include "markdown/md.h"
#include "markdown/md_block.h"
#include "markdown/md_inline.h"
#include "markdown/md_internal.h"
#include "markdown/md_lines.h"
#include "markdown/md_parse.h"

#include "foundation/diag.h"
#include "foundation/source.h"
#include "foundation/utf8.h"

#include <stdbool.h>
#include <stddef.h>

size_t mnd_md_parser_line_of(const mnd_md_parser_t *parser, size_t offset)
{
    size_t line = 0;

    /* offset は内容の中の位置なので、検索は失敗しない */
    if (!mnd_md_line_index_line_of(parser->line_index, offset, &line))
    {
        return 0;
    }

    return line;
}

void mnd_md_parser_set_node_lines(const mnd_md_parser_t *parser, mnd_md_node_t *node)
{
    node->start_line = mnd_md_parser_line_of(parser, node->span.start);
    if (node->span.end > node->span.start)
    {
        node->end_line = mnd_md_parser_line_of(parser, node->span.end - 1);
    }
    else
    {
        node->end_line = node->start_line;
    }
}

/* 作業用の配列を解放する（文書は解放しない） */
static void parser_free_work(mnd_md_parser_t *parser)
{
    mnd_md_array_free(&parser->open);
    mnd_md_array_free(&parser->leaf_lines);
}

/* 失敗時の後始末: 作業用の配列と文書をすべて解放する */
static void parser_discard(mnd_md_parser_t *parser)
{
    parser_free_work(parser);
    mnd_md_doc_free(parser->doc);
    parser->doc = NULL;
}

/* 行番号の表を文書のアリーナに作る。メモリ不足なら false */
static bool parser_build_line_index(mnd_md_parser_t *parser)
{
    mnd_md_line_index_t *index = NULL;

    index = (mnd_md_line_index_t *)mnd_md_arena_alloc(&parser->doc->arena, sizeof(*index));
    if (index == NULL)
    {
        return false;
    }

    if (!mnd_md_line_index_build(index, parser->data, parser->len, &parser->doc->arena))
    {
        return false;
    }

    parser->line_index = index;

    return true;
}

bool mnd_md_parse(const char *data, size_t len, const mnd_md_options_t *options,
                  mnd_md_doc_t **out, mnd_diag_list_t *diags)
{
    static const mnd_md_options_t default_options = MND_MD_OPTIONS_INIT;
    mnd_md_parser_t parser;
    size_t invalid_offset = 0;
    size_t line = 0;

    *out = NULL;
    if (options == NULL)
    {
        options = &default_options;
    }

    /* オプションの検査（1.4）。方言の規則の検査はここに加える */
    if (options->body_start > len)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, options->path, 0,
                     "body start offset %zu exceeds content length %zu", options->body_start, len);
        return false;
    }

    /* UTF-8 の検査（8.1）: 開始位置より前も含めて、内容全体を検査する */
    if (!mnd_utf8_validate(data, len, &invalid_offset))
    {
        if (!mnd_source_line_of(data, len, invalid_offset, &line))
        {
            line = 0;
        }

        mnd_diag_add(diags, MND_SEVERITY_ERROR, options->path, line, "invalid UTF-8 sequence");
        return false;
    }

    parser.data = data;
    parser.len = len;
    parser.body_start = options->body_start;
    parser.path = options->path;
    parser.allocator = mnd_md_allocator_or_default(options->allocator);
    parser.dialect = options->dialect;
    parser.doc = NULL;
    parser.line_index = NULL;
    mnd_md_array_init(&parser.open, sizeof(mnd_md_open_block_t), parser.allocator);
    mnd_md_array_init(&parser.leaf_lines, sizeof(mnd_md_line_t), parser.allocator);

    parser.doc = mnd_md_doc_create(data, len, parser.allocator);
    if ((parser.doc == NULL) || !parser_build_line_index(&parser) || !mnd_md_block_run(&parser)
        || !mnd_md_inline_run(&parser))
    {
        /* ここまでの失敗はメモリ不足だけである（8.2） */
        parser_discard(&parser);
        mnd_diag_add(diags, MND_SEVERITY_ERROR, options->path, 0, "out of memory");
        return false;
    }

    parser_free_work(&parser);
    *out = parser.doc;

    return true;
}
