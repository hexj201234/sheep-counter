"""自动数羊云端小服务

设备本地先计数, 有网再把当日出门/回圈同步上来; 网页看历史与实时数
- POST /api/report : 设备上报当日数字, 需 token
- GET  /          : 历史表格 + 当前实时数
- GET  /api/records : JSON
- GET  /healthz   : 健康检查
"""
from __future__ import annotations

import os
import sqlite3
from contextlib import closing
from datetime import date
from pathlib import Path

from fastapi import FastAPI, Header, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse
from jinja2 import Environment, FileSystemLoader, select_autoescape
from pydantic import BaseModel, Field

BASE_DIR = Path(__file__).resolve().parent
DB_PATH = Path(os.environ.get("SHEEP_DB", BASE_DIR / "data" / "sheep.db"))
TOKEN = os.environ.get("SHEEP_TOKEN", "dev-token")

app = FastAPI(title="自动数羊云", version="0.1.0")
_jinja = Environment(
    loader=FileSystemLoader(str(BASE_DIR / "templates")),
    autoescape=select_autoescape(["html"]),
)


def _connect() -> sqlite3.Connection:
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with closing(_connect()) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS daily (
                day      TEXT PRIMARY KEY,
                out_cnt  INTEGER NOT NULL DEFAULT 0,
                back_cnt INTEGER NOT NULL DEFAULT 0,
                updated  TEXT NOT NULL
            )
            """
        )
        conn.commit()


class Report(BaseModel):
    day: str = Field(default_factory=lambda: date.today().isoformat())
    out: int = Field(ge=0, default=0)
    back: int = Field(ge=0, default=0)


@app.on_event("startup")
def _startup() -> None:
    init_db()


@app.get("/healthz")
def healthz() -> dict:
    return {"ok": True}


@app.post("/api/report")
def report(body: Report, x_token: str | None = Header(default=None)) -> JSONResponse:
    if x_token != TOKEN:
        raise HTTPException(status_code=401, detail="bad token")
    now = _now_iso()
    with closing(_connect()) as conn:
        conn.execute(
            """
            INSERT INTO daily (day, out_cnt, back_cnt, updated)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(day) DO UPDATE SET
                out_cnt=excluded.out_cnt,
                back_cnt=excluded.back_cnt,
                updated=excluded.updated
            """,
            (body.day, body.out, body.back, now),
        )
        conn.commit()
    return JSONResponse(
        {"day": body.day, "out": body.out, "back": body.back,
         "not_returned": body.out - body.back}
    )


def _rows() -> list[dict]:
    with closing(_connect()) as conn:
        cur = conn.execute(
            "SELECT day, out_cnt, back_cnt, updated FROM daily ORDER BY day DESC"
        )
        return [
            {
                "day": r["day"],
                "out": r["out_cnt"],
                "back": r["back_cnt"],
                "not_returned": r["out_cnt"] - r["back_cnt"],
                "updated": r["updated"],
            }
            for r in cur.fetchall()
        ]


@app.get("/api/records")
def records() -> list[dict]:
    return _rows()


@app.get("/", response_class=HTMLResponse)
def index() -> HTMLResponse:
    rows = _rows()
    today = date.today().isoformat()
    live = next((r for r in rows if r["day"] == today), None)
    tmpl = _jinja.get_template("index.html")
    return HTMLResponse(tmpl.render(rows=rows, live=live, today=today))


def _now_iso() -> str:
    from datetime import datetime, timezone

    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
