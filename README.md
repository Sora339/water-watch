# M5StickC PLUS2 BLE UART Sample

このプロジェクトは、M5StickC PLUS2を使用してiPhoneやAndroidとBluetooth Low Energy (BLE) で通信するサンプルです。
Nordic UART Service互換のUUIDを使用しているため、一般的なBLEアプリで通信テストが可能です。

## 必要なもの

- M5StickC PLUS2
- スマートフォン (iPhone または Android)
- BLEスキャナー/ターミナルアプリ
    - **iOS**: [nRF Connect](https://apps.apple.com/jp/app/nrf-connect-for-mobile/id1054362403) または [LightBlue](https://apps.apple.com/jp/app/lightblue/id557428110)
    - **Android**: [nRF Connect](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp) または [Serial Bluetooth Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal)

## ビルドと書き込み

Arduino IDEを使用します。

1. Arduino IDEを開きます。
2. **ライブラリマネージャ** から以下のライブラリをインストールします。
    - `M5StickCPlus2` by M5Stack
    - `NimBLE-Arduino` by h2zero
3. `M5StickCPlus2_BLE/M5StickCPlus2_BLE.ino` を開きます。
4. ボード設定で `M5StickC PLUS2` を選択します。
5. 書き込みボタン（矢印）をクリックして書き込みます。

## 使い方 (CTSモード)

1. 書き込みが完了すると、画面に "BLE CTS" "Waiting..." と表示されます。
2. iPhoneの「設定」->「Bluetooth」を開きます。
3. デバイス一覧から **"M5StickC_CTS"** を探してタップします。
4. **「Bluetoothペアリングの要求」** というポップアップが出ます。**「ペアリング」** をタップしてください。
    - ※これを許可しないと時刻を読み取れません。
5. 接続が完了すると、自動的にiPhoneの時刻がM5StickC PLUS2に同期され、時計が表示されます。

**注意:**
- うまくいかない場合は、iPhoneのBluetooth設定でデバイス登録を解除（削除）してから、再度試してください。
- Androidでも同様にCTSに対応している機種であれば動作する可能性があります。
