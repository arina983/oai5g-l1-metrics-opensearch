#!/usr/bin/env python3
import json
import time
import urllib.request

COUNTERS_FILE = "/tmp/gnb_l1_counters.json"
OPENSEARCH_URL = "http://localhost:9200/gnb-l1-metrics/_doc"
INTERVAL_SEC = 1

def load_counters(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)

    doc = {
        "timestamp": data.get("timestamp"),
        "period_sec": data.get("period_sec"),
        "source": "gnb",
    }

    for item in data.get("counters", []):
        name = item.get("name")
        value = item.get("value")
        if name is not None:
            doc[name] = value

    return doc

def send_doc(doc):
    body = json.dumps(doc).encode("utf-8")
    req = urllib.request.Request(
        OPENSEARCH_URL,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=5) as resp:
        return resp.read().decode("utf-8")

def main():
    while True:
        try:
            doc = load_counters(COUNTERS_FILE)
            result = send_doc(doc)
            print(
                time.strftime("%H:%M:%S"),
                "ok",
                "dl_bytes=", doc.get("l1_dl_bytes"),
                "ul_trials=", doc.get("l1_ul_round_trials"),
            )
        except FileNotFoundError:
            print(time.strftime("%H:%M:%S"), "no file", COUNTERS_FILE)
        except Exception as e:
            print(time.strftime("%H:%M:%S"), "error:", e)

        time.sleep(INTERVAL_SEC)

if __name__ == "__main__":
    main()
