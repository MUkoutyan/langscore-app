# 注意点 {#points_to_note}

## ウイルス対策ソフトに検出される

Langscore内部で使用しているrvcnv.exeが、トロイの木馬として誤検出されることがあります。

該当ソフトはウイルスではないので、除外をお願いします。

### 各ソフトのMD5値

Powershellやハッシュ値比較ソフトでMD5の比較を行う際に利用してください。

* Langscore.exe : LANGSCORE_HASH
* divisi.exe : DIVISI_HASH
* rvcnv.exe : RVCNV_HASH