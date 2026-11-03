#ifndef MONDE_FOUNDATION_DIAG_H
#define MONDE_FOUNDATION_DIAG_H

#include <stdbool.h>
#include <stddef.h>

/* printf 形式の書式をコンパイル時に検査させる属性（MSVC では空） */
#if defined(__MINGW32__)
#define MND_PRINTF_LIKE(fmt, args) __attribute__((format(gnu_printf, fmt, args)))
#elif defined(__GNUC__) || defined(__clang__)
#define MND_PRINTF_LIKE(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define MND_PRINTF_LIKE(fmt, args)
#endif

typedef enum mnd_severity
{
    MND_SEVERITY_ERROR,
    MND_SEVERITY_WARNING
} mnd_severity_t;

typedef struct mnd_diag
{
    mnd_severity_t severity;
    char *message;  /* 英語のメッセージ（UTF-8、NUL 終端） */
    char *path;     /* ファイルパス（UTF-8）。不明なら NULL */
    size_t line;    /* 1 起点の行番号。不明なら 0 */
} mnd_diag_t;

typedef struct mnd_diag_list
{
    mnd_diag_t *items;
    size_t count;
    size_t cap;
    bool dropped;   /* メモリ不足で記録できなかった診断がある */
} mnd_diag_list_t;

#define MND_DIAG_LIST_INIT { NULL, 0, 0, false }

void mnd_diag_list_init(mnd_diag_list_t *list);
void mnd_diag_list_free(mnd_diag_list_t *list);  /* 初期状態・解放済みに対しても安全 */

/* printf 形式でメッセージを組み立てて末尾に追加する。path は NULL 可、line は 0 可 */
void mnd_diag_add(mnd_diag_list_t *list, mnd_severity_t severity,
                  const char *path, size_t line, const char *format, ...)
    MND_PRINTF_LIKE(5, 6);

size_t mnd_diag_list_count(const mnd_diag_list_t *list);
const mnd_diag_t *mnd_diag_list_at(const mnd_diag_list_t *list, size_t index); /* index < count */
/* エラーの診断が 1 つ以上ある、または dropped なら true */
bool mnd_diag_list_has_error(const mnd_diag_list_t *list);

#endif
