#ifndef PAGING_H
#define PAGING_H

#include "types.h"

#define PAGE_SIZE 4096
#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

// Page table entry
typedef struct {
    uint32_t present : 1;
    uint32_t rw      : 1;
    uint32_t user    : 1;
    uint32_t wthru   : 1;
    uint32_t cache   : 1;
    uint32_t accessed: 1;
    uint32_t dirty   : 1;
    uint32_t pat     : 1;
    uint32_t global  : 1;
    uint32_t avail   : 3;
    uint32_t addr    : 20;
} __attribute__((packed)) page_t;

// Page directory entry
typedef struct {
    uint32_t present : 1;
    uint32_t rw      : 1;
    uint32_t user    : 1;
    uint32_t wthru   : 1;
    uint32_t cache   : 1;
    uint32_t accessed: 1;
    uint32_t ign     : 1;
    uint32_t ps      : 1;
    uint32_t global  : 1;
    uint32_t avail   : 3;
    uint32_t addr    : 20;
} __attribute__((packed)) pd_entry_t;

// Physical Memory Manager
void pmm_init(uint32_t mem_size, uint32_t kernel_end);
uint32_t pmm_alloc(void);
void pmm_free(uint32_t addr);

// Paging
void paging_init(void);
void page_fault_handler(uint32_t cr2, uint32_t err);

// Kernel heap
void kheap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);

extern uint32_t _kernel_end;

#endif
