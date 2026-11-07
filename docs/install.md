# インストール

`monde` は、GitHub Releases にあるビルド済みのアーカイブから入手するか、ソースからビルドして使います。ビルド済みの実行ファイルは外部の実行時ライブラリに依存しないため、アーカイブを展開して実行ファイルを置くだけで使えます。

- [GitHub Releases から入手する](#github-releases-から入手する)
- [ソースからビルドする](#ソースからビルドする)

## GitHub Releases から入手する

### アーカイブを選ぶ

このリポジトリの GitHub Releases から、使う OS とアーキテクチャに合うアーカイブをダウンロードします。アーカイブの名前は `monde-X.Y.Z-<os>-<arch>.<ext>` の形式です。`X.Y.Z` はバージョン番号で、リリースのタグ `vX.Y.Z` から `v` を除いたものです。

| OS | アーキテクチャ | アーカイブ名の形式 | 例（バージョン 0.1.0） |
| --- | --- | --- | --- |
| Windows | x64 | `monde-X.Y.Z-windows-x64.zip` | `monde-0.1.0-windows-x64.zip` |
| macOS | arm64（Apple シリコン） | `monde-X.Y.Z-macos-arm64.tar.gz` | `monde-0.1.0-macos-arm64.tar.gz` |
| macOS | x64（Intel） | `monde-X.Y.Z-macos-x64.tar.gz` | `monde-0.1.0-macos-x64.tar.gz` |
| Linux | x64 | `monde-X.Y.Z-linux-x64.tar.gz` | `monde-0.1.0-linux-x64.tar.gz` |

各 Release には、4 つのアーカイブのほかに、それぞれの SHA-256 を列挙した `SHA256SUMS` が添付されています。

### チェックサムを確かめる

ダウンロードしたアーカイブが壊れていないことを、`SHA256SUMS` で確かめます。アーカイブと `SHA256SUMS` を同じディレクトリに置き、そのディレクトリで次のコマンドを実行します。以下の例はバージョン 0.1.0 のものです。

Linux では `sha256sum` を使います。`--ignore-missing` を付けると、ダウンロードしていない他の OS 向けのアーカイブの行を無視します。

```sh
sha256sum -c --ignore-missing SHA256SUMS
```

ダウンロードしたアーカイブについて `monde-0.1.0-linux-x64.tar.gz: OK` のように `OK` が表示されれば一致しています。

macOS では `shasum -a 256` を使います。

```sh
shasum -a 256 -c --ignore-missing SHA256SUMS
```

Windows では、PowerShell の `Get-FileHash` でアーカイブのハッシュを表示し、`SHA256SUMS` の同じファイル名の行にある値と一致することを確かめます（`Get-FileHash` は英字を大文字で表示しますが、大文字と小文字の違いは無視して比べます）。

```powershell
Get-FileHash -Algorithm SHA256 monde-0.1.0-windows-x64.zip
Select-String -Path SHA256SUMS -Pattern 'windows-x64'
```

### 展開して配置する

アーカイブを展開すると、アーカイブと同じ名前のディレクトリ `monde-X.Y.Z-<os>-<arch>` が作られ、その下に次の 3 つのファイルが置かれます。

| ファイル | 内容 |
| --- | --- |
| `monde`（Windows は `monde.exe`） | 実行ファイル |
| `LICENSE` | ライセンス（MIT License） |
| `README.md` | Monde の概要 |

実行ファイルを、PATH の通ったディレクトリに置きます。

Linux と macOS の例（`/usr/local/bin` に置く場合）:

```sh
tar -xzf monde-0.1.0-linux-x64.tar.gz
sudo cp monde-0.1.0-linux-x64/monde /usr/local/bin/
```

macOS では、アーカイブ名の `linux-x64` を `macos-arm64` または `macos-x64` に読み替えてください。ブラウザーでダウンロードしたアーカイブから取り出した実行ファイルを macOS が開けないときは、`xattr -d com.apple.quarantine /usr/local/bin/monde` でダウンロード時に付いた属性を外してから実行します。

Windows の例（PowerShell）:

```powershell
Expand-Archive -Path monde-0.1.0-windows-x64.zip -DestinationPath .
```

展開した `monde-0.1.0-windows-x64` の下の `monde.exe` を、任意のディレクトリ（例: `%LOCALAPPDATA%\Programs\monde`）に置き、そのディレクトリを環境変数 `PATH` に加えます。`PATH` は「システムのプロパティ」の「環境変数」から編集できます。

### 動作を確かめる

新しく開いたターミナルで次のコマンドを実行します。

```sh
monde --version
```

`monde 0.1.0` のように、`monde` に続いてバージョン番号が表示されれば、インストールは完了です。表示されるバージョン番号が、ダウンロードしたアーカイブの名前の `X.Y.Z` と同じであることも確かめてください。

続けて [翻訳フロー](workflow.md) に進むと、新規翻訳と原文更新の手順を確かめられます。

## ソースからビルドする

### 必要なツール

- CMake 3.21 以上
- C コンパイラ
  - Windows: MSVC（Visual Studio）または MinGW-w64
  - macOS: Apple Clang（Xcode または Command Line Tools）
  - Linux: GCC または Clang
- Git（テストを含めてビルドするときだけ。テストフレームワークの Naos の取得に使う）

ビルドの手順はどの OS でも同じです。以下のコマンドは、リポジトリのルートで実行します。

### 製品だけをビルドする

テストをせずに `monde` の実行ファイルだけをビルドするときは、構成時に `-DBUILD_TESTING=OFF` を指定します。このときテスト用の依存（テストフレームワークの Naos）は取得しないため、ネットワークに接続していなくてもビルドできます。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
```

実行ファイルは `build/src/monde/` の下にできます。Visual Studio や Xcode のような複数の構成を扱う生成器では、その下の `Release/` にできます。できた実行ファイルを [展開して配置する](#展開して配置する) と同じように PATH の通ったディレクトリに置き、`monde --version` で確かめます。

### テストを含めてビルドし、実行する

`-DBUILD_TESTING=OFF` を指定しなければ、テストもビルドします。構成時に、テストフレームワークの Naos（v0.4.0）を Git で GitHub から取得します。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`ctest` の最後に `100% tests passed` と表示されれば、すべてのテストが通っています。

- `--config Release`（`cmake --build`）と `-C Release`（`ctest`）は、Visual Studio や Xcode のような複数の構成を扱う生成器で必要です。Makefile や Ninja のような 1 つの構成だけを扱う生成器では `-DCMAKE_BUILD_TYPE=Release` が使われ、これらを付けても動作は変わりません。
- MinGW-w64 では、MSYS2 などの環境で `-G Ninja` を構成のコマンドに加えて、Ninja を生成器に指定できます。
- GNU gettext のツールが見つかる環境では、gettext との互換性を確かめるテストも実行します（`msgfmt` は見つかれば、`msgcat` と `msgmerge` はバージョン 0.21 のときだけ）。見つからない環境では、そのツールを使うテストは登録されません。
- ネットワークに接続できない環境では、手元にある Naos のソースを `-DFETCHCONTENT_SOURCE_DIR_NAOS=<Naos のソースのパス>` で構成時に指定すると、取得の代わりにそのソースを使います。
