# CommonMark spec のテストデータ

このディレクトリには、CommonMark の仕様書 `spec.txt` を、取得したまま **無改変** で置いている。markdown-parser の単体テスト（`tests/markdown/` の spec_reader と spec のスイート）と、monde-cli の受け入れテストが、このファイルから例（Markdown と期待 HTML）を読む。

## 出典

| 項目 | 値 |
| --- | --- |
| 題名 | CommonMark Spec |
| 著者 | John MacFarlane |
| バージョン | 0.31.2（2024-01-28） |
| リポジトリ | https://github.com/commonmark/commonmark-spec |
| タグ | `0.31.2` |
| 取得元の URL | https://raw.githubusercontent.com/commonmark/commonmark-spec/0.31.2/spec.txt |
| 大きさ | 205,025 バイト（改行は LF のみ） |
| SHA-256 | `257c41ad946f7a1414a499aca402a1aa8fdac3678532266611348c1cf54f4b80` |
| 例の数 | 652 |

spec.commonmark.org で配布される `spec.txt` には先頭のメタデータ（ライセンスの記載を含む）がないため、GitHub のタグ `0.31.2` のものを使う。

## ライセンス

`spec.txt` は John MacFarlane による CommonMark Spec であり、[クリエイティブ・コモンズ 表示 - 継承 4.0 国際（CC-BY-SA 4.0）](https://creativecommons.org/licenses/by-sa/4.0/) の下で提供されている（ファイル先頭のメタデータ `license: '[CC-BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)'` を参照）。

- `spec.txt` は改変していない。内容は上の SHA-256 で確かめられる。
- `spec.txt` は Monde の製品のソースではなく、Monde のライセンスの対象ではない。

## 扱い

- `spec.txt` を編集しない。改行の変換を防ぐため、リポジトリ直下の `.gitattributes` で `-text` を指定している。テスト（spec_reader スイート）は、ファイルの大きさが 205,025 バイトで CR を含まないことを確かめる。
- テストは、ビルドのコンパイル定義 `MND_MD_SPEC_FILE` で渡すパスから読む。ディレクトリの場所やファイル名を変えると、monde-cli の受け入れテストにも影響する。
- `spec.txt` は製品の実行ファイルとリリース成果物（インストールの対象、配布するアーカイブ）に含めない。
- 例の取り出し方は、同じタグの `test/spec_tests.py` の `get_tests` と同じ規則に従う（`→` をタブに置き換える）。

## 確かめ方

```sh
sha256sum tests/data/commonmark/spec.txt
# 257c41ad946f7a1414a499aca402a1aa8fdac3678532266611348c1cf54f4b80  tests/data/commonmark/spec.txt
```
