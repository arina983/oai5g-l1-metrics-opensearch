# OAI 5G L1 metrics, OpenSearch

Сбор L1-метрик gNB OpenAirInterface5G: библиотека счётчиков, JSON и отправка в OpenSearch.

# oai-counters

Подключаемая библиотека счётчиков для OpenAirInterface5G (gNB и UE).

Именованные счётчики (create / set / add / get / clear / delete) и выгрузка в JSON с заданной периодичностью (например, 1 с).

Связано с разбором L1-статистики OAI: метрики накапливаются в PHY (`phy_stats`), пишутся в `nrL1_stats.log`. Эта библиотек - свой слой учёта в JSON; 

## Сборка

```bash
mkdir -p build && cd build
gcc -fPIC -shared -o liboai_counters.so ../src/oai_counters.c -I../include -lpthread
gcc -o test_counters ../test/test_counters.c -I../include -L. -loai_counters -lpthread -Wl,-rpath,'$ORIGIN'
```
### Метрики (L1):

| Метрика | Описание |
| --- | --- |
| `l1_dl_bytes` | Сумма байт DL TB |
| `l1_dl_tb_count` | Число DL TB |
| `l1_ul_round_trials` | Число UL‑раундов |
| `l1_ul_power` | Оценка мощности UL |

# OpenSearch
`opensearch/docker-compose.yml` OpenSearch + Dashboards
`opensearch/send_l1_metrics.py` Чтение JSON счётчиков и POST в OpenSearch

```bash
cd opensearch
docker compose up -d
curl -s http://localhost:9200 
```
Отправка метрик

```bash 
python3 send_l1_metrics.py
```
- API: http://localhost:9200
- Dashboards: http://localhost:5601
- Индекс: gnb-l1-metrics
