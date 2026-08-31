#!/usr/bin/env bash
# 自动数羊 Cloud Agent 环境安装脚本（幂等，可重复执行）
#   - 固件工具链: PlatformIO 装在仓库根 .venv, 预取 ESP32 平台/工具链/库
#   - 云服务: cloud/.venv 装 FastAPI 等依赖
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "[install] repo: $REPO_ROOT"

# 1) venv 所需系统包
if ! dpkg -s python3.12-venv >/dev/null 2>&1; then
  echo "[install] apt: python3.12-venv"
  sudo apt-get update -qq
  sudo apt-get install -y -qq python3.12-venv
fi

# 2) 固件工具链: PlatformIO
if [ ! -x .venv/bin/pio ]; then
  echo "[install] create .venv + platformio"
  python3 -m venv .venv
  .venv/bin/pip install -q -U pip wheel
  .venv/bin/pip install -q -U platformio
fi
echo "[install] $(.venv/bin/pio --version)"

# 3) 预取 ESP32 平台 / 工具链 / 库到 ~/.platformio（进快照, 让新 agent 免下载）
echo "[install] pio pkg install (gate/handheld/native)"
( cd firmware && "$REPO_ROOT/.venv/bin/pio" pkg install )

# 4) 云服务依赖
if [ ! -x cloud/.venv/bin/python ]; then
  echo "[install] create cloud/.venv"
  python3 -m venv cloud/.venv
  cloud/.venv/bin/pip install -q -U pip
fi
echo "[install] cloud deps"
cloud/.venv/bin/pip install -q -r cloud/requirements.txt

echo "[install] done"
