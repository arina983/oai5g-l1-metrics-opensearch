# oai-counters

Подключаемая библиотека счётчиков для OpenAirInterface5G (gNB и UE).

Именованные счётчики (create / set / add / get / clear / delete) и выгрузка в JSON с заданной периодичностью (например, 1 с).

Связано с разбором L1-статистики OAI: метрики накапливаются в PHY (`phy_stats`), пишутся в `nrL1_stats.log`. Эта библиотека свой слой учёта в JSON; дальше планируется вызов из L1 и выгрузка в OpenSearch.

## Сборка

```bash
mkdir -p build && cd build
gcc -fPIC -shared -o liboai_counters.so ../src/oai_counters.c -I../include -lpthread
gcc -o test_counters ../test/test_counters.c -I../include -L. -loai_counters -lpthread -Wl,-rpath,'$ORIGIN'