/* string replacement list class */
#ifndef _REPLIST_HXX_
#define _REPLIST_HXX_
#include "modules/spellchecker/hunspell_3p/src/hunspell/w_char.h"

class RepList
{
protected:
    replentry ** dat;
    int size;
    int pos;

public:
    RepList(int n);
    ~RepList();

    int get_pos();
    int add(char * pat1, char * pat2);
    replentry * item(int n);
    int near_(const char * word);
    int match(const char * word, int n);
    int conv(const char * word, char * dest);
};
#endif
