#include "oai_counters.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define MAX_COUNTERS 128
#define NAME_LEN 32
#define SAMPLE_MAX 1024

typedef struct {
    int64_t time_ms;
    double value;
} sample_t;

typedef struct {
    int used;
    char name[NAME_LEN];
    double value;
    int window_sec;
    int window_enabled; // влкючен ли sliding window для счетчика
    sample_t samples[SAMPLE_MAX];
    int sample_count;
} counter_t;

static counter_t g_counters[MAX_COUNTERS];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_auto_running = 0;
static int g_auto_period = 1;
static char g_auto_path[256];
static pthread_t g_auto_thread;

static int64_t now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000; // в мс
}

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
static void remove_old(counter_t *counter){
    if (!counter->window_enabled || counter->window_sec <= 0 || counter->sample_count == 0)
        return;

    int64_t cut = now_ms() - (int64_t)counter->window_sec * 1000;
    int write_idx = 0;

    for (int i = 0; i < counter->sample_count; i++) {
        if (counter->samples[i].time_ms >= cut)
            counter->samples[write_idx++] = counter->samples[i];
    }
    counter->sample_count = write_idx;
}

static void push_sample(counter_t *counter, double sample_value){
    if (!counter->window_enabled || counter->window_sec <= 0)
        return;

    if (counter->sample_count == SAMPLE_MAX) {
        for (int i = 1; i < SAMPLE_MAX; i++)
            counter->samples[i - 1] = counter->samples[i];
        counter->sample_count = SAMPLE_MAX - 1;
    }
    counter->samples[counter->sample_count].time_ms = now_ms();
    counter->samples[counter->sample_count].value = sample_value;
    counter->sample_count++;
    remove_old(counter);
}

static double avg_samples(counter_t *counter){
    if (!counter->window_enabled || counter->window_sec <= 0)
        return counter->value;

    remove_old(counter);
    if (counter->sample_count == 0)
        return counter->value;

    double sum = 0.0;
    for (int i = 0; i < counter->sample_count; i++)
        sum += counter->samples[i].value;
    return sum / (double)counter->sample_count;
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
    g_counters[idx].value = 0.0;
    g_counters[idx].window_sec = 0;
    g_counters[idx].window_enabled = 0;
    g_counters[idx].sample_count = 0;

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_set(const char *name, double value){
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

int counter_add(const char *name, double delta){
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

int counter_get(const char *name, double *out){
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
    return counter_set(name, 0.0);
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
    g_counters[idx].value = 0.0;
    g_counters[idx].window_sec = 0;
    g_counters[idx].window_enabled = 0;
    g_counters[idx].sample_count = 0;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

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
    g_counters[idx].window_enabled = (window_sec > 0) ? 1 : 0;

    if (g_counters[idx].window_enabled)
        remove_old(&g_counters[idx]);
    else
        g_counters[idx].sample_count = 0;

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int counter_window_avg(const char *name, double *out){
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

        double avg = avg_samples(&g_counters[i]);

        fprintf(f,
            "    { \"name\": \"%s\", \"value\": %.6f, "
            "\"window_enabled\": %d, \"window_sec\": %d, \"window_avg\": %.6f }",
            g_counters[i].name, g_counters[i].value, g_counters[i].window_enabled, g_counters[i].window_sec, avg);
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