#include "foundation/utf8.h"

/*
 * 先頭バイトから、シーケンスの長さと 2 バイト目の許容範囲を求める（Unicode Standard Table 3-7）。
 * 先頭バイトとして不正なら 0 を返す
 */
static size_t sequence_length(unsigned char lead, unsigned char *second_min, unsigned char *second_max)
{
    *second_min = 0x80;
    *second_max = 0xBF;

    if (lead <= 0x7F)
    {
        return 1;
    }

    if ((lead >= 0xC2) && (lead <= 0xDF))
    {
        return 2;
    }

    if ((lead >= 0xE0) && (lead <= 0xEF))
    {
        if (lead == 0xE0)
        {
            *second_min = 0xA0;
        }
        else if (lead == 0xED)
        {
            *second_max = 0x9F;
        }

        return 3;
    }

    if ((lead >= 0xF0) && (lead <= 0xF4))
    {
        if (lead == 0xF0)
        {
            *second_min = 0x90;
        }
        else if (lead == 0xF4)
        {
            *second_max = 0x8F;
        }

        return 4;
    }

    /* 単独の継続バイト 0x80〜0xBF、0xC0・0xC1、0xF5〜0xFF */
    return 0;
}

bool mnd_utf8_validate(const char *data, size_t len, size_t *invalid_offset)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t pos = 0;

    while (pos < len)
    {
        unsigned char second_min;
        unsigned char second_max;
        size_t seq_len = sequence_length(bytes[pos], &second_min, &second_max);
        size_t i;

        if ((seq_len == 0) || (seq_len > (len - pos)))
        {
            *invalid_offset = pos;
            return false;
        }

        if ((seq_len > 1) && ((bytes[pos + 1] < second_min) || (bytes[pos + 1] > second_max)))
        {
            *invalid_offset = pos;
            return false;
        }

        for (i = 2; i < seq_len; ++i)
        {
            if ((bytes[pos + i] < 0x80) || (bytes[pos + i] > 0xBF))
            {
                *invalid_offset = pos;
                return false;
            }
        }

        pos += seq_len;
    }

    return true;
}
