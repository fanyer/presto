/* file manager class - read lines of files [filename] OR [filename.hz] */
#ifndef _FILEMGR_HXX_
#define _FILEMGR_HXX_
#include "modules/spellchecker/hunspell_3p/src/hunspell/hunvisapi.h"

#include "modules/spellchecker/hunspell_3p/src/hunspell/hunzip.h"

class LIBHUNSPELL_DLL_EXPORTED FileMgr
{
protected:
    HUNSPELL_FILE * fin;
    Hunzip * hin;
    char in[BUFSIZE + 50]; // ARRAY OK 2009-04-23 jb // input buffer
    int fail(const char * err, const char * par);
    int linenum;

public:
    FileMgr(const char * filename, const char * key = NULL);
    ~FileMgr();
    char * getline();
    int getlinenum();
};
#endif
