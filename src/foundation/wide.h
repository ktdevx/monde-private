#ifndef MONDE_FOUNDATION_WIDE_H
#define MONDE_FOUNDATION_WIDE_H

/*
 * Windows の API との境界で、UTF-8 と UTF-16 を相互に変換する。
 * Windows でだけ宣言する。他の OS では何も宣言しない
 */
#ifdef _WIN32
#include <wchar.h>

/*
 * NUL 終端の UTF-8 を、確保した NUL 終端の UTF-16 に変換する。呼び出し側が free で解放する。
 * 失敗時は NULL を返し、*reason に "invalid UTF-8" または "out of memory" を入れる
 */
wchar_t *mnd_wide_from_utf8(const char *utf8, const char **reason);

/*
 * NUL 終端の UTF-16 を、確保した NUL 終端の UTF-8 に変換する。呼び出し側が free で解放する。
 * 失敗時は NULL を返し、*reason に "invalid UTF-16" または "out of memory" を入れる
 */
char *mnd_wide_to_utf8(const wchar_t *wide, const char **reason);
#endif

#endif
