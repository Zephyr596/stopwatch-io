# Phase 1 架构与硬件依据

## 执行模型

所有 App、Provider、Manager、Renderer 在 `main.cpp` 静态实例化。Registry 持有 4 个固定条目，不提供动态注册、卸载或堆分配。Manager 拒绝未知 ID、空应用和重复进入；切换顺序固定为旧应用 `onExit` → 更新活动指针 → 新应用 `onEnter`。只有当前应用收到 update / input。输入处理返回目标 ID，回调结束后才切换，避免应用在自己的调用栈中触发生命周期重入。

应用不引用其它应用。Launcher 用静态目录展示入口；Tesla 仅引用 `VehicleDataProvider`，Demo 切换通过组装时提供的回调控制 Mock，不把 Mock 类型写进 Tesla App。未来只读网络 Provider 可传空回调。Settings 使用 RAM 保存偏好，不写 Flash。

Renderer 独立读取应用状态，不发起数据请求或改变生命周期。输入命中框与 UI 共用常量；466×466 中心安全区内排列三张 320×76 卡片，Host 检查所有卡片及 Home 按钮角点距中心小于 223px。官方驱动当前提供 468×468 逻辑尺寸，UI 仍以标称 466 的安全区布局，不修改驱动 offset 或触控标定。

## 主循环、刷新与内存

每轮 M5.update → 检查输入 / 振动截止 → Manager input → 活动应用 update → 最多每 33ms 检查渲染。底部 `vTaskDelay(1)` 只让出一个 FreeRTOS tick；无 UI 长延时、网络等待或连接串口等待。显示和 I2C 调用本身是同步的，尚未测量最坏执行时间，因此不宣称硬实时。

卡片通过固定长度字符串 key 比较，仅变化的卡片推送屏幕；清全屏只发生在应用切换。Renderer 启动时分配一次 320×76×2 = 48,640 字节 tile，优先 PSRAM，不足时尝试内部 RAM。两次均失败则停止 App 启动并报告错误。M5GFX 自身另有 StopWatch panel framebuffer，占用不能只按 tile 计算。显示帧缓冲由官方驱动管理。

应用切换不分配或释放对象，业务更新无 String / vector / new。电池 / 内存每 5 秒读取，健康日志每 30 秒输出。固定时差均使用无符号减法支持 millis 溢出；Mock 每秒产生至多一次快照，暂停后不补跑大量历史 tick。状态 5 秒未更新则视为 stale。

启动校验实际板型与显示尺寸，触控缺失时按键仍可操作；电池未知显示 `--`。失败时循环仍让出 CPU，不进入无限重试。独立电源按键及 PMIC 标准初始化交给官方库；没有应用层寄存器写入、充电电流修改或电源轨开关。

## 依赖与硬件确认

本次依赖从现有 `.pio/libdeps` 的干净 Git checkout 读取 SHA 并固定，不升级框架或替换板型：

| 库 | 版本 | 提交 |
|---|---|---|
| M5Unified | 0.2.28 | `368ad60807161625aa65baad37a4a3af2a7eccea` |
| M5GFX | 0.2.35 | `65c816a38386062d2c3dc47d41237a9083acbc75` |
| M5PM1 | 1.0.7 | `be9a5456c007c333e7ac963f33bfde1ffa5d82ee` |
| M5IOE1 | 1.0.9 | `846eec7d05e25c09013be2acdb8804487f48a62e` |

官方 Arduino IDE 示例建议较新的 M5Stack Board Manager；本工程保留已实际编译与启动的 PlatformIO Arduino 2.0.17 基线，不将 Arduino IDE 板包版本直接套用到 PlatformIO 平台。

| 功能 | 已核对的官方实现 |
|---|---|
| A / B | M5Unified `board_M5StopWatch` 分支：GPIO2 / GPIO1，低电平有效 |
| 电源键 | 不作为 App 输入；不读取 BtnPWR |
| 内部 I2C | SDA47 / SCL48；应用不重复配置总线 |
| AMOLED QSPI | CS39、CLK40、IO0..3 为 41/42/46/45，TE38 |
| 触控 | CST820B 硬件，库通过 `Touch_CST816S` 兼容驱动；IRQ13，M5.Touch 统一事件与坐标 |
| 电池 | M5.Power.getBatteryLevel，M5PM1 电压估算；负值表示不可用 |
| 振动 | M5.Power.setVibration，官方 StopWatch 分支使用 M5IOE1 PWM1 / IO9；35ms 异步截止，默认关闭 |

应用不直接控制上述引脚。Display 使用 M5.Display，触摸使用 `wasClicked()`（抬起的短点击，不将拖动当点击），A/B 使用 `wasClicked()` 和 A 的 `wasHold()`。同轮只派发一个事件，Home 优先；多指同时点击仅采纳首个事件。实际方向、抬起行为、GPIO 对应机身哪侧仍需要实体确认。

查阅来源：

- [StopWatch C152 官方硬件说明](https://docs.m5stack.com/en/core/StopWatch)
- [官方按键示例](https://docs.m5stack.com/en/arduino/stopwatch/button)
- [官方触控示例](https://docs.m5stack.com/en/arduino/stopwatch/touch)
- [锁定版本 M5Unified 主实现](https://github.com/m5stack/M5Unified/blob/368ad60807161625aa65baad37a4a3af2a7eccea/src/M5Unified.inl)
- [锁定版本电源与振动实现](https://github.com/m5stack/M5Unified/blob/368ad60807161625aa65baad37a4a3af2a7eccea/src/utility/Power_Class.inl)
- [锁定版本显示与触摸初始化](https://github.com/m5stack/M5GFX/blob/65c816a38386062d2c3dc47d41237a9083acbc75/src/M5GFX.cpp)

## 扩展约束

新增应用：实现 Application，将静态对象加入 Registry 并扩展 Launcher 目录与 Renderer 页面。当前 4 项 Registry 是有意保留的编译期容量；新增应用需调整数组容量，避免引入注册框架。

新增数据源：实现 update / getState，保留缓存快照和明确的有效性。update 必须有界；不能直接在主循环加入会长时间等待的 HTTP 请求。真实 API 的时间戳应新增为独立 UTC 字段，不能混用当前 monotonic updatedAtMs。未来应增加状态来源 / 错误原因枚举；当前 UI 的 MOCK 标签只适用于本 Demo，替换 Provider 时需一起更新来源展示和移除 Demo 控制。

当前无 LVGL：M5GFX 已满足三卡片 UI，额外控件树、tick / display adapter 和内存管理没有必要。暂不引入依赖注入框架、任务调度框架或插件系统。
