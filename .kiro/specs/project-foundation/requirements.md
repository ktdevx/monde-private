# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者。C99 の自前実装部品を Windows / macOS / Linux の複数のコンパイラ（MSVC、MinGW-w64、Apple Clang、GCC、Clang）で一貫してビルド・テストする必要がある。

**現状**: リポジトリには LICENSE、README、入力資料（`docs/inputs/`）しかなく、ソースコード、ビルド構成、CI はまだない。ビルド・テスト・CI の基盤と、各部品が共通で使う低レベルのユーティリティがないため、以降の spec（cli-args、po-format、markdown-parser、yaml-front-matter など）を並行して進められない。

**何を変えるか**:
- CMake（3.21 以上）でモジュールごとにライブラリターゲットを分けた構成（`src/<module>/`、`tests/<module>/`）を用意し、全対象環境でビルドが通るようにする。コンパイラの警告とフラグを統一する（VLA の禁止、MSVC の `/utf-8`、`/Zc:preprocessor` など）。
- テストフレームワーク Naos を FetchContent などで固定したタグ・コミットで取り込み、CTest からテストを実行できるようにする。
- GitHub Actions の CI マトリクスで、上記 5 種のコンパイラによるビルドとテストを自動で行う。
- 以降の部品が使う共通ユーティリティをテスト付きで揃える:
  - UTF-8 ファイルの読み書き（常にバイナリモード、BOM の検出と除去、Windows での非 ASCII パス対応）
  - 改行コード（LF / CRLF）の検出
  - 可変長の文字列・バイトバッファ
  - ソース範囲（バイトオフセットと行番号）の型
  - エラーと診断の報告

**制約**: C99 を MSVC 互換の範囲で使う（VLA と `_Complex` は使わない）。外部の実行時ライブラリには依存しない（Naos はテスト専用、pre-1.0 のためバージョン固定）。

**対象外**: Markdown、YAML、PO、引数の解析ロジック。リリース配布のワークフロー（docs-and-release が担う）。`msgfmt --check` の CI ジョブ（monde-cli で追加する）。

## Introduction
project-foundation は、Monde の以降のすべての spec が依存する開発基盤と共通部品を提供する。対象の利用者は Monde の開発者（および後続 spec の部品）である。具体的には、(1) 全対象環境で同じ結果になるビルド構成、(2) 部品ごとの単体テストを実行できるテスト基盤、(3) 5 種のコンパイラでビルドとテストを自動で行う CI、(4) UTF-8 ファイルの読み書き・改行コードの検出・可変長バッファ・ソース範囲・診断という共通部品を揃える。これにより、cli-args、po-format、markdown-parser、yaml-front-matter を互いに独立して並行に開発できるようにする。

## Boundary Context
- **In scope**:
  - ビルド構成（モジュール単位のライブラリ、統一したコンパイラ警告・フラグ）
  - テスト基盤（Naos の固定バージョンでの取り込み、CTest からの実行）
  - CI（5 種のコンパイラでのビルドとテスト）
  - 共通部品: UTF-8 ファイルの読み込みと書き込み、BOM の検出・除去・付与、不正な UTF-8 の検出、改行コードの検出、可変長の文字列・バイトバッファ、ソース範囲、診断
- **Out of scope**:
  - Markdown、YAML、PO、コマンドライン引数の解析ロジック
  - リリース配布のワークフロー（docs-and-release が担う）
  - `msgfmt --check` の CI ジョブ（monde-cli で追加する）
  - 診断をユーザーに表示する形式と終了コードの決定（CLI 層＝monde-cli が担う）
  - 出力時に改行コードを変換・統一する処理（共通部品は検出結果を返すだけで、出力の改行は呼び出し側が組み立てる）
- **Adjacent expectations**:
  - 後続の部品は、ファイルの読み書きを本 spec の共通部品経由で行い、直接ファイルを開かない。
  - 後続の部品は、エラーを本 spec の診断として呼び出し側に返し、自分で標準エラー出力に書いたりプロセスを終了したりしない。
  - markdown-parser と yaml-front-matter は本 spec のソース範囲を使って要素の位置を返し、translation-units はそれを範囲置換に使う。

## Requirements

### Requirement 1: 全対象環境で一貫したビルド
**Objective:** As a Monde の開発者, I want 1 つのビルド構成で全対象環境のビルドを通したい, so that 環境ごとの差異を気にせずに部品を開発できる

#### Acceptance Criteria
1. The Monde ビルド構成 shall CMake 3.21 以上で、Windows（MSVC、MinGW-w64）、macOS（Apple Clang）、Linux（GCC、Clang）のそれぞれにおいて、同じ手順（構成・ビルド・テスト）で全ターゲットをビルドできる。
2. If CMake のバージョンが 3.21 未満である, then the Monde ビルド構成 shall 構成の段階で必要なバージョンを示すエラーを出して停止する。
3. The Monde ビルド構成 shall 部品（モジュール）ごとに独立したライブラリターゲットを持ち、各モジュールのソースとテストを同じモジュール名で対応付ける。
4. The Monde ビルド構成 shall 全ターゲットに同じコンパイラ警告とフラグの設定を適用する。
5. If ソースコード中にコンパイラ警告が 1 つでも発生する, then the Monde ビルド構成 shall そのビルドを失敗させる。
6. If ソースコード中で可変長配列（VLA）が使われている, then the Monde ビルド構成 shall すべての対象コンパイラでそのビルドを失敗させる。
7. While MSVC でビルドしている, the Monde ビルド構成 shall ソースファイルを UTF-8 として解釈し、C 標準に準拠したプリプロセッサでコンパイルする。
8. The Monde ビルド構成 shall 外部の実行時ライブラリに依存しない実行ファイルを生成し、テストフレームワークを製品の実行ファイルにリンクしない。

### Requirement 2: 部品ごとの単体テストの実行
**Objective:** As a Monde の開発者, I want 各部品の単体テストを共通の手順で実行したい, so that 部品を追加・変更したときにすぐに回帰を確認できる

#### Acceptance Criteria
1. The Monde テスト基盤 shall テストフレームワーク Naos を、固定したタグまたはコミットで取り込む。
2. When 開発者がテストランナー（CTest）でテストを実行する, the Monde テスト基盤 shall 登録されたすべてのモジュールのテストを実行し、テストごとの成否を表示する。
3. If 1 つ以上のテストが失敗する, then the Monde テスト基盤 shall テストランナーの終了ステータスを失敗（0 以外）にし、失敗したテストを特定できる出力を表示する。
4. When 開発者が新しいモジュールのテストを追加する, the Monde テスト基盤 shall 既存モジュールと同じ手順でそのテストをテストランナーに登録できるようにする。
5. The Monde テスト基盤 shall 本 spec の共通部品（Requirement 4〜9）それぞれに単体テストを持つ。

### Requirement 3: CI による自動ビルドとテスト
**Objective:** As a Monde の開発者, I want 変更のたびに全対象環境でビルドとテストが自動で行われてほしい, so that 特定の環境でだけ壊れる変更を取り込む前に気付ける

#### Acceptance Criteria
1. When デフォルトブランチへの push またはプルリクエストが作成・更新される, the Monde CI shall MSVC、MinGW-w64、Apple Clang、GCC、Clang の 5 つの環境でビルドとテストを実行する。
2. If いずれかの環境でビルドまたはテストが失敗する, then the Monde CI shall その実行全体を失敗として報告する。
3. When ある環境でビルドまたはテストが失敗する, the Monde CI shall 他の環境の実行を中断せずに最後まで行い、環境ごとの成否を報告する。
4. If いずれかの環境でテストが失敗する, then the Monde CI shall 失敗したテストを特定できるテスト出力をログに残す。

### Requirement 4: UTF-8 ファイルの読み込み
**Objective:** As a 後続部品の開発者, I want ファイルの内容をバイト列のまま、BOM と文字コードを確認したうえで読み込みたい, so that 原文をバイト単位で保持したまま解析できる

#### Acceptance Criteria
1. When 呼び出し側がファイルの読み込みを要求する, the foundation ファイル入出力 shall ファイルの内容を改行コードの変換をせずにバイト単位でそのまま返す。
2. When 読み込んだ内容の先頭に UTF-8 の BOM（EF BB BF）がある, the foundation ファイル入出力 shall BOM を取り除いた内容を返し、BOM があったことを呼び出し側に伝える。
3. When 読み込んだ内容の先頭に BOM がない, the foundation ファイル入出力 shall 内容を変更せずに返し、BOM がなかったことを呼び出し側に伝える。
4. If 読み込んだ内容に UTF-8 として不正なバイト列が含まれる, then the foundation ファイル入出力 shall 読み込みを失敗とし、最初の不正箇所のバイトオフセットと行番号を含むエラーの診断を返す。
5. When 空のファイルを読み込む, the foundation ファイル入出力 shall 長さ 0 の内容を正常な結果として返す。
6. When パスに ASCII 以外の文字（例: 日本語のファイル名・ディレクトリ名）を含む UTF-8 のパスが指定される, the foundation ファイル入出力 shall Windows を含むすべての対象環境でそのファイルを読み込む。
7. If ファイルが存在しない、または読み込めない, then the foundation ファイル入出力 shall 読み込みを失敗とし、対象のパスと失敗の理由を含むエラーの診断を返す。

### Requirement 5: UTF-8 ファイルの書き込み
**Objective:** As a 後続部品の開発者, I want 組み立てた内容をそのままのバイト列でファイルに書き出したい, so that 出力の改行コードや書式を呼び出し側が意図したとおりに保てる

#### Acceptance Criteria
1. When 呼び出し側がファイルへの書き込みを要求する, the foundation ファイル入出力 shall 渡された内容を改行コードの変換をせずにバイト単位でそのまま書き込む。
2. Where 呼び出し側が BOM の付与を指定する, the foundation ファイル入出力 shall 内容の先頭に UTF-8 の BOM を付けて書き込む。
3. The foundation ファイル入出力 shall 既定では BOM を付けずに書き込む。
4. When パスに ASCII 以外の文字を含む UTF-8 のパスが指定される, the foundation ファイル入出力 shall Windows を含むすべての対象環境でそのパスに書き込む。
5. If ファイルを作成または書き込みできない, then the foundation ファイル入出力 shall 書き込みを失敗とし、対象のパスと失敗の理由を含むエラーの診断を返す。
6. When BOM なしで読み込んだ内容を、BOM の付与を指定せずにそのまま書き込む, the foundation ファイル入出力 shall 元のファイルとバイト単位で一致するファイルを生成する。

### Requirement 6: 改行コードの検出
**Objective:** As a 後続部品の開発者, I want 入力の改行コードを判定したい, so that 出力の改行コードを入力に合わせられる

#### Acceptance Criteria
1. When 呼び出し側が内容の改行コードの検出を要求する, the foundation 改行検出 shall 内容に含まれる LF のみの改行と CRLF の改行の出現数を数え、多い方をその内容の改行コードとして返す。
2. If LF と CRLF の出現数が同じである, then the foundation 改行検出 shall 内容の中で先に現れた方をその内容の改行コードとして返す。
3. When 内容に LF と CRLF の両方が含まれる, the foundation 改行検出 shall 改行コードが混在していることを呼び出し側に伝える。
4. When 内容に改行が 1 つも含まれない, the foundation 改行検出 shall 改行がないことを呼び出し側に伝える。
5. The foundation 改行検出 shall LF を伴わない単独の CR を改行として数えない。

### Requirement 7: 可変長の文字列・バイトバッファ
**Objective:** As a 後続部品の開発者, I want 長さを気にせずに文字列やバイト列を組み立てたい, so that 出力の組み立てや範囲置換を簡潔かつ安全に書ける

#### Acceptance Criteria
1. When 呼び出し側がバッファにバイト列または文字列を追加する, the foundation バッファ shall 必要に応じて容量を拡張し、追加した内容を既存の内容の後ろにそのまま連結する。
2. The foundation バッファ shall NUL（0x00）を含む任意のバイト列を内容の一部として保持する。
3. When 呼び出し側がバッファの内容を参照する, the foundation バッファ shall 内容とその長さ（バイト数）を返し、内容の直後を NUL で終端して C 文字列としても参照できるようにする。
4. If 容量の拡張に必要なメモリを確保できない, then the foundation バッファ shall 異常終了せずに失敗を呼び出し側に返し、それまでの内容を壊さずに保つ。
5. When 呼び出し側がバッファを空にする、または解放する, the foundation バッファ shall 内容を破棄し、確保していたメモリを解放（または再利用可能に）する。

### Requirement 8: ソース範囲
**Objective:** As a 後続部品の開発者, I want 入力内の位置と範囲を共通の形で表したい, so that パーサが返した位置を範囲置換と行番号の表示の両方に使える

#### Acceptance Criteria
1. The foundation ソース範囲 shall 範囲を、BOM を除去した後の内容を基準とする開始と終了のバイトオフセット（開始を含み終了を含まない）で表す。
2. The foundation ソース範囲 shall 行番号を 1 から数える。
3. When 呼び出し側がバイトオフセットに対応する行番号を求める, the foundation ソース範囲 shall LF と CRLF をいずれも 1 つの改行として数え、単独の CR を改行として数えずに行番号を返す。
4. If 内容の長さを超えるバイトオフセット、または開始が終了より後ろにある範囲が指定される, then the foundation ソース範囲 shall 不正な範囲であることを呼び出し側に返す。
5. When 呼び出し側が範囲が指す内容を求める, the foundation ソース範囲 shall 元の内容のその範囲のバイト列を変更せずに返す。

### Requirement 9: エラーと診断の報告
**Objective:** As a 後続部品の開発者, I want エラーや警告を共通の形で呼び出し側に返したい, so that 表示の仕方と終了コードを CLI 層でまとめて決められる

#### Acceptance Criteria
1. The foundation 診断 shall 各診断に、重大度（エラーまたは警告）、英語のメッセージ、および位置が分かる場合はファイルパスと行番号を持たせる。
2. When 部品が処理中に複数の問題を検出する, the foundation 診断 shall それらを発生順に蓄積し、呼び出し側がすべて取り出せるようにする。
3. When 呼び出し側が蓄積された診断を確認する, the foundation 診断 shall エラーの診断が 1 つ以上含まれるかどうかを返す。
4. The foundation 診断 shall 診断の記録時に標準エラー出力への書き込みやプロセスの終了を行わない。
