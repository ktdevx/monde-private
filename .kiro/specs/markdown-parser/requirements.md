# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者（とくに translation-units と、次の版で GFM 対応を実装する開発者）。Monde は Markdown 文書から翻訳すべきブロックを正確に特定し、その原文上の位置（バイト範囲）を知る必要がある。構造を正しく認識できないと、コードブロックを翻訳対象にしてしまったり、訳文の差し戻しで書式を壊したりする。既存の Markdown ライブラリは使えない。

**現状**: Markdown パーサはまだない。

**何を変えるか**:
- CommonMark 0.31.2 のブロック構造を正しく解析し、各ブロックとその内容のソース範囲を返す再利用部品を自前で実装する。CommonMark 仕様の 2 段階方式（ブロック構造の解析 → インライン解析）に沿う。
- 各ノードに、原文バッファ上のバイト範囲と行番号を持たせる。複数行にわたるブロックについては、リスト項目や引用の継続行にあるプレフィックス（`>` やインデント）を除いた、行ごとの内容範囲を保持する。
- 翻訳に必要な範囲でインラインも解析する。
- 次の版で GFM を追加できるように、方言ごとの拡張点（ブロックとインラインの規則の追加）をインターフェースとして定義する（既定は CommonMark）。
- 対象範囲:
  - CommonMark の全ブロック要素: 見出し（ATX / Setext）、段落、リスト、引用、フェンスコードブロックとインデントコードブロック、HTML ブロック、テーマ区切り、リンク参照定義
  - Monde が必要とするインライン情報: リンクと画像の URL・タイトル・alt の範囲、コードスパン、生の HTML など
  - ノードのソース範囲と、行ごとの内容範囲
  - 方言インターフェース
  - CommonMark spec のテストケースによる構造の検証
- 対象外: HTML へのレンダリング、GFM の拡張（次の版）、翻訳単位を決める処理（translation-units）、訳文の差し戻し、フロントマター（yaml-front-matter が検出する）
- 依存関係: 上流は project-foundation（バッファ、ソース範囲の型）。下流は translation-units（構文木とソース範囲を利用する）と次の版の GFM 対応。
- 制約:
  - 受け入れ基準（spec.txt のラウンドトリップ）に直結するため、CommonMark 0.31.2 に厳密に従う。
  - spec.txt は CC-BY-SA 4.0 であり、取り扱いは roadmap の制約に従う。
  - インライン要素を msgid に含めつつ、URL を翻訳対象外とし、画像の alt を翻訳対象とする扱い（Q2-1、Q2-2）は、translation-units と要件を揃える。

## Introduction
markdown-parser は、CommonMark 0.31.2 に従って Markdown 文書を解析し、構文木を返す汎用の部品である。各ノードには原文上のソース範囲（バイトオフセットと行番号）を持たせる。利用者は Monde の開発者で、とくに translation-units と、次の版で GFM 対応を実装する開発者である。translation-units はこの部品の構文木とソース範囲を使って、どこが翻訳対象かを判断し、訳文を原文の正しい位置に差し戻す。そのため、ブロックの種類と入れ子を CommonMark のとおりに認識すること、複数行のブロックについて行ごとに内容の範囲（引用の `>` やリストのインデントを除いた範囲）を返すこと、リンク・画像の URL やタイトル、alt の範囲を個別に返すことが求められる。本部品は Monde 固有の知識を持たず、方言を差し替えるインターフェースを通して、次の版で GFM を追加できるようにする。

### 要件確認で決めた挙動
- インラインは、強調・強い強調・改行を含めて、CommonMark のインライン要素をすべて解析する。
- 構造の正しさは、テスト専用の HTML 出力で検証する。CommonMark spec 0.31.2 の全例について、出力が期待 HTML と一致することを確認する。HTML 出力は製品（`monde` の実行ファイル）には含めない。
- インライン記法を msgid に含めるか（Q2-1）、URL を翻訳対象外とし、alt を翻訳対象とするか（Q2-2）は、translation-units が判断する。本部品は、そのどちらの方式でも使えるように、リンク・画像の全体の範囲に加えて、リンクテキスト・alt・URL・タイトルの範囲を個別に返す。
- ブロック構造は、CommonMark のとおり LF、CRLF、単独の CR のいずれも行末として扱う。一方、ノードの行番号は、project-foundation のソース範囲の規則（単独の CR を改行として数えない）に従う。単独の CR を含む文書では、CommonMark 上の複数の行が同じ行番号になることがある。
- 構文エラーは存在しない（CommonMark では、どの入力も何らかの文書として解釈される）。解析が失敗するのは、不正な UTF-8 を含む場合と、メモリを確保できない場合だけである。

## Boundary Context
- **In scope**:
  - CommonMark 0.31.2 の全ブロック要素の認識: 引用、リスト、リスト項目、テーマ区切り、ATX 見出し、Setext 見出し、インデントコードブロック、フェンスコードブロック、HTML ブロック、リンク参照定義、段落
  - CommonMark 0.31.2 の全インライン要素の認識: テキスト、バックスラッシュエスケープ、実体参照・数値文字参照、コードスパン、強調・強い強調、リンク、画像、自動リンク、生の HTML、ハード改行、ソフト改行
  - リンク参照定義によるリンク・画像の参照の解決
  - 各ノードのソース範囲と行番号、複数行のブロックの行ごとの内容範囲とプレフィックス範囲
  - フロントマターの後ろなど、呼び出し側が指定した位置から本文を解析すること
  - 方言を差し替えるインターフェース（既定は CommonMark）
  - テスト専用の HTML 出力と、CommonMark spec 0.31.2 の全例による検証
- **Out of scope**:
  - 製品としての HTML へのレンダリング
  - GFM の拡張（表、タスクリスト、取り消し線、自動リンクの拡張）。次の版で方言として追加する
  - どのノードを翻訳単位とするか、msgid をどう組み立てるか、訳文をどう差し戻すか（translation-units が担う）
  - フロントマターの検出と解析（yaml-front-matter が担う）
  - ファイルの読み書き、BOM の除去、メッセージの表示、終了コードの決定（foundation と monde-cli が担う）
- **Adjacent expectations**:
  - 呼び出し側は、BOM を除去した後の内容を本部品に渡す。フロントマターがある場合は、yaml-front-matter が返した本文の開始位置を渡す。
  - translation-units は、本部品が返すソース範囲と行ごとの内容範囲を使って、範囲置換とプレフィックスの復元を行う。本部品は原文の内容を変更しない。
  - 本部品は、ソース範囲を project-foundation のソース範囲の型で表し、エラーを project-foundation の診断として返す。自分で標準出力・標準エラー出力に書いたり、プロセスを終了したりしない。
  - CommonMark spec の spec.txt は CC-BY-SA 4.0 であり、roadmap の制約（無改変のままライセンス表記を添えて独立したディレクトリに置くか、テスト時に取得する。リリース成果物には含めない）に従って扱う。

## Requirements

### Requirement 1: 文書の解析
**Objective:** As a translation-units の開発者, I want メモリ上の Markdown の内容を渡して構文木を得たい, so that ファイルの入出力と切り離して翻訳単位を特定できる

#### Acceptance Criteria
1. When 呼び出し側が Markdown の内容を渡して解析を要求する, the Markdown パーサ shall CommonMark 0.31.2 に従って内容を解析し、文書全体を根とする構文木を返す。
2. When 呼び出し側が本文の開始位置（バイトオフセット）を指定する, the Markdown パーサ shall その位置より前の内容を解析の対象から除き、その位置を文書の先頭として解析する。
3. While 本文の開始位置が指定されている, the Markdown パーサ shall ソース範囲と行番号を、開始位置からではなく、渡された内容全体の先頭を基準にして返す。
4. If 指定された本文の開始位置が内容の長さを超える, then the Markdown パーサ shall 構文木を返さずに、不正な指定であることをエラーの診断として返す。
5. The Markdown パーサ shall 解析の前後で、渡された内容のバイト列を変更しない。
6. The Markdown パーサ shall NUL（0x00）を含む内容を、NUL で打ち切らずに最後まで解析する。
7. The Markdown パーサ shall 空の内容と、空行だけからなる内容を、子を持たない文書として返す。

### Requirement 2: ブロック構造の認識
**Objective:** As a translation-units の開発者, I want ブロックの種類と入れ子を CommonMark のとおりに知りたい, so that コードブロックを翻訳対象にしたり、引用やリストの中の段落を取りこぼしたりしない

#### Acceptance Criteria
1. The Markdown パーサ shall 引用、リスト、リスト項目を、子のブロックを持つコンテナブロックとして返し、その子を原文の出現順に並べる。
2. The Markdown パーサ shall 段落、ATX 見出し、Setext 見出し、テーマ区切り、インデントコードブロック、フェンスコードブロック、HTML ブロック、リンク参照定義を、それぞれ種類を区別したリーフブロックとして返す。
3. The Markdown パーサ shall 見出しについて、レベル（1〜6）と、ATX と Setext のどちらの形式かを返す。
4. The Markdown パーサ shall リストについて、順序付きか順序なしか、使われたリスト記号（`-`、`+`、`*`、`.`、`)`）、順序付きの場合の開始番号、タイトかルーズかを返す。
5. The Markdown パーサ shall フェンスコードブロックについて、フェンスの文字（`` ` `` または `~`）と長さ、情報文字列の範囲、閉じフェンスがあるかどうかを返す。
6. The Markdown パーサ shall HTML ブロックについて、CommonMark が定める開始条件の種類（1〜7）を返す。
7. The Markdown パーサ shall リンク参照定義について、ラベル、リンク先、タイトル（ある場合）のそれぞれの範囲を返す。
8. When 段落の先頭にリンク参照定義として解釈できる行がある, the Markdown パーサ shall それらの行をリンク参照定義として返し、残りの行だけを段落として返す。
9. When 段落の継続として扱われる遅延継続行（引用の `>` やリスト項目のインデントを持たない行）がある, the Markdown パーサ shall その行を、続いている段落の一部として返す。

### Requirement 3: ソース範囲と行番号
**Objective:** As a translation-units の開発者, I want 各ノードの原文上の位置を知りたい, so that 原文バッファの該当範囲だけを訳文で置き換え、参照コメントに行番号を出せる

#### Acceptance Criteria
1. The Markdown パーサ shall すべてのブロックとインラインのノードに、渡された内容を基準とするソース範囲（開始を含み終了を含まないバイトオフセット）と、開始行と終了行の行番号を持たせる。
2. The Markdown パーサ shall 子のノードのソース範囲を、親のノードのソース範囲の内側に収める。
3. The Markdown パーサ shall 同じ親を持つノードのソース範囲を、互いに重ならず、原文の出現順に並ぶようにする。
4. The Markdown パーサ shall ブロックのソース範囲を、ブロックの最初の行の記号（リスト記号、`>`、`#`、フェンスなど）から最後の行の内容の終わりまでとし、最後の行の行末記号を含めない。
5. The Markdown パーサ shall 見出しについて、見出しの記号（ATX の開きと閉じの `#` の並びと前後の空白、Setext の下線の行）を除いた内容の範囲を返す。
6. The Markdown パーサ shall リスト項目について、リスト記号の範囲を返す。
7. The Markdown パーサ shall ソース範囲が指すバイト列を、元の内容のバイト列と一致させる（正規化した文字列ではなく、原文の位置を返す）。

### Requirement 4: 行ごとの内容範囲
**Objective:** As a translation-units の開発者, I want 複数行のブロックの各行について、プレフィックスと内容の境界を知りたい, so that msgid から行頭のプレフィックスを除き、差し戻しのときにプレフィックスを復元できる

#### Acceptance Criteria
1. The Markdown パーサ shall 段落、見出し、コードブロック、HTML ブロック、リンク参照定義について、その内容を構成する各行の行番号、内容の範囲、プレフィックスの範囲を、原文の行順に返す。
2. The Markdown パーサ shall 各行のプレフィックスの範囲に、その行が属するコンテナ（引用の `>` とその後ろの空白、リスト項目のインデント）と、そのブロック自身が取り除く先頭の空白を含め、内容の範囲に含めない。
3. The Markdown パーサ shall 各行の内容の範囲に、行末記号（LF、CRLF、CR）を含めない。
4. When 行が遅延継続行である, the Markdown パーサ shall その行が遅延継続行であることを示す。
5. When タブの一部だけがプレフィックスとして取り除かれる, the Markdown パーサ shall 内容の範囲をそのタブの後ろから始め、内容の前に補うべき空白の数（タブのうち取り除かれなかった桁数）を返す。
6. The Markdown パーサ shall フェンスコードブロックの各行について、開きフェンスのインデントに応じて CommonMark が取り除く空白をプレフィックスに含め、開きフェンスと閉じフェンスの行を内容の行に含めない。
7. The Markdown パーサ shall インデントコードブロックの各行について、コードブロックの 4 桁のインデントをプレフィックスに含める。

### Requirement 5: インラインの認識
**Objective:** As a translation-units の開発者, I want 段落と見出しのインライン要素とその範囲を知りたい, so that リンク先 URL を翻訳対象外とし、画像の alt を翻訳対象とする判断ができる

#### Acceptance Criteria
1. The Markdown パーサ shall 段落と見出しの内容を、CommonMark 0.31.2 に従ってインラインのノード（テキスト、コードスパン、強調、強い強調、リンク、画像、自動リンク、生の HTML、ハード改行、ソフト改行）に分けて返す。
2. The Markdown パーサ shall リンクについて、リンク全体の範囲、リンクテキストの範囲、リンク先の範囲、タイトルの範囲（ある場合）を返す。
3. The Markdown パーサ shall 画像について、画像全体の範囲、alt の範囲、リンク先の範囲、タイトルの範囲（ある場合）を返す。
4. The Markdown パーサ shall リンクと画像について、インライン形式、完全参照、省略参照、ショートカット参照のどれかを返し、参照形式の場合は参照ラベルの範囲を返す。
5. When リンクまたは画像が参照形式である, the Markdown パーサ shall 対応するリンク参照定義を返し、リンク先とタイトルの範囲としてその定義の範囲を返す。
6. The Markdown パーサ shall コードスパンについて、バッククォートの並びを除いた内容の範囲を返す。
7. The Markdown パーサ shall 自動リンクについて、URI とメールアドレスのどちらかと、`<` と `>` を除いた範囲を返す。
8. The Markdown パーサ shall バックスラッシュエスケープ、実体参照、数値文字参照について、原文の範囲と、それが表す文字を返す。
9. The Markdown パーサ shall 強調と強い強調について、区切り記号の範囲と、使われた文字（`*` または `_`）を返す。
10. When インラインのノードが複数行にまたがる, the Markdown パーサ shall そのノードのソース範囲を原文上の開始と終了の位置で返し、ノードの内容がどの行のどの位置にあるかを Requirement 4 の行ごとの内容範囲と対応づけられるようにする。
11. The Markdown パーサ shall インラインを、段落と見出しの内容に対してだけ解析し、コードブロック、HTML ブロック、リンク参照定義の内容に対しては解析しない。

### Requirement 6: リンク参照定義の解決
**Objective:** As a translation-units の開発者, I want 参照形式のリンクがどの定義を指すかを知りたい, so that 参照形式のリンクでも URL の範囲を正しく特定できる

#### Acceptance Criteria
1. The Markdown パーサ shall 文書内のどの位置にあるリンク参照定義も、文書全体の参照の解決に使う（参照より後ろにある定義も含む）。
2. The Markdown パーサ shall ラベルの照合を、CommonMark が定めるとおり、Unicode の大文字と小文字の区別をなくし、連続する空白を 1 つにまとめ、前後の空白を除いて行う。
3. When 同じラベルを持つリンク参照定義が複数ある, the Markdown パーサ shall 文書内で最初に現れた定義を使う。
4. If 参照形式のリンクまたは画像に対応する定義がない, then the Markdown パーサ shall それをリンクまたは画像とせず、CommonMark のとおりテキストとして返す。

### Requirement 7: 方言の差し替え
**Objective:** As a 次の版で GFM を実装する開発者, I want 方言ごとにブロックとインラインの規則を追加したい, so that CommonMark の解析部分を書き換えずに GFM を追加できる

#### Acceptance Criteria
1. When 呼び出し側が方言を指定せずに解析を要求する, the Markdown パーサ shall CommonMark 0.31.2 の方言で解析する。
2. When 呼び出し側が方言を指定して解析を要求する, the Markdown パーサ shall その方言が定めるブロックとインラインの規則を、CommonMark の規則に加えて適用する。
3. Where 方言が新しいブロックの規則を追加する, the Markdown パーサ shall その規則が認識したブロックを、方言が定めるノードの種類とソース範囲を持つノードとして構文木に含める。
4. Where 方言が新しいインラインの規則を追加する, the Markdown パーサ shall その規則が認識したインライン要素を、方言が定めるノードの種類とソース範囲を持つノードとして構文木に含める。
5. Where 方言が CommonMark の規則に何も追加しない, the Markdown パーサ shall CommonMark の方言と同じ構文木を返す。
6. The Markdown パーサ shall 方言の拡張による解析を、ブロックとインラインの規則を 1 つずつ追加するテスト用の方言で検証できるようにする。

### Requirement 8: 異常な入力とリソース不足
**Objective:** As a monde-cli の開発者, I want 解析できない場合に理由を受け取りたい, so that ユーザーに英語のエラーメッセージを表示し、終了コードを決められる

#### Acceptance Criteria
1. If 内容に不正な UTF-8 のバイト列が含まれる, then the Markdown パーサ shall 構文木を返さずに、その位置の行番号を持つエラーの診断を返す。
2. If 解析中に必要なメモリを確保できない, then the Markdown パーサ shall 異常終了せずに、構文木を返さずにエラーの診断を返し、それまでに確保したメモリを解放する。
3. The Markdown パーサ shall 入れ子の深い引用やリスト、多数の区切り記号や括弧を含む入力でも、異常終了せずに解析を完了する。
4. The Markdown パーサ shall 診断のメッセージを英語で記録し、標準出力・標準エラー出力への書き込みやプロセスの終了を行わない。
5. When 呼び出し側が構文木を解放する, the Markdown パーサ shall 構文木のために確保したメモリをすべて解放する。

### Requirement 9: CommonMark spec による検証
**Objective:** As a Monde の開発者, I want パーサの構造が CommonMark 0.31.2 に厳密に従うことを確認したい, so that 受け入れ基準（spec.txt のラウンドトリップ）を満たす土台にできる

#### Acceptance Criteria
1. The Markdown パーサのテスト shall 構文木を CommonMark のリファレンス実装と同じ形の HTML として出力するテスト専用の出力を持つ。
2. When テストを実行する, the Markdown パーサのテスト shall CommonMark spec 0.31.2 の全例について、テスト専用の HTML 出力が期待 HTML と一致することを確認し、一致しない例をその番号とともに報告する。
3. The Markdown パーサのテスト shall HTML 出力の比較を、CommonMark spec の公式のテストランナーと同じ HTML の正規化を行ったうえで行う。
4. When テストを実行する, the Markdown パーサのテスト shall CommonMark spec 0.31.2 の全例について、すべてのノードのソース範囲と行ごとの内容範囲が Requirement 3 と Requirement 4 の条件（内容の範囲内、親の内側、兄弟と重ならない）を満たすことを確認する。
5. The Markdown パーサのテスト shall 代表的なブロックとインラインについて、ソース範囲と行ごとの内容範囲が期待する値と一致することを個別のテストで確認する。
6. The Monde ビルド shall テスト専用の HTML 出力と spec.txt を、製品の実行ファイルとリリース成果物に含めない。
