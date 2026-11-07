/* スイート time: 固定時刻からの日時文字列、正負のオフセット、日付境界（4.2、5.8） */

/* POSIX の setenv / unsetenv / tzset を C99 の厳密なモードで宣言させる（Windows は _putenv / _tzset を使う） */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <naos/naos.h>

#include "cli/cli_time.h"
#include "foundation/diag.h"
#include "test_util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* time_t のバイト数（定数の条件式は MSVC の C4127 になるため、関数の戻り値で比べる） */
static size_t time_t_bytes(void)
{
    return sizeof(time_t);
}

/* 2026-01-01 00:00:00 UTC の time_t の値（UNIX 時刻） */
#define TIME_2026_01_01 ((time_t)1767225600)

/* struct tm を年・月・日・時・分だけで組み立てる（他の欄は 0） */
static struct tm make_tm(int year, int month, int day, int hour, int minute)
{
    struct tm value;

    memset(&value, 0, sizeof(value));
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    return value;
}

/*
 * 期待するオフセットを、実装とは別の方法で求める: localtime と gmtime の結果の
 * 年と年内の通算日（tm_yday）から日の前後（-1、0、+1）を決め、時と分の差を足す。
 * オフセットの絶対値が 24 時間未満であることを前提とする
 */
static long expected_offset(const struct tm *local_time, const struct tm *utc_time)
{
    long day = 0;

    if (local_time->tm_year != utc_time->tm_year)
    {
        day = (local_time->tm_year > utc_time->tm_year) ? 1 : -1;
    }
    else if (local_time->tm_yday != utc_time->tm_yday)
    {
        day = (local_time->tm_yday > utc_time->tm_yday) ? 1 : -1;
    }
    return (day * 1440L) + ((long)(local_time->tm_hour - utc_time->tm_hour) * 60L)
           + (long)(local_time->tm_min - utc_time->tm_min);
}

/*
 * now の時刻について、mnd_cli_format_now の結果を実装とは別に組み立てた期待値と比べる。
 * 期待値の日時の部分は localtime の結果を strftime で整形し、オフセットは expected_offset で求める。
 * 実装の結果を actual に、期待したオフセットを offset に返す
 */
static void check_format_now(time_t now, char actual[MND_PO_DATE_SIZE], long *offset)
{
    mnd_cli_env_t env;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    struct tm local_copy;
    struct tm utc_copy;
    const struct tm *result;
    char expected[MND_PO_DATE_SIZE];
    size_t len;
    long magnitude;
    bool ok;

    /* localtime と gmtime は静的な領域を返すため、1 つ目を写してから 2 つ目を呼ぶ */
    result = localtime(&now);
    NAOS_REQUIRE_NOT_NULL(result);
    local_copy = *result;
    result = gmtime(&now);
    NAOS_REQUIRE_NOT_NULL(result);
    utc_copy = *result;

    *offset = expected_offset(&local_copy, &utc_copy);
    magnitude = (*offset < 0) ? -*offset : *offset;
    len = strftime(expected, sizeof(expected), "%Y-%m-%d %H:%M", &local_copy);
    NAOS_REQUIRE_UINT_EQ(len, 16);
    expected[len] = (*offset < 0) ? '-' : '+';
    expected[len + 1] = (char)('0' + ((magnitude / 600) % 10));
    expected[len + 2] = (char)('0' + ((magnitude / 60) % 10));
    expected[len + 3] = (char)('0' + ((magnitude % 60) / 10));
    expected[len + 4] = (char)('0' + (magnitude % 10));
    expected[len + 5] = '\0';

    memset(&env, 0, sizeof(env));
    env.now = test_clock_now;
    test_clock_set(now);

    actual[0] = '\0';
    ok = mnd_cli_format_now(&env, actual, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_CHECK_STR_EQ(actual, expected);
    mnd_diag_list_free(&diags);
}

/* TZ を変えたかどうかと、変える前の値（teardown で戻す） */
static bool tz_changed = false;
static bool tz_had_value = false;
static char tz_saved[256];

/* プロセスの TZ を value にしてタイムゾーンの情報を読み直す。失敗したら false */
static bool set_tz(const char *value)
{
    if (!tz_changed)
    {
        const char *current = getenv("TZ");

        tz_had_value = (current != NULL);
        if (tz_had_value)
        {
            if (strlen(current) >= sizeof(tz_saved))
            {
                return false;
            }
            strcpy(tz_saved, current);
        }
        tz_changed = true;
    }
#if defined(_WIN32)
    {
        char assignment[64];

        if ((strlen(value) + 4) > sizeof(assignment))
        {
            return false;
        }
        strcpy(assignment, "TZ=");
        strcat(assignment, value);
        if (_putenv(assignment) != 0)
        {
            return false;
        }
        _tzset();
    }
#else
    if (setenv("TZ", value, 1) != 0)
    {
        return false;
    }
    tzset();
#endif
    return true;
}

/* TZ を変えたテストの後で元の値（なければ未設定）に戻す */
NAOS_TEARDOWN(time_restore_tz)
{
    if (!tz_changed)
    {
        return;
    }
    tz_changed = false;
#if defined(_WIN32)
    {
        char assignment[sizeof(tz_saved) + 4];

        strcpy(assignment, "TZ=");
        if (tz_had_value)
        {
            strcat(assignment, tz_saved);
        }
        (void)_putenv(assignment);
        _tzset();
    }
#else
    if (tz_had_value)
    {
        (void)setenv("TZ", tz_saved, 1);
    }
    else
    {
        (void)unsetenv("TZ");
    }
    tzset();
#endif
}

/* TZ を value にして now を整形し、結果が期待値（独立に計算）と text に一致し、オフセットが minutes であること */
static void check_with_tz(const char *value, time_t now, long minutes, const char *text)
{
    char actual[MND_PO_DATE_SIZE];
    long offset = 0;

    NAOS_REQUIRE_TRUE(set_tz(value));
    check_format_now(now, actual, &offset);
    /* TZ が効いていること（独立に求めたオフセットが意図した値であること）を確かめる */
    NAOS_CHECK_INT_EQ(offset, minutes);
    NAOS_CHECK_STR_EQ(actual, text);
}

/* オフセットの計算: 同じ日時なら 0 */
NAOS_TEST(time_offset_same_time_is_zero)
{
    struct tm local_time = make_tm(2026, 3, 1, 12, 34);
    struct tm utc_time = make_tm(2026, 3, 1, 12, 34);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_time, &utc_time), 0);
}

/* オフセットの計算: 東（正）のオフセット */
NAOS_TEST(time_offset_positive)
{
    struct tm local_time = make_tm(2026, 3, 1, 9, 0);
    struct tm utc_time = make_tm(2026, 3, 1, 0, 0);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_time, &utc_time), 540);
}

/* オフセットの計算: 西（負）のオフセット */
NAOS_TEST(time_offset_negative)
{
    struct tm local_time = make_tm(2026, 3, 1, 0, 0);
    struct tm utc_time = make_tm(2026, 3, 1, 5, 0);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_time, &utc_time), -300);
}

/* オフセットの計算: 30 分単位のオフセット（正と負） */
NAOS_TEST(time_offset_half_hour)
{
    struct tm local_east = make_tm(2026, 3, 1, 5, 30);
    struct tm utc_east = make_tm(2026, 3, 1, 0, 0);
    struct tm local_west = make_tm(2026, 3, 1, 2, 30);
    struct tm utc_west = make_tm(2026, 3, 1, 12, 0);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_east, &utc_east), 330);
    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_west, &utc_west), -570);
}

/* オフセットの計算: ローカル時刻が UTC の翌日 */
NAOS_TEST(time_offset_local_next_day)
{
    struct tm local_time = make_tm(2026, 1, 2, 5, 0);
    struct tm utc_time = make_tm(2026, 1, 1, 20, 0);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_time, &utc_time), 540);
}

/* オフセットの計算: ローカル時刻が UTC の前日（年の境界） */
NAOS_TEST(time_offset_local_previous_day_across_year)
{
    struct tm local_time = make_tm(2025, 12, 31, 21, 0);
    struct tm utc_time = make_tm(2026, 1, 1, 2, 0);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_time, &utc_time), -300);
}

/* オフセットの計算: 月の境界（うるう年の 2 月末） */
NAOS_TEST(time_offset_across_leap_month_end)
{
    struct tm local_time = make_tm(2024, 3, 1, 0, 30);
    struct tm utc_time = make_tm(2024, 2, 29, 15, 30);

    NAOS_CHECK_INT_EQ(mnd_cli_utc_offset_minutes(&local_time, &utc_time), 540);
}

/* 固定時刻（実行環境のタイムゾーンのまま）: 日時の部分がローカル時刻、オフセットが独立に求めた値と一致する */
NAOS_TEST(time_format_fixed_time_in_environment_tz)
{
    char actual[MND_PO_DATE_SIZE];
    long offset = 0;

    check_format_now(TIME_2026_01_01 + (12 * 3600) + (34 * 60), actual, &offset);
    check_format_now(TIME_2026_01_01 + (181 * 86400L) + (23 * 3600) + (59 * 60), actual, &offset);
}

/* TZ が UTC: "+0000" */
NAOS_TEST(time_format_tz_utc)
{
    check_with_tz("UTC0", TIME_2026_01_01 + (12 * 3600) + (34 * 60), 0, "2026-01-01 12:34+0000");
}

/* TZ が UTC+9: 正のオフセット */
NAOS_TEST(time_format_tz_positive)
{
    check_with_tz("JST-9", TIME_2026_01_01 + (12 * 3600) + (34 * 60), 540, "2026-01-01 21:34+0900");
}

/* TZ が UTC-5: 負のオフセット */
NAOS_TEST(time_format_tz_negative)
{
    check_with_tz("EST5", TIME_2026_01_01 + (12 * 3600) + (34 * 60), -300, "2026-01-01 07:34-0500");
}

/* TZ が UTC+5:30: 30 分単位のオフセット */
NAOS_TEST(time_format_tz_half_hour)
{
    check_with_tz("IST-5:30", TIME_2026_01_01 + (12 * 3600) + (34 * 60), 330, "2026-01-01 18:04+0530");
}

/* ローカル時刻が UTC の翌日: 2026-01-01 20:00 UTC は UTC+9 で 2026-01-02 05:00 */
NAOS_TEST(time_format_tz_local_next_day)
{
    check_with_tz("JST-9", TIME_2026_01_01 + (20 * 3600), 540, "2026-01-02 05:00+0900");
}

/* ローカル時刻が UTC の前日（年の境界）: 2026-01-01 02:30 UTC は UTC-5 で 2025-12-31 21:30 */
NAOS_TEST(time_format_tz_local_previous_day)
{
    check_with_tz("EST5", TIME_2026_01_01 + (2 * 3600) + (30 * 60), -300, "2025-12-31 21:30-0500");
}

/* env->now が NULL なら実際の時刻を使い、書式どおりの文字列を作る */
NAOS_TEST(time_format_real_clock)
{
    mnd_cli_env_t env;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    char actual[MND_PO_DATE_SIZE];
    size_t i;

    memset(&env, 0, sizeof(env));
    env.now = NULL;
    actual[0] = '\0';
    NAOS_CHECK_TRUE(mnd_cli_format_now(&env, actual, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(strlen(actual), 21);
    for (i = 0; i < 21; i++)
    {
        char c = actual[i];

        if ((i == 4) || (i == 7))
        {
            NAOS_CHECK_INT_EQ(c, '-');
        }
        else if (i == 10)
        {
            NAOS_CHECK_INT_EQ(c, ' ');
        }
        else if (i == 13)
        {
            NAOS_CHECK_INT_EQ(c, ':');
        }
        else if (i == 16)
        {
            NAOS_CHECK_TRUE((c == '+') || (c == '-'));
        }
        else
        {
            NAOS_CHECK_TRUE((c >= '0') && (c <= '9'));
        }
    }
    mnd_diag_list_free(&diags);
}

/*
 * 書式に収まらない時刻（10001 年）は、localtime が NULL を返すか整形に失敗するかのどちらかで、
 * 診断を加えて失敗する
 */
NAOS_TEST(time_format_out_of_range_fails_with_diagnostic)
{
    mnd_cli_env_t env;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    char actual[MND_PO_DATE_SIZE];

    if (time_t_bytes() < 8)
    {
        NAOS_SKIP("time_t cannot represent the year 10001");
    }
    memset(&env, 0, sizeof(env));
    env.now = test_clock_now;
    /* 10000-01-01 00:00:00 UTC（253402300800）の 400 日後 */
    test_clock_set((time_t)(253402300800LL + (400LL * 86400LL)));

    strcpy(actual, "unchanged");
    NAOS_CHECK_FALSE(mnd_cli_format_now(&env, actual, &diags));
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 1);
    NAOS_CHECK_INT_EQ(mnd_diag_list_at(&diags, 0)->severity, MND_SEVERITY_ERROR);
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));
    NAOS_CHECK_STR_EQ(actual, "");
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(time,
    NAOS_SET_TEARDOWN(time_restore_tz),
    NAOS_ADD_TEST(time_offset_same_time_is_zero),
    NAOS_ADD_TEST(time_offset_positive),
    NAOS_ADD_TEST(time_offset_negative),
    NAOS_ADD_TEST(time_offset_half_hour),
    NAOS_ADD_TEST(time_offset_local_next_day),
    NAOS_ADD_TEST(time_offset_local_previous_day_across_year),
    NAOS_ADD_TEST(time_offset_across_leap_month_end),
    NAOS_ADD_TEST(time_format_fixed_time_in_environment_tz),
    NAOS_ADD_TEST(time_format_tz_utc),
    NAOS_ADD_TEST(time_format_tz_positive),
    NAOS_ADD_TEST(time_format_tz_negative),
    NAOS_ADD_TEST(time_format_tz_half_hour),
    NAOS_ADD_TEST(time_format_tz_local_next_day),
    NAOS_ADD_TEST(time_format_tz_local_previous_day),
    NAOS_ADD_TEST(time_format_real_clock),
    NAOS_ADD_TEST(time_format_out_of_range_fails_with_diagnostic)
)
