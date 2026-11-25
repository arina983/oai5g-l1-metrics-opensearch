import json
import time
import urllib.request

COUNTERS_FILE = "/tmp/ue_l1_counters.json"
OPENSEARCH_URL = "http://localhost:9200/gnb-l1-metrics/_doc"
INTERVAL_SEC = 1

def load_counters():
    with open(COUNTERS_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)

    doc = {
        "timestamp": data.get("timestamp"),
        "@timestamp": data["timestamp"],
        "period_sec": data.get("period_sec"),
        "source": "ue",
    }

    for item in data.get("counters", []):
        name = item.get("name")
        if name is not None:
            doc[name] = item.get("value")
            doc[name + "_window_avg"] = item.get("window_avg")
            doc[name + "_window_sec"] = item.get("window_sec")

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
            doc = load_counters()
            send_doc(doc)

            print(time.strftime("%H:%M:%S"), "OK", "source=", doc.get("source"), "RSRP=", doc.get("rsrp"), "RSSI=", doc.get("rssi"),)

        except Exception as e:
            print(time.strftime("%H:%M:%S"), "ERROR:", e)

        time.sleep(INTERVAL_SEC)

if __name__ == "__main__":
    main()
