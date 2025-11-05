# PicoRapidX

Raspberry Pi Picoを使用したゲームコントローラー用連射・マクロ機能実装デバイス

## 特徴

- 12入力×12出力の柔軟な入出力設定
- 7種類の連射モード + マクロモード
- 最大150フレームのマクロ機能（内部処理は120フレーム推奨）
- USB MSCモードでの設定ファイル編集
- SSD1306 OLEDディスプレイ対応（128×64ピクセル、I2C）
- 2つの基板モード（A/B）に対応
- フラッシュメモリによる設定保存

## ハードウェア仕様

- **マイコン**: Raspberry Pi Pico (RP2040)
- **フラッシュ**: W25Q16JV (Block 31使用)
- **ディスプレイ**: SSD1306 OLED (128×64, I2C address 0x3C)
  - SDA: GP20
  - SCL: GP21
- **同期信号**: GP28 (垂直同期信号入力、約60Hz)
- **LED**: GP25
- **ボタン**:
  - モードスイッチ: GP27
  - 決定ボタン: GP26
  - 設定ボタン: GP16-19

## 連射モード

| モード | 説明 |
|--------|------|
| R5 | 5fps連射 |
| R10 | 10fps連射 |
| R12 | 12fps連射 |
| R15 | 15fps連射（同期信号使用） |
| R20 | 20fps連射 |
| R30 | 30fps連射（同期信号使用） |
| R60 | 60fps連射 |
| MACRO | マクロ再生モード |

## 使い方

### 通常モード
電源投入で通常の連射・マクロ機能が動作します。

### USB MSCモード
起動時にモードボタンを押しながら電源を入れるとUSBストレージとして認識され、設定ファイルを編集できます。

#### 提供されるファイル
- `Setting.txt` - IO設定と連射設定
- `MACRO_00.txt` ～ `MACRO_11.txt` - 各入力用のマクロファイル

#### ファイル編集の制限
- **直接編集**: 最大141フレームまで
- **ドラッグ&ドロップ**: 150フレーム完全対応（推奨）

マクロファイルを保存すると、自動的に該当入力の連射モードがMACROに設定されます。

## ドキュメント

- [設計思想](DESIGN.md) - 重要な設計判断と技術的背景
- [ハードウェア仕様](docs/hardware.md) - GPIO配置と基板モード
- [フラッシュメモリマップ](docs/flash-memory-map.md) - メモリ配置とデータ構造
- [MSCモード仕様](docs/msc-mode.md) - USBストレージモードの詳細
- [マクロフォーマット](docs/macro-format.md) - マクロファイルの形式

## ビルド方法

### 必要な環境
- Pico SDK
- CMake
- Ninja または Visual Studio

### ビルド手順
```bash
cd PicoRapidX
mkdir build_ninja
cd build_ninja
cmake -G Ninja -DPICO_SDK_PATH=c:/develop/pico-sdk ..
ninja
```

生成される `PicoRapidX.uf2` をPicoに書き込みます。

## 開発者

- sabokaichou

## ライセンス

このプロジェクトは個人用途での使用を想定しています。
