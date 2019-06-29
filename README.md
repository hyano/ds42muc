# ds42muc
日本ファルコムのMSX/MSX2用ゲームであるドラゴンスレイヤーIV ドラスレファミリー(以下ドラスレIV)のサウンドデータをMUCOM88形式のMMLに変換するソフトウェアです。

## 概要
ドラスレIV(MSX/MSX2版)のサウンドデータを
[MUCOM88](https://www.ancient.co.jp/~mucom88/)のMML形式に変換します。
変換したデータは、[MUCOM88 Windows](https://onitama.tv/mucom88/)で演奏可能なほか、
N88-BASIC形式に変換すれば、PC-88実機やエミュレータで動作するMUCOM88でも演奏可能になります。

簡単なプログラムですので、
同等の機能を備えたほかのサウンドドライバ向けへの移植も容易です。

## 動作環境
* 一般的なC言語で記述されています。
* コマンドライン実行向けのプログラムです。
* Windowsのmsys2のMinGW(gcc)環境でビルドと実行を確認しています。

## ビルド
```sh
git clone https://github.com/hyano/ds42muc.git
cd ds42muc
make
```

## 使い方
### 基本的な使い方
```sh
./ds42muc -o 出力ファイル ROMイメージファイル
```

### コマンドラインオプション
実行時に以下のオプションを指定することができます。

  * <b>-h</b>

    ヘルプを表示して終了します。

  * <b>-v</b>

    デバッグ用の出力を有効にします。
    出力されたデータはコンパイルできません。

  * <b>-w</b>

    警告を無視して強制的に変換します。
    現時点では警告項目がないため効果がありません。

  * <b>-o</b> `FILE`

    出力先のファイル名を指定します。
    指定がない場合は、標準出力に出力します。

  * <b>-n</b> `BGM番号`

    変換するBGMの番号を指定します。
    0から19までが有効です。範囲チェックはしません。

  * <b>-m</b> `VERSION`

    `#mucom88`タグの内容を指定します。

  * <b>-t</b> `TITLE`

    `#title`タグの内容を指定します。

  * <b>-a</b> `AUTHOR`

    `#author`タグの内容を指定します。

  * <b>-c</b> `COMPOSER`

    `#composer`タグの内容を指定します。

  * <b>-d</b> `DATE`

    `#date`タグの内容を指定します。

  * <b>-C</b> `COMMENT`

    `#comment`タグの内容を指定します。


### コマンドラインの例
#### メイア・ウォーゼンの曲をMMLに変換
```sh
COMMENT="[MSX] Dragon Slayer IV - DRASLEFAMILY -"
COMPOSER="古代祐三"
AUTHOR="日本ファルコム"
DATE="1987/07/09"
ROM=ds4msx2.rom
./ds42muc -n 2 -d "$DATE" -a "$AUTHOR" -C "$COMMENT" -c "$COMPOSER" -t "メイア・ウォーゼン" -o muc/DS4_02.MUC $ROM
```

## 注意事項
* サウンドデータは各自で入手してください。
* 本ソフトウェアで変換したデータを不正に利用しないでください。
* 限られたデータと出力オプションでのみ動作確認しています。
* 本ソフトウェアは、エラー処理を実装しておりません。
  想定外のデータを入力すると、誤動作します。
* ドラスレIVのサウンドドライバとMUCOM88の機能やデータ形式は非常によく似ていますが、
  実装は異なるため、同じ鳴り方をしない部分があります。

## ライセンス
このソフトウェアは[MITライセンス](LICENSE)にて提供しています。  
Copyright (c) 2019 Hirokuni Yano