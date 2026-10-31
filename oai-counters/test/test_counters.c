#include "oai_counters.h"
#include <stdio.h>
#include <unistd.h>

int main(void){

    counter_create("l1_dl_bytes");
    counter_create("l1_ul_bytes");
    counter_create("l1_ul_power");

    counter_set("l1_dl_bytes", 1000);
    counter_add("l1_ul_bytes", 50);
    counter_add("l1_ul_bytes", 25);
    counter_set("l1_ul_power", 51);

    long v = 0;
    counter_get("l1_ul_bytes", &v);
    printf("l1_ul_bytes = %ld\n", v); // должно быть 50+25

    counters_dump_json("oai_counters.json"); //tmp зачем

    counters_start_auto_dump("oai_counters.json", 1);//tmp зачем

    for (int i = 0; i < 5; i++) {
        counter_add("l1_dl_bytes", 100);
        sleep(1);
    }

    counters_stop_auto_dump();

    counter_clear("l1_ul_power");
    counter_get("l1_ul_power", &v);
    printf("после clear l1_ul_power = %ld\n", v);

    counter_delete("l1_ul_power");
    counters_dump_json("oai_counters.json");

    return 0;
}