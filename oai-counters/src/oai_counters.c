#include "oai_counters.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_COUNTERS 128
#define NAME_LEN 64

typedef struct {
    int used;
    char name[NAME_LEN];
    long value;
} counter_t;

static counter_t g_counters[MAX_COUNTERS];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_auto_running = 0; 
static int g_auto_period = 1;
static char g_auto_path[256];
static pthread_t g_auto_thread;

// найти счётчик по имени
static int find_index(const char *name){
    for (int i = 0; i < MAX_COUNTERS; i++) {
        if (g_counters[i].used && strcmp(g_counters[i].name, name) == 0)
        return i;
    }
    return -1;
}
// найти свободную ячейку (для create)
static int find_free(void){
    for (int i = 0; i < MAX_COUNTERS; i++) {
        if (!g_counters[i].used)
        return i;
    }
    return -1;
}
//создать счетчик
int counter_create(const char *name){
    if (!name || name[0] == '\0' || strlen(name) >= NAME_LEN)
        return -1;

    pthread_mutex_lock(&g_mutex);

    if (find_index(name) >= 0) {
        pthread_mutex_unlock(&g_mutex);
        return 0;
    }

    int idx = find_free();
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    g_counters[idx].used = 1;
    strncpy(g_counters[idx].name, name, NAME_LEN - 1);
    g_counters[idx].name[NAME_LEN - 1] = '\0';
    g_counters[idx].value = 0;

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_set(const char *name, long value){
    if (!name)
        return -1;

    pthread_mutex_lock(&g_mutex);
    int idx = find_index(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    g_counters[idx].value = value;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_add(const char *name, long delta){
    if (!name)
        return -1;

    pthread_mutex_lock(&g_mutex);
    int idx = find_index(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    g_counters[idx].value += delta;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_get(const char *name, long *out){
    if (!name || !out)
        return -1;

    pthread_mutex_lock(&g_mutex);
    int idx = find_index(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    *out = g_counters[idx].value;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_clear(const char *name){
    return counter_set(name, 0);
}

int counter_delete(const char *name){
    if (!name)
        return -1;

    pthread_mutex_lock(&g_mutex);
    int idx = find_index(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    g_counters[idx].used = 0;
    g_counters[idx].name[0] = '\0';
    g_counters[idx].value = 0;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}
// один раз записать все счётчики в JSON
int counters_dump_json(const char *path){
    if (!path)
        return -1;

    FILE *f = fopen(path, "w"); // w-перезапись
    if (!f)
    return -1;

    time_t now = time(NULL);

    pthread_mutex_lock(&g_mutex);

    fprintf(f, "{\n");
    fprintf(f, "  \"timestamp\": %ld,\n", (long)now);
    fprintf(f, "  \"period_sec\": %d,\n", g_auto_period);
    fprintf(f, "  \"counters\": [\n");

    int first = 1;
    for (int i = 0; i < MAX_COUNTERS; i++) {
        if (!g_counters[i].used)
            continue;
        if (!first)
            fprintf(f, ",\n");
            first = 0;
            fprintf(f, "    { \"name\": \"%s\", \"value\": %ld }", g_counters[i].name, g_counters[i].value);
    }

  fprintf(f, "\n  ]\n}\n");

  pthread_mutex_unlock(&g_mutex);
  fclose(f);
  return 0;
}

static void *auto_dump_thread(void *arg){
    (void)arg;
    while (g_auto_running) {
    counters_dump_json(g_auto_path);
    for (int i = 0; i < g_auto_period && g_auto_running; i++)
        sleep(1);
    }
    return NULL;
}
// писать JSON каждые N секунд
int counters_start_auto_dump(const char *path, int period_sec){
    if (!path || period_sec < 1)
        return -1;
    if (g_auto_running)
        return 0;

    strncpy(g_auto_path, path, sizeof(g_auto_path) - 1);
    g_auto_path[sizeof(g_auto_path) - 1] = '\0';
    g_auto_period = period_sec;
    g_auto_running = 1;

    if (pthread_create(&g_auto_thread, NULL, auto_dump_thread, NULL) != 0) {
        g_auto_running = 0;
        return -1;
    }
    return 0;
}
// остановить автозапись
void counters_stop_auto_dump(void){
    if (!g_auto_running)
        return;
    g_auto_running = 0;
    pthread_join(g_auto_thread, NULL);
}