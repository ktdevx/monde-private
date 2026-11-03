/* 未使用のローカル変数（GCC/Clang -Wunused-variable、MSVC C4101）だけを含み、警告をエラーとする設定でビルドが失敗すべきソース */
int monde_probe_warning(int value);

int monde_probe_warning(int value)
{
    int unused;
    return value + 1;
}
