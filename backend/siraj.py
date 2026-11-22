"""Siraj inference. Load the exported model once per server process."""
import math
from pathlib import Path

import pandas as pd
from xgboost import XGBRegressor

MODEL_ID = "siraj-cleaned782-synthetic134-v1"
FEATURES = ["lux_ambient", "human", "vehicle", "human_dist_cm", "veh_dist_m",
            "veh_speed_kmh", "veh_approaching", "road_baseline_est"]
GAIN = 11.70


def number(payload, key):
    value = payload.get(key)
    if type(value) not in (float, int):
        raise ValueError(f"{key} requires a finite JSON number")
    try:
        value = float(value)
    except OverflowError as exc:
        raise ValueError(f"{key} is out of range") from exc
    if not math.isfinite(value):
        raise ValueError(f"{key} is out of range")
    return value


class Predictor:
    def __init__(self):
        self.model = XGBRegressor(n_jobs=1)
        self.model.load_model(Path(__file__).with_name("siraj_xgboost_model.json"))
        if self.model.get_booster().feature_names != FEATURES:
            raise RuntimeError("Model feature order differs from API contract")

    def predict(self, payload):
        if not isinstance(payload, dict):
            raise ValueError("Expected a JSON object")
        forbidden = set(payload) - set(FEATURES) - {"lux_road", "lamp_pct"}
        if forbidden:
            raise ValueError("Unknown fields: " + ", ".join(sorted(forbidden)))
        values = {key: number(payload, key) for key in FEATURES[:-1]}
        for key in ("human", "vehicle", "veh_approaching"):
            if values[key] not in (0, 1):
                raise ValueError(f"{key} must be 0 or 1")
        if values["lux_ambient"] < 0:
            raise ValueError("lux_ambient must be nonnegative")
        for key in ("human_dist_cm", "veh_dist_m", "veh_speed_kmh"):
            if values[key] < 0 and values[key] != -1:
                raise ValueError(f"{key} must be nonnegative or -1")
        if "road_baseline_est" in payload:
            if "lux_road" in payload or "lamp_pct" in payload:
                raise ValueError("Supply one baseline representation")
            baseline = number(payload, "road_baseline_est")
            if baseline < 0:
                raise ValueError("road_baseline_est must be nonnegative")
        else:
            road = number(payload, "lux_road")
            pwm = number(payload, "lamp_pct")
            if road < 0 or not 0 <= pwm <= 100:
                raise ValueError("Check lux_road and current lamp_pct")
            # pandas rounding matches the training transformation.
            baseline = pd.Series([max(0.0, road - GAIN*pwm/100)]).round(2).iat[0]
        values["road_baseline_est"] = baseline
        raw = float(self.model.predict(pd.DataFrame([values], columns=FEATURES))[0])
        if not math.isfinite(raw):
            raise RuntimeError("Invalid model output")
        floor = 30 if values["vehicle"] else 20 if values["human"] else 10
        daylight = values["lux_ambient"] >= 200
        command = 0.0 if daylight else max(floor, min(100.0, raw))
        return {"model_id": MODEL_ID, "raw_pwm": round(raw, 3),
                "pwm": round(command, 2), "daylight": daylight}
