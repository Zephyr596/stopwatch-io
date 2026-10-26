# Phase 2：Wi-Fi 与车辆 API 接入准备

本文保留第二阶段设计背景；当前部署与扩展契约以 [Phase 3](phase3-api.md) 为准。

本阶段不依赖 SSH 访问 TeslaMate 即可构建；真实服务器适配与联调仍待完成。没有新增车辆控制接口。

## 本地配置与构建

复制 `include/LocalConfig.example.h` 为 `include/LocalConfig.h` 后填写 Wi-Fi / API 配置。后者已被 `.gitignore` 排除；本机已按用户最新要求配置 ZephY-2.4G。密码不写入 README、日志或编译命令参数。编译产物含凭据，不应公开分发。Arduino 核心日志从 verbose 降至 warning，避免 HTTP 调试日志打印认证头。

```sh
pio run
pio run -t upload --upload-port /dev/cu.usbmodem3101
./scripts/test-host.sh
python3 scripts/test-fixture.py
```

PlatformIO 平台、板型、USB、Flash / PSRAM 参数均沿用 Phase 1。新增依赖仅 `ArduinoJson @ 6.21.5`，固定文档容量与输入大小；不引入 Web 框架或新构建系统。

配置字段：

| 宏 | 含义 |
|---|---|
| SW_WIFI_SSID / SW_WIFI_PASSWORD | 2.4GHz Wi-Fi；空 SSID 时保持离线 |
| SW_API_URL | 完整只读 endpoint，空值时明确显示 API NOT CONFIGURED |
| SW_API_TOKEN | HTTPS Bearer token；没有 token 时不发送 Authorization |
| SW_API_CA | HTTPS endpoint 对应的 PEM 信任根证书 |
| SW_ALLOW_HTTP_TEST | 默认 false；仅允许无 token、source=fixture 的 HTTP 测试数据 |

HTTPS 必须配置 CA，证书校验不使用 `setInsecure()`；不跟随重定向。联网后异步 SNTP 校时，时间未就绪时返回 CLOCK SYNC NEEDED，界面不等待校时。真实服务器端还需部署访问鉴权，最小只读权限与凭据轮换。

## 交互

- 公开模板默认启动 Mock；本机可用 SW_DEFAULT_API 配置启动来源，连上 Wi-Fi 不会自动变成真实车辆模式。
- Settings 第一张卡片显示 Wi-Fi 状态、IP 和设备电池估算。
- 点击第二张卡片在 Mock / API 间切换；按键焦点循环为 Haptic → Home → Source。刚进入 Settings 后按 A 两次，再按 B 可切换来源。
- Tesla 的 Mock 模式保留模拟充电开关；API 模式的 B / 点击状态卡片仅请求刷新，绝不发送充电控制指令。
- API 无有效数据时显示占位；失败保留电量 / 续航缓存，同时显示缓存、离线或过期状态。
- 只有新鲜且最近请求成功的数据可显示速度，否则速度为 `--`。测试响应始终标记 TEST FIXTURE，不冒充真实数据。
- USB：`a/b/h/s` 保持原功能，`m` 切换 Mock，`r` 切换 API，`w` 主动断开设备 Wi-Fi 并在 30 秒后重连，用于诊断。`s` 增加 wifi/mock/age/api_error/worker_stack/live/source 字段。

## 请求、缓存与线程边界

`Network` 负责连接与重试：首次连接立即发起，15 秒未成功则结束尝试，30 秒节拍重试。第一次超时后在冷却期异步扫描诊断；不记录附近 SSID 列表。没有 Wi-Fi 等待循环。

`ApiVehicleDataProvider` 使用一个低优先级 FreeRTOS worker；所有 DNS、TCP、TLS、HTTP 和 JSON 解析均在 worker 中执行。主循环仅使用长度为 1 的请求 / 结果队列，零等待发送和接收。只有主循环拥有与更新缓存，worker 不调用 M5GFX、不操作应用指针。任务 / 队列在启动时创建一次；未配置 API 时不创建。失败时释放已分配的队列并报告 WORKER ERROR。

每次最多一个请求。自动请求间隔至少 5 秒；手动刷新也不快于 2 秒。切回 Mock 后不再调度新的 API 请求，已有请求允许完成。HTTPClient 与 TLS 会在 worker 内分配并释放内部内存，因此不能宣称整个网络层零分配。

连接超时 3 秒、流读取超时 2.5 秒、TLS 握手超时 5 秒；body 读取检查总请求时长 7 秒。DNS / 底层库仍受它们自身超时约束，**7 秒不是整个请求严格的墙钟上限**，但不会阻塞 UI 线程。HTTP 仅接受 200、已知 Content-Length 且不超过 1536 字节的响应，不支持 chunked、压缩或重定向。JSON 文档容量 1024 字节、嵌套深度最多 2。

缓存保存最后一次成功快照及原始接收时间，失败不重置时间。API 新鲜阈值为 15 秒；年龄 = 后端提供的数据年龄 + 保守计算的传输耗时 + 本地接收后经过的时间。年龄饱和处理，不与 UTC 时间混算。缓存只在 RAM 中保留，重启后不会展示持久化旧速度。

## 拟定 API 契约

`GET /v1/vehicle`，成功响应为 `application/json`，必须带准确的 Content-Length，建议 `Cache-Control: no-store`。以下是接口契约示例，不是实际车辆数据：

```json
{
  "schema_version": 1,
  "source": "teslamate",
  "battery_percent": 78.0,
  "range_km": 351.0,
  "speed_kmh": 0.0,
  "charging": true,
  "observed_at": 1793000000,
  "data_age_ms": 2000,
  "vehicle_state": "online",
  "speed_valid": true,
  "range_kind": "estimated"
}
```

- `observed_at` 为采集时刻的 Unix 秒，不是 API 响应生成时刻。
- `data_age_ms` 必须由后端根据真实采集时刻计算。不能每次返回旧值时将其重置为 0；TeslaMate 睡眠 / 离线状态尤其需要保留真实年龄。
- `source` 只能为 `teslamate` 或 `fixture`。明文 HTTP 测试模式拒绝 teslamate 来源。
- 数值必须为 JSON number，不接受数字字符串、null 或 bool；charging 必须为 bool。
- 校验范围：电量 0..100、续航 0..1500km、速度 0..350km/h、年龄 0..4294967295ms；充电时速度必须为 0。超出范围应由后端返回无数据 / 错误，设备保留旧缓存并标明异常。
- 401/403/429/5xx 等均视为错误，不更新缓存；目前 UI 合并显示 HTTP ERROR，尚未按状态码区分重试策略。
- 这不是对 TeslaMate 已有 REST API 的假定。后续需要在服务器部署适配层，把实际数据库 / MQTT 状态映射到此契约，并核实 TeslaMate 原始字段单位。

## 本地测试接口

`scripts/vehicle-fixture.py` 使用 Python 标准库，只生成合成数据。默认监听 127.0.0.1；设备测试时明确绑定设备能够访问的本机 LAN 地址。不应部署为生产服务。

```sh
printf live > /tmp/stopwatch-fixture-mode
python3 scripts/vehicle-fixture.py --host <本机LAN地址> \
  --mode-file /tmp/stopwatch-fixture-mode
```

临时将本地配置中的 URL 指向 `http://<LAN地址>:8765/v1/vehicle` 并开启 HTTP 测试开关。修改 mode 文件可模拟 live、charging、stale、invalid、oversized、error、slow、truncated、chunked、redirect、auth、real_over_http。测试结束恢复空 URL / false 并重新烧录。

`test-fixture.py` 会自动启动一个 loopback 服务，将 HTTP 响应送入**固件使用的同一个 C++ JSON 解码器**，验证正常、充电、过期、格式错误、超大响应和 401/503。此测试不等价于 ESP32 HTTP/TLS 传输验证。

## 后续服务器工作

解锁 SSH 后检查 TeslaMate 部署、版本和可用数据来源，再实现只读适配层与鉴权。完成 HTTPS CA、真实数据采集年龄、车辆睡眠状态、限流和断网恢复联调后，才能声称真实车辆链路可用。

参考：[Arduino-ESP32 Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)、[ArduinoJson 6 deserializeJson](https://arduinojson.org/v6/api/json/deserializejson/)。实现参数同时核对了当前安装的 Arduino 2.0.17 WiFi / HTTPClient / WiFiClientSecure 源码。
