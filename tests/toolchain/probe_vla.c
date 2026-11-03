/* 実行時の値で長さを決める配列（VLA）だけを含み、ビルドが失敗すべきソース */
int monde_probe_vla(int length);

int monde_probe_vla(int length)
{
    int values[length];
    int sum = 0;
    int i;

    for (i = 0; i < length; ++i)
    {
        values[i] = i;
    }

    for (i = 0; i < length; ++i)
    {
        sum += values[i];
    }

    return sum;
}
