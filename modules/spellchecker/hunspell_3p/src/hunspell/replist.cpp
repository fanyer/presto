#include "core/pch.h"
#ifdef SPC_USE_HUNSPELL_ENGINE
#include "modules/spellchecker/hunspell_3p/src/hunspell/hunspell_common.h"
#include "modules/spellchecker/hunspell_3p/src/hunspell/license.hunspell"
#include "modules/spellchecker/hunspell_3p/src/hunspell/license.myspell"

#include "modules/spellchecker/hunspell_3p/src/hunspell/replist.h"
#include "modules/spellchecker/hunspell_3p/src/hunspell/csutil.h"

RepList::RepList(int n) {
    dat = (replentry **) op_malloc(sizeof(replentry *) * n);
    if (dat == 0) size = 0; else size = n;
    pos = 0;
}

RepList::~RepList()
{
    for (int i = 0; i < pos; i++) {
        op_free(dat[i]->pattern);
        op_free(dat[i]->pattern2);
        op_free(dat[i]);
    }
    op_free(dat);
}

int RepList::get_pos() {
    return pos;
}

replentry * RepList::item(int n) {
    return dat[n];
}

int RepList::near_(const char * word) {
    int p1 = 0;
    int p2 = pos;
    while ((p2 - p1) > 1) {
      int m = (p1 + p2) / 2;
      int c = op_strcmp(word, dat[m]->pattern);
      if (c <= 0) {
        if (c < 0) p2 = m; else p1 = p2 = m;
      } else p1 = m;
    }
    return p1;
}

int RepList::match(const char * word, int n) {
    if (op_strncmp(word, dat[n]->pattern, op_strlen(dat[n]->pattern)) == 0) return op_strlen(dat[n]->pattern);
    return 0;
}

int RepList::add(char * pat1, char * pat2) {
    if (pos >= size || pat1 == NULL || pat2 == NULL) return 1;
    replentry * r = (replentry *) op_malloc(sizeof(replentry));
    if (r == NULL) return 1;
    r->pattern = mystrrep(pat1, "_", " ");
    r->pattern2 = mystrrep(pat2, "_", " ");
    r->start = false;
    r->end = false;
    dat[pos++] = r;
    for (int i = pos - 1; i > 0; i--) {
      r = dat[i];
      if (op_strcmp(r->pattern, dat[i - 1]->pattern) < 0) {
          dat[i] = dat[i - 1];
          dat[i - 1] = r;
      } else break;
    }
    return 0;
}

int RepList::conv(const char * word, char * dest) {
    int stl = 0;
    int change = 0;
    for (size_t i = 0; i < op_strlen(word); i++) {
        int n = near_(word + i);
        int l = match(word + i, n);
        if (l) {
          op_strcpy(dest + stl, dat[n]->pattern2);
          stl += op_strlen(dat[n]->pattern2);
          i += l - 1;
          change = 1;
        } else dest[stl++] = word[i];
    }
    dest[stl] = '\0';
    return change;
}
#endif //SPC_USE_HUNSPELL_ENGINE
