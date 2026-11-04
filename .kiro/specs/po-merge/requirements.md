# Requirements Document

## Project Description (Input)
**対象者**: monde で Markdown 文書を翻訳・保守する翻訳者と、`monde update` を実装する monde-cli の開発者。

**現状**: 原文 Markdown が更新されると翻訳テンプレート（`.pot`）の msgid も変わるが、既存の翻訳ファイル（`.po`）を新しいテンプレートへマージする部品がまだない。gettext の `msgmerge` は外部依存になるため使えない。

**変えたいこと**: po-format のデータモデルの上で、新しい `.pot` と既存の `.po` を入力として `msgmerge` と同等の結果となる更新後の `.po` を生成する再利用部品を用意する。マージは次の 3 段階で行う。
1. 完全一致（msgctxt + msgid）による翻訳の引き継ぎ
2. あいまい一致（文字列の類似度にしきい値を設ける）による fuzzy での引き継ぎ。前回の msgid（`#|`）を付与する
3. テンプレートにないエントリの廃止エントリ化（`#~`）と、廃止エントリが再び出現したときの復活

あわせて、翻訳者コメントの保持、参照コメントと抽出コメントのテンプレートからの更新、ヘッダの更新（POT-Creation-Date など）、マージ結果の統計（一致・fuzzy・新規・廃止の件数）を扱う。

**範囲外**: PO ファイルの入出力（po-format）、CLI のオプション（monde-cli）、翻訳メモリや機械翻訳との連携、複数形エントリの本格的なマージ（データの保持にとどめる）。

**制約**: `msgmerge` と同等の振る舞いを目標とし、テストでは `msgmerge` の出力と比較できるケースを用意する。類似度のアルゴリズムと件数が多いときの計算量の扱いは設計時に決める。一般的な文書の規模で体感上すぐ終わること。

**関連**: Upstream は po-format、Downstream は monde-cli（`update` サブコマンド）。

## Introduction
po-merge は、新しい翻訳テンプレート（`.pot`）と既存の翻訳ファイル（`.po`）をマージして、更新後の翻訳ファイルの内容を作る汎用の部品である。原文 Markdown が更新されて msgid が変わっても、翻訳者のこれまでの作業を失わないように、変更のない箇所の訳を引き継ぎ、似た箇所の訳を fuzzy として再利用し、使われなくなった訳を廃止エントリとして残す。挙動は GNU gettext の `msgmerge`（`--previous` 指定時）と同等であることを目標とする。利用者は `monde update` を実装する monde-cli の開発者で、その先の利用者は翻訳者である。本部品は po-format のエントリの表現の上で動き、Monde 固有の知識を持たない。

### 要件確認で決めた挙動
- あいまい一致で引き継いだエントリには、常に前回の msgctxt・msgid・msgid_plural（`#|`）を付ける（`msgmerge --previous` と同じ）。
- 単数形と複数形が食い違うエントリは、`msgmerge` と同じく訳を引き継いだうえで fuzzy にする。
- 呼び出し側は、あいまい一致を無効にできる（`msgmerge --no-fuzzy-matching` 相当）。CLI のオプション名は monde-cli が決める。
- 訳のないまま使われなくなったエントリは、`msgmerge` と同じく廃止エントリにせずに捨てる。
- 性能の目安は、テンプレートと既存の翻訳ファイルがそれぞれ 2,000 エントリの規模で 1 秒以内とする。
- 設計時に GNU gettext 0.21 の `msgmerge` のソースと実行結果で挙動を確かめ、ヘッダの項目の並び、ヘッダのない場合、あいまい一致の候補の選び方、前回の msgid、新規の複数形エントリの msgstr[n] の個数を `msgmerge` に合わせた。`msgmerge` と意図して異なる点は、テンプレートの廃止エントリを無視すること（1.4）、廃止エントリと廃止エントリでないエントリの重複をエラーにしないこと（2.7）、未知のフラグを保つこと（2.3）、Language-Team からの言語コードの推測をしないこと（5.5）である。

## Boundary Context
- **In scope**:
  - msgctxt と msgid の完全一致による訳の引き継ぎと、廃止エントリの復活
  - 文字列の類似度によるあいまい一致と、fuzzy フラグ・前回の msgid の付与
  - 使われなくなったエントリの廃止エントリ化
  - コメント・参照・フラグを、既存の翻訳ファイルとテンプレートのどちらから取るかの決定
  - ヘッダエントリの更新
  - 単数形と複数形の食い違いの最小限の扱い
  - マージ結果の統計（完全一致・fuzzy・新規・廃止の件数）
- **Out of scope**:
  - PO の内容の読み込みと書き出し、折り返し（po-format が担う）
  - ファイルの読み書き、CLI のオプション、統計やメッセージの表示（monde-cli が担う）
  - 翻訳メモリ・機械翻訳・辞書ファイル（`msgmerge --compendium` 相当）との連携
  - 複数形の本格的なマージ（言語ごとの Plural-Forms の決定や、msgstr[n] の個数の検査）
  - `msgmerge` の上記以外のオプション（`--update` によるバックアップ、並べ替え、`--for-msgfmt` など）
  - 書式文字列（`c-format` など）や `range` の検査に基づく fuzzy の付与
- **Adjacent expectations**:
  - 入力と出力は po-format のエントリの列であり、本部品はエントリの表現を再定義しない。
  - monde-cli は、テンプレートと既存の翻訳ファイルを po-format で読み込んで本部品に渡し、結果を po-format で書き出す。改行コードや折り返しの設定は monde-cli と po-format が扱う。
  - 本部品は、エラーを project-foundation の診断として呼び出し側に返し、自分で標準出力・標準エラー出力に書いたりプロセスを終了したりしない。

## Requirements

### Requirement 1: マージの入力と出力
**Objective:** As a monde-cli の開発者, I want テンプレートと既存の翻訳ファイルのエントリの列を渡して、更新後のエントリの列を受け取りたい, so that `monde update` を薄い層として実装できる

#### Acceptance Criteria
1. When 呼び出し側がテンプレートのエントリの列と既存の翻訳ファイルのエントリの列を渡す, the PO マージ shall 更新後の翻訳ファイルのエントリの列と、マージ結果の統計を返す。
2. The PO マージ shall 更新後のエントリの列を、ヘッダエントリ、テンプレートに並んでいる順の廃止エントリでないエントリ、廃止エントリの順に並べる。
3. The PO マージ shall テンプレートの廃止エントリでないエントリごとに、更新後のエントリの列にちょうど 1 つの廃止エントリでないエントリを出力する。
4. The PO マージ shall テンプレートの中の廃止エントリを無視する。
5. The PO マージ shall 呼び出し側が渡したテンプレートと既存の翻訳ファイルのエントリの列を変更しない。
6. When 既存の翻訳ファイルのエントリの列が空である, the PO マージ shall テンプレートの各エントリを未翻訳のエントリとして出力する。

### Requirement 2: 完全一致による引き継ぎ
**Objective:** As a 翻訳者, I want 原文が変わっていない箇所の訳がそのまま残ってほしい, so that 翻訳済みの箇所を訳し直さずに済む

#### Acceptance Criteria
1. When テンプレートのエントリと、msgctxt（ないことを含む）と msgid がともに一致する既存のエントリがある, the PO マージ shall 既存のエントリの msgstr を引き継いだエントリを出力する。
2. When 完全一致したエントリを出力する, the PO マージ shall 翻訳者コメントを既存のエントリから、抽出コメントと参照をテンプレートのエントリから取る。
3. When 完全一致したエントリを出力する, the PO マージ shall fuzzy フラグの有無を既存のエントリに合わせ、fuzzy 以外のフラグ（`c-format`、`no-wrap` など）をテンプレートのエントリから取る。
4. When 完全一致した既存のエントリが fuzzy で訳がある（最初の msgstr が空でない）, the PO マージ shall 既存のエントリの前回の msgctxt・msgid・msgid_plural を残す。
5. When 出力するエントリが fuzzy でない、または未翻訳である, the PO マージ shall 出力するエントリに前回の msgctxt・msgid・msgid_plural を付けない。
6. When 完全一致する既存のエントリが廃止エントリである, the PO マージ shall その訳とコメントを同じ規則で引き継ぎ、廃止エントリでないエントリとして出力する（廃止エントリの復活）。
7. When 完全一致する既存のエントリが、廃止エントリでないものと廃止エントリの両方にある, the PO マージ shall 廃止エントリでないものから引き継ぐ。
8. When 完全一致した既存のエントリが未翻訳である, the PO マージ shall 未翻訳のまま、同じ規則でコメントとフラグを引き継いだエントリを出力する。

### Requirement 3: あいまい一致による fuzzy での引き継ぎ
**Objective:** As a 翻訳者, I want 少しだけ変わった原文には以前の訳が下書きとして入っていてほしい, so that 一から訳さずに差分だけ直せる

#### Acceptance Criteria
1. While あいまい一致が有効である（既定）, when テンプレートのエントリに完全一致する既存のエントリがない, the PO マージ shall 訳のある（最初の msgstr が空でない）既存のエントリのうち、msgid の類似度がしきい値を超えて最も高いものを候補とする。
2. The PO マージ shall あいまい一致の類似度の計算方法としきい値を、`msgmerge` と同じ候補が選ばれるように定める。
3. The PO マージ shall msgctxt が異なる既存のエントリと廃止エントリも、あいまい一致の候補とする。
4. The PO マージ shall `msgmerge` と同じく、msgctxt を持たない候補と msgctxt が一致する候補の類似度に、ごくわずかな値を加えて比べる。
5. When 類似度が最も高い候補が複数ある, the PO マージ shall `msgmerge` と同じ規則で 1 つを選び、同じ入力に対して常に同じ候補を選ぶ。
6. When あいまい一致の候補が見つかる, the PO マージ shall 候補の msgstr と翻訳者コメントを引き継ぎ、抽出コメント・参照・fuzzy 以外のフラグをテンプレートのエントリから取り、fuzzy フラグを付けたエントリを出力する。
7. When あいまい一致の候補が見つかる, the PO マージ shall 出力するエントリの前回の msgctxt・msgid・msgid_plural に、候補が fuzzy でなければ候補の msgctxt・msgid・msgid_plural を、候補が fuzzy であれば候補の前回の msgctxt・msgid・msgid_plural を設定する。
8. The PO マージ shall 1 つの既存のエントリを、複数のテンプレートのエントリのあいまい一致の候補として使えるようにする。
9. The PO マージ shall ヘッダエントリと未翻訳の既存のエントリを、あいまい一致の候補にしない。
10. When あいまい一致の候補が見つからない, the PO マージ shall テンプレートのエントリをそのまま未翻訳のエントリとして出力する。
11. Where 呼び出し側があいまい一致の無効を指定する, the PO マージ shall あいまい一致を行わず、完全一致しないテンプレートのエントリをすべて未翻訳のエントリとして出力する。

### Requirement 4: 廃止エントリの管理
**Objective:** As a 翻訳者, I want 原文から消えた箇所の訳を消さずに残してほしい, so that 原文が元に戻ったときや似た文を訳すときに再利用できる

#### Acceptance Criteria
1. When 訳のある既存のエントリが、完全一致にもあいまい一致にも使われなかった, the PO マージ shall そのエントリを廃止エントリとして出力する。
2. When エントリを廃止エントリにする, the PO マージ shall msgctxt・msgid・msgid_plural・msgstr・翻訳者コメント・フラグ・前回の msgid を保ち、抽出コメントと参照を取り除く。
3. When 未翻訳の既存のエントリが、完全一致に使われなかった, the PO マージ shall そのエントリを出力しない。
4. When 訳のある既存の廃止エントリが、完全一致にもあいまい一致にも使われなかった, the PO マージ shall そのエントリを受け入れ基準 2 と同じ規則で、廃止エントリのまま出力する。
5. The PO マージ shall 廃止エントリを、既存の翻訳ファイルに現れた順に出力する。
6. The PO マージ shall あいまい一致に使われた既存のエントリ（廃止エントリを含む）を、廃止エントリとして出力しない。

### Requirement 5: ヘッダの更新
**Objective:** As a 翻訳者, I want 翻訳ファイルのヘッダの自分の情報を残したまま、テンプレートの作成日時だけ更新してほしい, so that 翻訳者情報を毎回入れ直さずに済み、どのテンプレートに基づく訳かがわかる

#### Acceptance Criteria
1. When テンプレートと既存の翻訳ファイルの両方にヘッダエントリがある, the PO マージ shall 既存の翻訳ファイルのヘッダエントリを基に、POT-Creation-Date と Report-Msgid-Bugs-To の値をテンプレートのヘッダの値に置き換えたヘッダエントリを出力する。
2. When ヘッダエントリを更新する, the PO マージ shall 上記以外の項目（Language、PO-Revision-Date、Last-Translator、Plural-Forms など）の値、翻訳者コメント、fuzzy フラグの有無を既存の翻訳ファイルのヘッダのまま残す。
3. When 置き換える項目がテンプレートのヘッダにだけある, the PO マージ shall その項目を出力するヘッダに追加する。
4. When ヘッダエントリを更新する, the PO マージ shall `msgmerge` と同じく、Project-Id-Version、Report-Msgid-Bugs-To、POT-Creation-Date、PO-Revision-Date、Last-Translator、Language-Team、Language、MIME-Version、Content-Type、Content-Transfer-Encoding をこの順に並べ、その後にほかの行を既存のヘッダに現れた順に並べる。
5. When 既存のヘッダに Language がなく Language-Team がある, the PO マージ shall 値が空の Language を追加する（`msgmerge` が行う Language-Team の英語名からの言語コードの推測は行わない）。
6. When 既存の翻訳ファイルにだけヘッダエントリがある, the PO マージ shall そのヘッダエントリを受け入れ基準 2 と 4 の規則で整えて出力する。
7. When テンプレートにだけヘッダエントリがある, the PO マージ shall `msgmerge` と同じく、ヘッダエントリを出力しない。
8. When どちらにもヘッダエントリがない, the PO マージ shall ヘッダエントリを出力しない。

### Requirement 6: 単数形と複数形の食い違い
**Objective:** As a 翻訳者, I want 単数形と複数形の形が変わったエントリの訳も下書きとして残ってほしい, so that 複数形エントリを含むファイルでも訳を失わない

#### Acceptance Criteria
1. When 引き継ぐ既存のエントリとテンプレートのエントリがともに複数形で、msgid_plural が一致する, the PO マージ shall 既存のエントリのすべての msgstr[n] を添字とともに引き継ぐ。
2. When テンプレートのエントリが複数形で、引き継ぐ既存のエントリが単数形である, the PO マージ shall 既存の msgstr を、既存の翻訳ファイルのヘッダの Plural-Forms の nplurals（ないか解釈できなければ 2）の個数の msgstr[n] のそれぞれに写し、fuzzy フラグを付けたエントリを出力する。
3. When テンプレートのエントリが単数形で、引き継ぐ既存のエントリが複数形である, the PO マージ shall 既存の msgstr[0] を msgstr として引き継ぎ、fuzzy フラグを付けたエントリを出力する。
4. When 引き継ぐ既存のエントリとテンプレートのエントリがともに複数形で、msgid_plural が異なる, the PO マージ shall 既存の msgstr[n] を引き継ぎ、fuzzy フラグを付けたエントリを出力する。
5. When 単数形と複数形の食い違いで fuzzy フラグを付ける, the PO マージ shall 前回の msgctxt・msgid・msgid_plural に既存のエントリの値を設定する。
6. When テンプレートの複数形のエントリを、引き継ぐ訳のない新規のエントリとして出力し、その msgstr[n] がすべて空である, the PO マージ shall msgstr[n] の個数を受け入れ基準 2 と同じ nplurals にする。
7. The PO マージ shall 受け入れ基準 2 と 6 の場合を除き、msgstr[n] の個数を Plural-Forms に合わせて増減したり、検査したりしない。

### Requirement 7: マージ結果の統計
**Objective:** As a monde-cli の開発者, I want マージで何件が引き継がれ、何件が新しく訳す必要があるかを知りたい, so that 利用者に結果を表示できる

#### Acceptance Criteria
1. The PO マージ shall 統計として、完全一致で引き継いだ件数（廃止エントリの復活を含む）、あいまい一致で引き継いだ件数、どちらにも一致しなかった新規の件数、今回のマージで新たに廃止エントリにした件数を返す。
2. The PO マージ shall 統計の件数にヘッダエントリを含めない。
3. The PO マージ shall 完全一致・あいまい一致・新規の件数の合計を、テンプレートの廃止エントリでもヘッダでもないエントリの数と等しくする。
4. The PO マージ shall 単数形と複数形の食い違いで fuzzy にしたエントリを、完全一致の件数に数える。
5. The PO マージ shall もともと廃止エントリで廃止のまま残ったエントリと、出力しなかった未翻訳のエントリを、廃止の件数に含めない。

### Requirement 8: 入力の不整合の報告
**Objective:** As a monde-cli の開発者, I want マージできない入力をはっきりしたエラーとして受け取りたい, so that 壊れた結果を書き出さずに利用者へ問題を伝えられる

#### Acceptance Criteria
1. If テンプレートに、同じ msgctxt と msgid の組を持つ廃止エントリでないエントリが複数ある, then the PO マージ shall 重複した msgctxt と msgid を示すエラーを報告し、結果を返さない。
2. If 既存の翻訳ファイルに、同じ msgctxt と msgid の組を持つ廃止エントリでないエントリが複数ある, then the PO マージ shall 重複した msgctxt と msgid を示すエラーを報告し、結果を返さない。
3. When エラーを報告する, the PO マージ shall エントリに入力上の行番号があれば、その行番号を含める。
4. The PO マージ shall エラーなく返した結果を po-format のライターで書き出したとき、重複のエラーにならないようにする。

### Requirement 9: msgmerge との同等性
**Objective:** As a Monde の開発者, I want マージ結果が msgmerge と同じであることを確かめたい, so that gettext のツールで更新した場合と結果が変わらないと保証できる

#### Acceptance Criteria
1. When テストで用意したテンプレートと既存の翻訳ファイルを、GNU gettext の `msgmerge --previous` と本部品でそれぞれマージし、本部品の結果を po-format のライターの既定の設定で書き出す, the PO マージ shall `msgmerge` の出力とバイト単位で同じ内容を出力する。
2. The PO マージ shall 受け入れ基準 1 の比較用のテストケースに、完全一致、あいまい一致、新規、廃止エントリ化、廃止エントリの復活、翻訳者コメント・参照・抽出コメントの更新、ヘッダの更新、単数形と複数形の食い違いを含める。
3. Where あいまい一致を無効にする, the PO マージ shall `msgmerge --previous --no-fuzzy-matching` の出力とバイト単位で同じ内容を出力する。

### Requirement 10: 性能
**Objective:** As a 翻訳者, I want `monde update` が待たされずに終わってほしい, so that 原文の更新のたびに気軽にマージできる

#### Acceptance Criteria
1. When テンプレートと既存の翻訳ファイルがそれぞれ 2,000 エントリ（msgid の平均が 200 バイト程度）で、テンプレートのエントリの 1 割が完全一致しない（原文の一部が更新された）, the PO マージ shall あいまい一致を有効にした状態で、一般的な開発用 PC で 1 秒以内にマージを終える。
2. The PO マージ shall 入力の規模にかかわらず、同じ入力に対して常に同じ結果を返す。

### Requirement 11: 再利用できる部品としての独立性
**Objective:** As a Monde の開発者, I want マージ処理を Monde 固有の処理から切り離したい, so that 単体でテストでき、ほかの用途にも使える

#### Acceptance Criteria
1. The po-merge 部品 shall Markdown や Monde のサブコマンドについての知識を持たず、po-format のエントリの列と設定だけを入出力とする。
2. The po-merge 部品 shall ファイルの読み書き、標準出力・標準エラー出力への書き込み、プロセスの終了を行わず、エラーを project-foundation の診断として呼び出し側に返す。
3. The po-merge 部品 shall 外部ライブラリや外部コマンド（`msgmerge` を含む）を使わずに、すべての対象環境で同じ結果を返す。
