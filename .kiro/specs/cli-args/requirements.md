# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者（とくに monde-cli を実装する開発者）。`monde` は 4 つのサブコマンドとそれぞれのオプションを持つが、外部ライブラリ（getopt 系）を使えず、MSVC には `getopt` もないため、プラットフォームの機能にも頼れない。

**現状**: 引数パーサはまだない。RFP は、この引数パーサを Monde 固有の知識を持たない再利用可能な部品にすることを求めている。

**何を変えるか**:
- サブコマンドとオプションをテーブルで宣言的に定義し、パーサがそのテーブルを使って `argv` を解析する部品を用意する。
- 解析対象: サブコマンドの振り分け、長いオプション・短いオプション・値付きオプション（`--opt value` と `--opt=value`）、カンマ区切りのリスト値（例: `--front-matter-keys title,description`）、位置引数、`--` による終端、必須チェック、未知のオプションの検出。
- ヘルプとバージョンの出力、使い方のエラーメッセージを英語で出し、ヘルプの文面は同じ宣言から生成する。
- 単体テストで挙動を検証する。
- 範囲外: Monde 固有のサブコマンド・オプションの定義（monde-cli が担う）、各サブコマンドの処理内容、環境変数や設定ファイルからの設定の読み込み。

**制約**: C99、外部依存なし。project-foundation のバッファとエラー報告を使う。Windows の `argv`（UTF-8 化が必要か）の扱いは、foundation と設計時に揃える。メッセージは英語。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
