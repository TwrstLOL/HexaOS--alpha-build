#include "types.h"
#include "replay.h"
#include "hexafs.h"
#include "process.h"
#include "log.h"

extern volatile uint32_t system_ticks;

static replay_event_t replay_buffer[MAX_REPLAY_EVENTS];
static int replay_count = 0;
static int replay_recording = 0;

extern void *memset(void *dest, int c, size_t len);
extern void print_string(const char *str);
extern void print_color(const char *str, uint8_t color);
extern void itoa(int num, char *str, int base);

int replay_init(void) {
    memset(replay_buffer, 0, sizeof(replay_buffer));
    replay_count = 0;
    replay_recording = 1;
    log_write(LOG_LEVEL_INFO, "replay: event recording started");
    return 0;
}

int replay_record(uint32_t syscall_num, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (!replay_recording) return 0;
    if (replay_count >= MAX_REPLAY_EVENTS) return -1;
    replay_event_t *e = &replay_buffer[replay_count];
    e->tick = system_ticks;
    e->pid = (uint32_t)(current_task >= 0 ? tasks[current_task].pid : 0);
    e->syscall_num = syscall_num;
    e->args[0] = arg0;
    e->args[1] = arg1;
    e->args[2] = arg2;
    e->args[3] = arg3;
    e->snap_before = 0;
    e->snap_after = 0;
    replay_count++;
    return replay_count;
}

int replay_execute(uint32_t snap_id, uint32_t event_count) {
    (void)snap_id;
    if (event_count > (uint32_t)replay_count) event_count = (uint32_t)replay_count;
    print_color("[REPLAY] Executing ", 0x0E);
    char buf[16];
    itoa((int)event_count, buf, 10);
    print_string(buf);
    print_string(" events\n");
    for (uint32_t i = 0; i < event_count; i++) {
        replay_event_t *e = &replay_buffer[i];
        print_string("  event ");
        itoa((int)i, buf, 10);
        print_string(buf);
        print_string(": syscall ");
        itoa((int)e->syscall_num, buf, 10);
        print_string(buf);
        print_string("\n");
    }
    log_write(LOG_LEVEL_INFO, "replay: execution complete");
    return (int)event_count;
}

int replay_dry_run(uint32_t snap_id, uint32_t event_count) {
    print_color("[REPLAY] Dry run (read-only) for ", 0x0E);
    char buf[16];
    itoa((int)event_count, buf, 10);
    print_string(buf);
    print_string(" events\n");
    return replay_execute(snap_id, event_count);
}
