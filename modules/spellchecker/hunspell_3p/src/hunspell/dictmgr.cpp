
#include "core/pch.h"
#ifdef SPC_USE_HUNSPELL_ENGINE
#include "modules/spellchecker/hunspell_3p/src/hunspell/hunspell_common.h"

#include "modules/spellchecker/hunspell_3p/src/hunspell/dictmgr.h"

DictMgr::DictMgr(const char * dictpath, const char * etype) : numdict(0)
{
  // load list of etype entries
  pdentry = (dictentry *)op_malloc(MAXDICTIONARIES*sizeof(struct dictentry));
  if (pdentry) {
     if (parse_file(dictpath, etype)) {
        numdict = 0;
        // no dictionary.lst found is okay
     }
  }
}


DictMgr::~DictMgr() 
{
  dictentry * pdict = NULL;
  if (pdentry) {
     pdict = pdentry;
     for (int i=0;i<numdict;i++) {
        if (pdict->lang) {
            op_free(pdict->lang);
            pdict->lang = NULL;
        }
        if (pdict->region) {
            op_free(pdict->region);
            pdict->region=NULL;
        }
        if (pdict->filename) {
            op_free(pdict->filename);
            pdict->filename = NULL;
        }
        pdict++;
     }
     op_free(pdentry);
     pdentry = NULL;
     pdict = NULL;
  }
  numdict = 0;
}


// read in list of etype entries and build up structure to describe them
int  DictMgr::parse_file(const char * dictpath, const char * etype)
{

    int i;
    char line[MAXDICTENTRYLEN+1]; // ARRAY OK 2009-04-23 jb (NOT CHECKED)
    dictentry * pdict = pdentry;

    // open the dictionary list file
    HUNSPELL_FILE * dictlst;
    dictlst = hunspell_fopen(dictpath,"r");
    if (!dictlst) {
      return 1;
    }

    // step one is to parse the dictionary list building up the 
    // descriptive structures

    // read in each line ignoring any that dont start with etype
    while (hunspell_fgets(line,MAXDICTENTRYLEN,dictlst)) {
       mychomp(line);

       /* parse in a dictionary entry */
       if (op_strncmp(line,etype,4) == 0) {
          if (numdict < MAXDICTIONARIES) {
             char * tp = line;
             char * piece;
             i = 0;
             while ((piece=mystrsep(&tp,' '))) {
                if (*piece != '\0') {
                    switch(i) {
                       case 0: break;
                       case 1: pdict->lang = mystrdup(piece); break;
                       case 2: if (op_strcmp(piece, "ANY") == 0)
                                 pdict->region = mystrdup("");
                               else
                                 pdict->region = mystrdup(piece);
                               break;
                       case 3: pdict->filename = mystrdup(piece); break;
                       default: break;
                    }
                    i++;
                }
                op_free(piece);
             }
             if (i == 4) {
                 numdict++;
                 pdict++;
             } else {
                 //fprintf(hunspell_stderr,"dictionary list corruption in line \"%s\"\n",line);
                 //fflush(hunspell_stderr);
             }
          }
       }
    }
    hunspell_fclose(dictlst);
    return 0;
}

// return text encoding of dictionary
int DictMgr::get_list(dictentry ** ppentry)
{
  *ppentry = pdentry;
  return numdict;
}



// strip strings into token based on single char delimiter
// acts like strsep() but only uses a delim char and not 
// a delim string

char * DictMgr::mystrsep(char ** stringp, const char delim)
{
  char * rv = NULL;
  char * mp = *stringp;
  size_t n = op_strlen(mp);
  if (n > 0) {
     char * dp = (char *)op_memchr(mp,(int)((unsigned char)delim),n);
     if (dp) {
        *stringp = dp+1;
        size_t nc = dp - mp;
        rv = (char *) op_malloc(nc+1);
        if (rv) {
           op_memcpy(rv,mp,nc);
           *(rv+nc) = '\0';
        }
     } else {
       rv = (char *) op_malloc(n+1);
       if (rv) {
          op_memcpy(rv, mp, n);
          *(rv+n) = '\0';
          *stringp = mp + n;
       }
     }
  }
  return rv;
}


// replaces strdup with ansi version
char * DictMgr::mystrdup(const char * s)
{
  char * d = NULL;
  if (s) {
     int sl = op_strlen(s)+1;
     d = (char *) op_malloc(sl);
     if (d) op_memcpy(d,s,sl);
  }
  return d;
}


// remove cross-platform text line end characters
void DictMgr:: mychomp(char * s)
{
  int k = op_strlen(s);
  if ((k > 0) && ((*(s+k-1)=='\r') || (*(s+k-1)=='\n'))) *(s+k-1) = '\0';
  if ((k > 1) && (*(s+k-2) == '\r')) *(s+k-2) = '\0';
}

#endif //SPC_USE_HUNSPELL_ENGINE
