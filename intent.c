#include "types.h"
#include "intent.h"
#include "hexafs.h"
#include "process.h"
#include "log.h"

extern void *memset(void *dest, int c, size_t len);
extern void *memcpy(void *dest, const void *src, size_t len);
extern int strcmp(const char *s1, const char *s2);
extern size_t strlen(const char *str);
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);
extern int find_form(const char *name);
extern struct hexa_formentry {
    char name[32];
    char *content;
    int size;
    int cap;
    int owner;
    uint16_t mode;
} form_table[];
extern int form_count;

static intent_entry_t intent_table[MAX_INTENTS];
static uint32_t next_handle = 1;

int intent_init(void) {
    memset(intent_table, 0, sizeof(intent_table));
    next_handle = 1;
    log_write(LOG_LEVEL_INFO, "intent: system initialized");
    return 0;
}

int intent_create(int pid, hexaos_intent_t *intent, uint32_t *handle) {
    if (!intent || !handle) return -1;
    for (int i = 0; i < MAX_INTENTS; i++) {
        if (!intent_table[i].active) {
            intent_table[i].active = 1;
            intent_table[i].pid = pid;
            intent_table[i].intent = *intent;
            intent_table[i].handle = next_handle++;
            intent_table[i].buffer_block = 0;
            intent_table[i].buffer_len = 0;
            *handle = intent_table[i].handle;
            return 0;
        }
    }
    return -1;
}

int intent_fulfill(uint32_t handle, void *buffer, int len) {
    for (int i = 0; i < MAX_INTENTS; i++) {
        if (intent_table[i].active && intent_table[i].handle == handle) {
            intent_entry_t *e = &intent_table[i];
            if (e->intent.intent_type == INTENT_PRODUCE) {
                uint32_t hash = hexafs_content_hash(buffer, (uint32_t)len);
                e->buffer_block = hash;
                e->buffer_len = len;
                return len;
            } else if (e->intent.intent_type == INTENT_CONSUME) {
                if (buffer && len > 0) {
                    int copy = len < e->buffer_len ? len : e->buffer_len;
                    if (copy > 0) memcpy(buffer, &e->buffer_block, copy);
                    return copy;
                }
            }
            return 0;
        }
    }
    return -1;
}

int intent_close(uint32_t handle) {
    for (int i = 0; i < MAX_INTENTS; i++) {
        if (intent_table[i].active && intent_table[i].handle == handle) {
            intent_table[i].active = 0;
            return 0;
        }
    }
    return -1;
}

int intent_compat_open(const char *path, int flags) {
    (void)flags;
    int idx = find_form(path);
    if (idx < 0) return -1;
    hexaos_intent_t intent;
    intent.intent_type = INTENT_CONSUME;
    intent.target_hash = (uint32_t)(uint32_t)path;
    intent.schema_hash = 0;
    intent.filter_offset = 0;
    intent.flags = INTENT_ONCE;
    uint32_t handle;
    if (intent_create(tasks[current_task].pid, &intent, &handle) < 0) return -1;
    return (int)handle;
}

int intent_compat_read(int fd, char *buf, int count, int pos) {
    (void)pos;
    intent_entry_t *e = 0;
    for (int i = 0; i < MAX_INTENTS; i++) {
        if (intent_table[i].active && (int)intent_table[i].handle == fd) {
            e = &intent_table[i];
            break;
        }
    }
    if (!e) return -1;
    if (e->intent.intent_type != INTENT_CONSUME && e->intent.intent_type != INTENT_OBSERVE) return -1;
    return intent_fulfill((uint32_t)fd, buf, count);
}

int intent_compat_write(int fd, const char *buf, int count, int pos) {
    (void)pos;
    intent_entry_t *e = 0;
    for (int i = 0; i < MAX_INTENTS; i++) {
        if (intent_table[i].active && (int)intent_table[i].handle == fd) {
            e = &intent_table[i];
            break;
        }
    }
    if (!e) return -1;
    if (e->intent.intent_type != INTENT_PRODUCE) return -1;
    return intent_fulfill((uint32_t)fd, (void *)buf, count);
}

int intent_compat_stat(const char *path, void *node) {
    (void)path;
    (void)node;
    return -1;
}
