# touchscreen driver

raspberrypi-ts ドライバをi2c用に改造した raspberrypi-i2c-ts ドライバ。


## How to build

```
make DTC=/usr/bin/dtc
sudo make install
sudo depmod -A
sudo cp rpi-i2c-ts.dtbo /boot/overlays/
```

linux-headers パッケージには、scripts/dtc が含まれていないので dtc コマンドへのフルパスを DTC 変数で指定する必要があります。
ビルド済みの source tree を KDIR 変数で指定する場合は不要です。

source tree の外で modules_install した場合、
> Warning: modules_install: missing 'System.map' file. Skipping depmod.

となり、depmod が実行されないので手動で depmod する。


## 使い方

/boot/firmware/config.txt へ下記の様にに記述する。
```
dtoverlay=vc4-kms-dsi-7inch,disable_touch
dtoverlay=rpi-i2c-ts
```

 Touch Display を DISP 0 へ接続する場合は、以下とする。
```
dtoverlay=vc4-kms-dsi-7inch,dsi0,disable_touch
dtoverlay=rpi-i2c-ts,dsi0
```

## 背景

Pi 5 で 7" Official Touch Display を使用する場合は、
/boot/firmware/config.txt に
```
dtoverlay=vc4-kms-dsi-7inch
```
を追記します。
すると edt-ft5506 も同時に有効になります。

しかし、7" Official Touch Display 互換のサードパーティ製ディスプレイの中には、edt-ft5506 ドライバで正常に動作しないものがあります。
これは raspberrypi-ts ドライバを想定しているためと思われます。

> raspberrypi-ts ドライバがアクセスしないレジスタがリードされると、SCK を Low にしたままになり、タッチ操作およびバックライト制御がでなくなる。
> 電源OFFするまで復旧しない。

vc4-kms-dsi-7inch パラメータに disable_touch を指定することで edt-ft5506 を無効にし、代わりにこのドライバを使用します。


Pi 4 までは、touchscreen は firmware 配下でした。  
```
/ {
    soc {
        firmware {
            /* rpi-ft5406.dtbo */
            ts: touchscreen {
                compatible = "raspberrypi,firmware-ts";
            };
        };
    };
};
```

Pi 5 では、RP1チップのi2cバス配下に変わっています。  
```
/ {
    axi: axi {
        pcie2: pcie@120000 {
            rp1: rp1 {
                i2c_csi_dsi: i2c@80000 { /* rp1_i2c4 i2c_csi_dsi1, i2c4 */

                    /* edt-ft5406.dtbo */
                    ft5406: ts@38 {
                        compatible="edt,edt-ft5506";
                        reg = <0x38>;
                    };
                };

                i2c_csi_dsi0: i2c@88000 { /* rp1_i2c6, i2c6 */
                };

            };
        };
    };
};
```
