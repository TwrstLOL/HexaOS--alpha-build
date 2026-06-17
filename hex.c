#include "types.h"
#include "hex.h"
#include "elf.h"
#include "hexafs.h"
#include "log.h"

extern void *memset(void *dest, int c, size_t len);
extern void *memcpy(void *dest, const void *src, size_t len);
extern void print_string(const char *str);
extern void print_color(const char *str, uint8_t color);
extern int strcmp(const char *s1, const char *s2);
extern size_t strlen(const char *str);
extern void itoa(int num, char *str, int base);

int hex_validate(hex_header_t *hdr) {
    if (!hdr) return 0;
    if (hdr->magic != HEX_MAGIC) return 0;
    uint32_t ck = 0;
    uint8_t *p = (uint8_t *)hdr;
    for (uint32_t i = 0; i < sizeof(hex_header_t) - 4; i++)
        ck = ((ck << 5) + ck) + p[i];
    if (ck != hdr->checksum) return 0;
    return 1;
}

int hex_load(hex_header_t *hdr, uint32_t *entry, uint32_t *heap_start) {
    if (!hex_validate(hdr)) return -1;
    if (entry) *entry = hdr->entry_point;
    if (heap_start) *heap_start = 0x40000000;
    log_write(LOG_LEVEL_INFO, "hex: binary loaded");
    return 0;
}

int hex_pack_elf(const char *elf_name, const char *caps_str, uint32_t in_schema, uint32_t out_schema) {
    (void)elf_name;
    (void)caps_str;
    (void)in_schema;
    (void)out_schema;
    print_string("hexpack: wrapping ELF into HEX format (stub)\n");
    return 0;
}
