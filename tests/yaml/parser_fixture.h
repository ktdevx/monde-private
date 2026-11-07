#ifndef MONDE_TESTS_YAML_PARSER_FIXTURE_H
#define MONDE_TESTS_YAML_PARSER_FIXTURE_H

/*
 * 解析の作業状態の組み立て（入口の mnd_yaml_parse を使わずに、文書と行の表から作業状態を作り、片付ける）。
 * scalar と flow のスイートが内部の解析の関数（mnd_yaml_scalar_parse、mnd_yaml_flow_parse）を呼ぶために使う。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない
 */

#include "foundation/diag.h"
#include "foundation/source.h"
#include "yaml/yaml_internal.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 作業状態と、その診断の追加先。parser.diags は diags を指すので、初期化したあとは移動（複製）しない
 */
typedef struct mnd_yaml_fixture
{
    mnd_yaml_parser_t parser;
    mnd_diag_list_t diags;
} mnd_yaml_fixture_t;

/*
 * data[0..len) の content を解析する作業状態を作る: 行末記号を content の範囲の mnd_eol_detect で決め
 * （改行がなければ LF）、空の文書（mnd_yaml_doc_new）と行の表（mnd_yaml_lines_build）を作り、
 * parser の各欄（path は NULL、failed は false）と diags（空）を設定する。allocator が NULL なら malloc と free。
 * content は data の範囲内であること。
 * 失敗（メモリ不足）なら、作ったものを片付けて false（diags は初期化済みで、cleanup を呼んでよい）
 */
bool mnd_yaml_fixture_init(mnd_yaml_fixture_t *fx, const char *data, size_t len, mnd_span_t content,
                           const mnd_yaml_allocator_t *allocator);

/*
 * 行の表、文書（parser.doc が NULL でなければ）、diags を解放する。文書を呼び出し側が引き取るときは、
 * 先に parser.doc を NULL にする。init が失敗したあとや、2 回目の呼び出しに対しても安全
 */
void mnd_yaml_fixture_cleanup(mnd_yaml_fixture_t *fx);

/* offset（content.start <= offset <= content.end）の位置のカーソル（その位置を含む行の添字と offset） */
mnd_yaml_cursor_t mnd_yaml_fixture_cursor(const mnd_yaml_fixture_t *fx, size_t offset);

#endif
