#ifdef __cplusplus
extern "C" {
#endif

void StartupSpelling( void );
void* StartSession( void );
void EndSession(void* session);
int CheckSpelling(const unsigned short* text, int len, int* err_start, int* err_length);
unsigned short** SuggestAlternativesForWord(const unsigned short* text, int len);
char* GetLanguage( void );

void *AllocMem(int size);
void Free(void* buf);

#ifdef __cplusplus
}
#endif
