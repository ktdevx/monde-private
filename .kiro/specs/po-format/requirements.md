# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者（とくに po-merge、translation-units、monde-cli を実装する開発者）。Monde は翻訳テンプレート（`.pot`）と翻訳ファイル（`.po`）を読み書きする必要があり、生成するファイルは GNU gettext のツールや既存の翻訳プラットフォームと互換でなければならない。外部ライブラリ（libgettextpo など）は使えない。

**現状**: PO を扱う部品はまだない。

**何を変えるか**:
- `.po` / `.pot` を読み込んでエントリの列に変換し、そのエントリから書き出せる再利用部品を用意する。書き出したファイルは `msgfmt --check` を通り、一般的な PO ファイルを読んで書き直しても意味が変わらない。
- PO のエントリ（msgctxt、msgid、msgstr、各種コメント、フラグ、廃止エントリ）をデータモデルとして定義し、字句解析と構文解析を行うリーダーと、gettext の慣例に沿って整形するライターを実装する。
- 対象範囲:
  - ヘッダエントリ（Content-Type、Language など）の解析と生成
  - コメントの読み書き: 翻訳者コメント `#`、抽出コメント `#.`、参照 `#:`、フラグ `#,`、前回の msgid `#|`
  - fuzzy フラグ、廃止エントリ `#~`、msgctxt
  - 文字列のエスケープとアンエスケープ、複数行の文字列の連結と折り返し
  - 複数形エントリ（`msgid_plural` / `msgstr[n]`）を読み込んでもエラーにせず、保持して書き戻す
  - 解析エラーの位置（行番号）の報告
  - `.pot` から `.po` を新規作成する初期化処理（ヘッダの Language の設定など、`msginit` 相当の最小限）
- 範囲外: マージ処理（あいまい一致を含む。po-merge が担う）、Markdown から msgid を作る処理（translation-units が担う）、`.mo` のコンパイル。

**制約**: UTF-8 のみ。互換性は GNU gettext の `msgfmt --check` で確認する。出力は gettext ツールの出力形式（行の折り返しなど）に近づけ、差分を読みやすくする。上流は project-foundation、データモデルは po-merge と共有する。

## Introduction
po-format は、GNU gettext の翻訳テンプレート（`.pot`）と翻訳ファイル（`.po`）を読み書きする汎用の部品である。利用者は Monde の開発者（po-merge、translation-units、monde-cli を実装する開発者）で、外部ライブラリを使わずに、PO の内容をエントリの列として読み込み、加工し、gettext のツールと互換な形式で書き出せるようにする。あわせて、テンプレートから翻訳ファイルを新規作成する最小限の初期化処理を提供する。本部品は Monde 固有の知識を持たず、単体テストで挙動を検証できる。

### 要件確認で決めた挙動
- 書き出しでは、既定で GNU gettext と同じく 79 桁で文字列を折り返す。呼び出し側は「折り返しなし」（`\n` の後ろでだけ改行する）も指定できる。
- ラウンドトリップでは、エントリ・コメント・フラグの内容と順序を常に保持する（最初の msgstr が空のエントリの `fuzzy` は除く。下記）。さらに、gettext のツールが既定の設定で書き出したファイルを読み込んで書き直した場合は、バイト単位で同じ出力にする。
- 初期化処理は最小限とし、Language の設定、ヘッダの fuzzy の解除、PO-Revision-Date の設定（および文字コードを UTF-8 にすること）だけを行う。Last-Translator などのほかの項目はテンプレートの値のまま残し、Plural-Forms は設定しない。
- fuzzy フラグを持つエントリの最初の msgstr が空のときは、GNU gettext のツールと同じく、書き出しで `fuzzy` を書かず、状態を未翻訳とする。エントリの fuzzy フラグ自体はデータモデルに残す。
- 解析エラーがあると、読み込みは失敗になる。ただし最初のエラーで止めずに、次のエントリから解析を再開して、見つかったエラーをすべて行番号付きで報告する。

## Boundary Context
- **In scope**:
  - PO のエントリ（ヘッダ、msgctxt、msgid、msgid_plural、msgstr、msgstr[n]、各種コメント、フラグ、廃止エントリ）の表現と、その状態（翻訳済み・fuzzy・未翻訳）の判定
  - メモリ上の内容からエントリの列を読み込む処理と、エントリの列から PO の内容を書き出す処理
  - 文字列のエスケープとアンエスケープ、複数行の文字列の連結、gettext 互換の折り返し
  - ヘッダの項目の取得と設定、新しいテンプレート用ヘッダの生成
  - 解析エラーと警告の行番号付きの報告
  - テンプレートから翻訳ファイルを作る最小限の初期化
- **Out of scope**:
  - テンプレートと既存の翻訳ファイルのマージ、あいまい一致、廃止エントリ化（po-merge が担う）
  - Markdown から msgid・参照・抽出コメントを作る処理（translation-units が担う）
  - ファイルの読み書き、メッセージの表示、終了コードの決定（foundation と monde-cli が担う）
  - `.mo` のコンパイル、書式文字列（`c-format` など）の検査
  - 複数形の本格対応（言語ごとの Plural-Forms の決定、msgstr[n] の個数の検査）。複数形エントリは保持して書き戻すだけである
  - UTF-8 以外の文字コードの PO ファイル
  - `msginit` が行う翻訳者情報の推測や、言語ごとの Plural-Forms の設定
- **Adjacent expectations**:
  - po-merge と translation-units は、本部品のエントリの表現を共有し、上位で再定義しない。
  - monde-cli は、foundation の機能でファイルを読み書きし、その内容を本部品に渡す。初期化に使う言語コードと日時は monde-cli が決めて渡す。
  - 本部品は、エラーと警告を project-foundation の診断として呼び出し側に返し、自分で標準出力・標準エラー出力に書いたりプロセスを終了したりしない。

## Requirements

### Requirement 1: エントリの表現
**Objective:** As a po-merge・translation-units の開発者, I want PO の各エントリを共通の形で扱いたい, so that 読み込んだ内容を加工したり、新しいエントリを作って書き出したりできる

#### Acceptance Criteria
1. The PO データモデル shall 各エントリに、省略可能な msgctxt、msgid、省略可能な msgid_plural、msgstr（複数形エントリでは添字付きの msgstr の列）、翻訳者コメントの行の列、抽出コメントの行の列、参照の列、フラグの列、前回の msgctxt・msgid・msgid_plural、廃止エントリかどうか、入力上の開始行番号を持たせる。
2. The PO データモデル shall 参照を、ファイル名と省略可能な行番号の組として表す。
3. The PO データモデル shall 空の msgctxt と msgctxt がないことを区別して表す。
4. The PO データモデル shall msgid が空文字列で msgctxt を持たない最初の有効なエントリを、ヘッダエントリとして識別できるようにする。
5. When 呼び出し側がエントリの状態を求める, the PO データモデル shall fuzzy フラグを持ち最初の msgstr が空でないエントリを fuzzy、fuzzy フラグがなくすべての msgstr が空でないエントリを翻訳済み、それ以外（fuzzy フラグを持ち、msgstr がないか最初の msgstr が空のエントリを含む）を未翻訳として返す。
6. When 呼び出し側がフラグの追加・削除を求める, the PO データモデル shall 同じフラグを重複させずに追加し、既存のほかのフラグの順序を保つ。
7. When 呼び出し側が msgctxt と msgid の組でエントリを検索する, the PO データモデル shall 一致する廃止エントリでないエントリを返し、ない場合はないことを返す。
8. The PO データモデル shall エントリの列の順序を、読み込んだ順または呼び出し側が並べた順のまま保つ。

### Requirement 2: PO の読み込み
**Objective:** As a Monde の開発者, I want `.po` / `.pot` の内容をエントリの列に変換したい, so that 翻訳の内容をプログラムで扱える

#### Acceptance Criteria
1. When 呼び出し側が PO の内容（メモリ上のバイト列）と、診断に使うファイル名を渡す, the PO リーダー shall 内容をエントリの列に変換して返す。
2. The PO リーダー shall `#`（後ろに空白か行末が続くもの）を翻訳者コメント、`#.` を抽出コメント、`#:` を参照、`#,` をフラグ、`#|` を前回の msgctxt・msgid・msgid_plural として、それぞれのエントリに関連付ける。
3. When `#:` の行に空白で区切られた複数の参照がある, the PO リーダー shall それぞれを `ファイル名:行番号` または `ファイル名` の形の参照として順に読み込む。
4. When `#,` の行にカンマで区切られた複数のフラグがある, the PO リーダー shall 前後の空白を除いたフラグとして順に読み込む。
5. When キーワードの後ろの文字列に続いて、引用符で囲まれた文字列だけの行が続く, the PO リーダー shall それらを順に連結して 1 つの文字列にする。
6. When 行が `#~` で始まる, the PO リーダー shall その行を廃止エントリの一部として、`#~` の後ろの内容を通常のエントリと同じ規則で解析する。
7. When エントリに `msgid_plural` と `msgstr[n]` がある, the PO リーダー shall エラーにせずに、複数形エントリとして添字とともにすべての msgstr を読み込む。
8. The PO リーダー shall 改行コードとして LF と CRLF のどちらも受け付ける。
9. When 内容の先頭に UTF-8 の BOM がある, the PO リーダー shall BOM を除いて解析する。
10. When 内容が空、またはコメントと空行だけからなる, the PO リーダー shall エラーにせずに空のエントリの列を返す。
11. When ヘッダエントリがない, the PO リーダー shall エラーにせずにヘッダのないエントリの列を返す。

### Requirement 3: 文字列のエスケープ
**Objective:** As a Monde の開発者, I want PO の文字列リテラルと実際の文字列を正しく相互に変換したい, so that 改行や引用符を含むテキストを壊さずに読み書きできる

#### Acceptance Criteria
1. When 文字列リテラルに `\\`、`\"`、`\n`、`\t`、`\r`、`\a`、`\b`、`\f`、`\v` のエスケープがある, the PO リーダー shall それぞれ対応する 1 文字に変換する。
2. When 文字列リテラルに 8 進数（`\` と 1〜3 桁）または 16 進数（`\x` と 1 桁以上）のエスケープがある, the PO リーダー shall 対応するバイトに変換する。
3. If 文字列リテラルに上記以外のエスケープがある, then the PO リーダー shall そのエスケープと行番号を示すエラーを報告する。
4. When 文字列を書き出す, the PO ライター shall `\`、`"`、改行、タブ、CR、BEL、BS、FF、VT をそれぞれ `\\`、`\"`、`\n`、`\t`、`\r`、`\a`、`\b`、`\f`、`\v` として書き出し、それ以外の文字（非 ASCII 文字を含む）はそのまま書き出す。
5. The PO リーダー and PO ライター shall 任意の UTF-8 文字列について、書き出してから読み込むと元の文字列と同じになるようにする。

### Requirement 4: 解析エラーと警告の報告
**Objective:** As a 翻訳ファイルを扱う利用者, I want 壊れた PO ファイルの問題箇所をまとめて知りたい, so that 1 回の確認で全部の問題を直せる

#### Acceptance Criteria
1. The PO リーダー shall 各エラーと警告に、呼び出し側が渡したファイル名と、問題のある行の 1 から数えた行番号を含める。
2. If 構文エラーが見つかる, then the PO リーダー shall そのエラーを報告し、次のエントリの始まりから解析を再開して、残りのエラーも報告する。
3. If エラーが 1 つ以上報告された, then the PO リーダー shall 読み込みを失敗として返す。
4. If 閉じていない文字列リテラル、未知のキーワード、msgid のないエントリ、msgstr のないエントリ、キーワードの後ろの文字列の欠落、文字列リテラルの後ろの余分な文字がある, then the PO リーダー shall それぞれの問題を示すエラーを報告する。
5. If `msgid_plural` のあるエントリに添字のない `msgstr` がある、または `msgid_plural` のないエントリに `msgstr[n]` がある, then the PO リーダー shall 単数形と複数形の不一致を示すエラーを報告する。
6. If 1 つのエントリの中に廃止エントリの行（`#~`）と通常の行のキーワードが混在している, then the PO リーダー shall その混在を示すエラーを報告する。
7. If 同じ msgctxt と msgid の組を持つ廃止エントリでないエントリが複数ある, then the PO リーダー shall 重複を示すエラーを、後のエントリの行番号と、先のエントリの行番号を含めて報告する。
8. If 内容に UTF-8 として不正なバイト列がある, then the PO リーダー shall その行番号を示すエラーを報告する。
9. If ヘッダの Content-Type に UTF-8 以外の charset が指定されている（大文字と小文字を区別せず、未設定を示す `CHARSET` を除く）, then the PO リーダー shall その charset を示すエラーを報告する。
10. If 翻訳済みのエントリで、msgid と msgstr の一方だけが改行で始まる、または一方だけが改行で終わる, then the PO リーダー shall その不一致を示す警告を報告し、読み込みは失敗にしない。

### Requirement 5: PO の書き出し
**Objective:** As a Monde の開発者, I want エントリの列を gettext の慣例どおりの PO として書き出したい, so that 既存の翻訳ツールで使え、差分も読みやすい

#### Acceptance Criteria
1. When 呼び出し側がエントリの列を渡す, the PO ライター shall エントリを渡された順に、空行で区切って書き出す。
2. The PO ライター shall 各エントリのコメントを、翻訳者コメント、抽出コメント、参照、フラグ、前回の msgctxt・msgid・msgid_plural の順に書き出し、その後に msgctxt、msgid、msgid_plural、msgstr（複数形では添字の順の msgstr[n]）を書き出す。
3. When 廃止エントリを書き出す, the PO ライター shall GNU gettext と同じく、msgctxt・msgid・msgid_plural・msgstr の行の先頭に `#~ ` を、前回の msgctxt・msgid・msgid_plural の行の先頭に `#~| ` を付け、翻訳者コメント・抽出コメント・参照・フラグの行は通常のエントリと同じ形で書き出す。
4. The PO ライター shall 各行の改行コードを、呼び出し側が指定した LF または CRLF とし、指定がなければ LF とする。
5. If 同じ msgctxt と msgid の組を持つ廃止エントリでないエントリが複数ある, then the PO ライター shall 何も書き出さずに、重複した msgctxt と msgid を示すエラーを報告する。
6. When 呼び出し側が渡したエントリの列に、UTF-8 の charset を宣言するヘッダエントリがあり、重複がなく、翻訳済みのエントリの改行の始まりと終わりが msgid と msgstr で一致しており、複数形エントリがない（または Plural-Forms がヘッダにある）, the PO ライター shall GNU gettext の `msgfmt --check` がエラーなしで受け付ける内容を書き出す。
7. When fuzzy フラグを持つエントリの msgstr がないか最初の msgstr が空である, the PO ライター shall GNU gettext と同じく、フラグの行に `fuzzy` を書かずにほかのフラグだけを書き、ほかのフラグがなければフラグの行を書かない。この規則は廃止エントリとヘッダエントリにも当てはめる。

### Requirement 6: 文字列の折り返し
**Objective:** As a 翻訳ファイルを差分レビューする開発者, I want 長い文字列が gettext と同じ位置で折り返されてほしい, so that Monde と gettext のツールのどちらで書き出しても不要な差分が出ない

#### Acceptance Criteria
1. While 折り返しが有効である（既定）, the PO ライター shall 既定の幅（79 桁）を基準に、GNU gettext のツール（既定の設定の msgcat）と同じ位置で文字列を複数行に分けて書き出す。
2. The PO ライター shall 桁を数えるとき、GNU gettext と同じく東アジアの全角文字を 2 桁として数える。
3. When 文字列が途中に改行（`\n`）を含む, the PO ライター shall 折り返しの設定にかかわらず `\n` の直後で行を分ける。
4. When 文字列が複数行に分かれる, the PO ライター shall GNU gettext と同じく、キーワードの行には空文字列 `""` を書き、2 行目以降に文字列の各部分を書く。
5. Where 呼び出し側が折り返しなしを指定する, the PO ライター shall `\n` の直後でだけ行を分け、それ以外では行を分けない。
6. When エントリに `no-wrap` フラグがある, the PO ライター shall そのエントリの文字列を、折り返しなしの規則で書き出す。
7. While 折り返しが有効である, the PO ライター shall 参照コメント（`#:`）も GNU gettext と同じ位置で複数行に分けて書き出す。

### Requirement 7: ラウンドトリップ
**Objective:** As a Monde の開発者, I want 既存の PO ファイルを読み込んで書き直しても内容が変わらないようにしたい, so that 翻訳者の作業結果を失わずに Monde で更新できる

#### Acceptance Criteria
1. When エラーなく読み込めた PO の内容を書き出す, the PO リーダー and PO ライター shall エントリの数と順序、各エントリの msgctxt・msgid・msgid_plural・msgstr、コメント、参照、フラグ、前回の msgid、廃止エントリかどうかを保持する。ただし、Requirement 5 の受け入れ基準 7 で書き出さない `fuzzy` は保持しない。
2. When 書き出した内容をもう一度読み込む, the PO リーダー shall 書き出す前と同じエントリの列を返す。ただし、Requirement 5 の受け入れ基準 7 で書き出さなかった `fuzzy` は、読み込んだエントリにない。
3. When GNU gettext のツール（既定の設定の msgcat または msgmerge）が書き出した LF の PO ファイルを読み込み、既定の設定で書き出す, the PO リーダー and PO ライター shall 入力とバイト単位で同じ内容を出力する。
4. When 受け入れ基準 3 に該当しない入力を読み込んで書き出す, the PO ライター shall 空行の数や文字列の分け方といった整形を、入力ではなく本要件と Requirement 6 の規則に従って決める。

### Requirement 8: ヘッダの取得と設定
**Objective:** As a Monde の開発者, I want ヘッダの項目を名前で読み書きしたい, so that Language や作成日時を扱える

#### Acceptance Criteria
1. When 呼び出し側がヘッダの項目名を指定して値を求める, the PO データモデル shall ヘッダの msgstr の `名前: 値` の行から、名前を大文字と小文字を区別せずに照合して値を返し、ない場合はないことを返す。
2. When 呼び出し側が既存の項目に値を設定する, the PO データモデル shall その項目の値だけを置き換え、ほかの項目の内容と順序を保つ。
3. When 呼び出し側がヘッダにない項目に値を設定する, the PO データモデル shall その項目をヘッダの最後に追加する。
4. When 呼び出し側が新しいテンプレート用のヘッダを求める, the PO データモデル shall fuzzy フラグを持ち、Project-Id-Version、Report-Msgid-Bugs-To、POT-Creation-Date、PO-Revision-Date、Last-Translator、Language-Team、Language、MIME-Version、Content-Type（charset=UTF-8）、Content-Transfer-Encoding の各項目を GNU gettext の xgettext と同じ順序と初期値で持つヘッダエントリを返す。
5. The PO データモデル shall テンプレート用ヘッダの POT-Creation-Date と Project-Id-Version には呼び出し側が指定した値を使い、自分で現在時刻を読まない。

### Requirement 9: テンプレートからの翻訳ファイルの初期化
**Objective:** As a monde-cli の開発者, I want `.pot` の内容から特定の言語の `.po` を作りたい, so that `monde init` で翻訳者がすぐに作業を始められる

#### Acceptance Criteria
1. When 呼び出し側がテンプレートのエントリの列、言語コード、日時を渡す, the PO 初期化 shall ヘッダの Language を言語コードに、PO-Revision-Date を渡された日時に設定し、ヘッダから fuzzy フラグを外したエントリの列を返す。
2. When テンプレートのヘッダの charset が未設定（`CHARSET`）である, the PO 初期化 shall charset を UTF-8 に設定する。
3. The PO 初期化 shall ヘッダ以外のエントリと、ヘッダの上記以外の項目（Last-Translator、Language-Team など）を、テンプレートの内容のまま残し、Plural-Forms を追加しない。
4. When テンプレートにヘッダエントリがない, the PO 初期化 shall Language、PO-Revision-Date、MIME-Version、Content-Type（charset=UTF-8）、Content-Transfer-Encoding を持つ fuzzy でないヘッダエントリを先頭に追加する。
5. If 言語コードが空、または空白文字や制御文字を含む, then the PO 初期化 shall その言語コードを示すエラーを報告し、エントリの列を返さない。
6. When 初期化した結果をテンプレートの内容とともに PO ライターで書き出す, the PO 初期化 shall GNU gettext の `msgfmt --check` がエラーなしで受け付ける内容になるようにする（既定値のまま残ったヘッダ項目についての警告は許容する）。

### Requirement 10: 再利用できる部品としての独立性
**Objective:** As a Monde の開発者, I want PO の読み書きを Monde 固有の処理から切り離したい, so that 単体でテストでき、ほかの部品から再利用できる

#### Acceptance Criteria
1. The po-format 部品 shall Markdown や Monde のサブコマンドについての知識を持たず、PO の内容とエントリの列だけを入出力とする。
2. The po-format 部品 shall ファイルの読み書き、標準出力・標準エラー出力への書き込み、プロセスの終了を行わず、エラーと警告を project-foundation の診断として呼び出し側に返す。
3. The po-format 部品 shall 外部ライブラリを使わずに、すべての対象環境で同じ結果を返す。
