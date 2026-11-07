# Requirements Document

## Project Description (Input)
**誰が困っているか**: `monde` の利用者。`monde` を入手し、翻訳フローでの使い方を日本語で理解する必要がある。RFP は、日本語のドキュメントと、GitHub での配布を成果物として求めている。

**現状**: README.md は最小限の内容しかない。利用マニュアルとリリースのワークフローはない。

**何を変えるか**:
- 日本語の README と利用マニュアル（`docs/` 配下）で、インストール、ソースからのビルド、各サブコマンドとオプション、2 つの翻訳フローの手順がわかるようにする。README には概要、インストール、クイックスタート、ビルド手順、ライセンス、spec.txt などのサードパーティ表記を載せる。
- タグを付けると、GitHub Actions のワークフローが Windows、macOS、Linux 向けにビルドしてアーカイブを作り、GitHub Releases に公開する。バージョン番号の付け方を定める。

**範囲外**: パッケージマネージャーへの登録、英語版のドキュメント、機能の追加や変更、開発者向けの内部設計ドキュメント。

**制約**:
- ドキュメントは日本語で書く。利用マニュアルは monde-cli で確定したサブコマンドとオプションの仕様から作る。
- CC-BY-SA 4.0 のテストデータ（spec.txt）はリリース成果物に含めない。
- 著作権者は The Monde Authors、ライセンスは MIT とする。
- 利用マニュアルの既知の制限に、yaml-front-matter の design.md の「改訂」の節の「受け入れた制限」（ルートがインデント 0 のスカラーの継続、フロー形式の継続行のインデント、ダブルクオートの中の U+2028・U+2029）を載せる。
- 上流: monde-cli（CLI 仕様）、project-foundation（ビルドと CI）。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
