# Monde

Markdown 文書を gettext の翻訳テンプレート（`.pot`）と翻訳ファイル（`.po`）を介して翻訳するための CLI ツール `monde` です。

最新のドキュメントはリポジトリの `docs/` にあります。

## 概要

Monde は、Markdown 文書から翻訳する文字列を取り出して `.pot` を作り、翻訳済みの `.po` から翻訳済みの Markdown を作ります。原文が更新されたときは、新しい `.pot` を既存の `.po` にマージして、これまでの翻訳を引き継げます。

- **書式をバイト単位で保持する**: 翻訳する文字列以外は、原文をバイト単位でそのまま残します（リスト記号、強調記号の種類、空行、インデント、改行コード）。構文木から文書を作り直さないため、原文と翻訳済み Markdown の差分は訳した箇所だけになります。
- **翻訳作業は PO 対応のツールに任せる**: Monde 自身は `.po` を編集しません。`.po` の編集には、PO に対応した翻訳プラットフォームやエディタ（Poedit など）を使います。生成する `.pot` と `.po` は GNU gettext の `msgfmt --check` を通ります。
- **依存のない単一の実行ファイル**: 外部の実行時ライブラリに依存しません。

## サブコマンド

| 操作 | コマンド | 入力 → 出力 |
| --- | --- | --- |
| テンプレート生成 | `monde extract` | `.md` → `.pot` |
| 翻訳ファイル作成 | `monde init` | `.pot` → `.po` |
| マージ | `monde update` | `.pot` + `.po` → `.po` |
| 翻訳済み Markdown 生成 | `monde translate` | `.md` + `.po` → `.md` |

各サブコマンドのオプションは `monde <command> --help` で確かめられます。

## インストール

このリポジトリの GitHub Releases から、使う OS とアーキテクチャに合うアーカイブを入手します（`X.Y.Z` はバージョン番号）。

| 対象 | アーカイブ |
| --- | --- |
| Windows x64 | `monde-X.Y.Z-windows-x64.zip` |
| macOS arm64（Apple シリコン） | `monde-X.Y.Z-macos-arm64.tar.gz` |
| macOS x64（Intel） | `monde-X.Y.Z-macos-x64.tar.gz` |
| Linux x64 | `monde-X.Y.Z-linux-x64.tar.gz` |

アーカイブを展開すると、`monde-X.Y.Z-<os>-<arch>` ディレクトリの下に実行ファイル（Windows は `monde.exe`、それ以外は `monde`）、`LICENSE`、`README.md`、`THIRD_PARTY_NOTICES` と、`licenses/` ディレクトリ（実行ファイルに取り込んだ素材のライセンス）が置かれます。実行ファイルを PATH の通ったディレクトリに置き、`monde --version` でバージョンが表示されることを確かめてください。各アーカイブの SHA-256 は Release に添付した `SHA256SUMS` で確かめられます。

詳しい手順は [インストール](docs/install.md) を参照してください。

## クイックスタート

新規翻訳のフロー（`extract` → `init` → 翻訳 → `translate`）を、リポジトリにある小さな英語の文書 [`docs/examples/hello.md`](docs/examples/hello.md) で試します。`hello.md` を作業用のディレクトリにコピーし、そのディレクトリで次のコマンドを順に実行します。

<!-- monde-check: quickstart -->
```sh
monde extract --front-matter-keys title -o hello.pot hello.md
monde init -l ja -o hello.ja.po hello.pot
# hello.ja.po を翻訳する。ここでは docs/examples/hello.ja.po の訳を使う
monde translate --front-matter-keys title -o hello.ja.md hello.md hello.ja.po
```

各段階の入力と出力は次のとおりです。

1. `extract`: `hello.md` から翻訳する文字列を取り出し、翻訳テンプレート `hello.pot` を作ります。`--front-matter-keys title` で、YAML フロントマターの `title` の値も翻訳の対象にします。
2. `init`: `hello.pot` から日本語（`-l ja`）の翻訳ファイル `hello.ja.po` を作ります。この時点では訳はすべて空です。
3. 翻訳: `hello.ja.po` の各エントリに訳を入れます。この作業には Poedit などの PO 対応のエディタや翻訳プラットフォームを使います。ここでは、訳を入れ終えた [`docs/examples/hello.ja.po`](docs/examples/hello.ja.po) を作業用のディレクトリにコピーして、`init` が作った `hello.ja.po` を置き換えます。
4. `translate`: `hello.md` と `hello.ja.po` から翻訳済みの Markdown `hello.ja.md` を作ります。`extract` と同じ `--front-matter-keys` を指定します。

各コマンドは、標準エラー出力に統計を 1 行で表示します（`extract` と `init` はメッセージの数、`translate` は翻訳済み・fuzzy・未翻訳のエントリの数と翻訳率）。統計は `-q` で抑止できます。`hello.ja.md` では本文とフロントマターの `title` が日本語になり、コードブロック、リンク先の URL、空行やリスト記号などの書式は `hello.md` のまま残ります。

原文が更新されたときのフロー（`extract` → `update` → 翻訳 → `translate`）は [翻訳フロー](docs/workflow.md) で説明しています。

## ソースからのビルド

必要なツールは次のとおりです。

- CMake 3.21 以上
- C コンパイラ: Windows は MSVC または MinGW-w64、macOS は Apple Clang、Linux は GCC または Clang

リポジトリのルートで次の 3 行を実行すると、ビルドとテストを行います。実行ファイルは `build/src/monde/` の下にできます。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

テストをせずに製品だけをビルドするときは、構成時に `-DBUILD_TESTING=OFF` を指定します。このときテスト用の依存（テストフレームワークの Naos）は取得しません。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
```

詳しくは [インストール](docs/install.md) を参照してください。

## 対応範囲

- **初版**: [CommonMark 0.31.2](https://spec.commonmark.org/0.31.2/) の Markdown と、YAML フロントマター（`--front-matter-keys` で指定したキーの値）。
- **次の版で対応する予定**: GFM（表のセル単位の翻訳、タスクリスト、取り消し線、自動リンク）。初版では、これらは CommonMark としての解釈のまま扱います。脚注はさらに先の版で対応する予定です。

初版の制限は [既知の制限](docs/limitations.md) にまとめています。

## バージョン

バージョン番号はセマンティックバージョニングに従い、`X.Y.Z` の 3 つの数で表します。リリースのタグは `vX.Y.Z` です。

- **X**: CLI の互換性を壊す変更で上げます。
- **Y**: 互換性を保つ機能の追加で上げます。
- **Z**: 修正だけのときに上げます。

X が 0 の間（`0.Y.Z`）は、CLI の互換性を壊す変更でも Y を上げることがあります。

## ドキュメント

利用マニュアルは `docs/` にあります。章の一覧は [目次](docs/README.md) を参照してください。

- [インストール](docs/install.md): GitHub Releases からの入手と、ソースからのビルド
- [翻訳フロー](docs/workflow.md): 新規翻訳と原文更新の手順
- [コマンドリファレンス](docs/commands.md): 各サブコマンドとオプション、統計、診断と終了コード
- [既知の制限](docs/limitations.md): 初版で対応していない記法や制限

## ライセンス

Monde は [MIT License](LICENSE) で提供します。

Copyright (c) 2026 The Monde Authors

## サードパーティ

### 実行ファイルに取り込んでいるもの

`monde` の実行ファイルには、次の第三者の素材から生成した表が組み込まれています。素材ごとの出典、版、ライセンスは [`THIRD_PARTY_NOTICES`](THIRD_PARTY_NOTICES) に、著作権表示とライセンスの全文は [`licenses/`](licenses/) にあります。どちらも配布物（Release のアーカイブ）に含まれます。

| 名称 | 用途 | ライセンス | 配布物 |
| --- | --- | --- | --- |
| Unicode Character Database 15.0.0 | Markdown の句読点と空白の判定、リンクのラベルの大文字・小文字の同一視（ケースフォールディング）、`.po` の行の折り返し（行分割の規則、文字幅、結合文字）に使う表 | Unicode License V3（`Unicode-3.0`）。全文は [`licenses/unicode-ucd.txt`](licenses/unicode-ucd.txt) | 含まれる（表示とライセンスの全文も同梱） |
| WHATWG HTML Living Standard の名前付き文字参照の一覧（`entities.json`） | Markdown の文字参照（`&amp;` など）の解釈に使う表 | ソースコードに取り込んだ部分は BSD 3-Clause License（`BSD-3-Clause`。標準そのものは CC BY 4.0）。全文は [`licenses/whatwg-entities.txt`](licenses/whatwg-entities.txt) | 含まれる（表示とライセンスの全文も同梱） |

### テスト専用で配布物に含まれないもの

次の資料はテストだけに使います。Monde の製品の実行ファイルと配布物（Release のアーカイブ）には含まれず、`THIRD_PARTY_NOTICES` にも載せません。

| 名称 | 用途 | ライセンス | 配布物 |
| --- | --- | --- | --- |
| CommonMark Spec 0.31.2 の `spec.txt`（John MacFarlane） | Markdown の解析のテストデータ。詳細は [`tests/data/commonmark/README.md`](tests/data/commonmark/README.md) | [CC-BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 含まれない |
| Naos v0.4.0 | テストフレームワーク。`-DBUILD_TESTING=OFF` のビルドでは取得しない | MIT License | 含まれない |
