#include <asm/io.h>

struct map_desc {
	unsigned long virtual;
	unsigned long pfn;
	unsigned long length;
	unsigned int type;
};

/* types 0-3 are defined in asm/io.h */
#define MT_UNCACHED		4
#define MT_CACHECLEAN		5
#define MT_MINICLEAN		6
#define MT_LOW_VECTORS		7
#define MT_HIGH_VECTORS		8
#define MT_MEMORY		9
#define MT_ROM			10
#define MT_MEMORY_NONCACHED	11
#define MT_DEVICE_STRONGLY_ORDERED 12
#define MT_MEMORY_R		13
#define MT_MEMORY_RW		14
#define MT_MEMORY_RX		15

#ifdef CONFIG_MMU
extern void iotable_init(struct map_desc *, int);

struct mem_type {
	unsigned int prot_pte;
	unsigned int prot_l1;
	unsigned int prot_sect;
	unsigned int domain;
};

extern const struct mem_type *get_mem_type(unsigned int type);

extern int ioremap_page(unsigned long virt, unsigned long phys,
			const struct mem_type *mtype);
#else
#define iotable_init(map,num)	do { } while (0)
#endif
