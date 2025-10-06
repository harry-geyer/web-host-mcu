from typing import Annotated, List
from fastapi import FastAPI, Request
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from contextlib import asynccontextmanager
from pathlib import Path


class Config(BaseModel):
    name: str
    blinking_ms: int


class Measurements(BaseModel):
    name: str
    value: str
    unit: str


@asynccontextmanager
async def lifespan(app: FastAPI):
    static_config = Config(name="Web-Host MCU", blinking_ms=250)
    var = {"static_config": static_config}
    yield {"var": var}

app = FastAPI(lifespan=lifespan)

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
