# Research & Design Decisions: po-merge

## Summary
- **Feature**: `po-merge`
- **Discovery Scope**: New Feature（既存の po-format の上に新しいモジュールを追加する。外部の挙動 = GNU gettext の `msgmerge` との同等性が中心の課題）
- **Key Findings**:
  - `msgmerge` 0.21 は、ヘッダの既知の 10 項目を決まった順に並べ直し、テンプレートにだけヘッダがある場合はヘッダを出力しない。要件の初版はこの点を誤っていたため、設計の段階で要件を修正した（5.2、5.4〜5.8）。
  - あいまい一致は「文字の 4-gram を 1 つ以上共有する候補」だけを、共有数の多い順・ファイル順に調べ、`2 × LCS / (長さの和)`（バイト単位）にごく小さなボーナスを足した値が 0.6 を超えて最大のものを選ぶ。4 文字未満の msgid だけは長さの範囲で候補を選ぶ。廃止エントリも候補になり、fuzzy の候補を使うと前回の msgid は候補の前回の msgid になる。
  - `msgmerge` の実装（gnulib の fstrcmp、diffseq）は GPLv3 であり、MIT の Monde には取り込めない。挙動（候補の集合、順序、類似度の値）だけを仕様として、独自に実装する。類似度は bit-parallel の LCS で厳密に計算する。

## Research Log

### msgmerge 0.21 のマージ規則（ソース）
- **Context**: 要件 9.1 は `msgmerge --previous` とのバイト単位の一致を求める。要件の記述の多くは `msgmerge` の既知の挙動からの推測だった。
- **Sources Consulted**: gettext-0.21.tar.gz（ftp.gnu.org）の `gettext-tools/src/msgmerge.c`（`message_merge`、`match_domain`、`merge`）、`message.c`（`fuzzy_search_goal_function`、`message_list_search_fuzzy`）、`msgl-fsearch.c`（`message_fuzzy_index_*`）、`message.h`（`FUZZY_THRESHOLD`）、`gnulib-lib/fstrcmp.c`。
- **Findings**:
  - 出力の順序: テンプレートの順に結果を並べ、使われなかった既存のエントリを既存のファイルの順に廃止エントリとして末尾に加える。既存の廃止エントリもこの対象で、元から廃止のものも同じ扱いになる。
  - 完全一致: `msgctxt + msgid` のハッシュ検索。既存の廃止エントリも検索の対象（廃止エントリの復活）。
  - `message_merge(def, ref, force_fuzzy)`: msgctxt・msgid・msgid_plural・参照・抽出コメントは ref（テンプレート）から、msgstr・翻訳者コメントは def（既存）から取る。fuzzy は `def の fuzzy || force_fuzzy || msgid_plural の不一致`。書式フラグ・`no-wrap`・`range` は ref から取る。前回の msgid は、def が fuzzy なら def の前回の msgid、そうでなければ def の msgctxt・msgid・msgid_plural。
  - 書式文字列の検査（`check_msgid_msgstr_format_i`）と `range` の比較で fuzzy を加える処理がある。Monde のテンプレートには書式フラグがないため、範囲外とした。
  - 後処理 1（複数形）: ref が複数形で def が単数形なら、def の msgstr を nplurals 個に複製して fuzzy にする。ref が単数形で def が複数形なら、msgstr[0] だけを残して fuzzy にする。nplurals は結果のヘッダ（= 既存のヘッダ）の Plural-Forms から求め、なければ 2。
  - 後処理 2（新規の複数形）: 一致しなかったテンプレートの複数形エントリで msgstr がすべて空なら、msgstr の個数を、既存のヘッダから求めた nplurals にする。
  - 後処理 3（前回の msgid）: 廃止エントリでない結果のうち、fuzzy でない、または msgstr[0] が空のものから、前回の msgid を消す。廃止エントリは後で加えるため、この処理を受けない。
  - 廃止エントリ化: `message_copy` の後に抽出コメントと参照を消す。訳のない（msgstr[0] が空の）廃止エントリは、書き出し（`write-po.c` の `message_print_obsolete`）で捨てられる。
  - ヘッダ: 既存のヘッダの msgstr を行に分け、既知の 10 項目（Project-Id-Version、Report-Msgid-Bugs-To、POT-Creation-Date、PO-Revision-Date、Last-Translator、Language-Team、Language、MIME-Version、Content-Type、Content-Transfer-Encoding）を行頭の大文字・小文字を区別しない照合で取り出す。同じ項目が複数あれば後のものが勝つ。それ以外の行は順に連結する。テンプレートの msgstr から `Report-Msgid-Bugs-To:` と `POT-Creation-Date:` を（大文字・小文字を区別する部分文字列として）探し、行末までの値で置き換える。既知の項目を表の順と表の綴りで並べ、最後にほかの行を置く。Language がなく Language-Team があれば、Language-Team の英語名を言語の表で引いて Language を加える（見つからなければ空の値）。
  - テンプレートにヘッダがなければ、空のヘッダを仮に先頭に置いてから処理する。既存の翻訳ファイルにヘッダがなければ、テンプレートのヘッダは完全一致せず、ヘッダはあいまい一致もしないため、出力されない。
  - 統計（`-v`）: merged（ヘッダを含む）、fuzzied、missing、obsolete（使われなかった既存のエントリすべて。捨てられる未翻訳のものと元から廃止のものを含む）。
- **Implications**: 要件 2.4、2.5、3.1、3.3、3.4、3.6、3.11、4.4、4.6、5.2、5.4〜5.8、6.2、6.6、6.7 を、上記に合わせて修正した。統計は Monde 独自の定義（要件 7）のままとした。

### msgmerge 0.21 の実行による確認
- **Context**: ソースの読解を、実物の出力で確かめる。
- **Sources Consulted**: Ubuntu の `gettext` パッケージの `msgmerge (GNU gettext-tools) 0.21`。スクラッチの PO で `msgmerge --previous old.po t.pot` を実行した。
- **Findings**:
  - ヘッダ: `X-Custom` と `Plural-Forms` が既知の項目の後ろに移り、既知の項目は表の順に並んだ。Report-Msgid-Bugs-To と POT-Creation-Date はテンプレートの値になった。翻訳者コメントは既存のヘッダのもの、テンプレートの `#, fuzzy` は消えた。
  - フラグ: 既存のエントリの `c-format` と未知のフラグ `my-flag` は消え、テンプレートの `no-wrap` が付いた。出力は `#, fuzzy, no-wrap` の順。
  - 廃止エントリ: 訳のない既存のエントリ（通常・廃止とも）は出力されなかった。訳のある廃止エントリは前回の msgid を保ったまま残った。
  - 完全一致する廃止エントリは、fuzzy にならずに復活した。訳のある廃止エントリはあいまい一致の候補になり、使われると廃止エントリとして出力されなかった。
  - 既存の翻訳ファイルに、同じ msgid の通常のエントリと廃止エントリがあると、`duplicate message definition` の致命的なエラーになった。
  - fuzzy の候補を使うと、`#|` は候補の `#|`（前回の msgid）になった。
  - msgctxt `menu` のテンプレートのエントリが、msgctxt のない同じ msgid のエントリとあいまい一致した。
  - 単数形 → 複数形: `nplurals=1` で msgstr[0] だけ、Plural-Forms なしで 2 個、`nplurals=3` で 3 個に複製された。`#| msgid_plural` は既存のエントリに msgid_plural があるときだけ付いた。
  - 新規の複数形エントリ: `nplurals=1` では msgstr[0] だけ、`nplurals=3` では 3 個になった。
  - テンプレートにだけヘッダがあると、出力にヘッダがなかった。どちらにもないと、ヘッダは出力されなかった。
- **Implications**: 正解ファイル（`tests/po_merge/data/`）は、同じ msgmerge 0.21 で作る。ヘッダの Language の推測と、通常・廃止の重複は比較のケースから外す。

### 類似度とあいまい一致の索引
- **Context**: 要件 3.2、3.5（msgmerge と同じ候補）と、要件 10.1（性能）。
- **Sources Consulted**: `msgl-fsearch.c`、`message.c`、`fstrcmp.c`（いずれも gettext 0.21）。Hyyrö, "Bit-Parallel LCS-length Computation Revisited"（2004）。Allison & Dix, "A bit-string longest-common-subsequence algorithm"（1986）。
- **Findings**:
  - 類似度 `fstrcmp(a, b)` は、バイト列の挿入・削除だけの最小編集数 `e` から `(|a| + |b| − e) / (|a| + |b|)` を返す。`e = |a| + |b| − 2 × LCS` なので、値は `2 × LCS / (|a| + |b|)`。片方だけが空なら 0、両方空なら 1。
  - `fstrcmp_bounded` の下限の打ち切り（長さによる上限、256 種のバイトの出現数の差による上限、編集数の上限）は、下限を下回る場合にだけ 0 を返す最適化で、結果の比較を変えない。
  - diffseq は、編集数の探索が `too_expensive`（長さの和の約 2 倍の平方根、最小 4096）を超えると、最小でない編集を返すことがある。長さの和が数万バイトを超える、かなり異なる文字列でだけ起こる。
  - 比較値は `fstrcmp + bonus`（double）。bonus は、候補が msgctxt を持たないか、問い合わせと同じ msgctxt を持つとき 0.00001、それ以外は 0。初期の最良値を 0.6 とし、`>` で更新する（0.6 ちょうどは採用しない。等しい値は先に調べた候補が勝つ）。
  - 索引: 訳のある（msgstr[0] が空でない）既存のエントリ（廃止エントリとヘッダを含む）を、ファイル順の添字で登録する。4 文字以上の msgid は、UTF-8 の文字単位の 4-gram ごとに添字の列（重複なし）を持つ。msgid のバイト長が 28 以下のものは、長さごとの列にも入れる。
  - 4 文字以上の問い合わせ: 問い合わせの 4-gram の各出現について、その 4-gram を持つ候補の数え上げを 1 増やす（同じ 4-gram が問い合わせに 2 回あれば 2 回数える）。1 回以上数えた候補を、数え上げの降順・添字の昇順に並べ、すべて調べる（`heuristic = false`）。4-gram を共有しない候補は調べない。
  - 4 文字未満の問い合わせ（長さ `l` バイト）: `ceil(l / (2 / 0.6 − 1))` から `(int)(l × (2 / 0.6 − 1))` までの長さの列を、短い順・ファイル順に調べる。
- **Implications**: Similarity（LCS）、Fuzzy（索引と順序）の 2 つの部品に分ける。比較の式と順序は上記と同じにし、上限による枝刈りは「最良値を超えられない候補だけを飛ばす」ことを不変条件とする。

### ライセンス
- **Context**: gettext のコードの流用の可否。
- **Findings**: `msgmerge.c`、`msgl-fsearch.c`、gnulib の `fstrcmp.c`・`diffseq.h` は GPLv3 以降。Monde は MIT License で配布する。
- **Implications**: gettext のコードを複製・移植しない。挙動の仕様（上記の規則と定数）だけに従い、独自に実装する。LCS は公知のアルゴリズム（bit-parallel LCS）で実装する。テストは msgmerge の出力（データ）と比べる。

### 既存のコードベース（po-format）
- **Context**: 入出力のデータモデルと、再利用できる関数。
- **Sources Consulted**: `src/po/po.h`、`po_read.h`、`po_write.h`、`po_internal.h`、`tests/po/CMakeLists.txt`、`tests/po/data/README.md`、po-format の design.md。
- **Findings**:
  - 公開: `mnd_po_catalog_t`・`mnd_po_entry_t`、`mnd_po_strings_push`、`mnd_po_refs_push`、`mnd_po_catalog_append`、`mnd_po_flag_*`、`mnd_po_catalog_header`、`mnd_po_header_get`・`set`、`mnd_po_read`、`mnd_po_write`。エントリの複製の関数はない。
  - `mnd_po_find_duplicate` などは `po_internal.h`（src/po と tests/po だけが include する）にあり、po-merge からは使えない。
  - ライターはフラグを列の順に書く。未翻訳の廃止エントリも書く（捨てるのは po-merge の責任）。
  - 互換性テストは、正解ファイルをリポジトリに置き、gettext 0.21 のある環境でだけ「正解ファイル = gettext の出力」を確かめる形をとっている。CI の Ubuntu のジョブには gettext（msgmerge を含む）がすでに入っている。
- **Implications**: エントリの複製と重複の検出は po-merge の内部に持つ。正解ファイルの扱いは po-format と同じ形にする。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 単一ファイルの手続き | 1 つの .c にすべて置く | 小さい | 類似度・索引・ヘッダの単体テストが書きにくい | 不採用 |
| 段階ごとの部品（採用） | Merge（手順）、Builder（エントリの組み立て）、Header、KeyIndex、Fuzzy、Similarity、Hash | 各規則を単体でテストでき、msgmerge の規則と 1 対 1 に対応する | 内部ヘッダが必要 | po-format の `po_internal.h` と同じ扱い |
| 汎用の「類似検索」部品を別モジュールに | 翻訳メモリなどで再利用 | 将来の再利用 | 現在の要件にない（Out of Scope） | 不採用。インターフェースを内部に閉じる |

### 類似度の計算方法

| Option | 計算量（1 組） | 結果 | 判断 |
|--------|----------------|------|------|
| 動的計画法の LCS | O(n·m) | 厳密 | 200 バイトで 4 万回の更新。性能目標に届かない恐れ |
| Myers の O(ND) と打ち切り | O((n+m)·D) | 厳密（打ち切りで下限未満は 0） | 類似していない組（D が大きい）で遅い |
| bit-parallel LCS（採用） | O(⌈m/64⌉·n) | 厳密 | 問い合わせ側のマスクを 1 回作れば、候補ごとに 200 × 4 語程度の演算 |

## Design Decisions

### Decision: msgmerge の挙動に合わせて要件を修正する
- **Context**: 要件の初版は、ヘッダの並び、ヘッダのない場合、あいまい一致の同点の扱い、前回の msgid、新規の複数形の msgstr[n] の個数で、実際の msgmerge と異なっていた。要件 9.1（バイト単位の一致）と矛盾する。
- **Alternatives Considered**:
  1. 要件の初版のまま、msgmerge と異なる挙動にする — 9.1 を満たせない。
  2. msgmerge に合わせて要件を直す — 9.1 と整合する。
- **Selected Approach**: 2。修正点は requirements.md の「要件確認で決めた挙動」と各受け入れ基準に反映した。
- **Rationale**: 利用者の決定（msgmerge と同等）が上位にある。
- **Trade-offs**: ヘッダの項目の順序が、利用者が並べた順から msgmerge の順に変わる（msgmerge を使った場合と同じ）。
- **Follow-up**: 正解ファイルのケースで確認する。

### Decision: msgmerge と意図して異なる点
- **Context**: msgmerge の一部の挙動は、Monde の部品の境界やデータモデルに合わない。
- **Selected Approach**:
  - テンプレートの廃止エントリは無視する（msgmerge は廃止エントリとして出力する）。`.pot` に廃止エントリがあるのは誤用である。
  - 既存の翻訳ファイルの通常・廃止の重複はエラーにせず、通常のエントリを使う（msgmerge は致命的なエラー）。po-format のリーダーはこれを受け付けるため。
  - フラグは、テンプレートのフラグ（fuzzy を除く）をそのままの順に保つ。msgmerge は既知のフラグだけを決まった順に書き、未知のフラグを捨てる。Monde のテンプレートのフラグは `no-wrap` だけで、結果は同じになる。
  - 書式文字列と `range` の検査による fuzzy の付与はしない。
  - Language の推測（Language-Team の英語名の表）はせず、値が空の Language を加える。Monde の `init` は常に Language を設定する。
  - Plural-Forms は `nplurals` の値だけを読む（msgmerge は plural の式も解釈し、式が壊れていると 2 にする）。
  - 空の msgid で msgctxt を持つテンプレートのエントリは、ヘッダとはあいまい一致しない（要件 3.9）。
  - （解消済み）訳のない（最初の msgstr が空の）エントリが fuzzy になる場合（fuzzy で訳のない既存との完全一致、訳のない単数形の既存と複数形のテンプレートなど）に、po-format のライターが `#, fuzzy` を書き、gettext 0.21（msgstr[0] が空なら fuzzy を書かない）と異なっていた（実装時の検証で確認。tasks.md の Implementation Notes 4.2）。po-format 5.7（po-format のタスク 7.2）でライターが gettext と同じく書かなくなり、差異はなくなった。po-merge の製品のコードは変えていない（出力のカタログでは fuzzy のフラグを保ち、書き出しで省かれる）。正解ファイルの `untranslated_fuzzy` ケースで msgmerge 0.21 との一致を確かめる（po-format のタスク 7.4）。
- **Rationale**: いずれも（解消済みのものを除く）Monde の通常の使い方では出力が msgmerge と変わらない。
- **Trade-offs**: 上記の入力（解消済みのものを除く）では msgmerge と出力が異なる。比較のケースに含めない。

### Decision: 類似度は bit-parallel LCS で厳密に計算し、比較は msgmerge と同じ double の式で行う
- **Context**: 3.2、3.5、10.1、10.2、11.3。
- **Selected Approach**: 問い合わせ（テンプレートの msgid）のバイトごとのマスクを 1 回作り、各候補の msgid を走査して LCS を求める。比較値は `(double)(2 × LCS) / (double)(長さの和) + bonus`。上限（長さ、バイトの出現数の差）を同じ形の式で計算し、最良値を超えられない候補を飛ばす。
- **Rationale**: 枝刈りは結果を変えない。浮動小数点の演算は除算 1 回と加算 1 回で、IEEE 754 の倍精度で丸めが決まる（x87 の余分な精度は `volatile` で落とす）。
- **Trade-offs**: diffseq が最小でない編集を返す、非常に長く異なる文字列では、msgmerge と異なる候補を選ぶことがある（Risks に記載）。

### Decision: 4-gram 索引と候補の順序を msgmerge と同じにする
- **Context**: 3.2、3.5。同じ値の候補が複数あると、調べる順で結果が決まる。
- **Selected Approach**: Fuzzy は、msgmerge と同じ候補の集合（4-gram の共有、短い msgid の長さの範囲）と順序（数え上げの降順・添字の昇順、短い msgid は長さの昇順・ファイル順）で候補を調べる。並べ替えは添字を含む全順序で行い、安定でない qsort でも同じ結果にする。
- **Rationale**: 類似度だけで選ぶと、同点のときに msgmerge と異なる候補を選びうる。
- **Trade-offs**: 4-gram を共有しない候補は、類似度が 0.6 を超えても選ばない（msgmerge と同じ）。

### Decision: エントリの複製と重複の検出を po-merge の内部に持つ
- **Context**: po-format の公開 API に、エントリの複製と重複の検出がない。
- **Alternatives Considered**:
  1. po-format に `mnd_po_entry_copy` などを追加する — 上流の spec の変更と再検証が必要。
  2. po-merge の内部（Builder、KeyIndex）に持つ — 公開 API だけで書ける。
- **Selected Approach**: 2。
- **Trade-offs**: 似たコードが 2 か所に置かれる。将来 po-format に複製の関数が入れば置き換える。

### Decision: 正解ファイルを msgmerge 0.21 の出力として置き、msgmerge のある環境でだけ再確認する
- **Context**: 9.1〜9.3 と、gettext のない環境（Windows、macOS）での実行。
- **Selected Approach**: po-format と同じく、`tests/po_merge/data/<case>/` に入力と msgmerge の出力を置く。単体テストは全環境で出力と比べる。msgmerge 0.21 がある環境では、CTest が msgmerge を実行して正解ファイルと比べる。
- **Rationale**: 全環境で同じ確認ができ、正解ファイルが本当に msgmerge の出力であることも CI で保証できる。

## Risks & Mitigations
- 非常に長く大きく異なる文字列で、diffseq の打ち切りにより msgmerge の類似度が厳密値より小さくなり、異なる候補を選ぶことがある — 段落単位の msgid（数百バイト）では起こらない。設計の Open Questions に残し、正解ファイルには含めない。
- 全エントリが完全一致しない最悪の場合（2,000 × 2,000 の組）は 1 秒を超えうる — 要件 10.1 は 1 割の更新を基準とする。性能テストで基準の場合を測り、最悪の場合の時間を記録する。
- gettext を上げると msgmerge の出力が変わりうる — 互換性テストを 0.21 に限定し、版が違えば登録しない（po-format と同じ）。
- 浮動小数点の環境差 — 比較を除算 1 回と加算 1 回に限り、`volatile` で余分な精度を落とす。正解ファイルの比較を全環境で行う。

## References
- [GNU gettext 0.21 source](https://ftp.gnu.org/pub/gnu/gettext/gettext-0.21.tar.gz) — msgmerge.c、message.c、msgl-fsearch.c、fstrcmp.c（GPLv3。挙動の確認だけに使い、コードは使わない）
- [GNU gettext manual: msgmerge Invocation](https://www.gnu.org/software/gettext/manual/html_node/msgmerge-Invocation.html) — `--previous`、`--no-fuzzy-matching`
- H. Hyyrö, "Bit-Parallel LCS-length Computation Revisited", AWOCA 2004 — bit-parallel LCS
- L. Allison, T. I. Dix, "A bit-string longest-common-subsequence algorithm", Information Processing Letters 23 (1986)
- `.kiro/specs/po-format/design.md` — データモデル、ライター、互換性テストの形
