# Phase 2 验证记录（2026-10-26）

> 本文为 Phase 2 历史记录；当前配置与结果以 [Phase 3 验证](phase3-validation.md) 为准。

## 最新 Wi-Fi 验证

用户更新 SSID 为 `ZephY-2.4G` 后，已重新编译、烧录并通过 Flash hash 校验。串口确认 `[wifi] connected ip=192.168.31.52 rssi=-35`，状态 `wifi=1`。当前 Wi-Fi 连接阻塞已解除；密码仍仅保存于 Git 忽略的本地配置。API URL 保持空值，默认 Mock；真实 API / HTTPS 联调未在本次连接验证中执行。以下 JK1007 / ZephY 记录为此前测试历史。

## 已完成

- 保留 espressif32 6.12.0、Arduino 2.0.17、ESP32-S3、原 16MB Flash / OPI PSRAM 与 USB 配置。
- Wi-Fi 凭据只写入 Git 忽略的本机配置；最初尝试 ZephY，用户确认未开启 2.4GHz 后改为 JK1007。
- 新增 ArduinoJson 6.21.5、后台只读 API Provider、固定容量队列、缓存 / stale 状态、Mock / API 选择及 UI 展示。
- PlatformIO 构建与多次烧录成功，包括配置临时 HTTP fixture endpoint 时的完整 HTTP/JSON worker 构建。
- 最终交付配置：JK1007、空 API URL、HTTP 测试开关关闭、默认 Mock。临时 LAN fixture 服务已关闭。

## Host 测试

`./scripts/test-host.sh` 成功：原生命周期 / 10,000 次往返切换 / 输入 / Mock 测试继续通过；新增真实固件 JSON 解码器的 schema、字段类型、范围、缺失、超大输入、嵌套深度测试，验证失败不修改原快照。

缓存测试覆盖 API LIVE / API TEST / CACHED / STALE / OFFLINE 状态、失败保留接收时间与 revision、上游数据年龄、uint32 回绕和饱和处理。Settings 按键 / 触摸数据源选择测试通过。Host 使用 UBSan；ASan 的本机兼容限制见 Phase 1 记录。

`python3 scripts/test-fixture.py` 成功：通过真正的 loopback HTTP 请求获取合成数据，再送入固件使用的 C++ 解码器；正常、充电、过期、格式错误、超大响应和 HTTP 401/503 均符合预期。它验证接口契约，不代表 ESP32 网络传输已通过。

## 设备端测试

- 启动确认 StopWatch、触控驱动、PSRAM 和 UI tile 正常初始化。
- 启用 Wi-Fi 重试时执行 `smoke-device.py --cycles 100`，全部通过：静止 Launcher 不重绘、Mock 更新 / 充电切换、各 App 路径及 600 次应用切换。
- 该轮测试发生在 ZephY 配置阶段且未连接 Wi-Fi：可用 heap 从 280,940 变为 280,204 字节（差 736 字节），minimum heap 保持 278,744 字节。没有观察到持续下降；这不是 HTTP/TLS 内存稳定性测试。
- 改为 JK1007 后已重新编译、烧录，串口确认固件正常运行，但仍得到 `Reason: 201 - NO_AP_FOUND`，尚未进入密码验证。
- 对 JK1007 执行了一次临时“启动先扫描，后连接”的诊断，独立扫描也未成功完成。这意味着还不能把原因简单归为 SSID 或密码错误。诊断修改已恢复，未修改硬件电源 / 射频引脚或强行更换框架。

最终 JK1007 固件额外验证：API 模式无 endpoint 时 valid=0 / live=0 / api_error=1，进入 Tesla 仍为占位；显式切回 Mock 后恢复有效数据，再返回 Launcher。串口状态请求响应约 2–3ms（小样本，不代表所有场景最坏延迟）。设备最终停在 Launcher / Mock。

最终构建静态 RAM 49,452 / 327,680 字节（15.1%），Flash 949,641 / 6,553,600 字节（14.5%）；应用镜像 950,000 字节。烧录成功并通过 Flash hash 验证。镜像 SHA256：`ddd689f309f12bca8aeaadbe94c27f211e8b0c6eb3aec5270e49164b2f94d5f1`。

Git 忽略规则与凭据检查通过：本地配置被忽略，当前密码 / token 未出现在 tracked 或 unignored 源文件中。

## 仍未验证 / 阻塞

- 此前 JK1007 未连接；现已通过 ZephY-2.4G 获得 DHCP IP，见顶部最新记录。未对旧热点失败原因作进一步判定。
- 尚无法验证 ESP32 到 LAN fixture 的完整请求、真实网络断开再连、SNTP 成功、HTTPS CA / token、worker 在慢响应与 TLS 期间的栈 / 内存稳定性。连接恢复后应逐项执行。
- SSH 仍依赖用户解锁 1Password；没有读取、修改或部署 TeslaMate 服务器。
- API 契约是拟定的适配层契约，真实 TeslaMate 字段 / 单位 / 数据年龄尚待映射。真实车辆链路未接通。
- UI 的真实画面观感、实体输入、振动、耗电和长期运行仍需要人工硬件验收。

## 网络恢复后的验收步骤

1. 串口出现 `[wifi] connected ip=...`，Settings 显示 IP；用 `s` 确认 wifi=1。
2. 配置局域网 fixture URL（同一可达网络）、HTTP 测试开关 true，烧录；使用 `r` 选择 API，确认 source=2、valid=1、live=1。
3. 依次切换 fixture 的 charging / stale / invalid / oversized / auth / error / truncated / chunked / redirect / slow 模式；无效响应不得覆盖缓存；过期 / 请求错误后速度应隐藏；slow 时 A/B/Home 与串口应持续响应。
4. `w` 触发设备断网，观察 OFFLINE / CACHE，约 30 秒后自动重连并恢复快照；不得退回 Mock。
5. 在成功 / 错误 / 重连循环中观察 heap 和 worker_stack，检查持续泄漏、任务栈不足或看门狗重启。
6. 配置真实后端 HTTPS / CA / token，验证证书错误、401、429、数据采集时间与睡眠车辆。完成后撤掉测试 URL / fixture 标记。
