// ============================================================================
// DemoButtons.cpp v3 - DIY G1000 演示固件（2026-09-12, demo only）
//
// 用途：按键矩阵硬件未接线，由固件周期性模拟按键/编码器事件，
//       验证 "Pro Micro -> RealSimGear 驱动 -> CommandMapping.ini -> MSFS"
//       端到端映射链路。
//
// v3 变更：双控制器支持。按 XFD_UNIT 自动切换握手名与动作表：
// v3.1 变更（2026-09-12）：PFD 板启用 12 个 AP 键（G1000.cpp unit 11 改 AP_NXI=1，
//   与 MFD 板一致，硬件接 MUX 模块 2 通道 0-11），PFD 动作表 61 -> 73 项
//   （41 single 按键 + 4 repeat 摇杆 + 28 编码器 UP/DN 事件）。
//   - XFD_UNIT=11 -> PFD 板：握手名 mrusk-G1000XFD2 / BOARD_ID 0011，
//     动作表 73 项，对照 G1000.cpp unit 11 注册序（含 AP_NXI 块，同 MFD 板）。
//   - XFD_UNIT=12 -> MFD 板（默认）：握手名 mrusk-G1000XFD1 / BOARD_ID 0012，
//     动作表 73 项，对照 G1000.cpp unit 12 注册序（含 AP_NXI 块），同 v2。
//
// 预期告警（非缺陷）：BTN_NAV_VOL / BTN_ALT_SEL / BTN_COM_VOL 在官方 CM 中
//       即为 UNAVAILABLE（分号注释禁用），固件仍会发送 → 驱动将记录
//       "not mapped"，属预期行为。除此之外不允许出现任何告警。
//       PFD 板 LEFT_PANEL 已禁用（MUX 2 通道 0-11 让位给 AP 键，且 Pro Micro
//       仅支持 6 个 MUX 模块 96 通道，装不下三组输入），SW_* demo 不发送。
//
// 串口协议与正式固件 src/G1000.cpp 逐字节一致：
//   - 115200 8N1
//   - keepalive 握手行（每 500ms，G1000.cpp loop()）:
//       PFD: ####RealSimGear#mrusk-G1000XFD2#1#3.1.2#0011\n
//       MFD: ####RealSimGear#mrusk-G1000XFD1#1#3.1.2#0012\n
//   - 按键：  "BTN_AP=1\n" 按下 / "BTN_AP=0\n" 释放（repeat 型持续发 =1）
//   - 编码器："ENC_HDG_UP\n" 单发（无按下/释放概念）
//
// 烧录：
//   MFD demo: pio run -e demo_mfd -t upload --upload-port <MFD板COM口>
//   PFD demo: pio run -e demo_pfd -t upload --upload-port <PFD板COM口>
// 恢复正式固件（源码无需任何改动）：
//   MFD: pio run -e micro_mfd -t upload
//   PFD: pio run -e micro_pfd -t upload
// ============================================================================

#ifdef DEMO_BUILD

#include <Arduino.h>

// ---- 与正式固件一致的单元选择（对照 G1000.cpp L8-10）----
#ifndef XFD_UNIT
#define XFD_UNIT 12
#endif

#if XFD_UNIT == 11
// PFD 板（unit 11）：独立握手名，避免与 MFD 板同名混淆
#define DEMO_DEVICE_NAME "mrusk-G1000XFD2"
#define DEMO_BOARD_ID    "0011"
#define DEMO_PFD 1
#else
// MFD 板（unit 12，默认）：沿用历史握手名
#define DEMO_DEVICE_NAME "mrusk-G1000XFD1"
#define DEMO_BOARD_ID    "0012"
#define DEMO_PFD 0
#endif

// ---- 与正式固件一致的握手参数（对照 G1000.cpp L446-459）----
static const char HANDSHAKE[] =
    "####RealSimGear#" DEMO_DEVICE_NAME "#1#3.1.2#" DEMO_BOARD_ID "\n";

static const unsigned long KEEPALIVE_DELAY_MS = 500;  // 与 KEEPALIVE_DELAY 一致
static const unsigned long BOOT_QUIET_MS      = 5000; // 上电静默期，等驱动连接
static const unsigned long ACTION_PERIOD_MS   = 1300; // 动作轮换间隔
static const unsigned int  BUTTON_HOLD_MS     = 120;  // 按键按住时长
static const unsigned int  ENCODER_GAP_MS     = 120;  // 编码器连发间隔
static const unsigned int  REPEAT_GAP_MS      = 120;  // repeat 型按键重复间隔

// ---- 事件名常量：全部放入 PROGMEM，节省 SRAM ----
// 未被动作表引用的常量会被链接器 --gc-sections 丢弃，不占 flash。
// unused 属性消除 PFD 构建（部分常量未引用）时的编译告警。
#define NAMESTR(x) static const char _s_##x[] PROGMEM __attribute__((unused)) = #x;

// MUX 0：导航/通信旋钮、航向、气压
NAMESTR(BTN_NAV_TOG)
NAMESTR(ENC_NAV_INNER_UP)
NAMESTR(ENC_NAV_INNER_DN)
NAMESTR(ENC_NAV_OUTER_UP)
NAMESTR(ENC_NAV_OUTER_DN)
NAMESTR(BTN_COM_TOG)
NAMESTR(ENC_COM_INNER_UP)
NAMESTR(ENC_COM_INNER_DN)
NAMESTR(ENC_COM_OUTER_UP)
NAMESTR(ENC_COM_OUTER_DN)
NAMESTR(BTN_CRS_SYNC)
NAMESTR(ENC_CRS_UP)
NAMESTR(ENC_CRS_DN)
NAMESTR(ENC_BARO_UP)
NAMESTR(ENC_BARO_DN)
// MUX 1：高度、FMS、界面键
NAMESTR(BTN_ALT_SEL)
NAMESTR(ENC_ALT_INNER_UP)
NAMESTR(ENC_ALT_INNER_DN)
NAMESTR(ENC_ALT_OUTER_UP)
NAMESTR(ENC_ALT_OUTER_DN)
NAMESTR(BTN_FMS)
NAMESTR(ENC_FMS_INNER_UP)
NAMESTR(ENC_FMS_INNER_DN)
NAMESTR(ENC_FMS_OUTER_UP)
NAMESTR(ENC_FMS_OUTER_DN)
NAMESTR(BTN_DIRECT)
NAMESTR(BTN_FPL)
NAMESTR(BTN_CLR)
NAMESTR(BTN_MENU)
NAMESTR(BTN_PROC)
NAMESTR(BTN_ENT)
// MUX 2：自动驾驶（仅 MFD：AP_NXI 块）+ NAV_VOL
NAMESTR(BTN_AP)
NAMESTR(BTN_FD)
NAMESTR(BTN_YD)
NAMESTR(BTN_HDG)
NAMESTR(BTN_NAV)
NAMESTR(BTN_ALT)
NAMESTR(BTN_VS)
NAMESTR(BTN_FLC)
NAMESTR(BTN_APR)
NAMESTR(BTN_VNAV)
NAMESTR(BTN_NOSE_UP)
NAMESTR(BTN_NOSE_DN)
NAMESTR(BTN_NAV_VOL)
NAMESTR(ENC_NAV_VOL_UP)
NAMESTR(ENC_NAV_VOL_DN)
NAMESTR(BTN_NAV_FF)
// MUX 3：软按键 + 音频面板
NAMESTR(BTN_SOFT_1)
NAMESTR(BTN_SOFT_2)
NAMESTR(BTN_SOFT_3)
NAMESTR(BTN_SOFT_4)
NAMESTR(BTN_SOFT_5)
NAMESTR(BTN_SOFT_6)
NAMESTR(BTN_SOFT_7)
NAMESTR(BTN_SOFT_8)
NAMESTR(BTN_SOFT_9)
NAMESTR(BTN_SOFT_10)
NAMESTR(BTN_SOFT_11)
NAMESTR(BTN_SOFT_12)
NAMESTR(BTN_COM_VOL)
NAMESTR(ENC_COM_VOL_UP)
NAMESTR(ENC_COM_VOL_DN)
NAMESTR(BTN_COM_FF)
// MUX 4：摇杆（repeat 型）、航向同步、范围
NAMESTR(BTN_PAN_SYNC)
NAMESTR(BTN_PAN_UP)
NAMESTR(BTN_PAN_LEFT)
NAMESTR(BTN_PAN_DN)
NAMESTR(BTN_PAN_RIGHT)
NAMESTR(ENC_RANGE_UP)
NAMESTR(ENC_RANGE_DN)
NAMESTR(ENC_HDG_UP)
NAMESTR(ENC_HDG_DN)
NAMESTR(BTN_HDG_SYNC)

enum ActType : uint8_t
{
    ACT_BTN = 0,     // single 按键：=1 ... =0
    ACT_BTN_RPT = 1, // repeat 按键：=1 + N 次重复 =1 ... =0（模拟按住，如摇杆平移）
    ACT_ENC = 2      // 编码器：单发 UP/DN 事件 x count
};

struct Action
{
    uint8_t type;
    PGM_P   name;
    uint8_t count;
};

#define BTN(name)     { ACT_BTN,     _s_##name, 1 }
#define BTN_RPT(name) { ACT_BTN_RPT, _s_##name, 2 }
#define ENC(name, n)  { ACT_ENC,     _s_##name, n }

#if DEMO_PFD
// ---- PFD 动作表：73 项，顺序对照 G1000.cpp unit 11 的事件注册（MUX 0 -> 4）----
// 注：unit 11 自 v3.1 起编译 AP 按键块（AP_NXI=1，与 MFD 板一致）；
//     LEFT_PANEL 已禁用（MUX 2 通道让位给 AP 键），SW_* 开关 demo 不发送。
static const Action ACTIONS[] = {
    // ---- MUX 0：导航/通信/航向/气压 ----
    BTN(BTN_NAV_TOG),
    ENC(ENC_NAV_INNER_UP, 2), ENC(ENC_NAV_INNER_DN, 2),
    ENC(ENC_NAV_OUTER_UP, 2), ENC(ENC_NAV_OUTER_DN, 2),
    BTN(BTN_COM_TOG),
    ENC(ENC_COM_INNER_UP, 2), ENC(ENC_COM_INNER_DN, 2),
    ENC(ENC_COM_OUTER_UP, 2), ENC(ENC_COM_OUTER_DN, 2),
    BTN(BTN_CRS_SYNC),
    ENC(ENC_CRS_UP, 2), ENC(ENC_CRS_DN, 2),
    ENC(ENC_BARO_UP, 2), ENC(ENC_BARO_DN, 2),
    // ---- MUX 1：高度/FMS/界面 ----
    BTN(BTN_ALT_SEL), // 官方 UNAVAILABLE：预期 not mapped（正常）
    ENC(ENC_ALT_INNER_UP, 2), ENC(ENC_ALT_INNER_DN, 2),
    ENC(ENC_ALT_OUTER_UP, 2), ENC(ENC_ALT_OUTER_DN, 2),
    BTN(BTN_FMS),
    ENC(ENC_FMS_INNER_UP, 2), ENC(ENC_FMS_INNER_DN, 2),
    ENC(ENC_FMS_OUTER_UP, 2), ENC(ENC_FMS_OUTER_DN, 2),
    BTN(BTN_DIRECT), BTN(BTN_FPL), BTN(BTN_CLR),
    BTN(BTN_MENU), BTN(BTN_PROC), BTN(BTN_ENT),
    // ---- MUX 2：自动驾驶（AP_NXI 块，与 MFD 板同序）----
    BTN(BTN_AP), BTN(BTN_FD), BTN(BTN_YD), BTN(BTN_HDG),
    BTN(BTN_NAV), BTN(BTN_ALT), BTN(BTN_VS), BTN(BTN_FLC),
    BTN(BTN_APR), BTN(BTN_VNAV), BTN(BTN_NOSE_UP), BTN(BTN_NOSE_DN),
    // ---- MUX 2 续：NAV_VOL 组 ----
    BTN(BTN_NAV_VOL), // 官方 UNAVAILABLE：预期 not mapped（正常）
    ENC(ENC_NAV_VOL_UP, 2), ENC(ENC_NAV_VOL_DN, 2),
    BTN(BTN_NAV_FF),
    // ---- MUX 3：软按键 + 音频面板 ----
    BTN(BTN_SOFT_1), BTN(BTN_SOFT_2), BTN(BTN_SOFT_3), BTN(BTN_SOFT_4),
    BTN(BTN_SOFT_5), BTN(BTN_SOFT_6), BTN(BTN_SOFT_7), BTN(BTN_SOFT_8),
    BTN(BTN_SOFT_9), BTN(BTN_SOFT_10), BTN(BTN_SOFT_11), BTN(BTN_SOFT_12),
    BTN(BTN_COM_VOL), // 官方 UNAVAILABLE：预期 not mapped（正常）
    ENC(ENC_COM_VOL_UP, 2), ENC(ENC_COM_VOL_DN, 2),
    BTN(BTN_COM_FF),
    // ---- MUX 4：摇杆/航向同步/范围 ----
    BTN(BTN_PAN_SYNC),
    BTN_RPT(BTN_PAN_UP), BTN_RPT(BTN_PAN_LEFT),
    BTN_RPT(BTN_PAN_DN), BTN_RPT(BTN_PAN_RIGHT),
    ENC(ENC_RANGE_UP, 2), ENC(ENC_RANGE_DN, 2),
    ENC(ENC_HDG_UP, 3), ENC(ENC_HDG_DN, 3),
    BTN(BTN_HDG_SYNC),
};
#else
// ---- MFD 动作表：73 项，顺序对照 G1000.cpp unit 12 的事件注册（MUX 0 -> 4）----
static const Action ACTIONS[] = {
    // ---- MUX 0：导航/通信/航向/气压 ----
    BTN(BTN_NAV_TOG),
    ENC(ENC_NAV_INNER_UP, 2), ENC(ENC_NAV_INNER_DN, 2),
    ENC(ENC_NAV_OUTER_UP, 2), ENC(ENC_NAV_OUTER_DN, 2),
    BTN(BTN_COM_TOG),
    ENC(ENC_COM_INNER_UP, 2), ENC(ENC_COM_INNER_DN, 2),
    ENC(ENC_COM_OUTER_UP, 2), ENC(ENC_COM_OUTER_DN, 2),
    BTN(BTN_CRS_SYNC),
    ENC(ENC_CRS_UP, 2), ENC(ENC_CRS_DN, 2),
    ENC(ENC_BARO_UP, 2), ENC(ENC_BARO_DN, 2),
    // ---- MUX 1：高度/FMS/界面 ----
    BTN(BTN_ALT_SEL), // 官方 UNAVAILABLE：预期 not mapped（正常）
    ENC(ENC_ALT_INNER_UP, 2), ENC(ENC_ALT_INNER_DN, 2),
    ENC(ENC_ALT_OUTER_UP, 2), ENC(ENC_ALT_OUTER_DN, 2),
    BTN(BTN_FMS),
    ENC(ENC_FMS_INNER_UP, 2), ENC(ENC_FMS_INNER_DN, 2),
    ENC(ENC_FMS_OUTER_UP, 2), ENC(ENC_FMS_OUTER_DN, 2),
    BTN(BTN_DIRECT), BTN(BTN_FPL), BTN(BTN_CLR),
    BTN(BTN_MENU), BTN(BTN_PROC), BTN(BTN_ENT),
    // ---- MUX 2：自动驾驶（AP_NXI 块）----
    BTN(BTN_AP), BTN(BTN_FD), BTN(BTN_YD), BTN(BTN_HDG),
    BTN(BTN_NAV), BTN(BTN_ALT), BTN(BTN_VS), BTN(BTN_FLC),
    BTN(BTN_APR), BTN(BTN_VNAV), BTN(BTN_NOSE_UP), BTN(BTN_NOSE_DN),
    BTN(BTN_NAV_VOL), // 官方 UNAVAILABLE：预期 not mapped（正常）
    ENC(ENC_NAV_VOL_UP, 2), ENC(ENC_NAV_VOL_DN, 2),
    BTN(BTN_NAV_FF),
    // ---- MUX 3：软按键 + 音频面板 ----
    BTN(BTN_SOFT_1), BTN(BTN_SOFT_2), BTN(BTN_SOFT_3), BTN(BTN_SOFT_4),
    BTN(BTN_SOFT_5), BTN(BTN_SOFT_6), BTN(BTN_SOFT_7), BTN(BTN_SOFT_8),
    BTN(BTN_SOFT_9), BTN(BTN_SOFT_10), BTN(BTN_SOFT_11), BTN(BTN_SOFT_12),
    BTN(BTN_COM_VOL), // 官方 UNAVAILABLE：预期 not mapped（正常）
    ENC(ENC_COM_VOL_UP, 2), ENC(ENC_COM_VOL_DN, 2),
    BTN(BTN_COM_FF),
    // ---- MUX 4：摇杆/航向同步/范围 ----
    BTN(BTN_PAN_SYNC),
    BTN_RPT(BTN_PAN_UP), BTN_RPT(BTN_PAN_LEFT),
    BTN_RPT(BTN_PAN_DN), BTN_RPT(BTN_PAN_RIGHT),
    ENC(ENC_RANGE_UP, 2), ENC(ENC_RANGE_DN, 2),
    ENC(ENC_HDG_UP, 3), ENC(ENC_HDG_DN, 3),
    BTN(BTN_HDG_SYNC),
};
#endif

static const uint8_t ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

static unsigned long tmr_keepalive = 0;
static unsigned long tmr_action    = 0;
static uint8_t       action_index  = 0;

// 驱动可能下发数据（查询/亮度等），demo 不处理但必须丢弃防止缓冲堆积
static void drainRx()
{
    while (Serial.available())
        Serial.read();
}

static void sendState(const char *proName, const char *suffix)
{
    char buf[20];
    strcpy_P(buf, proName);
    Serial.write(buf);
    Serial.write(suffix);
    Serial.flush();
}

// repeat 按键：按下 + count 次重复 =1（模拟按住，如摇杆平移）+ 释放
static void pressButtonRepeat(PGM_P proName, uint8_t repeats)
{
    sendState(proName, "=1\n");
    for (uint8_t i = 0; i < repeats; i++)
    {
        delay(REPEAT_GAP_MS);
        sendState(proName, "=1\n");
    }
    delay(BUTTON_HOLD_MS);
    sendState(proName, "=0\n");
}

// 编码器单发事件 x N（与 handleEncoder 消息格式一致）
static void encoderTick(PGM_P proName, uint8_t times)
{
    char buf[20];
    strcpy_P(buf, proName);
    for (uint8_t i = 0; i < times; i++)
    {
        Serial.write(buf);
        Serial.write("\n");
        Serial.flush();
        delay(ENCODER_GAP_MS);
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Pro Micro 板载 LED 低电平点亮，先熄灭
}

void loop()
{
    unsigned long now = millis();

    // 每 500ms keepalive 握手（字节与正式固件完全一致）
    if (now >= tmr_keepalive)
    {
        Serial.write(HANDSHAKE);
        tmr_keepalive += KEEPALIVE_DELAY_MS;
    }

    drainRx();

    // 上电静默期过后，每 1.3 秒轮换模拟一个动作
    if (now < BOOT_QUIET_MS)
        return;
    if (now < tmr_action)
        return;
    tmr_action = now + ACTION_PERIOD_MS;

    // 板载 RX LED 短闪一下，提示正在发出模拟按键
    digitalWrite(LED_BUILTIN, LOW);
    delay(40);
    digitalWrite(LED_BUILTIN, HIGH);

    const Action &a = ACTIONS[action_index % ACTION_COUNT];
    switch (a.type)
    {
    case ACT_BTN:
        sendState(a.name, "=1\n");
        delay(BUTTON_HOLD_MS);
        sendState(a.name, "=0\n");
        break;
    case ACT_BTN_RPT:
        pressButtonRepeat(a.name, a.count);
        break;
    case ACT_ENC:
        encoderTick(a.name, a.count);
        break;
    }
    action_index++;
}

#else
// 非 demo 环境：编译为空翻译单元，正式固件 (G1000.cpp) 完全不受影响
#endif
