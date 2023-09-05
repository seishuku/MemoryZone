#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "system.h"
#include "memzone.h"

static const size_t CHUNK_SIZE=8;
static const size_t HEADER_SIZE=sizeof(MemBlock_t);

MemZone_t *Zone_Init(size_t Size)
{
	Size+=sizeof(MemZone_t);

	MemZone_t *Zone=(MemZone_t *)malloc(Size);

	if(Zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Zone_Init: Unable to allocate memory for zone.\n");
		return false;
	}

	memset(Zone, 0, Size);

	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Zone+sizeof(MemZone_t));
	Block->Prev=NULL;
	Block->Next=NULL;
	Block->Free=true;
	Block->Size=Size;

	Zone->Current=Block;
	Zone->Size=Size;

#ifdef _DEBUG
	DBGPRINTF(DEBUG_INFO, "Zone_Init: Allocated at %p, size: %lluB\n", Zone, Size);
#endif
	return Zone;
}

void Zone_Destroy(MemZone_t *Zone)
{
	if(Zone)
		free(Zone);
}

void Zone_Free(MemZone_t *Zone, void *Ptr)
{
	if(Ptr==NULL)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Zone_Free: Attempted to free NULL pointer.\n");
#endif
		return;
	}

	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Ptr-HEADER_SIZE);

	if(Block->Free)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Zone_Free: Attempted to free already freed pointer.\n");
#endif
		return;
	}

#ifdef _DEBUG
	DBGPRINTF(DEBUG_WARNING, "Zone_Free: Freed block, location: %p, size: %lluB\n", Block, Block->Size-HEADER_SIZE);
#endif

	// Freeing a block is as simple as setting it to free.
	Block->Free=true;

	// Check previous and next blocks to see if either are free blocks,
	//     if they are, merge them all into one large free block.
	MemBlock_t *Last=Block->Prev;

	// If it's valid and a free block...
	if(Last&&Last->Free)
	{
		// Enlarge Last by Block's size
		Last->Size+=Block->Size;
		// Last's next pointer points to block's next, linking over it and removing Block.
		Last->Next=Block->Next;

		// If that's a valid pointer (could be null if at end of list),
		// fix up the previous pointer of Block's next so it points to last, completely removing Block.
		if(Last->Next)
			Last->Next->Prev=Last;

		// If Block was the head of the list, Last becomes the new head.
		if(Block==Zone->Current)
			Zone->Current=Last;

		// Block now becomes Last, for the next check below.
		Block=Last;
	}

	MemBlock_t *Next=Block->Next;

	// If it's valid and a free block...
	if(Next&&Next->Free)
	{
		// If it is, enlarge Block by it's size.
		Block->Size+=Next->Size;
		// Block's next becomes Next's next, removing Next.
		Block->Next=Next->Next;

		// If that's a valid pointer (could be null if at end of list),
		// fix up Block's Next's previous to point to Block, completely removing Next.
		if(Block->Next)
			Block->Next->Prev=Block;
	}
}

static MemBlock_t *createBlock(void *Base, size_t Offset, size_t Size, bool Free, MemBlock_t *Previous, MemBlock_t *Next)
{
	// Address for the new block is the base address + offset.
	MemBlock_t *newBlock=(MemBlock_t *)((uint8_t *)Base+Offset);

	// New size is the current free block size minus the requested size.
	newBlock->Size=Size;
	newBlock->Free=Free;
	newBlock->Prev=Previous;
	newBlock->Next=Next;

	return newBlock;
}

void *Zone_Malloc(MemZone_t *Zone, size_t Size)
{
	const size_t MinimumBlockSize=64;

	Size+=(HEADER_SIZE+CHUNK_SIZE-1)&~(CHUNK_SIZE-1);	// Align to chunk boundary

	MemBlock_t *Base=Zone->Current;

	while(Base!=NULL)
	{
		if(Base->Free&&Base->Size>=Size)
		{
			size_t Extra=Base->Size-Size;

			if(Extra>MinimumBlockSize)
			{
				MemBlock_t *New=createBlock(Base, Size, Extra, true, Base, Base->Next);

				Base->Next=New;
				Base->Size=Size;
			}

			Base->Free=false;

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Malloc: Allocated block, location: %p, size: %lluB\n", Base, Size-HEADER_SIZE);
#endif
			return (void *)((uint8_t *)Base+HEADER_SIZE);
		}

		Base=Base->Next;
	}

	// Size-HEADER_SIZE because the represented size in the structure is including the header, so actual requested size is -HEADER_SIZE.
	DBGPRINTF(DEBUG_ERROR, "Zone_Malloc: Unable locate large enough free block (%lluB).\n", Size-HEADER_SIZE);
	return NULL;
}

void *Zone_Realloc(MemZone_t *Zone, void *Ptr, size_t Size)
{
	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Ptr-HEADER_SIZE);
	size_t currentSize=Block->Size;

	Size+=HEADER_SIZE;

	if(Size==HEADER_SIZE)
	{
		Zone_Free(Zone, Ptr);
		return NULL;
	}
	else if(!Ptr)
		return Zone_Malloc(Zone, Size);
	else if(Size<=currentSize)
	{
		// If the sizes are equal, just bail.
		if(Size==currentSize)
		{
#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: New size == old size.\n");
#endif
			return Ptr;
		}

		// Otherwise, if there is a free block after, expand that block otherwise, create a new free block if possible.
#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: New size < old size.\n");
#endif

		// Check if Block->Next is a free block.
		if(Block->Next&&Block->Next->Free)
		{
			// Calculate the total new free size.
			size_t freeSize=currentSize-Size+Block->Next->Size;

			// Resize the original block's size to the new size.
			Block->Size=Size;

			// Create a new block at the end of this block's new size.
			// We'll just forget about the existing free block's header and just link over to whatever block is after it.
			MemBlock_t *newFreeBlock=createBlock(Block, Size, freeSize, true, Block, Block->Next->Next);

			// Next block is now the new free block.
			Block->Next=newFreeBlock;

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Free block merge.\n");
#endif
		}
		else
		{
			// If there isn't a free next block, insert a free block in the size of the remaining size.
			// But only if the new size is enough to also fit a block header in it's place.
			size_t freeSize=currentSize-Size;

			if(freeSize>HEADER_SIZE)
			{
				// Resize the original block's size to the new size.
				Block->Size=Size;

				// Create a new block at the end of this block's new size
				MemBlock_t *NewFreeBlock=createBlock(Block, Size, freeSize, true, Block, Block->Next);

				// Link the next block to the new free block.
				Block->Next=NewFreeBlock;

#ifdef _DEBUG
				DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Free block insert.\n");
#endif
			}
		}

		// Return the adjusted block's address after adjusting free blocks
		return Ptr;
	}
	else
	{
		MemBlock_t *Next=Block->Next;

		// if there is an adjacent free block
		if(Next&&Next->Free)
		{
			// calculate total size of this block and the free block
			size_t totalSize=currentSize+Next->Size;

			// if it is large enough to expand this block into
			if(Size<=totalSize)
			{
				size_t freeSize=totalSize-Size;

				// free size must be larger than just the header
				// in order to split a free block 
				if(freeSize>HEADER_SIZE)
				{
					MemBlock_t *newBlock=createBlock(Block, Size, freeSize, true, Block, Next->Next);

					Block->Size=Size;
					Block->Next=newBlock;

#ifdef _DEBUG
					DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Enlarging block into adjacent free block.\n");
#endif
				}
				else
				{
					Block->Next=Next->Next;
					Block->Size=totalSize;

#ifdef _DEBUG
					DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Enlarging block, consuming free block.\n");
#endif
				}

				return Ptr;
			}
		}

#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Allocating new block and copying.\n");
#endif
		void *New=Zone_Malloc(Zone, Size);

		if(New)
		{
			memcpy(New, Ptr, currentSize);
			Zone_Free(Zone, Ptr);
		}

		return New;
	}
}

void Zone_Print(MemZone_t *Zone)
{
	DBGPRINTF(DEBUG_WARNING, "Zone size: %lluB  Location: %p\n", Zone->Size, Zone);

	MemBlock_t *Block=Zone->Current;

	while(Block!=NULL)
	{
		DBGPRINTF(DEBUG_WARNING, "\tBlock: %p, Address: %p, Size: %lluB, Free: %s\n", Block, (uint8_t *)Block+HEADER_SIZE, Block->Size-HEADER_SIZE, Block->Free?"yes":"no");
		Block=Block->Next;
	}
}
