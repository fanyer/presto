#ifndef _BASEAFF_HXX_
#define _BASEAFF_HXX_

#include "modules/spellchecker/hunspell_3p/src/hunspell/hunvisapi.h"

class LIBHUNSPELL_DLL_EXPORTED AffEntry
{
protected:
    char *         appnd;
    char *         strip;
    unsigned char  appndl;
    unsigned char  stripl;
    char           numconds;
    char           opts;
    unsigned short aflag;
    union {
        char       conds[MAXCONDLEN]; /* ARRAY OK 2009-02-04 jb */
        struct {
            char   conds1[MAXCONDLEN_1]; // ARRAY OK 2009-04-23 jb
            char * conds2;
        } l;
    } c;
    char *           morphcode;
    unsigned short * contclass;
    short            contclasslen;
};

#endif
