# M5Stack StopWatch — App Terminal

将 C152 扩展为静态多应用终端：圆屏 Launcher、可交互 Tesla Mock 仪表、Motorcycle 占位页和设备信息 / 振动设置。已加入异步 Wi-Fi、Mock / API 切换、只读 HTTP(S) Provider 与缓存状态。没有网络也能使用 Launcher / Mock；已加入 TeslaMate 只读后端；服务器内部验证通过，公网端到端联调待完成。

最新交付与部署见 [Phase 3](docs/phase3-api.md)，结果见 [Phase 3 验证](docs/phase3-validation.md)。

第二阶段历史说明见 [Wi-Fi / API 文档](docs/phase2-network-api.md) 和 [第二阶段验证](docs/phase2-validation.md)。本机 Wi-Fi 凭据只在 Git 忽略的 `include/LocalConfig.h` 中，公开模板不含密码。

## 构建与烧录

保持原工程根目录和已验证的配置：`espressif32 @ 6.12.0`、`esp32s3box`、Arduino 2.0.17、16MB Flash、`qio_opi` / 8MB PSRAM。`esp32s3box` 是构建基线，实际硬件由 M5GFX/M5Unified 自动识别；不要把它理解为使用 BOX 的引脚驱动。

```sh
pio run
pio device list
pio run -t upload --upload-port /dev/cu.usbmodem3101
pio device monitor --port /dev/cu.usbmodem3101 --baud 115200
```

端口以本机 `pio device list` 为准；本次连接端口为 `/dev/cu.usbmodem3101`。若终端找不到 `pio`，使用 `~/.platformio/penv/bin/pio`，或 VS Code 的 PlatformIO Terminal。

输出：`.pio/build/m5stack-stopwatch/firmware.bin` 与 `firmware.elf`。`firmware.bin` 是应用镜像，不是从地址 0 烧写的合并镜像；使用上述 PlatformIO 上传命令，由其配置 bootloader、分区表和偏移。当前默认分区的应用槽约 6.25MiB，16MB 是芯片容量。

如果 USB 烧录成功但串口只显示 `waiting for download`，可使用当前 esptool 的复位方式恢复（不改 PMIC 配置）：

```sh
~/.platformio/penv/bin/python \
  ~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32s3 --port /dev/cu.usbmodem3101 --after watchdog_reset flash_id
```

也可正常关机再开机。操作串口前关闭其它监视器，避免端口被占用。

## 操作

| 输入 | Launcher | Tesla / Settings | Motorcycle |
|---|---|---|---|
| A 短按并松开 | 下一项，循环 | Tesla：主操作 / Home；Settings：Haptic / Home / Source | 无操作 |
| B 短按并松开 | 打开选中项 | 执行选中操作 | 返回 Home |
| A 长按 700ms | 保持 Home | 返回 Home | 返回 Home |
| 触摸 | 点击卡片直接打开 | 点击状态卡片切换；点击 Home 返回 | 点击 Home 返回 |

Tesla 在 Mock 模式下切换模拟充电 / 行驶；充电时速度为 0。API 模式下主操作仅刷新只读状态。Settings 的第二张卡片可点击切换数据源；按键操作为进入后 A 两次再 B。Settings 的主操作是启用 / 关闭短振动反馈，默认关闭、重启不保存。绿色边框标识当前按键焦点。长按释放不应再触发短按；电源键保持官方行为，不接入应用输入。

Tesla 显示电量、对应类型的续航、速度、充电状态及数据年龄。Mock 明确显示 `DEMO` / `MOCK`；API 显示真实来源或 `TEST FIXTURE`，过期/离线时隐藏速度。API 使用采集年龄加本机单调计时，TLS 使用 SNTP 校时。当前选中的 Provider 在后台持续 update；Mock 每秒更新，API 请求交给独立 worker。失败缓存不更新数据年龄，不自动回退 Mock。

USB 调试命令：`a/b/h` 注入下一项 / 确认 / Home；`s` 输出状态；`m/r` 选择 Mock / API；`w` 测试断开 Wi-Fi，30 秒后重连。这仅用于诊断，不证明实体输入可靠性。无需串口连接也可独立运行。

## 模块

```text
platformio.ini          原构建入口，锁定已验证依赖提交，C++17
src/main.cpp            静态组装、主循环、限频绘制、USB 诊断
src/hardware/           M5Unified 生命周期、输入、电池读取、短振动
src/core/               无 Arduino 依赖的应用接口、Registry / Manager、命中区域
src/apps/               Launcher、Tesla、Motorcycle、Settings 应用状态
src/services/           模型、Mock / API Provider、JSON 校验、缓存
src/networking/         异步 Wi-Fi 连接 / 重试
include/                本地配置模板与配置入口
src/ui/                 M5GFX 分区渲染、重复使用的 PSRAM tile
scripts/                Host 测试与 USB 集成测试
backend/                TeslaMate 受限视图与鉴权 HTTPS 服务
tests/                  Host 断言测试（无网络、无硬件库）
docs/                   架构、引脚依据、验证记录与实机清单
```

模块放在 `src/` 下，利用 PlatformIO 的递归编译，避免空库、LDF 嵌套库发现问题和目录迁移。`backend/` 包含只读 HTTPS 适配服务；暂不创建 cad 空目录。

## 测试

```sh
./scripts/test-host.sh
python3 scripts/test-fixture.py
python3 -m unittest discover -s backend/tests -v
# 设备已运行本固件，关闭串口监视器后：
~/.platformio/penv/bin/python scripts/smoke-device.py \
  --port /dev/cu.usbmodem3101 --cycles 100
```

Host 使用 C++17、严格警告和 UBSan。兼容的系统可用 `SANITIZERS=address,undefined ./scripts/test-host.sh`；本机 Apple ASan 在初始化时死锁，不能宣称 ASan 通过。

Phase 1 历史结果见 [验证记录](docs/validation.md)，硬件依据见 [架构说明](docs/architecture.md)。第二阶段以页面顶部链接的说明与结果为准。

## 范围与下一阶段

已实现完整静态 App 流程与可切换充电的仪表 Demo。已有 TeslaMate 数据库适配与 AWS 服务部署，但设备公网联调尚未通过。尚无交互式 Wi-Fi 配网、OTA、BLE、GPS、CAN、语音或应用安装；不是可用于驾驶决策的真实仪表。设备电量是官方接口基于电压的估算，非精确电量计。

下一步完成真实服务器只读接口与 HTTPS 联调，并继续触控、实体按键、显示与供电实测。API Provider、缓存和过期提示已实现。保持 `VehicleState` 与 UI 分离；后端已提供受限视图、鉴权和 TLS；接下来解决公网可达性并验证实际数据。长期常亮前增加屏幕休眠与 AMOLED 防残影策略。
