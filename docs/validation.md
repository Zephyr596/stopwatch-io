# 验证记录（2026-10-26，Asia/Shanghai）

## 构建、镜像与基线

- 开始时 Git 工作区干净，原始最小固件 `pio run` 成功。
- 保留原平台、板型、分区、USB、PSRAM 和 Flash 配置；新增 C++17 并固定原有库 SHA。
- 最终 `pio run` 成功，无本次代码编译警告 / 错误。
- 编译工具链：Xtensa ESP32-S3 GCC 8.4.0，Arduino 2.0.17 / ESP-IDF 4.4.7，PlatformIO espressif32 6.12.0。
- 静态 RAM：23,472 / 327,680 字节（7.2%）；应用 Flash：534,533 / 6,553,600 字节（8.2%）。静态 RAM 报告不含运行时库缓冲区 / PSRAM 分配。
- 应用镜像 `firmware.bin`：534,896 字节。esptool `image_info --version 2` 确认 Chip ID 9 / ESP32-S3、16MB、80MHz、校验与 hash 有效。镜像头为 DIO（保留的 PlatformIO/Arduino 生成结果），内存配置仍为 `qio_opi`，未自行改 Flash 模式。
- SHA256：`06afa3884d0ba7bc57c7e64e6ee82fbe11171a3f54a561dffb60b82d1da634f7`。重新构建可能改变镜像，以上对应本次烧录版本。

## Host 自动化

`./scripts/test-host.sh` 成功，启用 C++17、`-Wall -Wextra -Werror -pedantic`、UndefinedBehaviorSanitizer。

覆盖：

- 无活动应用时的 update/input、未知 App ID、空注册项、重复进入。
- 明确的 enter/exit 顺序、只更新活动应用、全局 Home 不泄漏给应用。
- 10,000 次往返切换（20,000 次 transition）。
- Launcher 选择循环、触摸命中、右侧边界与区域外点击。
- Settings 开关、前后台偏好保留、主操作 / Home 焦点。
- Tesla 充电按钮 / 触摸动作、重新进入焦点复位、空 Demo 回调安全性。
- Provider 返回失败时撤销 valid 状态。
- Mock 初始有效性、1 秒节拍、立即模式切换、充电停车、电量上下限、速度 / 续航范围。
- 5 秒 stale 判断和 uint32_t millis 回绕。
- 所有交互区域完整处于圆屏安全边界内。

AddressSanitizer 在本机 macOS 26.6.2 / Apple 工具链的 `AsanInitInternal → InitializeShadowMemory → StaticSpinMutex` 初始化阶段卡住，尚未进入测试 main；已终止该进程。因此仅报告 UBSan 与断言通过，不报告 ASan / LeakSanitizer 通过。应用长期无泄漏也不能由短期堆稳定性完全证明。

## 真实设备：已执行

- USB `/dev/cu.usbmodem3101`，ESP32-S3 QFN56 revision v0.2；esptool 识别 8MB 嵌入式 PSRAM、16MB Flash。
- `pio run -t upload --upload-port ...` 成功；写入应用、bootloader / 分区相关镜像并校验 Flash hash。
- 常规 USB RTS reset 曾停留下载模式；使用 esptool `--after watchdog_reset flash_id` 恢复正常启动。未修改 PMIC 或 eFuse。
- 固件串口确认 M5StopWatch 自动识别，`board=30 display=468x468 touch=1`。
- PSRAM 报告 8,386,215 字节可识别容量，UI 48,640 字节 tile 创建成功。
- Launcher 成功启动；电池 API 返回 100（USB 接入时的电压估算，不能等同实测容量）。
- 无 Wi-Fi 配置、连接或 HTTP 代码；设备在本固件的离线模式正常进入应用。

以下为 USB 注入输入的集成测试；它会经过与实体事件相同的 Manager / App / Renderer 路径，但不驱动实体按键或触摸控制器。

`scripts/smoke-device.py --port /dev/cu.usbmodem3101 --cycles 100` 全部通过：

- Launcher 静止 1.2 秒，paint counter 不变。
- Tesla 在 1.2 秒观察窗口内 revision 增长；切换充电后速度为 0，关闭充电恢复行驶。
- 验证 Motorcycle / Settings ID 与 App 内 Home 控件，随后 100 轮遍历全部应用；每轮 6 次 App transition，共 600 次切换，外加预热与功能测试。
- 切换前后可用 heap 均为 **353,636 字节**；minimum heap 均为 **348,396 字节**。没有观察到持续堆下降。
- paint counter 从 80 增至 3418，确认实际渲染路径参与测试；不存在仅切换逻辑但不调用绘制的假验证。
- 测试结束时返回 Launcher，选择 Tesla；没有通过测试启用振动。
- 脚本最终输出：`PASS: offline startup, idle redraw suppression, mock updates, charging, all apps, switching and heap stability`。

本次设备空闲健康采样中 PSRAM free 为约 7,899,327 字节；框架静态 RAM、运行时内部堆和 PSRAM 属于不同统计口径。

## 实机人工验收清单（尚未代替人执行）

1. 上电确认 466 圆屏内标题、全部卡片与 Home 完整可见；检查字号、色彩、无残影 / 明显闪烁及触摸方向。
2. 短按实体 A 循环 Tesla → Motorcycle → Settings；B 打开；每个应用长按 A 700ms 返回一次，松手不误打开应用。
3. 点击三张 Launcher 卡片；检查边缘、拖动、长按、多指、快速连续点击不会误触；Home 点击可靠。
4. Tesla 连续观察至少 60 秒：模拟速度变化；点击状态卡片 / B 切换充电，速度立即为 0；恢复行驶正常。确认 DEMO / MOCK 始终清晰。
5. Settings 默认振动关闭；手动开启后每次有效输入应短振约 35ms，快速连按与关闭时不持续振动；检查噪声 / 触感。
6. 拔掉 USB，正常电池开机、使用与关机；独立电源键行为正常；USB 接入 / 拔出后电池显示合理，低电量与电池读失败显示需单独验证。
7. 至少运行 1 小时，反复进入所有应用，观察 `[health]` 的 heap / min / psram，无持续下降、看门狗重启或异常温升。
8. 分别检查 USB 常规上传后复位和冷启动；记录是否仍需 watchdog reset。

## 已知限制

- 仅代码几何检查和设备初始化 / 绘制调用成功，不等同于看到屏幕成像；没有拍照或实际触摸 / 按键操作证据。
- 未故障注入 PSRAM 分配失败、触摸缺失、电池 I2C 错误、棕断 / 低电量；代码包含降级或错误输出。
- 未测最坏帧时延、触控延迟、功耗、热表现或 AMOLED 长期残影。
- UI 保持英语以避免引入中文字库；电池值是电压估算；设置只在 RAM 中保留。
- 单点短点击；同轮输入取一个，A 长按返回优先。没有复杂手势、动画或屏幕休眠。
- Provider 的来源标识与 Demo 控制尚未泛化；接入真实 Provider 时需同步修改 UI 标签与控制行为。
