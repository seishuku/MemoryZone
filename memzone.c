#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "memzone.h"

#ifndef DBGPRINTF
#define DBGPRINTF(...) { fprintf(stderr, __VA_ARGS__); }
#endif

MemZone_t *Zone_Init(size_t Size)
{
	MemZone_t *Zone=(MemZone_t *)malloc(Size);

	if(Zone==NULL)
	{
		DBGPRINTF("Unable to allocate memory for zone.\n");
		return false;
	}

	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Zone+sizeof(MemZone_t));
	Block->Prev=&Zone->Blocks;
	Block->Next=&Zone->Blocks;
	Block->Free=false;
	Block->Size=Size-sizeof(MemZone_t);

	Zone->Blocks.Next=Block;
	Zone->Blocks.Prev=Block;
	Zone->Blocks.Free=true;
	Zone->Blocks.Size=0;

	Zone->Current=Block;

	Zone->Size=Size;

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
		DBGPRINTF("Attempting to free NULL pointer\n");
		return;
	}

	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Ptr-sizeof(MemBlock_t));

	if(!Block->Free)
	{
		DBGPRINTF("Attempting to free already freed pointer.\n");
		return;
	}

	Block->Free=false;

	MemBlock_t *Last=Block->Prev;

	if(!Last->Free)
	{
		Last->Size+=Block->Size;
		Last->Next=Block->Next;
		Last->Next->Prev=Last;

		if(Block==Zone->Current)
			Zone->Current=Last;

		Block=Last;
	}

	MemBlock_t *Next=Block->Next;

	if(!Next->Free)
	{
		Block->Size+=Next->Size;
		Block->Next=Next->Next;
		Block->Next->Prev=Block;

		if(Next==Zone->Current)
			Zone->Current=Block;
	}
}

void *Zone_Malloc(MemZone_t *Zone, size_t Size)
{
	const size_t MinimumBlockSize=64;

	Size+=sizeof(MemBlock_t);		// Size of block header
	Size=(Size+7)&~7;				// Align to 64bit boundary

	MemBlock_t *Base=Zone->Current;
	MemBlock_t *Current=Zone->Current;
	MemBlock_t *Start=Base->Prev;

	do
	{
		if(Current==Start)
			return NULL;

		if(Current->Free)
		{
			Base=Current->Next;
			Current=Current->Next;
		}
		else
			Current=Current->Next;
	}
	while(Base->Free||Base->Size<Size);

	size_t Extra=Base->Size-Size;

	if(Extra>MinimumBlockSize)
	{
		MemBlock_t *New=(MemBlock_t *)((uint8_t *)Base+Size);
		New->Size=Extra;
		New->Free=false;
		New->Prev=Base;
		New->Next=Base->Next;
		New->Next->Prev=New;

		Base->Next=New;
		Base->Size=Size;
	}

	Base->Free=true;

	Zone->Current=Base->Next;

	return (void *)((uint8_t *)Base+sizeof(MemBlock_t));
}
