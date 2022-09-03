#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "memzone.h"

#define DBGPRINTF(...) { fprintf(stderr, __VA_ARGS__); }

const size_t ZoneSize=4*1024*1024; // 4MB memory

void Zone_Print(MemZone_t *Zone)
{
	DBGPRINTF("\nZone size: %0.2fKB  Location: 0x%p\n", (float)(Zone->Size/1024.0f), Zone);

	for(MemBlock_t *Block=Zone->Blocks.Next;;Block=Block->Next)
	{
		DBGPRINTF("Block: 0x%p Size: %0.2fKB Block free: %s\n", Block, (float)(Block->Size/1024.0f), Block->Free?"no":"yes");

		if(Block->Next==&Zone->Blocks)
			break;
	}
}

int main(int argc, char **argv)
{
	const char TestString[]={ "Hello World!\0" };
	MemZone_t *Zone=Zone_Init(ZoneSize);

	if(Zone==NULL)
		return -1;

	for(int i=0;i<2000;i++)
	{
		char *test=Zone_Malloc(Zone, (size_t)(rand()%4096)+14);

		if(test)
		{
			memcpy(test, TestString, strlen(TestString)+1);
			printf("%s ", test);
		}
		else
			DBGPRINTF("Ran out of memory!\n");
	}

	// Print zone stats
	Zone_Print(Zone);

	while(!kbhit());

	// Free zone memory
 	Zone_Destroy(Zone);

	return 0;
}
