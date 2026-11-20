# OAI 5G L1 metrics, OpenSearch

Сбор L1-метрик gNB и UE OpenAirInterface5G: библиотека счётчиков со sliding window, выгрузка в JSON и отправка в OpenSearch.

# oai-counters

Подключаемая библиотека счётчиков для OpenAirInterface5G (gNB и UE).

Именованные счётчики (create / set / add / get / clear / delete) и выгрузка в JSON с заданной периодичностью (например, 1 с).

окно: counter_set_window, среднее по точкам за последние N секунд

Связано с разбором L1-статистики OAI: метрики накапливаются в PHY (`phy_stats`), пишутся в `nrL1_stats.log`. Эта библиотека - свой слой учёта в JSON; 

## Сборка

```bash
mkdir -p build && cd build
gcc -fPIC -shared -o liboai_counters.so ../src/oai_counters.c -I../include -lpthread
gcc -o test_counters ../test/test_counters.c -I../include -L. -loai_counters -lpthread -Wl,-rpath,'$ORIGIN'
```

## Запуск стенда (ZMQ)

gNB:
```bash
cd openairinterface5g/cmake_targets/ran_build/build
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
sudo ./nr-softmodem \
  -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf \
  --gNBs.[0].min_rxtxtime 6 \
  --device.name oai_zmqdevif \
  --zmq.[0].tx_channels tcp://127.0.0.1:4556 \
  --zmq.[0].rx_channels tcp://127.0.0.1:4557
```
UE:
```bash
sudo ./nr-uesoftmodem \
  -r 106 --numerology 1 --band 78 -C 3619200000 \
  --device.name oai_zmqdevif \
  --zmq.[0].tx_channels tcp://127.0.0.1:4557 \
  --zmq.[0].rx_channels tcp://127.0.0.1:4556 \
  --ssb 516 \
  --uicc0.imsi 001010000000001
```
Сборка ZMQ-устройства: 
```bash
./cmake_targets/build_oai --gNB --nrUE -c -w ZMQ
```
### Метрики (L1):

| Имя | Описание |
| --- | --- |
| `l1_dl_bytes` | Сумма байт DL TB |
| `l1_dl_tb_count` | Число DL TB |
| `l1_ul_round_trials` | Число UL‑раундов |
| `l1_ul_power` | Оценка мощности UL |


| Имя | Описание |
| --- | --- |
| `rsrp` | SS-RSRP: мощность SSS в SSB, dBm (nr_ue_ssb_rsrp_measurements) |
| `rssi` | rx_rssi_dBm OAI: мощность из оценок канала в nr_ue_measurements, dBm |
| `*_window_avg` | среднее по sliding window |
| `*_window_sec` | длина окна (с) |


Проверка файла метрик:
```bash
watch -n 1 cat /tmp/ue_l1_counters.json
```
### Интеграция в OAI UE
1. Скопировать библиотеку в дерево OAI:
```bash
cp oai-counters/src/oai_counters.c
openairinterface5g/common/utils/oai_counters/
cp oai-counters/include/oai_counters.h 
openairinterface5g/common/utils/oai_counters/
```
2. При инициализации UE (nr_init_ue.c / init_nr_ue_signal):
```c
counter_create("rssi");
counter_create("rsrp");
counter_set_window("rssi", 5);
counter_set_window("rsrp", 5);
counters_start_auto_dump("/tmp/ue_l1_counters.json", 1);
```
3. В PHY:

- после расчёта SS-RSRP: counter_set("rsrp", …)
- после расчёта rx_rssi_dBm: counter_set("rssi", …)

4. Пересборка UE:
``` bash
cd openairinterface5g/cmake_targets/ran_build/build
make nr-uesoftmodem -j$(nproc)
``` 
# OpenSearch
`opensearch/docker-compose.yml` OpenSearch + Dashboards

`opensearch/send_l1_metrics.py` Чтение JSON счётчиков и POST в OpenSearch
`opensearch/send_ue_l1_metrics.py`

```bash
cd opensearch
docker compose up -d
curl -s http://localhost:9200 
```
Отправка метрик

```bash 
python3 send_l1_metrics.py

python3 send_ue_l1_metrics.py
```
- API: http://localhost:9200
- Dashboards: http://localhost:5601
- Индекс: gnb-l1-metrics
