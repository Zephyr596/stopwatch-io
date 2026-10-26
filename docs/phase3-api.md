# Phase 3：TeslaMate 只读适配与 HTTPS 联调

## 当前边界

已实现数据库视图 → 鉴权 HTTPS → ESP32 Provider → 缓存与仪表状态。服务器内部 HTTPS 和鉴权已验证；**设备到 AWS 公网链路尚未打通，不能宣称真实车辆端到端可用**。按照用户要求，SSH 授权失效后暂停服务器排查，先交付本地代码与验证。

应用、硬件与绘图边界继续沿用 [架构说明](architecture.md)。网络请求只在独立 worker 执行，UI 不等待 TLS。原板型、Flash、PSRAM 和 Arduino 版本保持不变。

## 数据契约

`GET /v1/vehicle`，HTTPS Bearer token，固定 Content-Length，无重定向。示例为合成值：

```json
{"schema_version":1,"source":"teslamate","battery_percent":78,"range_km":351,"speed_kmh":0,"charging":false,"observed_at":1793000000,"data_age_ms":60000,"vehicle_state":"offline","speed_valid":false,"range_kind":"rated"}
```

TeslaMate 响应必须提供 `vehicle_state`（online/offline/asleep）、`speed_valid`（布尔）、`range_kind`（estimated/rated/ideal）。fixture 来源用于测试，不冒充车辆。续航标签按实际类型显示。年龄为采集时间到响应时刻的真实差值，设备再加入请求耗时与本地经过时间；饱和到 uint32 上限，不让很旧的数据回绕成新数据。

离线、睡眠、速度未知、超过 15 秒、最近请求失败时隐藏速度。电量与续航可保留缓存，但状态明确显示过期或错误；不自动切换成 Mock。数据库快照有采样延迟，本实现不是实时驾驶仪表，也不会唤醒或控制车辆。

本次服务器中只有 car_id=2（Model Y）存在有效观测，因此视图明确选取 2。换车需修改 `backend/schema.sql` 并重新部署。位置与充电记录中取最新可用电量/续航观测，续航优先 estimated、其次 rated、最后 ideal；不返回 VIN、坐标或 Tesla 访问令牌。`/healthz` 仅表示 HTTP 服务可用，不证明数据库可用。

## 部署与维护

实现位于 `backend/`：Python 标准库 HTTP/TLS、psycopg2、PostgreSQL 受限视图。最多 4 个并发 worker，TLS 握手也在线程中处理；查询超时 2 秒、连接超时 3 秒，快照缓存 3 秒，每次返回重新计算年龄。鉴权成功的请求全服务限流 60 次/分钟。该限制不是公网 DDoS 防护。

现有部署目录 `/home/ubuntu/stopwatch-api`，Compose 项目 `stopwatch-api`，容器 `stopwatch-api-api-1`。服务绑定 EC2 主私网地址 443，再经公网地址访问；不占用已有 Tailscale 地址的监听。使用现有外部 Docker 网络 `teslamate_default`，不重启 TeslaMate。

部署前检查目标机器的数据库容器名、网络、车辆 ID、端口和 PostgreSQL schema；脚本针对本次环境，并非通用 TeslaMate 安装器。复制 `backend/` 的 Dockerfile、compose.yaml、server.py、model.py、schema.sql、deploy.sh、.dockerignore 到专用目录，然后运行：

```sh
PUBLIC_IP=<EC2公网IP> BIND_IP=<EC2主私网IP> sh deploy.sh
```

`deploy.sh` 首次生成私有 CA、有效期一年的服务器证书、随机 API token 与数据库密码。已存在的凭据不会自动轮换；CA 私钥仅留在服务器 `private/`。该目录应为 0700，并纳入私密备份。容器只挂载服务器证书/私钥和两个口令，不挂载 CA 私钥。服务以非 root 用户、只读根文件系统运行。构建上下文排除 private/。

数据库角色 `stopwatch_reader` 仅获视图 SELECT 权限，默认只读事务，无建库、建角色、超级用户权限，连接上限 4。生产已验证原始 positions 表无 SELECT/INSERT 权限。不要将该角色复用于其他应用或额外授予原始表权限。

设备的 `include/LocalConfig.h` 填入 HTTPS URL、API token、公有 CA 证书和 Wi-Fi。`SW_DEFAULT_API=true` 可默认 API；公开模板默认 Mock。配置与编译产物含敏感信息，不要公开分发固件。凭据不写日志或命令行参数。

轮换 API token 后需要重新部署容器并更新、烧录设备配置；数据库密码轮换需同步角色和挂载文件。更换 CA 时设备也必须更新。服务器证书续期应保留旧证书备份，重新签发并重建容器；仅重复执行 deploy.sh 不会替换已有证书。公网 IP 变化也需要重新签发身份匹配的证书。优先后续使用稳定域名。

回滚仅停止新增服务：

```sh
cd /home/ubuntu/stopwatch-api
docker compose -p stopwatch-api down
```

不会删除 TeslaMate 数据。视图和只读角色可保留；如需删除，先确认没有使用者，另行维护。设备可在 Settings 切回 Mock。

## 本地验证复现

```sh
./scripts/test-host.sh
python3 scripts/test-fixture.py
python3 -m unittest discover -s backend/tests -v
pio run
```

当前 Arduino 2.0.17 内置 mbedTLS 对纯 IP SAN 证书校验失败；实测同时包含 `IP:<地址>,DNS:<地址>` 后可用。部署脚本已为新证书生成双 SAN，仍验证 CA、有效期与对端身份，不使用 setInsecure。已部署服务器证书尚未应用此修复：恢复 SSH 后须备份并重新签发 server.crt/server.key（保留 CA），再重建容器。稳定域名及 DNS SAN 是后续优先方案。

局域网 TLS 测试使用 `scripts/tls-fixture.py` 和随机测试 token。生成短期测试证书（SAN 同时包含 IP:<本机LAN-IP>,DNS:<本机LAN-IP>）、将公有测试证书配置为设备信任根，临时填写 LAN HTTPS URL，重新烧录。证书、私钥、token、mode 文件均放在忽略的 `.local/` 内。

```sh
mkdir -p .local/tls-test
chmod 700 .local/tls-test
openssl req -x509 -newkey rsa:2048 -nodes -days 2 \
  -keyout .local/tls-test/key.pem -out .local/tls-test/cert.pem \
  -subj '/CN=<本机LAN-IP>' \
  -addext 'subjectAltName=IP:<本机LAN-IP>,DNS:<本机LAN-IP>' \
  -addext 'basicConstraints=critical,CA:TRUE'
openssl rand -hex 32 > .local/tls-test/token
chmod 600 .local/tls-test/key.pem .local/tls-test/token
printf live > .local/tls-test/mode
# 将测试 URL、token、证书配置到 LocalConfig.h 并重新烧录后：
python3 scripts/tls-fixture.py --host <本机LAN-IP> \
  --cert .local/tls-test/cert.pem --key .local/tls-test/key.pem \
  --token-file .local/tls-test/token --mode-file .local/tls-test/mode
~/.platformio/penv/bin/python scripts/smoke-api.py \
  --port /dev/cu.usbmodem3101 --mode-file .local/tls-test/mode --reconnect
```

测试覆盖充电、过期、非法 JSON 字段、503、慢响应、缓存恢复与断网重连。最后恢复生产配置并重新构建、烧录，关闭本地 fixture。具体通过/未通过项目见 [Phase 3 验证记录](phase3-validation.md)。

## 待完成

恢复 SSH 授权后检查公网 443 的安全组、主机防火墙、地址绑定及路由，用设备验证服务器证书，再完成真实数据联调。不要为排错禁用证书校验或公开数据库。完成实体触控/按键/圆屏视觉验收后，再推进屏幕休眠、防残影与更长期内存稳定性测试。
