# picnic_io

[PICNIC2](https://www.tristate.ne.jp/picnic.htm)（トライステート製）のADC値をUDP経由で取得するためのI/Oライブラリおよびサンプルクライアントです。

> **注意:** PICNIC2は既に販売終了しており、現在は入手できません。

2026年の今となっては、ほとんど需要はないかもしれませんが、
自前IoTシステム向けに以前実装したものを暫定公開します。

## 実行環境

- Linux
- C++17
- CMake

## ビルド(linux)

```bash
./build.sh
```

## 使い方

### 引数指定

```bash
./picnic_cli <ADDRESS:PORT> [INTERVAL]
```

- `ADDRESS:PORT` — PICNIC2のIPアドレスとポート番号（例: `192.168.0.200:10020`）
- `INTERVAL` — ポーリング間隔（秒）。デフォルト3秒。0を指定すると1回だけ取得して終了。

```bash
# 例: 3秒間隔で取得
./picnic_cli 192.168.0.200:10020 3

# 例: 1回だけ取得
./picnic_cli 192.168.0.200:10020 0
```

### 対話モード

引数なしで実行すると、アドレス・ポート・間隔を対話的に入力できます。

```bash
./picnic_cli
# IP Address [192.168.0.200]:
# Port [10020]:
# Interval sec [3]:
```

### ヘルプ

```bash
./picnic_cli --help
```

## 呼び出し方

```cpp
#include "picnic_io.hpp"

PICNICIO picnic;
picnic.open("192.168.0.200:10020");

std::vector<uint16_t> values;
if (picnic.getValues(values)) {
    for (size_t i = 0; i < values.size(); i++) {
        printf("ADC Ch%zu: %u\n", i, values[i]);
    }
}

picnic.close();
```

| メソッド | 説明 |
|---|---|
| `open(address)` | 接続を開く。`address`は`"IP:PORT"`形式 |
| `close()` | 接続を閉じる |
| `isOpened()` | 接続中かどうか |
| `getValues(values)` | 8チャンネルのADC値を取得 |
| `getDebugMsg()` | 直前の操作のデバッグメッセージを取得 |

## License

[MIT License](./LICENSE)
