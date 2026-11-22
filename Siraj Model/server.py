"""Local WSGI service for PHP and the MQTT bridge."""
import errno
import time
import csv
import hashlib
import json
import os
from pathlib import Path
from waitress import serve
from siraj import Predictor, MODEL_ID

predictor = Predictor()


def lock_csv_reader(stream):
    """Coordinate with PHP flock(LOCK_EX); closing stream releases the lock."""
    if os.name == "nt":
        import msvcrt
    else:
        import fcntl

    deadline = time.monotonic() + 0.6
    while True:
        try:
            if os.name == "nt":
                # PHP locks the whole file. Its range overlaps this first byte.
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                fcntl.flock(stream.fileno(), fcntl.LOCK_SH | fcntl.LOCK_NB)
            return
        except OSError as exc:
            if exc.errno not in (errno.EACCES, errno.EAGAIN, errno.EDEADLK):
                raise
            if time.monotonic() >= deadline:
                raise TimeoutError("CSV busy") from exc
            time.sleep(0.01)


def csv_reading(ref):
    if not isinstance(ref, dict) or any(type(ref.get(k)) is not int or ref[k] < 0
                                        for k in ("offset", "lamp_id", "seq")):
        raise ValueError("Invalid CSV reference")
    path = Path(os.environ["SIRAJ_READINGS_CSV"])
    with path.open("rb") as stream:
        lock_csv_reader(stream)
        header = next(csv.reader([stream.readline(8192).decode("utf-8-sig")]))
        stream.seek(ref["offset"])
        line = stream.readline(8192)
    if not line.endswith(b"\n") or hashlib.sha256(line).hexdigest() != ref.get("sha256"):
        raise ValueError("CSV row differs from committed reading")
    fields = next(csv.reader([line.decode("utf-8")], strict=True))
    if len(header) != len(fields) or len(set(header)) != len(header):
        raise ValueError("Invalid CSV schema")
    row = dict(zip(header, fields))
    if any(int(row[k]) != ref[k] for k in ("lamp_id", "seq")):
        raise ValueError("CSV reading identity mismatch")
    keys = ("lux_ambient", "human", "vehicle", "human_dist_cm", "veh_dist_m",
            "veh_speed_kmh", "veh_approaching", "lux_road", "lamp_pct")
    return {k: float(row[k]) for k in keys}



def application(environ, start_response):
    path, method = environ["PATH_INFO"], environ["REQUEST_METHOD"]
    status = "200 OK"
    if (path, method) == ("/health", "GET"):
        result = {"status": "ok", "model_id": MODEL_ID}
    elif path not in ("/predict", "/predict-csv"):
        status, result = "404 Not Found", {"error": "Unknown route"}
    elif method != "POST":
        status, result = "405 Method Not Allowed", {"error": "Use POST"}
    elif environ.get("CONTENT_TYPE", "").split(";")[0].strip() != "application/json":
        status, result = "415 Unsupported Media Type", {"error": "Use application/json"}
    else:
        try:
            size = int(environ.get("CONTENT_LENGTH") or 0)
            if not 0 < size <= 8192:
                raise ValueError("Expected a JSON body of 1 to 8192 bytes")
            payload = json.loads(environ["wsgi.input"].read(size))
            result = predictor.predict(csv_reading(payload) if path == "/predict-csv" else payload)
        except TimeoutError as exc:
            status, result = "503 Service Unavailable", {"error": str(exc)}
        except (ValueError, KeyError, csv.Error, StopIteration, OSError) as exc:
            status, result = "400 Bad Request", {"error": str(exc)}
    body = json.dumps(result, allow_nan=False).encode()
    start_response(status, [("Content-Type", "application/json"), ("Content-Length", str(len(body)))])
    return [body]


if __name__ == "__main__":
    serve(application, host="127.0.0.1", port=8765, threads=4, max_request_body_size=8192)
