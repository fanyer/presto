#include "core/pch.h"
#ifdef SPC_USE_HUNSPELL_ENGINE
#include "modules/spellchecker/hunspell_3p/src/hunspell/hunspell_common.h"
#include "modules/spellchecker/hunspell_3p/src/hunspell/license.hunspell"
#include "modules/spellchecker/hunspell_3p/src/hunspell/license.myspell"

#include "modules/spellchecker/hunspell_3p/src/hunspell/filemgr.h"

int FileMgr::fail(const char * err, const char * par) {
    //fprintf(hunspell_stderr, err, par);
    return -1;
}

FileMgr::FileMgr(const char * file, const char * key) {
    linenum = 0;
    hin = NULL;
    fin = hunspell_fopen(file, "r");
    if (!fin) {
        // check hzipped file
        char * st = (char *) op_malloc(op_strlen(file) + op_strlen(HZIP_EXTENSION) + 1);
        if (st) {
            op_strcpy(st, file);
            op_strcat(st, HZIP_EXTENSION);
            hin = OP_NEW(Hunzip,(st, key));
            op_free(st);
        }
    }    
    if (!fin && !hin) fail(MSG_OPEN, file);
}

FileMgr::~FileMgr()
{
    if (fin) hunspell_fclose(fin);
    if (hin) OP_DELETE( hin);
}

char * FileMgr::getline() {
    const char * l;
    linenum++;
    if (fin) return hunspell_fgets(in, BUFSIZE - 1, fin);
    if (hin && (l = hin->getline())) return op_strcpy(in, l);
    linenum--;
    return NULL;
}

int FileMgr::getlinenum() {
    return linenum;
}
#endif //SPC_USE_HUNSPELL_ENGINE
