/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

/************************************************************************************/
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [0] GIVEN FUNCTIONS
//==================================
// [1] INITIALIZE BUDDY FREE LISTS:
//==================================
void buddy_init()
{
	//panic("buddy_init is not implemented yet");

	for (int l = BUDDY_LOWER_LEVEL; l <= BUDDY_UPPER_LEVEL; ++l)
	{
		LIST_INIT(&(freeBlockLists[l]));
	}
}
//====================================
// [2] PRINT SIZE OF FREE LISTS:
//====================================
void PrintFreeBlockLists()
{
	for (int i = BUDDY_LOWER_LEVEL; i <= BUDDY_UPPER_LEVEL; ++i)
	{
		cprintf("Level#%d: %d\n", i, LIST_SIZE(&freeBlockLists[i]));
	}
}
/************************************************************************************/

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [1] get_block_size
	//Your code is here
	//Comment the following line
	panic("get_block_size() not implemented");

}
//===========================
// [2] ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 requiredSize)
{
	//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [2] alloc_block
	//Your code is here
	//Comment the following line
	panic("alloc_block() not implemented");
}

//===================================================
// [3] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{
	//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [3] free_block
	//Your code is here
	//Comment the following line
	//panic("free_block() not implemented");
	if(va == NULL) return;
	struct BlockMetaData *block = (struct BlockMetaData *)((uint32)va - META_DATA_SIZE);
	if(block->is_free == 1) return;
	while(block->level <= BUDDY_UPPER_LEVEL){
		uint32 buddy_addr = (uint32)block ^ BLOCK_SIZE(block->level);
		struct BlockMetaData *buddy = (struct BlockMetaData *)buddy_addr;
		if(!buddy->is_free || buddy->level != block->level) break;
		LIST_REMOVE(&(freeBlockLists[buddy->level]), buddy);
		if ((uint32)buddy < (uint32)block){
			block->level = 0;
			block->is_free = 0;
			block = buddy;
		} else {
			buddy->level = 0;
			buddy->is_free = 0;
		}
		if (block->level == BUDDY_UPPER_LEVEL){
			block->level = 0;
			block->is_free = 0;
			delete_buddy_space((void*)block);
			return;
		}
		block->level++;
	}
	block->is_free = 1;
	LIST_INSERT_TAIL(&(freeBlockLists[block->level]), block);
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 newSize)
{
	//TODO: [PROJECT'26.BONUS#1] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
