/*
 * Scalar: プレーン・引用符付き・ブロックスカラーの走査と論理値、非対応の指示文字の検出。
 * 解析の作業状態の補助関数（エラーの報告、行番号、範囲の設定）もここに置く（Scalar が最下位の解析の部品）
 */

#include "yaml/yaml_scalar.h"

#include <stddef.h>

/* 解析の作業状態の補助関数 */

bool mnd_yaml_parser_claim_error(mnd_yaml_parser_t *p)
{
    if (p->failed)
    {
        return false;
    }
    p->failed = true;

    return true;
}

void mnd_yaml_parser_error(mnd_yaml_parser_t *p, size_t line, const char *message)
{
    if (mnd_yaml_parser_claim_error(p))
    {
        mnd_diag_add(p->diags, MND_SEVERITY_ERROR, p->path, line, "%s", message);
    }
}

void mnd_yaml_parser_oom(mnd_yaml_parser_t *p)
{
    mnd_yaml_parser_error(p, 0, "out of memory");
}

size_t mnd_yaml_parser_line_of(const mnd_yaml_parser_t *p, size_t offset)
{
    return mnd_yaml_lines_line_of(&p->lines, offset);
}

void mnd_yaml_parser_set_span(const mnd_yaml_parser_t *p, mnd_yaml_node_t *node, mnd_span_t span)
{
    node->span = span;
    node->start_line = mnd_yaml_parser_line_of(p, span.start);
    node->end_line = mnd_yaml_parser_line_of(p, span.end);
}

/*
 * スカラーの走査の本体はタスク 2.1（プレーンと引用符付き）と 2.2（ブロックスカラー）が実装する。
 * それまでは診断を追加せずに false を返す
 */

bool mnd_yaml_scalar_parse(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent, bool flow,
                           mnd_yaml_node_t **out)
{
    (void)p;
    (void)at;
    (void)parent_indent;
    (void)flow;
    (void)out;

    return false;
}

bool mnd_yaml_scalar_is_block_header(const mnd_yaml_parser_t *p, mnd_yaml_cursor_t at)
{
    (void)p;
    (void)at;

    return false;
}
