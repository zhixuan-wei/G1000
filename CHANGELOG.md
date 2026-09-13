# 更新日志 (Changelog)

本项目所有重要变更记录于此文件。
格式参考 [Keep a Changelog](https://keepachangelog.com/)，版本遵循语义化版本。

## [2.0.1] - 2026-09-14

### 修复 (Fixed)

- **配置**：NOSE UP/DN 改为释放沿（`-=`）触发，修复 FLC 模式下按住一次连续变速的问题
- **固件**：MFD 板（unit 12）禁用未接线的 RIGHT_PANEL（`RIGHT_PANEL 1->0`，`MAX_SWITCHES`/`MAX_POTIS` 归零），修复悬空模拟脚受 DM13A 数字噪声耦合、无操作时持续上报 `SW_INSTR_1/2`、`SW_FLOOD_1/2` 事件刷屏的问题
- **驱动映射**：激活映射 v2.2 将两板共 104 条 BTN 规则由保持语义（裸 `=`，按住重复触发）改为按下沿（`+=`，单次触发），覆盖 AP/softkey/FMS/FF/PAN/电台等全部按键；NOSE 保持 `-=`，CLR 长按语法、ENC/Range、LED 行不变
- **新增存档**：实测通过的双控制器驱动映射存档为 `config/MSFS/CommandMapping.mrusk-dual-v2.2.ini`，与驱动激活映射逐行一致，可直接复制回 RealSimGear 目录恢复

## [2.0.0] - 2026-09-12

### 新增 (Added)

- **双控制器支持（MFD + PFD）**：两块 Arduino Pro Micro 分别作为独立 USB 设备接入
  - 引入 `DEVICE_NAME` 编译期机制：握手帧中的设备名按单元参数化（`####RealSimGear#<DEVICE_NAME>#1#<VERSION>#<BOARD_ID>`），RealSimGear 驱动据此区分 MFD / PFD 并加载 `CommandMapping.ini` 中对应配置段
    - `XFD_UNIT=12`（MFD 板）→ `mrusk-G1000XFD1`
    - `XFD_UNIT=11`（PFD 板）→ `mrusk-G1000XFD2`
    - 未显式定义 `DEVICE_NAME` 时回退到历史名称 `mrusk-G1000XFD1`，保持旧固件行为兼容
- **PFD 板 Autopilot 按键激活**（`src/G1000.cpp`，`XFD_UNIT == 11` 分支）：
  - `AP_NXI` 由 `0` 改为 `1`：PFD 板 MUX 模块 2 的通道 0-11 接入 12 个 AP 键（通道 0 为 BTN_AP，通道 11 为 BTN_NOSE_DN，完整键序见 `src/G1000.cpp` 内注释）
  - `LEFT_PANEL` 由 `1` 改为 `0`：AP_NXI 与 LEFT_PANEL 共用 MUX 2，二者互斥，启用 AP 键后必须关闭 LEFT_PANEL
  - 资源占用：buttons 45/50、encoders 14/15
- **自定义板级定义 `boards/mrusk_promicro.json`**：
  - 支持 RealSimGear USB 身份（hwid `0x2341:0x0010`、`0x1B4F:0x9206/0x9207`、`0x2341:0x0037`）的 avr109 直连烧录（57600bps，1200bps touch）
  - `[env:micro_pfd]`、`[env:micro_mfd]` 的 `board` 由 `sparkfun_promicro16` 改为 `mrusk_promicro`（因固件刷入后 USB VID/PID 变化，原板定义无法再识别端口）
- **演示固件 `src/DemoButtons.cpp` + `[env:demo_mfd]` / `[env:demo_pfd]`**：
  - 不依赖驱动的按键/编码器自检固件（`DEMO_BUILD` 宏），用于装机后验证接线
  - 动作表按单元参数化：PFD 61 项（无 AP 键版本）、MFD 73 项
- **裁剪子板 Gerber `hardware/cut_gerber/`**：9 块子板（APL、DualEnc、Enc、FF、FMS、HAT、Key、MUX、Range）裁剪后的单板 Gerber，可供直接打样，或按 `hardware/PCB/README.md` 流程拼板

### 变更 (Changed)

- `platformio.ini`：`[platformio]` 段新增 `boards_dir = boards`

### 硬件说明 (Hardware Notes)

- AP 键物理上位于 MFD 控制器面板，但固件中将 12 个 AP 键映射到 PFD 板（`XFD_UNIT=11`）的 MUX 通道上报，映射细节见 `src/G1000.cpp` 注释
- 双控制器连接示例（COM 口号因机器而异）：控制器 1 = `XFD_UNIT 12`（MFD，`mrusk-G1000XFD1`）；控制器 2 = `XFD_UNIT 11`（PFD，`mrusk-G1000XFD2`）

### 烧录速查

```bash
pio run -e micro_pfd -t upload   # PFD 板 (XFD_UNIT=11)
pio run -e micro_mfd -t upload   # MFD 板 (XFD_UNIT=12)
pio run -e demo_pfd -t upload    # PFD 按键自检固件
pio run -e demo_mfd -t upload    # MFD 按键自检固件
```

## [1.0.0]

- 基于上游 `realsimgear` 分支原始单控制器固件的初始调通版本（单块 Pro Micro，`XFD_UNIT=12`，握手名 `mrusk-G1000XFD1`）
