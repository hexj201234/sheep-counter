# 自动数羊云服务

最小云端：设备本地先计数，有网再把当日出门/回圈同步上来；一页网页看历史与实时数。

## 运行

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
SHEEP_TOKEN=dev-token .venv/bin/uvicorn app:app --host 127.0.0.1 --port 8000
```

浏览器打开 http://127.0.0.1:8000 看表格。

## 接口

- `GET /` —— 历史表格 + 今日实时数（出门 / 回圈 / 未归）
- `POST /api/report` —— 设备上报，需请求头 `x-token`；body `{"day?":"YYYY-MM-DD","out":int,"back":int}`
- `GET /api/records` —— JSON 历史
- `GET /healthz` —— 健康检查

## 无硬件端到端演示

`sim_device.py` 用与固件一致的 `SheepSession` 逻辑模拟门口设备跑一场放牧再上报：

```bash
SHEEP_TOKEN=dev-token .venv/bin/python sim_device.py --out 812 --back 805
```

## 配置

- `SHEEP_TOKEN` —— 上报鉴权 token（默认 `dev-token`，生产请改）
- `SHEEP_DB` —— SQLite 路径（默认 `cloud/data/sheep.db`）
