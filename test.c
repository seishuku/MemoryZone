#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "system.h"
#include "memzone.h"

void WriteData(uint8_t *Ptr, size_t Size)
{
	for(size_t i=0;i<Size;i++)
		Ptr[i]=i%255;
}

int main(int argc, char **argv)
{
	MemZone_t *Zone=Zone_Init(32*1024*1024);

	void *ptr=Zone_Malloc(Zone, 4*1024*1024);
	WriteData(ptr, 4*1024*1024);

	Zone_Print(Zone);

	// Allocate memory with zone.
	uint8_t *ptr1=(uint8_t *)Zone_Malloc(Zone, 1001);
	WriteData(ptr1, 1001);

	uint8_t *ptr2=(uint8_t *)Zone_Malloc(Zone, 1002);
	WriteData(ptr2, 1002);

	uint8_t *ptr3=(uint8_t *)Zone_Malloc(Zone, 1003);
	WriteData(ptr3, 1003);

	uint8_t *ptr4=(uint8_t *)Zone_Malloc(Zone, 1004);
	WriteData(ptr4, 1004);

	uint8_t *ptr5=(uint8_t *)Zone_Malloc(Zone, 1005);
	WriteData(ptr5, 1005);

	Zone_Print(Zone);

	Zone_Free(Zone, ptr2);
	Zone_Free(Zone, ptr4);
	Zone_Print(Zone);

	ptr2=(uint8_t *)Zone_Malloc(Zone, 500);
	WriteData(ptr2, 500);

	ptr4=(uint8_t *)Zone_Malloc(Zone, 500);
	WriteData(ptr4, 500);
	Zone_Print(Zone);

	ptr2=(uint8_t *)Zone_Realloc(Zone, ptr2, 50);
	WriteData(ptr2, 50);

	ptr4=(uint8_t *)Zone_Realloc(Zone, ptr4, 51);
	WriteData(ptr4, 51);
	Zone_Print(Zone);

	ptr2=(uint8_t *)Zone_Realloc(Zone, ptr2, 800);
	WriteData(ptr2, 800);

	ptr4=(uint8_t *)Zone_Realloc(Zone, ptr4, 800);
	WriteData(ptr4, 800);
	Zone_Print(Zone);

	WriteData(ptr, 4*1024*1024);

	Zone_Free(Zone, ptr1);
	Zone_Print(Zone);
	Zone_Free(Zone, ptr2);
	Zone_Print(Zone);
	Zone_Free(Zone, ptr3);
	Zone_Print(Zone);
	Zone_Free(Zone, ptr4);
	Zone_Print(Zone);
	Zone_Free(Zone, ptr5);
	Zone_Print(Zone);

	Zone_Free(Zone, ptr);
	Zone_Print(Zone);

	DBGPRINTF(DEBUG_INFO, "Freeing zone...\n");
	free(Zone);

	return 0;
}
