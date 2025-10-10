from typing import Annotated, List
from fastapi import FastAPI, Request
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from contextlib import asynccontextmanager
from pathlib import Path


class Network(BaseModel):
    connected: bool
    station_ssid: str | None


class Status(BaseModel):
    network: Network


class Config(BaseModel):
    name: str
    blinking_ms: int
    station_ssid: str | None
    station_password: str | None


class Measurements(BaseModel):
    name: str
    value: str
    unit: str


@asynccontextmanager
async def lifespan(app: FastAPI):
    var = {}
    var["static_config"] = Config(
        name="Web-Host MCU",
        blinking_ms=250,
        station_ssid=None,
        station_password=None,
    )
    var["status"] = Status(
        network=Network(
            connected=False,
            station_ssid=None,
        ),
    )
    yield {"var": var}

app = FastAPI(lifespan=lifespan)

@app.get("/api/status")
async def get_status(request: Request) -> Status:
    return request.state.var["status"]

@app.get("/api/config")
async def get_config(request: Request) -> Config:
    return request.state.var["static_config"]

@app.post("/api/config")
async def post_config(
    request: Request,
    config: Config,
):
    request.state.var["static_config"] = config
    return {"status": "ok"}

@app.get("/api/meas")
async def get_meas() -> List[Measurements]:
    return [
        {
            "name": "relative_humidity",
            "value": "48.29",
            "unit": "%",
        },
        {
            "name": "temperature",
            "value": "18.78",
            "unit": "C",
        },
    ]

app.mount("/", StaticFiles(directory=Path("../webroot")), name="webroot")
