#include "oai_counters.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_COUNTERS 128
#define NAME_LEN 64
#define SAMPLE_MAX 64

typedef struct {
    time_t t;
    long v;
} sample_t;

typedef struct {
    int used;
    char name[NAME_LEN];
    long value;
    int window_sec;
    sample_t samples[SAMPLE_MAX];
    int n;
} counter_t;

static counter_t g_counters[MAX_COUNTERS];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_auto_running = 0;
static int g_auto_period  = 1;
static char g_auto_path[256];
static pthread_t g_auto_thread;

static int find_index(const char *name){
    for (int i = 0; i < MAX_COUNTERS; i++)
        if (g_counters[i].used && strcmp(g_counters[i].name, name) == 0)
            return i;
    return -1;
}

static int find_free(void){
    for (int i = 0; i < MAX_COUNTERS; i++)
        if (!g_counters[i].used)
            return i;
    return -1;
}

// убрать точки, которые вышли за окно
static void remove_old(counter_t *c){
    if (c->window_sec <= 0 || c->n == 0)
        return;

    time_t cut = time(NULL) - c->window_sec;
    int w = 0;
    for (int i = 0; i < c->n; i++) {
        if (c->samples[i].t >= cut)
            c->samples[w++] = c->samples[i];
    }
    c->n = w;
}

// добавить новую точку
static void push_sample(counter_t *c, long v){
    if (c->window_sec <= 0)
        return;

    // если буфер полный выкидываем самую старую
    if (c->n >= SAMPLE_MAX) {
        for (int i = 1; i < SAMPLE_MAX; i++)
            c->samples[i - 1] = c->samples[i];
        c->n = SAMPLE_MAX - 1;
    }
    c->samples[c->n].t = time(NULL);
    c->samples[c->n].v = v;
    c->n++;
    remove_old(c);
}

// среднее по окну
static long avg_samples(counter_t *c){
    remove_old(c);
    if (c->n == 0)
        return c->value;   // если нет точек возвращаем текущее значение

    long sum = 0;
    for (int i = 0; i < c->n; i++)
        sum += c->samples[i].v;
    return sum / c->n;
}

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
    g_counters[idx].window_sec = 5;
    g_counters[idx].n = 0;

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
    push_sample(&g_counters[idx], value);
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
    push_sample(&g_counters[idx], g_counters[idx].value);
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
    g_counters[idx].window_sec = 0;
    g_counters[idx].n = 0;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

// sliding window 
int counter_set_window(const char *name, int window_sec){
    if (!name || window_sec < 0)
        return -1;

    pthread_mutex_lock(&g_mutex);
    int idx = find_index(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    g_counters[idx].window_sec = window_sec;
    remove_old(&g_counters[idx]);
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_window_avg(const char *name, long *out){
    if (!name || !out)
        return -1;

    pthread_mutex_lock(&g_mutex);
    int idx = find_index(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    *out = avg_samples(&g_counters[idx]);
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counters_dump_json(const char *path){
    if (!path)
        return -1;

    FILE *f = fopen(path, "w");
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

        long avg = avg_samples(&g_counters[i]);

        fprintf(f, "    { \"name\": \"%s\", \"value\": %ld, "
            "\"window_sec\": %d, \"window_avg\": %ld }",
            g_counters[i].name, g_counters[i].value, g_counters[i].window_sec, avg);
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

int counters_start_auto_dump(const char *path, int period_sec){
    if (!path || period_sec < 1)
        return -1;
    if (g_auto_running)
        return 0;

    strncpy(g_auto_path, path, sizeof(g_auto_path) - 1);
    g_auto_path[sizeof(g_auto_path) - 1] = '\0';
    g_auto_period  = period_sec;
    g_auto_running = 1;

    if (pthread_create(&g_auto_thread, NULL, auto_dump_thread, NULL) != 0) {
        g_auto_running = 0;
        return -1;
    }
    return 0;
}

void counters_stop_auto_dump(void){
    if (!g_auto_running)
        return;
    g_auto_running = 0;
    pthread_join(g_auto_thread, NULL);
}