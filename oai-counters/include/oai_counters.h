#ifndef OAI_COUNTERS_H
#define OAI_COUNTERS_H

int counter_create(const char *name);
int counter_set(const char *name, long value);
int counter_add(const char *name, long delta);
int counter_get(const char *name, long *out);
int counter_clear(const char *name);
int counter_delete(const char *name);
int counters_dump_json(const char *path);
int counters_start_auto_dump(const char *path, int period_sec);
void counters_stop_auto_dump(void);

#endif