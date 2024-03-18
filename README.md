# touchscreen driver

raspberrypi-ts ドライバをi2c用に改造した raspberrypi-i2c-ts ドライバ。

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
