# Requirements Document

## Project Description (Input)
**対象者**: monde で Markdown 文書を翻訳・保守する翻訳者。新規翻訳と原文更新の 2 つのフロー（RFP §3）を、1 つのコマンド `monde` で実行したい。

**現状**: cli-args、po-format、po-merge、translation-units などの部品が提供される予定だが、それらをサブコマンドとして統合した実行ファイルはまだない。部品ができていても、統合して受け入れ基準で検証しなければ、ツールとして使えない。

**変えたいこと**: cli-args でサブコマンドとオプションを宣言し、各サブコマンドのハンドラから部品を呼び出す薄い統合層として、次の 4 つのサブコマンドを動作させる。

| コマンド | 入力 → 出力 |
| --- | --- |
| `monde extract` | `.md` → `.pot` |
| `monde init` | `.pot` → `.po` |
| `monde update` | `.pot` + `.po` → `.po` |
| `monde translate` | `.md` + `.po` → `.md` |

あわせて、入出力ファイルの指定（標準入出力に対応するかは要件で決める）、`--front-matter-keys`、fuzzy の翻訳を採用するオプション、言語の指定（`init`）、翻訳率などの統計の表示、終了コードの体系と英語のエラーメッセージを扱う。受け入れ基準として、CommonMark spec 0.31.2 の全テストケースで「extract → translate（翻訳なし）」の結果が原文と一致することを、Naos と CTest によるデータ駆動テストで確かめる。また、生成した `.pot` / `.po` が `msgfmt --check` を通ることを CI（少なくとも Ubuntu）で確認する。

**範囲外**: 部品のロジック、日本語のドキュメント、リリースの配布（docs-and-release が担う）、複数ファイルやディレクトリの一括処理、設定ファイル、翻訳率による出力の制御。

**制約**: 指定はコマンドライン引数のみで行い、メッセージは英語とする。spec.txt はライセンス上の理由から、無改変のまま独立したディレクトリに置くか、テスト時に取得し、リリース成果物には含めない。改行コードは入力に合わせ、BOM 付きの入力も受け付ける。

**関連**: Upstream は cli-args、po-format、po-merge、translation-units（間接的に markdown-parser、yaml-front-matter）、Downstream は docs-and-release。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
