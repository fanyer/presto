#ifndef debug_mem_H
#define debug_mem_H

void *zeroset(void *address, long byteCount);
void *deadbeefset(void *address, long byteCount);
void *feebdaedset(void *address, long byteCount);

#if defined(MAC_MEMORY_DEBUG)
void PlotBuffers();
void CheckAllBuffers();
void *WidgetMemNew(size_t size);
void WidgetMemDelete(void *buf);
void *WidgetMemSetSize(void* buf, size_t newSize);
void *AppMemNew(size_t size);
void AppMemDelete(void *buf);
void *AppMemSetSize(void* buf, size_t newSize);
#endif

#ifdef BUFFER_PROTECTION
	//Buffer protection works with buffers created with new, new[] and op_malloc
bool ProtectBuffer(void * buf);		//Stop in the debugger if deleted
void UnprotectBuffer(void * buf);	//Call before it's meant to be deleted.
bool IsBufferProtected(void * buf);
#define CHECK_DELETE_PROTECTION(buf) OP_ASSERT(!IsBufferProtected(buf));
#endif

#ifdef BUFFER_LIMIT_CHECK
	//find buffer overflows.
	//find new/free & malloc/delete mismatches.
	//find out whether a buffer was allocated by you, from anywhere in the code.
	//find double deletes.
	// note:	The last char in these markers are bullets in MacRoman,
	//			So it should be relatively easy to locate them in the memory window.
#define NEW_MAGIC		'new\245'
#define NEWA_MAGIC		'nwa\245'
#define OP_ALLOC_MAGIC	'olc\245'
#define MALLOC_MAGIC	'mal\245'

#if defined(OPERA_WIDGET_APP)
#define MASTER_MAGIC	'APP#'
#elif defined(MAC_WIDGET)
#define MASTER_MAGIC	'WDG#'
#else
#define MASTER_MAGIC	'MST#'
#endif

#define LIMIT_CHECK_PRE_SIZE	(16)
#define LIMIT_CHECK_POST_SIZE	(8)
#define LIMIT_CHECK_OVERHEAD	(LIMIT_CHECK_PRE_SIZE + LIMIT_CHECK_POST_SIZE)

#define LIMIT_CHECK_ALLOC_SIZE(len)	len += LIMIT_CHECK_OVERHEAD;

extern unsigned long gMemMarkerCount;
extern unsigned long gMemMaxSize;

#define BUFFER_SIZE(buf) ((buf) ? (*((unsigned long*) (((char*)buf) - LIMIT_CHECK_PRE_SIZE))) : 0)

#ifdef MAC_MEMORY_DEBUG
#define INC_MARKER_COUNT(marker) if (marker == MASTER_MAGIC) gMemMarkerCount++;
#else
#define INC_MARKER_COUNT(marker) gMemMarkerCount++;
#endif

#define SET_MARKERS(buf, len, marker)								\
			if (buf) {												\
				len -= LIMIT_CHECK_OVERHEAD;						\
				char * b = (char *)buf;								\
				*((unsigned long*) b) = len;						\
				*((unsigned long*) (b + 4)) = gMemMarkerCount;		\
				*((unsigned long*) (b + 8)) = marker;				\
				*((unsigned long*) (b + 12)) = '---<';				\
				*((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len)) = '>---';	\
				*((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len + 4)) = marker;	\
				INC_MARKER_COUNT(marker)							\
				if (len > gMemMaxSize) gMemMaxSize = len;			\
				buf = (b + LIMIT_CHECK_PRE_SIZE);					\
			}


#define CHECK_MARKERS(buf, marker)									\
			if (buf) {												\
				char * b = ((char*)buf) - LIMIT_CHECK_PRE_SIZE;		\
				unsigned long len = *((unsigned long*) b);			\
				OP_ASSERT(*((unsigned long*) (b + 12)) == '---<');		\
				OP_ASSERT(*((unsigned long*) (b + 8)) == marker);		\
				OP_ASSERT(*((unsigned long*) (b + 4)) <= gMemMarkerCount);\
				OP_ASSERT(len <= gMemMaxSize);							\
				OP_ASSERT(*((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len)) == '>---');	\
				OP_ASSERT(*((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len + 4)) == marker);	\
				*((unsigned long*) (b + 12)) = 'xxxx';			\
				*((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len)) = 'xxxx';	\
				buf = b;											\
			}

#define CHECK_AND_LEAVE_MARKERS(buf)									\
			if (buf) {												\
				char * b = ((char*)buf) - LIMIT_CHECK_PRE_SIZE;		\
				unsigned long len = *((unsigned long*) b);			\
				OP_ASSERT(*((unsigned long*) (b + 12)) == '---<');		\
				OP_ASSERT(*((unsigned long*) (b + 4)) <= gMemMarkerCount);\
				OP_ASSERT(len <= gMemMaxSize);							\
				OP_ASSERT(*((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len)) == '>---');	\
				OP_ASSERT(*((unsigned long*) (b + 8)) == *((unsigned long*) (b + LIMIT_CHECK_PRE_SIZE + len + 4)));	\
			}
#endif //BUFFER_LIMIT_CHECK

#endif //debug_mem_H
