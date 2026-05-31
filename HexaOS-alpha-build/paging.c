#include "types.h"
#include "paging.h"
#include "interrupts.h"

// ---- Physical Memory Manager (bitmap) ----
// Manages memory from kernel_end up to mem_max (32MB default)
#define PMM_MAX_MEM  (32 * 1024 * 1024)
#define PMM_PAGE_CNT (PMM_MAX_MEM / PAGE_SIZE)
#define PMM_BMAP_SZ  (PMM_PAGE_CNT / 8)

static uint8_t pmm_bitmap[PMM_BMAP_SZ];
static uint32_t pmm_max_page = 0;
static uint32_t pmm_last_alloc = 0;

#define PAGE_FREE 0
#define PAGE_USED 1

static void pmm_set(uint32_t page) {
    pmm_bitmap[page / 8] |= (1 << (page % 8));
}
static void pmm_clear(uint32_t page) {
    pmm_bitmap[page / 8] &= ~(1 << (page % 8));
}
static int pmm_test(uint32_t page) {
    return (pmm_bitmap[page / 8] >> (page % 8)) & 1;
}

void pmm_init(uint32_t mem_size, uint32_t kernel_end) {
    uint32_t top = mem_size;
    if (top > PMM_MAX_MEM) top = PMM_MAX_MEM;
    pmm_max_page = top / PAGE_SIZE;
    // Mark all pages as used initially
    for (uint32_t i = 0; i < PMM_BMAP_SZ; i++) pmm_bitmap[i] = 0xFF;
    // Mark pages from 0 to kernel_end as used, then free the rest
    uint32_t kernel_pages = kernel_end / PAGE_SIZE;
    if (kernel_end % PAGE_SIZE) kernel_pages++;
    if (kernel_pages > pmm_max_page) kernel_pages = pmm_max_page;
    // Free pages from kernel_pages to pmm_max_page
    for (uint32_t i = kernel_pages; i < pmm_max_page; i++) pmm_clear(i);
    // But keep the first page free (NULL guard)
    pmm_set(0);
    // Also mark VGA memory (0xA0000 - 0xBFFFF) as used
    // and BIOS/firmware area 0x9FC00-0xFFFFF
    for (uint32_t i = 0xA0; i < 0x100; i++) pmm_set(i); // 0xA0000-0xFFFFF
    pmm_last_alloc = kernel_pages;
}

uint32_t pmm_alloc(void) {
    for (uint32_t i = pmm_last_alloc; i < pmm_max_page; i++) {
        if (!pmm_test(i)) {
            pmm_set(i);
            pmm_last_alloc = i + 1;
            return i * PAGE_SIZE;
        }
    }
    // Wrap around
    for (uint32_t i = 1; i < pmm_last_alloc; i++) {
        if (!pmm_test(i)) {
            pmm_set(i);
            pmm_last_alloc = i + 1;
            return i * PAGE_SIZE;
        }
    }
    return 0; // OOM
}

void pmm_free(uint32_t addr) {
    uint32_t page = addr / PAGE_SIZE;
    if (page < pmm_max_page) {
        pmm_clear(page);
        if (page < pmm_last_alloc) pmm_last_alloc = page;
    }
}

// ---- Paging ----
static uint32_t *page_dir = 0;
static uint32_t page_dir_phys = 0;

void paging_init(void) {
    // Allocate page directory from PMM
    page_dir_phys = pmm_alloc();
    page_dir = (uint32_t *)page_dir_phys;
    // Clear page directory
    for (int i = 0; i < 1024; i++) page_dir[i] = 0x002; // supervisor, rw, not present
    // Identity-map first 8MB (2 page tables)
    // Page table 0: 0x00000000 - 0x003FFFFF (4MB)
    // Page table 1: 0x00400000 - 0x007FFFFF (4MB)
    for (int tbl = 0; tbl < 2; tbl++) {
        uint32_t pt_phys = pmm_alloc();
        uint32_t *pt = (uint32_t *)pt_phys;
        for (int i = 0; i < 1024; i++) {
            uint32_t addr = (tbl * 0x400000) + (i * PAGE_SIZE);
            pt[i] = addr | 0x003; // present, rw, supervisor
        }
        page_dir[tbl] = pt_phys | 0x003; // present, rw, supervisor
    }
    // Load CR3
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_dir_phys));
    // Enable paging
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // PG bit
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

// ---- Kernel Heap (simple linked-list allocator) ----
#define HEAP_START 0x00800000  // 8MB (past identity-mapped region)
#define HEAP_INIT_SIZE (1024 * 1024) // 1MB initial heap

typedef struct heap_block {
    uint32_t size;
    int free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_first = 0;

void kheap_init(void) {
    // Map initial heap pages FIRST, before any heap writes
    uint32_t heap_end = HEAP_START + HEAP_INIT_SIZE;
    for (uint32_t addr = HEAP_START; addr < heap_end; addr += PAGE_SIZE) {
        if ((page_dir[addr >> 22] & 1) == 0) {
            uint32_t pt_phys = pmm_alloc();
            uint32_t *pt = (uint32_t *)pt_phys;
            for (int i = 0; i < 1024; i++) pt[i] = 0x002;
            page_dir[addr >> 22] = pt_phys | 0x003;
            __asm__ volatile("mov %0, %%cr3" : : "r"(page_dir_phys));
        }
        uint32_t *pt = (uint32_t *)(page_dir[addr >> 22] & ~0xFFF);
        uint32_t pt_idx = (addr >> 12) & 0x3FF;
        if ((pt[pt_idx] & 1) == 0) {
            uint32_t phys = pmm_alloc();
            pt[pt_idx] = phys | 0x003;
        }
    }
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_dir_phys));
    // Now safe to set up heap metadata (pages are mapped)
    heap_first = (heap_block_t *)HEAP_START;
    heap_first->size = HEAP_INIT_SIZE - sizeof(heap_block_t);
    heap_first->free = 1;
    heap_first->next = 0;
}

void *kmalloc(size_t size) {
    if (!heap_first || size == 0) return 0;
    // Align size
    size = (size + 3) & ~3;
    heap_block_t *curr = heap_first;
    while (curr) {
        if (curr->free && curr->size >= size) {
            // Split if enough room
            if (curr->size > size + sizeof(heap_block_t) + 4) {
                heap_block_t *new_block = (heap_block_t *)((uint32_t)curr + sizeof(heap_block_t) + size);
                new_block->size = curr->size - size - sizeof(heap_block_t);
                new_block->free = 1;
                new_block->next = curr->next;
                curr->size = size;
                curr->next = new_block;
            }
            curr->free = 0;
            return (void *)((uint32_t)curr + sizeof(heap_block_t));
        }
        curr = curr->next;
    }
    return 0; // OOM
}

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *block = (heap_block_t *)((uint32_t)ptr - sizeof(heap_block_t));
    block->free = 1;
    // Merge with next if free
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
    }
    // Merge with prev (scan from start)
    heap_block_t *curr = heap_first;
    while (curr && curr->next != block) curr = curr->next;
    if (curr && curr->free) {
        curr->size += sizeof(heap_block_t) + block->size;
        curr->next = block->next;
    }
}
