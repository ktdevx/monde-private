# Brief: cli-args

## Problem
`monde` は 4 つのサブコマンドとそれぞれのオプションを持つ。外部ライブラリ（getopt 系）を使えないため、引数の解析を自前で実装する必要がある。RFP は、この引数パーサを再利用できる部品にすることを求めている。

## Current State
引数パーサはまだない。MSVC には `getopt` がないため、プラットフォームの機能にも頼れない。

## Desired Outcome
- Monde 固有の知識を持たず、宣言的に定義できるサブコマンド・オプション解析部品がある。
- 解析結果、ヘルプ、使い方のエラーを英語で出力でき、単体テストで検証されている。

## Approach
サブコマンドとオプションをテーブルで宣言し、パーサがそのテーブルを使って `argv` を解析する。ヘルプの文面も、同じ宣言から生成する。

## Scope
- **In**:
  - サブコマンドの振り分け
  - 長いオプション、短いオプション、値付きオプション（`--opt value` と `--opt=value`）
  - カンマ区切りのリスト値（例: `--front-matter-keys title,description`）
  - 位置引数、`--` による終端
  - 必須チェック、未知のオプションの検出
  - ヘルプとバージョンの出力、英語のエラーメッセージ
- **Out**: Monde 固有のサブコマンドとオプションの定義（monde-cli が担う）、設定ファイル

## Boundary Candidates
- 引数を宣言するデータ構造
- 解析エンジン
- ヘルプと使い方の整形

## Out of Boundary
- 各サブコマンドの処理内容
- 環境変数や設定ファイルからの設定の読み込み

## Upstream / Downstream
- **Upstream**: project-foundation（バッファ、エラー報告）
- **Downstream**: monde-cli

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: monde-cli（オプションの意味は monde-cli が持つ）

## Constraints
- C99、外部依存なし。Windows の `argv`（UTF-8 化が必要か）の扱いは、foundation と設計時に揃える。
- メッセージは英語。
