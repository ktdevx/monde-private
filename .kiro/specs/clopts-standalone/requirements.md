# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者と、コマンドライン引数パーサを Monde 以外でも使いたい開発者。引数パーサ（cli-args）は Monde 固有の知識を持たない汎用部品だが、Monde のビルドと共通部品に組み込まれており、単独では扱えない。将来、別リポジトリに切り離して独立ライブラリ「CLOpts」として管理する予定だが、現状のままでは切り離し時に依存の解消、改名、ビルド定義の書き直しが必要になる。

**現状**:
- ソースは `src/cli_args/`、テストは `tests/cli_args/` にある。
- ビルドは Monde 専用の CMake 関数（`monde_add_module` など）に依存し、ターゲットは `monde_cli_args`（別名 `monde::cli_args`）。単独では構成できない。
- foundation（`diag`、`buf`、`wide`）に依存している。公開 API が `mnd_diag_list_t` を引数に取り、ヘルプ整形の公開 API が `mnd_buf_t` を受け取り、Windows の UTF-16 変換に `mnd_wide_to_utf8` を使う。
- 公開名は `mnd_args_*` / `MND_ARGS_*`、ヘッダは `cli_args/args.h` など。利用側は `src/cli/` と `src/monde/main.c`。

**何を変えるか**:
- リポジトリのルートに `cli_args/` を作り、ライブラリのソース、公開ヘッダ、テスト、CMake 定義、README、ライセンスなど、CLOpts に必要なファイルをすべてそこに格納する。`src/cli_args/` と `tests/cli_args/` は残さない。
- `cli_args/` だけで構成・ビルド・テスト（CTest、Naos）ができるようにする。`cli_args/` の中から、その外にあるファイルを参照しない。
- ライブラリ名を CLOpts とする。CMake のターゲットは `clopts`、別名は `CLOpts::clopts`。
- foundation への依存をなくす。CLOpts が自前の最小限のエラー報告型を持ち、文字列バッファと UTF-16 変換は内部に持つ。
- 公開 API を `clopts_*` / `CLOPTS_*` に改名し、公開ヘッダを `<clopts/...>` の形で取り込めるようにする。
- `install` と `find_package(CLOpts)` に対応し、インストール先からも `CLOpts::clopts` でリンクできるようにする。
- Monde 本体はルートの `add_subdirectory(cli_args)` で取り込み、`CLOpts::clopts` にリンクする。CLOpts のエラーは `src/cli/` で `mnd_diag_list_t` に橋渡しする。`monde` の外から見える動作（ヘルプ、エラーメッセージ、終了コード）は変えない。
- CI に `cli_args/` 単独での構成・ビルド・テストを追加し、steering と既存ドキュメントのパスの記述を更新する。

**範囲外**: 別リポジトリへの実際の切り離し、引数パーサの機能追加と挙動の変更、共有ライブラリ対応、パッケージマネージャーへの登録、CLOpts 単体のリリース運用。

詳細（選ばなかった案、境界、制約）は `brief.md` を参照。

## Introduction
本 spec は、コマンドライン引数パーサ（cli-args）を、独立したライブラリ「CLOpts」として単独で扱える状態にする。利用者は、CLOpts を自分のプログラムに組み込む開発者、Monde の開発者、および将来 CLOpts を別リポジトリへ切り離す保守者である。

リポジトリのルートの `cli_args/` に CLOpts のファイルをすべて集め、そのディレクトリだけで構成・ビルド・テスト・インストールができるようにする。Monde 本体は `cli_args/` を取り込んで使い、`monde` の外から見える動作は変えない。

引数パーサの挙動（解析規則、ヘルプの書式、エラーメッセージの文言）は cli-args の要件が定めており、本 spec では変えない。本 spec が扱うのは、配置、名前、依存、ビルド、配布である。

### 用語
- **CLOpts**: 本 spec で独立させるコマンドライン引数パーサのライブラリ。CMake のパッケージ名は `CLOpts`、リンクに使うターゲット名は `CLOpts::clopts`。
- **CLOpts ディレクトリ**: リポジトリのルートの `cli_args/`。
- **単独構成**: CLOpts ディレクトリを最上位のプロジェクトとして構成すること。
- **取り込み構成**: 別のプロジェクト（Monde 本体など）が、CLOpts ディレクトリを自分のビルドの一部として取り込んで構成すること。
- **利用側プロジェクト**: CLOpts にリンクするプロジェクト。Monde 本体を含む。
- **公開名**: CLOpts の公開ヘッダが利用側プロジェクトに見せる関数、型、マクロ、列挙値の名前。

### discovery で決めたこと
- foundation への依存は、CLOpts を自己完結にすることで解消する。CLOpts が自前のエラー報告の手段を持ち、Monde 側が Monde の診断へ橋渡しする。
- 公開名は今回 `clopts_*` / `CLOPTS_*` に改名する。
- 取り込み構成に加えて、インストールして `find_package(CLOpts)` で使う形にも対応する。

### 要件の作成時に置いた前提
- CLOpts は Monde とは別の、自分自身のバージョン番号を持つ。初期値は設計で決める。
- CLOpts のライセンスは Monde と同じとする。
- CLOpts のテストは、Monde 本体と同じテストフレームワークの同じ固定版を使う。

## Boundary Context
- **In scope**:
  - CLOpts ディレクトリへのファイルの集約と、元の場所（`src/cli_args/`、`tests/cli_args/`）からの除去
  - CLOpts の単独構成でのビルドとテスト
  - Monde の共通部品（foundation）に頼らない公開インターフェース
  - 公開名と公開ヘッダの CLOpts 用の改名
  - 取り込み構成と、インストールして使う形の両方への対応
  - Monde 本体の追従（取り込み、改名、エラーの橋渡し）
  - CI での単独構成の検証、CLOpts の README とライセンス、steering と既存ドキュメントの更新
- **Out of scope**:
  - 別リポジトリの作成と、そこへの実際の切り離し
  - 引数パーサの機能追加と挙動の変更
  - 共有ライブラリとしてのビルド
  - パッケージマネージャーへの登録
  - CLOpts 単体のリリースのワークフローと、バージョン番号の運用規則
  - Monde のリリース成果物の構成の変更
  - cli-args spec の文書の書き換え（旧名と新名の対応は本 spec の設計で示す）
- **Adjacent expectations**:
  - cli-args: 引数パーサの挙動の基準を持ち続ける。本 spec の完了後も、cli-args の受け入れ基準はすべて満たされたままである。
  - project-foundation: 共通部品の公開インターフェースは変わらない。CLOpts が使わなくなるだけである。
  - monde-cli: `monde` のコマンドとオプションの宣言内容、各サブコマンドの処理は変わらない。変わるのは CLOpts の呼び出し方だけである。
  - docs-and-release: Monde の配布物と、リリースのワークフローの手順は変わらない。

## Requirements

### Requirement 1: CLOpts ディレクトリへの集約
**Objective:** As a CLOpts を切り離す保守者, I want CLOpts に必要なファイルが 1 つのディレクトリにまとまっていてほしい, so that そのディレクトリを移すだけで別リポジトリにできる

#### Acceptance Criteria
1. The CLOpts ディレクトリ shall CLOpts のライブラリのソース、公開ヘッダ、テスト、ビルド定義、README、ライセンスの全文を含む。
2. The CLOpts ディレクトリ shall ビルドとテストに必要なファイルとして、CLOpts ディレクトリの外にあるリポジトリ内のファイルを参照しない。
3. When CLOpts ディレクトリだけをリポジトリの外の場所に複製した, the CLOpts ビルド構成 shall その複製だけで構成、ビルド、テストを完了できる。
4. The Monde リポジトリ shall 引数パーサのソースとテストを CLOpts ディレクトリだけに持ち、`src/cli_args/` と `tests/cli_args/` を持たない。
5. The Monde リポジトリ shall 移した引数パーサのソースとテストについて、移す前のファイルの変更履歴をたどれる状態を保つ。

### Requirement 2: 単独構成でのビルドとテスト
**Objective:** As a CLOpts の開発者, I want CLOpts ディレクトリだけでビルドとテストをしたい, so that Monde 本体をビルドせずに CLOpts を開発・検証できる

#### Acceptance Criteria
1. When CLOpts ディレクトリを単独構成でビルドした, the CLOpts ビルド構成 shall CLOpts の静的ライブラリを生成する。
2. While 単独構成である, the CLOpts ビルド構成 shall 既定でテストをビルドし、CTest から実行できるように登録する。
3. When 単独構成でテストを実行した, the CLOpts のテスト shall 移す前の引数パーサのテストが検証していた挙動をすべて検証し、すべて成功する。
4. Where テストを無効にする指定で構成した, the CLOpts ビルド構成 shall テストフレームワークを取得せず、テストをビルドしない。
5. Where テストフレームワークのソースの場所を構成時に指定した, the CLOpts ビルド構成 shall ネットワークから取得せずに、指定された場所のソースを使う。
6. The CLOpts ビルド構成 shall Windows（MSVC、MinGW-w64）、macOS（Apple Clang）、Linux（GCC、Clang）で、単独構成のビルドとテストができる。
7. The CLOpts ビルド構成 shall CLOpts のソースを、Monde 本体のモジュールと同じ警告の水準で、警告をエラーとしてビルドする。
8. The CLOpts ビルド構成 shall C99 の範囲で書かれたソースを、対応するすべてのコンパイラでビルドできる。

### Requirement 3: Monde の共通部品に頼らない公開インターフェース
**Objective:** As a CLOpts を使う開発者, I want CLOpts を使うために Monde の部品を必要としたくない, so that Monde と無関係のプログラムでも CLOpts だけをリンクして使える

#### Acceptance Criteria
1. The CLOpts shall 公開ヘッダと実装のどちらでも、Monde の共通部品（foundation）とそのほかの Monde のモジュールのヘッダを取り込まない。
2. The CLOpts shall C の標準ライブラリと、対応する OS が標準で提供するライブラリだけにリンクする。
3. The CLOpts shall 解析、引数列の取得、宣言の検証で起きたエラーを、CLOpts 自身が定義する手段で利用側プロジェクトに返す。
4. When エラーを返した, the CLOpts shall エラーごとのメッセージの文言と、複数のエラーの順序を、移す前の引数パーサが診断として返していたものと同じにする。
5. If エラーを記録するためのメモリを確保できなかった, then the CLOpts shall 記録できなかったエラーがあることを利用側プロジェクトが判別できるようにする。
6. The CLOpts shall ヘルプ、バージョン、使い方の案内、コマンドの経路の文字列を、CLOpts 自身が定義する手段で利用側プロジェクトに渡す。
7. When ヘルプ、バージョン、使い方の案内、コマンドの経路の文字列を生成した, the CLOpts shall その内容を、移す前の引数パーサが同じ宣言と同じ引数列から生成していたものとバイト単位で同じにする。
8. The CLOpts shall エラーを標準エラー出力へ直接書かず、プロセスを終了させない。
9. The CLOpts shall 移す前の引数パーサが提供していた機能（宣言、解析、解析結果の参照、ヘルプなどの生成、UTF-8 の引数列の取得）をすべて提供する。

### Requirement 4: 公開名と公開ヘッダ
**Objective:** As a CLOpts を使う開発者, I want 公開名とヘッダがライブラリ名に沿っていてほしい, so that 切り離しの後も利用側のコードを書き換えずに使い続けられる

#### Acceptance Criteria
1. The CLOpts shall 公開する関数と型の名前に接頭辞 `clopts_` を、公開するマクロと列挙値の名前に接頭辞 `CLOPTS_` を付ける。
2. The CLOpts shall 接頭辞 `mnd_` または `MND_` で始まる名前を、公開ヘッダでも、ライブラリが外部に公開するシンボルでも使わない。
3. The CLOpts shall 公開ヘッダを、利用側プロジェクトが `clopts/` で始まるパスで取り込める形で提供する。
4. The CLOpts shall 公開ヘッダだけを利用側プロジェクトに見せ、内部用のヘッダをインストール先に置かない。
5. When 利用側プロジェクトが CLOpts と Monde の共通部品の両方にリンクした, the CLOpts shall シンボルの重複によるリンクエラーを起こさない。
6. The CLOpts shall 公開ヘッダを、C++ のソースから取り込んでもコンパイルできる形で提供する。

### Requirement 5: 取り込み構成での利用
**Objective:** As a 利用側プロジェクトの開発者, I want CLOpts のディレクトリを自分のビルドに取り込んで使いたい, so that インストールの手順なしに CLOpts をリンクできる

#### Acceptance Criteria
1. When 利用側プロジェクトが CLOpts ディレクトリを自分のビルドに取り込んだ, the CLOpts ビルド構成 shall ターゲット `CLOpts::clopts` を利用側プロジェクトに提供する。
2. When 利用側プロジェクトのターゲットが `CLOpts::clopts` にリンクした, the CLOpts ビルド構成 shall 公開ヘッダの場所と、CLOpts が必要とする OS のライブラリを、追加の指定なしにそのターゲットへ伝える。
3. While 取り込み構成である, the CLOpts ビルド構成 shall 既定では CLOpts のテストをビルドせず、テストフレームワークを取得しない。
4. While 取り込み構成である, the CLOpts ビルド構成 shall 既定では CLOpts のインストールの規則を利用側プロジェクトのインストールに加えない。
5. While 取り込み構成である, the CLOpts ビルド構成 shall CLOpts の警告の指定と言語規格の指定を、利用側プロジェクトのターゲットに適用しない。
6. While 取り込み構成である, the CLOpts ビルド構成 shall 利用側プロジェクトの全体に効くビルドの設定を変更しない。
7. Where 利用側プロジェクトが CLOpts のテストを有効にする指定をした, the CLOpts ビルド構成 shall 取り込み構成でも CLOpts のテストをビルドし、CTest に登録する。
8. If 利用側プロジェクトが CLOpts と同じテストフレームワークをすでに取り込んでいる, then the CLOpts ビルド構成 shall テストフレームワークを重ねて取得も定義もせず、すでにあるものを使う。

### Requirement 6: インストールと find_package での利用
**Objective:** As a 利用側プロジェクトの開発者, I want インストールした CLOpts を `find_package` で見つけて使いたい, so that CLOpts のソースを自分のプロジェクトに含めずにリンクできる

#### Acceptance Criteria
1. When 単独構成でビルドした CLOpts をインストールした, the CLOpts ビルド構成 shall 静的ライブラリ、公開ヘッダ、CMake のパッケージ構成ファイル、ライセンスの全文をインストール先に置く。
2. When 利用側プロジェクトがインストール先を探索の対象にして `find_package(CLOpts)` を呼んだ, the CLOpts のパッケージ shall ターゲット `CLOpts::clopts` を提供する。
3. When 利用側プロジェクトのターゲットがインストールした `CLOpts::clopts` にリンクした, the CLOpts のパッケージ shall 公開ヘッダの場所と、CLOpts が必要とする OS のライブラリを、追加の指定なしにそのターゲットへ伝える。
4. When 利用側プロジェクトが `find_package(CLOpts <version>)` でバージョンを指定した, the CLOpts のパッケージ shall CLOpts のバージョン番号と照合し、合わなければ見つからなかったものとして扱う。
5. The CLOpts のパッケージ shall インストール先のディレクトリを別の場所に移しても使える。
6. The CLOpts ビルド構成 shall テスト、内部用のヘッダ、テストフレームワークをインストール先に置かない。
7. The CLOpts のテスト shall インストールした CLOpts を `find_package(CLOpts)` で見つけ、`CLOpts::clopts` にリンクした小さなプログラムをビルドして実行できることを検証する。
8. The CLOpts ビルド構成 shall CLOpts のバージョン番号を 1 か所だけで定義し、パッケージのバージョンにその値を使う。

### Requirement 7: Monde 本体からの利用
**Objective:** As a Monde の開発者, I want Monde が CLOpts を外部のライブラリと同じ形で使うようにしたい, so that 切り離しの後は取り込み方を差し替えるだけで済む

#### Acceptance Criteria
1. The Monde ビルド構成 shall CLOpts ディレクトリを取り込み、Monde のターゲットを `CLOpts::clopts` にリンクする。
2. The Monde ビルド構成 shall CLOpts の取り込みを 1 か所だけに書き、その 1 か所を差し替えれば別の入手方法に変えられるようにする。
3. The Monde 本体 shall CLOpts の公開ヘッダと公開名だけを使い、CLOpts の内部用のヘッダを取り込まない。
4. When CLOpts がエラーを返した, the Monde 本体 shall そのエラーを Monde の診断に変換し、ほかの診断と同じ経路で表示する。
5. When `monde` を任意の引数列で実行した, the monde shall 標準出力、標準エラー出力、終了コードを、本 spec の適用前と同じにする。
6. When Monde 本体を最上位のプロジェクトとしてテストを有効にして構成した, the Monde ビルド構成 shall CLOpts のテストも CTest に登録する。
7. When Monde 本体のテストをすべて実行した, the Monde のテスト shall CommonMark のラウンドトリップの受け入れテストとドキュメントの照合テストを含めて、すべて成功する。
8. When Monde の配布物を作成した, the Monde ビルド構成 shall 配布物の内容を本 spec の適用前と同じにし、CLOpts のヘッダ、ライブラリ、パッケージ構成ファイルを含めない。
9. The Monde ビルド構成 shall テストフレームワークを 1 回だけ取得し、Monde のテストと CLOpts のテストで同じものを使う。

### Requirement 8: 継続的な検証
**Objective:** As a Monde と CLOpts の保守者, I want CLOpts が単独で成り立つことを変更のたびに確かめたい, so that 気づかないうちに CLOpts ディレクトリの外への依存が入り込むのを防げる

#### Acceptance Criteria
1. When main への push または pull request があった, the CI shall CLOpts ディレクトリの単独構成でのビルドとテストを、Monde 本体が対応するすべての環境で実行する。
2. When main への push または pull request があった, the CI shall CLOpts のインストールと、インストール先からの `find_package(CLOpts)` による利用の検証を、少なくとも 1 つの環境で実行する。
3. The CI shall 単独構成の検証を、CLOpts ディレクトリの外のファイルがなくても成功する形で実行する。
4. If CLOpts の単独構成のビルド、テスト、インストール後の利用の検証のいずれかが失敗した, then the CI shall その実行を失敗として報告する。
5. The CI shall Monde 本体のビルドとテストの検証を、これまでと同じ環境で引き続き実行する。

### Requirement 9: ドキュメントとプロジェクトの記録
**Objective:** As a CLOpts を使う開発者と Monde の保守者, I want CLOpts の使い方と新しい配置が文書に書かれていてほしい, so that 取り込み方と、どこに何があるかを迷わずに把握できる

#### Acceptance Criteria
1. The CLOpts の README shall CLOpts の概要、対応する環境、単独構成でのビルドとテストの手順、取り込み構成での使い方、インストールして `find_package(CLOpts)` で使う方法、最小の使用例、ライセンスを日本語で説明する。
2. The CLOpts の README shall CLOpts ディレクトリの外にあるファイルを参照しない。
3. The CLOpts ディレクトリ shall Monde と同じライセンスの全文を持つ。
4. The Monde のドキュメント shall 引数パーサの場所、ターゲット名、公開名についての記述を、本 spec の適用後の状態に合わせる。
5. The steering shall モジュールの配置、命名の規約、依存の向き、エラーの返し方の規約について、CLOpts を「リポジトリ内にある独立したライブラリ」として扱う例外を記述する。
6. The 本 spec の設計 shall 移す前の公開名、ヘッダ、ターゲット名と、移した後のそれぞれとの対応を示す。
