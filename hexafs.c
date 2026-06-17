#include "types.h"
#include "hexafs.h"
#include "hexafs_disk.h"
#include "log.h"

extern void *memset(void *dest, int c, size_t len);
extern void *memcpy(void *dest, const void *src, size_t len);
extern size_t strlen(const char *str);
extern char *strcpy(char *dest, const char *src);
extern int strcmp(const char *s1, const char *s2);
extern void print_string(const char *str);
extern void *kmalloc(size_t size);
extern int find_form(const char *name);

extern hexafs_superblock_t sb_cache;
extern int form_count;

struct hexa_formentry {
    char name[32];
    char *content;
    int size;
    int cap;
    int owner;
    uint16_t mode;
};
extern struct hexa_formentry form_table[];

hexafs_tx_t hexafs_current_tx;
static int tx_gen = 0;
static uint32_t root_abs_block = 0;

int hexafs_tx_begin(void) {
    if (hexafs_current_tx.active) return 0;
    memset(&hexafs_current_tx, 0, sizeof(hexafs_current_tx));
    hexafs_current_tx.active = 1;
    hexafs_current_tx.tid = ++tx_gen;
    hexafs_current_tx.parent_snap_block = sb_cache.root_snap_block;
    hexafs_current_tx.dirty = 0;
    return 1;
}

int hexafs_tx_commit(void) {
    if (!hexafs_current_tx.active) return 0;
    if (!hexafs_current_tx.dirty) {
        hexafs_current_tx.active = 0;
        return 1;
    }
    if (!hexafs_save_bitmap()) {
        log_write(LOG_LEVEL_WARN, "HEXAFS: bitmap save failed on commit");
        return 0;
    }
    if (!hexafs_write_superblock()) {
        log_write(LOG_LEVEL_WARN, "HEXAFS: superblock write failed on commit");
        return 0;
    }
    hexafs_current_tx.active = 0;
    log_write(LOG_LEVEL_INFO, "HEXAFS: transaction committed");
    return 1;
}

int hexafs_tx_abort(void) {
    if (!hexafs_current_tx.active) return 0;
    hexafs_current_tx.active = 0;
    log_write(LOG_LEVEL_INFO, "HEXAFS: transaction aborted");
    return 1;
}

int hexafs_mount(void) {
    if (!hexafs_mount_disk()) {
        print_string("[HEXAFS] No form store found, formatting...\n");
        if (!hexafs_format()) {
            print_string("[HEXAFS] Format failed!\n");
            return 0;
        }
        uint32_t root_abs = hexafs_abstraction_create();
        if (!root_abs) {
            print_string("[HEXAFS] Root abstraction create failed!\n");
            return 0;
        }
        hexafs_tx_begin();
        hexafs_current_tx.new_root_block = root_abs;
        hexafs_current_tx.dirty = 1;
        hexafs_snap_create("boot_ok");
        hexafs_tx_commit();
        print_string("[HEXAFS] Fresh form store created.\n");
        return 1;
    }
    print_string("[HEXAFS] Mounted existing form store.\n");
    return 1;
}

int hexafs_format_fs(void) {
    return hexafs_format();
}

static uint32_t form_from_fentry(const char *content, int size, int owner, uint16_t mode) {
    (void)content;
    uint32_t form_block = hexafs_object_alloc(HEXAFS_FORM);
    if (!form_block) return 0;
    uint8_t buf[HEXAFS_BLOCK_SIZE - 64];
    uint32_t meta_offset = 0;
    memcpy(buf + meta_offset, &owner, 4); meta_offset += 4;
    memcpy(buf + meta_offset, &mode, 2); meta_offset += 2;
    uint32_t cap = 0;
    memcpy(buf + meta_offset, &cap, 4); meta_offset += 4;
    uint32_t content_copy_size = (uint32_t)size;
    if (content_copy_size > sizeof(buf) - meta_offset)
        content_copy_size = sizeof(buf) - meta_offset;
    if (content_copy_size > 0 && content)
        memcpy(buf + meta_offset, content, content_copy_size);
    uint32_t total_size = meta_offset + content_copy_size;
    if (!hexafs_object_write_data(form_block, buf, total_size)) {
        hexafs_free_block(form_block);
        return 0;
    }
    return form_block;
}

void hexafs_save_all(void) {
    if (!hexafs_mounted) return;
    hexafs_tx_begin();
    uint32_t new_abs = hexafs_abstraction_create();
    if (!new_abs) {
        hexafs_tx_abort();
        return;
    }
    for (int i = 0; i < form_count; i++) {
        uint32_t fb = form_from_fentry(form_table[i].content,
                                        form_table[i].size,
                                        form_table[i].owner,
                                        form_table[i].mode);
        if (fb) {
            hexafs_abstraction_add_entry(new_abs, form_table[i].name, fb, HEXAFS_FORM);
        }
    }
    hexafs_current_tx.new_root_block = new_abs;
    hexafs_current_tx.dirty = 1;
    hexafs_snap_create("live");
    hexafs_tx_commit();
}

void hexafs_load_all(void) {
    root_abs_block = sb_cache.root_snap_block;
    if (!root_abs_block) return;
    hexafs_snap_t snap;
    if (!hexafs_block_read(root_abs_block, &snap)) return;
    if (snap.magic != HEXAFS_SNAP_MAGIC) return;
    root_abs_block = snap.root_object_block;
    if (!root_abs_block) return;

    uint8_t buf[HEXAFS_BLOCK_SIZE];
    if (!hexafs_block_read(root_abs_block, buf)) return;
    hexafs_abs_header_t *hdr = (hexafs_abs_header_t *)buf;
    if (hdr->magic != HEXAFS_ABSTRACT_MAGIC) return;
    form_count = 0;
    for (int i = 0; i < (int)hdr->entry_count && form_count < 64; i++) {
        int j = 0;
        while (hdr->entries[i].name[j] && j < 31) {
            form_table[form_count].name[j] = hdr->entries[i].name[j];
            j++;
        }
        form_table[form_count].name[j] = 0;
        uint8_t content_buf[HEXAFS_BLOCK_SIZE];
        uint32_t data_size = sizeof(content_buf);
        uint8_t rtype;
        if (hexafs_object_read_data(hdr->entries[i].object_block, content_buf, &data_size, &rtype) && rtype == HEXAFS_FORM) {
            int owner = 0;
            uint16_t mode = 0x1A4;
            uint32_t meta_offset = 0;
            if (data_size >= 4) {
                memcpy(&owner, content_buf + meta_offset, 4);
                meta_offset += 4;
            }
            if (data_size >= meta_offset + 2) {
                memcpy(&mode, content_buf + meta_offset, 2);
                meta_offset += 2;
            }
            if (data_size >= meta_offset + 4) meta_offset += 4;
            int content_bytes = (int)(data_size - meta_offset);
            if (content_bytes < 0) content_bytes = 0;
            form_table[form_count].content = kmalloc((uint32_t)(content_bytes + 1));
            if (form_table[form_count].content) {
                if (content_bytes > 0)
                    memcpy(form_table[form_count].content, content_buf + meta_offset, (uint32_t)content_bytes);
                form_table[form_count].content[content_bytes] = 0;
                form_table[form_count].size = content_bytes;
                form_table[form_count].cap = content_bytes + 1;
            } else {
                form_table[form_count].content = kmalloc(1);
                if (form_table[form_count].content) {
                    form_table[form_count].content[0] = 0;
                    form_table[form_count].size = 0;
                    form_table[form_count].cap = 1;
                }
            }
            form_table[form_count].owner = (owner >= 0 && owner < 12) ? owner : 0;
            form_table[form_count].mode = mode;
        } else {
            form_table[form_count].content = kmalloc(1);
            if (form_table[form_count].content) {
                form_table[form_count].content[0] = 0;
                form_table[form_count].size = 0;
                form_table[form_count].cap = 1;
            }
            form_table[form_count].owner = 0;
            form_table[form_count].mode = 0x1A4;
        }
        form_count++;
    }
}
