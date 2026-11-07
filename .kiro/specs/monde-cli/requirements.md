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

## Introduction
monde-cli は、これまでの spec で作った部品を 1 つの実行ファイル `monde` に統合する最上位の層である。`extract` / `init` / `update` / `translate` の 4 つのサブコマンドで、新規翻訳のフロー（extract → init → 翻訳 → translate）と原文更新のフロー（extract → update → 翻訳 → translate）を実行できるようにする。本 spec が担うのは、サブコマンドとオプションの定義、ファイルと標準入出力の読み書き、部品の呼び出し、統計と診断の表示、終了コードの決定、そして CommonMark spec のラウンドトリップと `msgfmt --check` による受け入れ検証である。翻訳単位の抽出と再構成、PO の読み書き、マージの論理は部品が担い、本 spec では再定義しない。利用者は翻訳者と文書の保守者で、CLI のメッセージはすべて英語である。

### 要件確認で決めた挙動
- 入力は位置引数、出力は `-o/--output` で指定する。`-o` を省略すると標準出力に書く。入力と `-o` の値に `-` を指定すると標準入力・標準出力を使う（foundation に標準入出力の読み書きを追加する）。
- 統計は既定で標準エラー出力に表示し、`-q/--quiet` で抑止する。`translate` は翻訳率、`update` はマージ結果、`extract` と `init` はエントリ数を表示する。
- 終了コードは 0（成功。警告のみの場合を含む）、1（入力の内容やファイル入出力のエラー）、2（使い方のエラー）の 3 段階とする。
- 参照コメント `#:` のファイル名は、コマンドラインで指定したパスをそのまま使う。標準入力のときは `-` とする。
- `init` の言語コードは `-l/--language` → 環境変数（`LC_ALL` → `LC_MESSAGES` → `LANG`）→ 使い方のエラーの順で決める。環境変数の参照は、この既定値に限って「コマンドライン引数のみ」の制約の例外とする。出力ファイル名からの推定はしない。
- `extract` は `--project-id-version` と `--msgid-bugs-address` で POT ヘッダの値を指定できる。省略時は xgettext と同じ既定値とする。
- BOM は `translate` だけが入力 `.md` に合わせて出力に付ける。`.pot` / `.po` は常に BOM なしで書く。
- fuzzy に関するオプション名は gettext に合わせ、`update` は `--no-fuzzy-matching`、`translate` は `--use-fuzzy` とする。

## Boundary Context
- **In scope**:
  - `monde` コマンドのサブコマンド・オプション・位置引数の定義と、ヘルプ・バージョンの表示
  - ファイルと標準入出力の読み書き、BOM と改行コードの扱いの決定、出力先の決定
  - 4 つのサブコマンドのハンドラ（部品の呼び出し、ヘッダに書く日時と言語の決定、オプションの受け渡し）
  - 統計の表示、診断（エラー・警告）の表示形式、終了コード
  - CommonMark spec 0.31.2 のラウンドトリップによる受け入れテストと、`msgfmt --check` による互換性の検証
  - 製品の実行ファイル `monde` のビルド構成
- **Out of scope**:
  - 翻訳単位の抽出と再構成の規則（translation-units）、PO の書式とマージの論理（po-format、po-merge）、引数の構文解析（cli-args）
  - 複数ファイルやディレクトリの一括処理、設定ファイル、`init` の言語の既定値以外の環境変数の参照
  - 翻訳率による出力の制御（表示のみ）
  - 日本語のドキュメント（README、利用マニュアル）と GitHub Releases への配布ワークフロー（docs-and-release）
  - Markdown の方言の選択（初版は CommonMark のみ。方言のオプションは次の版）
- **Adjacent expectations**:
  - cli-args は、宣言に基づく解析結果、ヘルプと使い方の文面、使い方のエラーの診断を返す。表示と終了コードは本 spec が決める。
  - po-format は、メモリ上のバイト列からの読み込み、`init` 相当のヘッダの更新、改行コードと折り返しを指定した書き出しを提供する。日時の文字列と言語コードは本 spec が渡す。
  - po-merge は、2 つのカタログからマージ結果と件数（一致・fuzzy・新規・廃止）を返す。あいまい一致の有効・無効は本 spec が渡す。
  - translation-units は、原文バッファからエントリの列を返し、原文とカタログから出力バッファと状態ごとの件数（翻訳済み・fuzzy・未翻訳・カタログにない）を返す。
  - foundation は、ファイルの読み書き（BOM の検出と除去、UTF-8 の検証）、改行コードの検出、診断の一覧を提供する。標準入出力の読み書きは本 spec の実装に合わせて foundation に追加する。
  - いずれの部品も、標準出力・標準エラー出力への書き込みとプロセスの終了を行わず、診断として本 spec に返す。

## Requirements

### Requirement 1: コマンド体系とヘルプ
**Objective:** As a 翻訳者, I want 1 つのコマンド `monde` から 4 つの操作を呼び出し、使い方をコマンド自身から確認したい, so that マニュアルを開かずに翻訳のフローを実行できる

#### Acceptance Criteria
1. The monde CLI shall `extract`、`init`、`update`、`translate` の 4 つのサブコマンドを提供する。
2. When `monde --help`、`monde -h`、または `monde help` が実行される, the monde CLI shall 全体の使い方（各サブコマンドの名前と説明の一覧を含む）を英語で標準出力に書き、終了コード 0 で終了する。
3. When `monde <subcommand> --help`、`monde <subcommand> -h`、または `monde help <subcommand>` が実行される, the monde CLI shall そのサブコマンドの使い方（位置引数とすべてのオプションの説明を含む）を英語で標準出力に書き、終了コード 0 で終了する。
4. When `monde --version` が実行される, the monde CLI shall プログラム名とビルドに埋め込まれたバージョン文字列からなる 1 行を標準出力に書き、終了コード 0 で終了する。
5. If 引数なしで実行される, then the monde CLI shall 全体の使い方を標準エラー出力に書き、終了コード 2 で終了する。
6. If 未知のサブコマンド、未知のオプション、値の欠落、位置引数の過不足、不正なリスト値など、引数の誤りがある, then the monde CLI shall 誤りを示す英語のメッセージと、該当するヘルプの表示方法を示す 1 行を標準エラー出力に書き、終了コード 2 で終了する。
7. The monde CLI shall すべてのサブコマンドで、オプションと位置引数をどの順序で指定しても同じ結果を返す。
8. While Windows 上で動作している, the monde CLI shall 非 ASCII の文字を含むパスやオプションの値を UTF-8 として受け取り、他の OS と同じように扱う。

### Requirement 2: 入出力の指定
**Objective:** As a 翻訳者, I want 入力をファイルで指定し、出力先をファイルか標準出力から選びたい, so that 単独でもシェルのパイプと組み合わせても使える

#### Acceptance Criteria
1. The monde CLI shall 各サブコマンドの入力ファイルを位置引数で受け取る（`extract` は `.md` を 1 つ、`init` は `.pot` を 1 つ、`update` は `.pot` と既存の `.po` をこの順に 2 つ、`translate` は `.md` と `.po` をこの順に 2 つ）。
2. The monde CLI shall 各サブコマンドで、出力先を `-o <path>` または `--output <path>` で受け取る。
3. When `-o/--output` が省略される, the monde CLI shall 結果を標準出力に書く。
4. When 位置引数または `-o/--output` の値がちょうど `-` である, the monde CLI shall その入力を標準入力から読み、またはその出力を標準出力に書く。
5. If 同じサブコマンドで 2 つ以上の入力に `-` が指定される, then the monde CLI shall 標準入力を 2 回は読めないことを示す使い方のエラーを報告し、終了コード 2 で終了する。
6. When 出力先のファイルがすでに存在する, the monde CLI shall 確認を求めずにその内容を置き換える。
7. When 出力先が入力ファイルのいずれかと同じパスである（`update` で既存の `.po` を上書きする場合など）, the monde CLI shall すべての入力を読み終えてから出力を書き、入力の内容を失わずに結果を書き込む。
8. The monde CLI shall 処理がエラーで失敗したときに出力先のファイルを作成も変更もせず、処理が成功した場合にだけ出力を書く。
9. If 入力ファイルが存在しない、読めない、または出力先に書けない, then the monde CLI shall そのパスと原因を示すエラーを報告し、終了コード 1 で終了する。
10. The monde CLI shall 標準出力への結果の書き込みと、標準エラー出力への統計・診断の書き込みを混ぜず、統計と診断を標準出力に書かない。

### Requirement 3: 文字コード、改行コード、BOM
**Objective:** As a 文書の保守者, I want 入力の改行コードと BOM が出力で勝手に変わらないこと, so that バージョン管理の差分に不要な変更が出ない

#### Acceptance Criteria
1. The monde CLI shall すべての入力を UTF-8 として読み、先頭の BOM があれば取り除いてから処理する。
2. If 入力が正しい UTF-8 でない, then the monde CLI shall そのパスと位置を示すエラーを報告し、終了コード 1 で終了する。
3. The monde CLI shall 各サブコマンドの出力の改行コードを、主となる入力（`extract` と `translate` は `.md`、`init` は `.pot`、`update` は既存の `.po`）の改行コードに合わせる。
4. When 主となる入力に LF と CRLF が混在する, the monde CLI shall 多いほうの改行コード（同数なら先に現れたほう）を出力の改行コードにする。
5. When 主となる入力に改行がない, the monde CLI shall 出力の改行コードを LF にする。
6. When `translate` の入力 `.md` に BOM がある, the monde CLI shall 出力の翻訳済み Markdown の先頭にも BOM を付ける。
7. The monde CLI shall `extract`、`init`、`update` の出力（`.pot` / `.po`）を、入力の BOM の有無にかかわらず BOM なしで書く。
8. The monde CLI shall 標準入力からの読み込みと標準出力への書き込みで、改行コードの自動変換を行わず、バイト列をそのまま扱う。

### Requirement 4: `monde extract`（翻訳テンプレートの生成）
**Objective:** As a 翻訳者, I want 原文 Markdown から翻訳テンプレートを作りたい, so that 新規翻訳と原文更新のどちらのフローも同じテンプレートから始められる

#### Acceptance Criteria
1. When `monde extract <input.md>` が実行される, the monde CLI shall 原文から抽出した翻訳単位のエントリの列にヘッダエントリを付けた翻訳テンプレートを出力する。
2. The monde CLI shall ヘッダの `POT-Creation-Date` に、実行時のローカル日時とタイムゾーンのオフセットを gettext と同じ書式（`YYYY-MM-DD HH:MM+ZZZZ`）で書く。
3. Where `--project-id-version <value>` が指定されている, the monde CLI shall ヘッダの `Project-Id-Version` にその値を書き、省略時は `PACKAGE VERSION` を書く。
4. Where `--msgid-bugs-address <value>` が指定されている, the monde CLI shall ヘッダの `Report-Msgid-Bugs-To` にその値を書き、省略時は空にする。
5. Where `--front-matter-keys <key[,key...]>` が指定されている, the monde CLI shall 指定したキーパスをそのまま翻訳単位の抽出に渡し、一致したフロントマターの値を翻訳単位に含める。
6. If `--front-matter-keys` が省略される, then the monde CLI shall フロントマターの値を翻訳単位に含めない。
7. The monde CLI shall 参照コメント `#:` のファイル名に、コマンドラインで指定した入力のパスをそのまま（区切り文字も変換せずに）使い、標準入力のときは `-` を使う。
8. Where `--no-wrap` が指定されている, the monde CLI shall 出力の長い行を折り返さない。
9. If 指定したキーパスの書式が不正、またはフロントマターの YAML を解析できない, then the monde CLI shall エラーを報告し、終了コード 1 で終了する。
10. When 翻訳単位が 1 つもない, the monde CLI shall ヘッダエントリだけの翻訳テンプレートを出力し、終了コード 0 で終了する。

### Requirement 5: `monde init`（翻訳ファイルの新規作成）
**Objective:** As a 翻訳者, I want 翻訳テンプレートから言語ごとの翻訳ファイルを作りたい, so that PO 対応のツールですぐに翻訳を始められる

#### Acceptance Criteria
1. When `monde init <template.pot>` が実行される, the monde CLI shall テンプレートの全エントリを msgstr が空のまま引き継ぎ、ヘッダの `Language` と `PO-Revision-Date` を設定し、ヘッダの fuzzy を外した翻訳ファイルを出力する。
2. Where `-l <code>` または `--language <code>` が指定されている, the monde CLI shall その値をそのままヘッダの `Language` に書く。
3. If `-l/--language` が省略される, then the monde CLI shall 環境変数 `LC_ALL`、`LC_MESSAGES`、`LANG` をこの順に見て、最初に空でない値から `.` 以降と `@` 以降を除いた部分（例: `ja_JP.UTF-8` → `ja_JP`）を言語コードにする。
4. The monde CLI shall 環境変数の値が `C` または `POSIX` のときは未設定として扱い、次の環境変数を見る。
5. If `-l/--language` が省略され、環境変数からも言語コードを決められない, then the monde CLI shall `--language` の指定を求める使い方のエラーを報告し、終了コード 2 で終了する。
6. When 言語コードを環境変数から決めた, the monde CLI shall どの環境変数からどの値を使ったかを標準エラー出力に 1 行で示す（`--quiet` で抑止できる）。
7. If 言語コードが空、または空白か制御文字を含む, then the monde CLI shall その値を示す使い方のエラーを報告し、終了コード 2 で終了する。
8. The monde CLI shall ヘッダの `PO-Revision-Date` に、実行時のローカル日時とタイムゾーンのオフセットを gettext と同じ書式で書く。
9. Where `--no-wrap` が指定されている, the monde CLI shall 出力の長い行を折り返さない。
10. If テンプレートが PO として読めない（構文エラー、UTF-8 以外の charset、重複するエントリなど）, then the monde CLI shall 報告されたすべてのエラーを表示し、終了コード 1 で終了する。

### Requirement 6: `monde update`（既存の翻訳ファイルのマージ）
**Objective:** As a 翻訳者, I want 更新後のテンプレートに既存の翻訳を引き継ぎたい, so that 変更された箇所だけを訳し直せる

#### Acceptance Criteria
1. When `monde update <new.pot> <old.po>` が実行される, the monde CLI shall 新しいテンプレートと既存の翻訳ファイルを `msgmerge --previous` と同等の規則でマージした翻訳ファイルを出力する。
2. The monde CLI shall 既定であいまい一致を有効にし、似た msgid の訳を fuzzy と前回の msgid 付きで引き継ぐ。
3. Where `--no-fuzzy-matching` が指定されている, the monde CLI shall あいまい一致を行わず、完全一致だけで訳を引き継ぐ。
4. The monde CLI shall 既存の翻訳ファイルのヘッダ（`Language`、`PO-Revision-Date`、`Last-Translator` など）を引き継ぎ、`PO-Revision-Date` を書き換えない。
5. Where `--no-wrap` が指定されている, the monde CLI shall 出力の長い行を折り返さない。
6. If テンプレートまたは既存の翻訳ファイルが PO として読めない、または重複するエントリを含む, then the monde CLI shall 報告されたすべてのエラーをどちらのファイルかが分かる形で表示し、終了コード 1 で終了する。
7. When 出力先が既存の翻訳ファイルと同じパスである, the monde CLI shall Requirement 2.7 と 2.8 に従い、マージに失敗したときに既存の翻訳ファイルを変更しない。

### Requirement 7: `monde translate`（翻訳済み Markdown の生成）
**Objective:** As a 文書の保守者, I want 原文と翻訳ファイルから翻訳済み Markdown を作りたい, so that 原文の書式を保ったまま翻訳版を公開・管理できる

#### Acceptance Criteria
1. When `monde translate <input.md> <catalog.po>` が実行される, the monde CLI shall 翻訳対象の範囲だけを訳文に置き換え、それ以外を原文のバイト列のまま保った Markdown を出力する。
2. The monde CLI shall 既定で、未翻訳のエントリと fuzzy のエントリに対応する箇所を原文のまま出力する。
3. Where `--use-fuzzy` が指定されている, the monde CLI shall fuzzy のエントリの msgstr（空でないもの）を訳文として採用する。
4. Where `--front-matter-keys <key[,key...]>` が指定されている, the monde CLI shall 指定したキーパスに一致するフロントマターの値も訳文に置き換える。
5. The monde CLI shall `extract` と同じキーパスの指定と同じ原文に対して、`extract` が生成した msgid を持つエントリをすべて置き換えの対象として見つける。
6. When 翻訳カタログが空、または `extract` の出力をそのまま（msgstr が空のまま）渡した, the monde CLI shall 原文とバイト単位で同一の出力を書く。
7. When 訳文が空行やブロックを開始し得る記号を含むなど、構造を変え得る内容である, the monde CLI shall 警告を標準エラー出力に表示したうえで出力を書き、終了コード 0 で終了する。
8. If 翻訳カタログが PO として読めない、または訳文が正しい UTF-8 でない, then the monde CLI shall 報告されたすべてのエラーを表示し、出力を書かずに終了コード 1 で終了する。
9. The monde CLI shall 翻訳カタログの廃止エントリと msgctxt 付きのエントリを置き換えに使わない。

### Requirement 8: 統計の表示
**Objective:** As a 翻訳者, I want 各操作の結果の件数と翻訳率を知りたい, so that 翻訳の進み具合と原文更新の影響を把握できる

#### Acceptance Criteria
1. When `extract` が成功する, the monde CLI shall 抽出したエントリの件数を標準エラー出力に 1 行で表示する。
2. When `init` が成功する, the monde CLI shall 作成した翻訳ファイルのエントリの件数と言語コードを標準エラー出力に 1 行で表示する。
3. When `update` が成功する, the monde CLI shall 完全一致・fuzzy・新規・廃止の件数を標準エラー出力に 1 行で表示する。
4. When `translate` が成功する, the monde CLI shall 文書内の翻訳セグメントの出現を数えた翻訳済み・fuzzy・未翻訳・カタログにない件数と合計、および翻訳率を標準エラー出力に表示する。
5. The monde CLI shall 翻訳率を「翻訳済みの件数 ÷ 合計」の百分率として、小数点以下 1 桁で表示する。fuzzy は `--use-fuzzy` の有無にかかわらず翻訳済みに数えない。
6. If `translate` の合計が 0 である, then the monde CLI shall 翻訳率を表示せず、翻訳対象がないことを示す。
7. Where `-q` または `--quiet` が指定されている, the monde CLI shall 統計と情報の表示（Requirement 5.6 を含む）を抑止し、警告とエラーの表示は抑止しない。
8. The monde CLI shall 統計の文面を英語とし、件数と率を機械的に取り出せる固定の書式にする。

### Requirement 9: 診断の表示と終了コード
**Objective:** As a 翻訳者, I want エラーと警告の原因と場所を英語のメッセージで知り、スクリプトから成否を判定したい, so that 修正すべき箇所がすぐ分かり、自動化にも組み込める

#### Acceptance Criteria
1. The monde CLI shall エラーと警告を標準エラー出力に、1 件につき 1 行で表示する。
2. When 診断にパスと 1 起点の行番号がある, the monde CLI shall `<path>:<line>: error: <message>` または `<path>:<line>: warning: <message>` の形で表示する。
3. When 診断に行番号がない（0）, the monde CLI shall `<path>: error: <message>` の形で表示し、パスもない場合は `monde: error: <message>` の形で表示する。
4. When 部品が複数の診断を返す, the monde CLI shall 返された順にすべての診断を表示する。
5. The monde CLI shall 処理が成功したとき（警告のみの場合を含む）に終了コード 0 で終了する。
6. The monde CLI shall 入力の内容のエラー、ファイル入出力のエラー、メモリの確保の失敗で終了コード 1 で終了する。
7. The monde CLI shall 引数の誤り（Requirement 1.5、1.6、2.5、5.5、5.7）で終了コード 2 で終了する。
8. The monde CLI shall エラーで終了するとき、標準出力に結果の一部を書かない。
9. The monde CLI shall すべてのメッセージを英語で表示する。

### Requirement 10: CommonMark spec によるラウンドトリップの受け入れテスト
**Objective:** As a Monde の開発者, I want 実行ファイルを通したラウンドトリップを spec の全テストケースで自動検証したい, so that 書式のバイト単位の保持を統合後も機械的に保証できる

#### Acceptance Criteria
1. The monde CLI shall CommonMark spec 0.31.2（`tests/data/commonmark/spec.txt`）のすべてのテストケースの入力について、ビルドした実行ファイルで `extract` し、その出力をカタログとして `translate` した結果が入力とバイト単位で一致することをテストで検証する。
2. The monde CLI shall 同じテストケースについて、すべての msgstr を msgid と同じ内容にしたカタログで `translate` した結果も入力とバイト単位で一致することを検証する。
3. The monde CLI shall 同じテストケースの入力を CRLF に変換したものについても、10.1 を満たすことを検証する。
4. The monde CLI shall ラウンドトリップのテストを CTest から実行でき、失敗したときにどのテストケース（spec の例の番号）が失敗したかを出力で示す。
5. The monde CLI shall spec.txt を無改変のまま使い、実行ファイルとリリース成果物に spec.txt とその内容を含めない。
6. The monde CLI shall 4 つのサブコマンドの主要な振る舞い（Requirement 1〜9）を、ビルドした実行ファイルを起動して標準入出力と終了コードを確認する形でテストする。

### Requirement 11: gettext との互換性の検証
**Objective:** As a 翻訳者, I want `monde` が生成した `.pot` / `.po` を既存の gettext ツールで扱いたい, so that 使い慣れた翻訳プラットフォームやエディタをそのまま使える

#### Acceptance Criteria
1. The monde CLI shall `extract`、`init`、`update` が生成するファイルが GNU gettext の `msgfmt --check` を通ることをテストで検証する。
2. Where テスト環境に `msgfmt` がある, the monde CLI shall 11.1 の検証を CTest のテストとして登録する。
3. If テスト環境に `msgfmt` がない, then the monde CLI shall 11.1 の検証を登録せず、構成時にその旨を表示する。
4. The monde CLI shall CI の少なくとも Ubuntu の環境で、`msgfmt` を導入したうえで 11.1 の検証を実行する。

### Requirement 12: 実行ファイルのビルド
**Objective:** As a Monde の開発者, I want 各 OS で同じ手順で `monde` の実行ファイルをビルドしたい, so that docs-and-release がそのままバイナリを配布できる

#### Acceptance Criteria
1. The monde CLI shall 共通のビルド手順（構成・ビルド・テストの 3 行）で、`monde` という名前の実行ファイルをビルドできる。
2. The monde CLI shall CI の全環境（Windows の MSVC と MinGW-w64、macOS、Linux の GCC と Clang）で実行ファイルのビルドとテストが通る。
3. The monde CLI shall 実行ファイルが外部の実行時ライブラリに依存せず、単体で実行できる。
4. The monde CLI shall `--version` で表示するバージョン文字列をビルド構成から 1 か所で決め、ソースに重複して書かない。
