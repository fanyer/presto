#ifndef _CARBONMEMORY_H_
#define _CARBONMEMORY_H_
#include <stdlib.h>

enum
{
	kAllocStandard,
	kAllocCalloc
};

void *CarbonMalloc(size_t size);
void *CarbonCalloc(size_t nmemb, size_t size);
void *CarbonRealloc(void* buf, size_t newSize);
void CarbonFree(void *buf);
char *CarbonStrdup(const char * src);

#endif
