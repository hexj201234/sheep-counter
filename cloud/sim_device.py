"""门口设备模拟器: 用真实的 SheepSession 判定逻辑跑一场放牧, 再把结果上报云端

没有硬件时用它做端到端演示: 模拟四道光电通过的羊, 走会话状态机,
最后像门口 ESP32 一样 HTTPS POST 当日出门/回圈到云服务
"""
from __future__ import annotations

import argparse
import os
import sys

import httpx


class Session:
    """SheepSession 的 Python 镜像, 语义与固件 lib/SheepSession 一致"""

    OUT, BACK = 0, 1
    IDLE, RUNNING, ENDED = 0, 1, 2

    def __init__(self) -> None:
        self.mode = self.OUT
        self.state = self.IDLE
        self.out = 0
        self.back = 0

    def _active_add(self, d: int) -> None:
        if self.mode == self.OUT:
            self.out = max(0, self.out + d)
        else:
            self.back = max(0, self.back + d)

    def select(self, m: int) -> bool:
        if self.state == self.RUNNING:
            return False
        self.mode = m
        self.state = self.IDLE
        return True

    def start(self) -> None:
        self.state = self.RUNNING

    def end(self) -> None:
        self.state = self.ENDED

    def detected(self) -> bool:
        if self.state != self.RUNNING:
            return False
        self._active_add(1)
        return True


def run_grazing(out_sheep: int, back_sheep: int) -> Session:
    s = Session()
    s.select(Session.OUT)
    s.start()
    for _ in range(out_sheep):
        s.detected()
    s.end()

    s.select(Session.BACK)
    s.start()
    for _ in range(back_sheep):
        s.detected()
    s.end()
    return s


def main() -> int:
    ap = argparse.ArgumentParser(description="sheep gate device simulator")
    ap.add_argument("--url", default=os.environ.get("SHEEP_URL", "http://127.0.0.1:8000"))
    ap.add_argument("--token", default=os.environ.get("SHEEP_TOKEN", "dev-token"))
    ap.add_argument("--out", type=int, default=812)
    ap.add_argument("--back", type=int, default=805)
    ap.add_argument("--day", default=None)
    args = ap.parse_args()

    s = run_grazing(args.out, args.back)
    print(f"[gate] 本地计数完成: 出门={s.out} 回圈={s.back} 未归={s.out - s.back}")

    payload = {"out": s.out, "back": s.back}
    if args.day:
        payload["day"] = args.day

    try:
        r = httpx.post(
            f"{args.url}/api/report",
            json=payload,
            headers={"x-token": args.token},
            timeout=10.0,
        )
    except httpx.HTTPError as e:
        print(f"[gate] 上云失败 (会本地排队重试): {e}", file=sys.stderr)
        return 1

    if r.status_code != 200:
        print(f"[gate] 云端拒绝: {r.status_code} {r.text}", file=sys.stderr)
        return 1
    print(f"[gate] 已同步云端: {r.json()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
