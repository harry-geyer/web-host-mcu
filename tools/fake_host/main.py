from typing import Annotated, List
from fastapi import FastAPI, Request, Response, status
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


class WifiScanStart(BaseModel):
    status: str
    scan: str


class WifiStations(BaseModel):
    ssid: str
    mac: str
    channel: int
    rssi: int
    auth: str


class WifiScanGet(BaseModel):
    status: str
    stations: List[WifiStations]


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
    var["wifi-scan"] = {
        "started": None,
    }
    yield {"var": var}

app = FastAPI(lifespan=lifespan)
WIFI_SCAN_TIME = 5

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

@app.get("/api/wifi-scan-start")
async def get_wifi_scan_start(
    request: Request,
    response: Response,
    ) -> WifiScanStart:
    if request.state.var["wifi-scan"]["started"] is not None:
        response.status_code = status.HTTP_409_CONFLICT
        return {
            "status": "fail",
            "scan": "already running",
        }
    request.state.var["wifi-scan"]["started"] = time.monotonic()
    return {
        "status": "ok",
        "scan": "started",
    }

@app.get("/api/wifi-scan-get")
async def get_wifi_scan_get(
    request: Request,
    response: Response,
    ) -> WifiScanGet:
    if request.state.var["wifi-scan"]["started"] is None:
        response.status_code = status.HTTP_409_CONFLICT
        return {
            "status": "fail",
            "scan": "not started",
        }
    if time.monotonic() < WIFI_SCAN_TIME + request.state.var["wifi-scan"]["started"]:
        response.status_code = status.HTTP_425_TOO_EARLY
        return {
            "status": "fail",
            "scan": "not ready",
        }
    request.state.var["wifi-scan"]["started"] = None
    return {
        "status": "ok",
        "stations": [{
                "ssid": "Example Wifi",
                "mac": "01:02:03:04:05:06",
                "channel": 6,
                "rssi": -10,
                "auth": "OPEN",
            }, {
                "ssid": "Another Spot",
                "mac": "09:08:07:06:05:04",
                "channel": 1,
                "rssi": -20,
                "auth": "WPA2",
            },
        ],
    }

app.mount("/", StaticFiles(directory=Path("../webroot")), name="webroot")
