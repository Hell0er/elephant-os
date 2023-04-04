#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

/* 内存池标记，用于判断用哪个内存池 */
enum pool_flags{
    PF_KERNEL=1,   // 内核内存池
    PF_USER=2   // 用户内存池
};

#define PG_P_1  1   // 存在位
#define PG_P_0  0
#define PG_RW_R 0   // R/W位，读/执行
#define PG_RW_W 2   //        读/写/执行
#define PG_US_S 0   // U/S位，系统级
#define PG_US_U 4   //        用户级

/* 用于虚拟地址管理 */
struct virtual_addr{
    struct bitmap vaddr_bitmap;
    uint32_t vaddr_start;
};

/* 内存块 */
struct mem_block{
    struct list_elem free_elem;
};

/* 内存块描述符 */
struct mem_block_desc{
    uint32_t block_size;        // 内存块大小
    uint32_t blocks_per_arena;  // arena中可容纳的内存块数
    struct list free_list;      // 空闲的mem_block组成的链表
};

#define DESC_CNT 7   // 内存块描述符个数

extern struct pool kernel_pool,user_pool;
void mem_init(void);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
void* malloc_init(void);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt);
void pfree(uint32_t pg_phy_addr);
void sys_free(void* ptr);
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr);
void free_a_phy_page(uint32_t pg_phy_addr);
#endif

