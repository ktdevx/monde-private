#ifndef CLOPTS_ERRORS_H
#define CLOPTS_ERRORS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 解析・宣言の検証・引数列の取得で起きたエラーのメッセージの列。
 * メッセージは英語、UTF-8、NUL 終端で、追加した順に保持する。
 * 利用側は読み取りと解放だけを行う（追加はライブラリの内部だけが行う）
 */
typedef struct clopts_errors
{
    char **messages;  /* count 個のメッセージ */
    size_t count;
    size_t cap;
    bool dropped;     /* メモリ不足で記録できなかったエラーがある */
} clopts_errors_t;

#define CLOPTS_ERRORS_INIT { NULL, 0, 0, false }

void clopts_errors_init(clopts_errors_t *errors);

/* メッセージと配列を解放し、初期状態に戻す。初期状態・解放済みに対しても安全 */
void clopts_errors_free(clopts_errors_t *errors);

size_t clopts_errors_count(const clopts_errors_t *errors);

/* index 番目のメッセージ。index >= count なら NULL */
const char *clopts_errors_at(const clopts_errors_t *errors, size_t index);

/* メモリ不足で記録できなかったエラーがあれば true */
bool clopts_errors_dropped(const clopts_errors_t *errors);

#ifdef __cplusplus
}
#endif

#endif
