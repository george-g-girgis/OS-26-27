#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#include "../conc/ksemaphore.h"
#include <kern/cpu/cpu.h>

#define max_pages 1024

static uint32 pages[max_pages];
static int count = 0;

/************************************************************************************/
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [0] GIVEN FUNCTIONS
//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		buddy_init();
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = KERNEL_HEAP_START;
		kheapPageAllocBreak = KERNEL_HEAP_START;
	}
	//==================================================================================
	//==================================================================================
}
/************************************************************************************/

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
/**************************************************/
/* BLOCK ALLOCATOR */
/**************************************************/
#define KHEAP_MAX_FRAMES 65536

static uint32 frameToVA[KHEAP_MAX_FRAMES];

static void kheap_record_mapping(uint32 va, struct FrameInfo *fi)
{
	uint32 ppn = PPN(to_physical_address(fi));
	if (ppn < KHEAP_MAX_FRAMES)
		frameToVA[ppn] = va;
}

static void kheap_clear_mapping(struct FrameInfo *fi)
{
	uint32 ppn = PPN(to_physical_address(fi));
	if (ppn < KHEAP_MAX_FRAMES)
		frameToVA[ppn] = 0;
}
#define KHEAP_PAGES ((KERNEL_HEAP_MAX - KERNEL_HEAP_START) / PAGE_SIZE)

#define PG_NONE   0
#define PG_USED   1
#define PG_BUDDY  2
#define PG_FREE   3
#define NO_AREA   0xFFFF

struct PageInfo
{
	uint16 numPages;
	uint16 type;
	uint16 next;
};

static struct PageInfo pageInfo[KHEAP_PAGES];
static uint16 freeHead = NO_AREA;

static inline uint32 va_to_index(uint32 va)
{
	return (va - KERNEL_HEAP_START) / PAGE_SIZE;
}

static inline uint32 index_to_va(uint32 index)
{
	return KERNEL_HEAP_START + index * PAGE_SIZE;
}
static void add_free_area(uint32 start, uint32 n)
{
	pageInfo[start].numPages = 0;
	pageInfo[start].type = PG_NONE;
	pageInfo[start].next = NO_AREA;

	uint32 pp = NO_AREA;
	uint32 prev = NO_AREA;
	uint32 cur = freeHead;

	while (cur != NO_AREA && cur < start)
	{
		pp = prev;
		prev = cur;
		cur = pageInfo[cur].next;
	}

	if (cur != NO_AREA && start + n == cur)
	{
		n += pageInfo[cur].numPages;
		uint32 after = pageInfo[cur].next;
		pageInfo[cur].numPages = 0;
		pageInfo[cur].type = PG_NONE;
		pageInfo[cur].next = NO_AREA;
		cur = after;
	}

	int mergePrev = (prev != NO_AREA && prev + pageInfo[prev].numPages == start);

	if (index_to_va(start + n) == kheapPageAllocBreak)
	{
		if (mergePrev)
		{
			kheapPageAllocBreak = index_to_va(prev);
			if (pp == NO_AREA)
				freeHead = NO_AREA;
			else
				pageInfo[pp].next = NO_AREA;
			pageInfo[prev].numPages = 0;
			pageInfo[prev].type = PG_NONE;
			pageInfo[prev].next = NO_AREA;
		}
		else
		{
			kheapPageAllocBreak = index_to_va(start);
		}
		return;
	}

	if (mergePrev)
	{
		pageInfo[prev].numPages += n;
		pageInfo[prev].next = cur;
	}
	else
	{
		pageInfo[start].numPages = n;
		pageInfo[start].type = PG_FREE;
		pageInfo[start].next = cur;
		if (prev == NO_AREA)
			freeHead = start;
		else
			pageInfo[prev].next = start;
	}
}
void* create_buddy_space()
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [4] create_buddy_space
	//Your code is here
	//Comment the following line
	//panic("create_buddy_space() not implemented");
    void* addr = kmalloc(PAGE_SIZE);
    if (addr == NULL)
    {
        return NULL;
    }
    if (count < max_pages)
    {
        pages[count] = (uint32)addr;
        count++;
    }
    return addr;
}

void  delete_buddy_space(void *va)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [5] delete_buddy_space
	//Your code is here
	//Comment the following line
	//panic("delete_buddy_space() not implemented");
    if (va == NULL)
    {
        return;
    }
    uint32 page = (uint32)va;
    for (int i = 0; i < count; i++)
    {
        if (pages[i] == page)
        {
            pages[i] = pages[count - 1];
            count--;
            break;
        }
    }
    kfree(va);
}

/**************************************************/
/* PAGE ALLOCATOR */
/**************************************************/
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [6] kmalloc
	//Your code is here
	if (size == 0)
		return NULL;
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
		return alloc_block(size);
	uint32 pagesNeeded = size / PAGE_SIZE;
	if (size % PAGE_SIZE != 0)
		pagesNeeded++;
	uint32 freePages = (KERNEL_HEAP_MAX - kheapPageAllocBreak) / PAGE_SIZE;
	if (pagesNeeded > freePages)
		return NULL;
	uint32 va = kheapPageAllocBreak;
	uint32 firstIndex = va_to_index(va);
	kheapPageAllocBreak += pagesNeeded * PAGE_SIZE;
	pageInfo[firstIndex].numPages = pagesNeeded;
	pageInfo[firstIndex].type = PG_USED;
	pageInfo[firstIndex].next = NO_AREA;
	for (uint32 i = 0; i < pagesNeeded; i++)
	{
		uint32 pageVA = va + i * PAGE_SIZE;
		struct FrameInfo *fi = NULL;
		allocate_frame(&fi);
		map_frame(ptr_page_directory, fi, pageVA, PERM_WRITEABLE);
		kheap_record_mapping(pageVA, fi);
	}
	return (void*)va;
}


void kfree(void* virtual_address)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [7] kfree
	//Your code is here
		uint32 va = (uint32)virtual_address;
		if (va < KERNEL_HEAP_START || va >= kheapPageAllocBreak)
			panic("kfree: invalid address");
		uint32 pageIdx = va_to_index(ROUNDDOWN(va, PAGE_SIZE));
		if (pageInfo[pageIdx].type == PG_BUDDY)
		{
			free_block(virtual_address);
			return;
		}
		if (va % PAGE_SIZE != 0 || pageInfo[pageIdx].type != PG_USED)
			panic("kfree: invalid address");
		uint32 n = pageInfo[pageIdx].numPages;
		for (uint32 i = 0; i < n; i++)
		{
			uint32 pageVA = index_to_va(pageIdx + i);
			uint32 *ptr_table = NULL;
			struct FrameInfo *fi = get_frame_info(ptr_page_directory, pageVA, &ptr_table);
			if (fi != NULL)
				kheap_clear_mapping(fi);
			unmap_frame(ptr_page_directory, pageVA);
		}
		add_free_area(pageIdx, n);
}



unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [8] kheap_physical_address
	//Your code is here
	uint32 *ptr_table = NULL;
	struct FrameInfo *fi = get_frame_info(ptr_page_directory, virtual_address, &ptr_table);
	if (fi == NULL)
		return 0;
	return to_physical_address(fi) + PGOFF(virtual_address);
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [9] kheap_virtual_address
	//Your code is here
	uint32 ppn = PPN(physical_address);
	if (ppn >= number_of_frames || ppn >= KHEAP_MAX_FRAMES)
		return 0;
	uint32 va = frameToVA[ppn];
	if (va == 0)
		return 0;
	return va + PGOFF(physical_address);
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().
void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'26.BONUS#1] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
