# 自动数羊

集羊圈出口漏斗 + 4 道单列通道。门口 ESP32-S3 光电计数、TM1637 数码管显示；ESP32-C3 手持器 ESP-NOW 遥控；断网本地计数，有网再同步云。

详细方案见 `docs/`，电子购买清单见 `docs/bom.md`。

## 仓库结构

- `firmware/` —— PlatformIO 工程（单工程多环境）
  - `env:gate` 门口主机 ESP32-S3；`env:handheld` 手持器 ESP32-C3；`env:native` PC 上跑纯逻辑单元测试
  - `lib/SheepSession/` 会话状态机 + 分道光电判定（不依赖 Arduino，可主机编译测试）
- `cloud/` —— FastAPI + SQLite 云端小服务，一页历史表格（出门/回圈/未归）+ 设备上报接口
- `docs/` —— 方案与 BOM

## 开发环境

Cloud Agent 环境由 `.cursor/environment.json` + `.cursor/install.sh` 定义：装 PlatformIO（固件工具链）、预取 ESP32 平台，并装云服务依赖。本地手动准备：

```bash
bash .cursor/install.sh          # 装 PlatformIO 与云服务依赖（幂等）
```

固件：

```bash
cd firmware
../.venv/bin/pio test -e native  # 逻辑单元测试（无需硬件）
../.venv/bin/pio run -e gate      # 编译门口固件
../.venv/bin/pio run -e handheld  # 编译手持固件
```

云服务：

```bash
cd cloud
SHEEP_TOKEN=dev-token .venv/bin/uvicorn app:app --host 127.0.0.1 --port 8000
.venv/bin/python sim_device.py    # 无硬件时: 模拟门口设备跑一场并上报，浏览器看 http://127.0.0.1:8000
```
