
// Some libraries (read: M2) needs to deallocate buffers allocated by Opera
// This is a standard interface for exchanging fonction pointers to CarbonNew, CarbonDelete & CarbonSetSize

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*MallocProc)(size_t);
typedef void* (*ReallocProc)(void *, size_t);
typedef void  (*FreeProc)(void *);

#if defined(_BUILDING_MAIL_)

extern MallocProc	opera_malloc;
extern ReallocProc	opera_realloc;
extern FreeProc		opera_free;

#else

typedef void  (*InitMemProc)(MallocProc,ReallocProc,FreeProc);
const unsigned char* InitMemProcName = "\pSetMemoryFuncs";

#endif // _BUILDING_OPERA_

#ifdef __cplusplus
}
#endif
