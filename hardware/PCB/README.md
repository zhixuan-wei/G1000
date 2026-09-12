# PCB 生产说明（拼板流程文档）

本目录中的 Gerber 压缩包为立创 EDA（EasyEDA）导出的原始子板 Gerber。
实际打样时为降低成本，将多块子板拼成一块大板一次生产。
裁剪后的单块子板 Gerber 已收录于同级目录 `hardware/cut_gerber/`；
拼版（panelized）成品 Gerber 依据 PCB 授权协议（CC BY-NC-ND 4.0，见 `hardware/LICENSE`）不随本仓库分发。
本文件记录完整复现流程与关键参数，供自行拼板参考。

## 生产流程

1. **清洗原始 Gerber**
   - 剔除 EasyEDA 导出中的无关内容：GDL 私有文档层、document 层中的尺寸标注等，仅保留有效电气/机械层。
2. **裁切出单块子板（crop / split）**
   - 按 Board Outline（GKO）将原始整版 Gerber 分割为独立子板文件，得到各子板的干净 Gerber 集。
   - 对每块子板做轮廓提取与尺寸测量，核对实际外形尺寸后再进入拼板。
3. **拼板（panelize）**
   - 按布局文件排布 16 块子板：MUX（旋转 90°）、APL、MICRO_HAT、FMS、Range、Enc ×3、FF ×2、DualEnc ×5、Softkey。
   - 板间距 2.0 mm；板间以 **mouse-bite（邮票孔）** 连接：每条桥长 8.0 mm，桥上钻 5 个 Ø0.5 mm NPTH 孔、孔距 0.8 mm（追加到 NPTH 层）。
   - 生成带断口的新 GKO；各层合并时 aperture 全局重编号，drill 合并为统一刀具表。
4. **出图检查**
   - 合成预览图核对桥位与子板边界，确认无误后再提交打样订单；单个大板订单到货后沿 mouse-bite 掰开即得各子板。

## 关键参数速查

| 参数 | 值 |
|---|---|
| 拼板尺寸 | 216.50 × 136.56 mm |
| 子板数量 | 16 |
| 板间距 | 2.0 mm |
| 连接桥长度 | 8.0 mm |
| 每桥孔数 / 孔径 / 孔距 | 5 × Ø0.5 mm @ 0.8 mm |

## 授权说明

- 本目录 PCB 文件授权：CC BY-NC-ND 4.0（禁止分发演绎版本）。
- 本仓库包含：原始子板 Gerber、裁剪后的单板 Gerber（`hardware/cut_gerber/`）与本流程说明；不含派生的拼版成品 Gerber。
