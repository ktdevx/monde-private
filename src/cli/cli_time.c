/* Clock: 現在日時と UTC オフセットから PO の日時文字列を作る */

#include "cli/cli_time.h"

#include "foundation/diag.h"
#include "po/po.h"

#include <stdbool.h>
#include <time.h>

/* 先発グレゴリオ暦の年月日（month は 1〜12）を 1970-01-01 からの通算の日に直す */
static long days_from_civil(long year, long month, long day)
{
    long era;
    long year_of_era;
    long day_of_year;
    long day_of_era;

    /* 3 月を年の始めとして数え、うるう日を年の最後に置く */
    if (month <= 2)
    {
        year -= 1;
    }
    era = ((year >= 0) ? year : (year - 399)) / 400;
    year_of_era = year - (era * 400);
    day_of_year = ((((153 * ((month > 2) ? (month - 3) : (month + 9))) + 2) / 5) + (day - 1));
    day_of_era = (year_of_era * 365) + (year_of_era / 4) - (year_of_era / 100) + day_of_year;
    return (era * 146097) + day_of_era - 719468;
}

long mnd_cli_utc_offset_minutes(const struct tm *local_time, const struct tm *utc_time)
{
    long local_days = days_from_civil((long)local_time->tm_year + 1900, (long)local_time->tm_mon + 1,
                                      (long)local_time->tm_mday);
    long utc_days = days_from_civil((long)utc_time->tm_year + 1900, (long)utc_time->tm_mon + 1,
                                    (long)utc_time->tm_mday);
    /* 日の差を先に取り、通算の分が long（32 ビットの環境もある）からあふれないようにする */
    long days = local_days - utc_days;
    long hours = (long)local_time->tm_hour - (long)utc_time->tm_hour;
    long minutes = (long)local_time->tm_min - (long)utc_time->tm_min;

    return (((days * 24) + hours) * 60) + minutes;
}

bool mnd_cli_format_now(const mnd_cli_env_t *env, char out[MND_PO_DATE_SIZE], mnd_diag_list_t *diags)
{
    time_t now = (env->now != NULL) ? env->now() : time(NULL);
    const struct tm *result;
    struct tm local_time;
    struct tm utc_time;

    out[0] = '\0';

    /* localtime と gmtime は静的な領域を返すため、1 つ目を写してから 2 つ目を呼ぶ */
    result = localtime(&now);
    if (result == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "cannot determine the local time");
        return false;
    }
    local_time = *result;

    result = gmtime(&now);
    if (result == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "cannot determine the UTC time");
        return false;
    }
    utc_time = *result;

    if (!mnd_po_date_format(&local_time, mnd_cli_utc_offset_minutes(&local_time, &utc_time), out))
    {
        out[0] = '\0';
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "cannot format the current time");
        return false;
    }
    return true;
}
