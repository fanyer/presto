#include "core/pch.h"
#ifdef SPC_USE_HUNSPELL_ENGINE
#include "modules/spellchecker/hunspell_3p/src/hunspell/hunspell_common.h"
#include "modules/spellchecker/hunspell_3p/src/hunspell/license.hunspell"
#include "modules/spellchecker/hunspell_3p/src/hunspell/license.myspell"

#include "modules/spellchecker/hunspell_3p/src/hunspell/hashmgr.h"
#include "modules/spellchecker/hunspell_3p/src/hunspell/csutil.h"
#include "modules/spellchecker/hunspell_3p/src/hunspell/atypes.h"

#ifdef MOZILLA_CLIENT
#ifdef __SUNPRO_CC // for SunONE Studio compiler
using namespace std;
#endif
#else
#ifndef WIN32
using namespace std;
#endif
#endif

// build a hash table from a munched word list

HashMgr::HashMgr(const char * tpath, const char * apath, const char * key)
{
  tablesize = 0;
  tableptr = NULL;
  flag_mode = FLAG_CHAR;
  complexprefixes = 0;
  utf8 = 0;
  langnum = 0;
  lang = NULL;
  enc = NULL;
  csconv = 0;
  ignorechars = NULL;
  ignorechars_utf16 = NULL;
  ignorechars_utf16_len = 0;
  numaliasf = 0;
  aliasf = NULL;
  numaliasm = 0;
  aliasm = NULL;
  forbiddenword = FORBIDDENWORD; // forbidden word signing flag
  load_config(apath, key);
  int ec = load_tables(tpath, key);
  if (ec) {
    /* error condition - what should we do here */
    HUNSPELL_WARNING(hunspell_stderr, "Hash Manager Error : %d\n",ec);
    if (tableptr) {
      op_free(tableptr);
      tableptr = NULL;
    }
    tablesize = 0;
  }
}


HashMgr::~HashMgr()
{
  if (tableptr) {
    // now pass through hash table freeing up everything
    // go through column by column of the table
    for (int i=0; i < tablesize; i++) {
      struct hentry * pt = tableptr[i];
      struct hentry * nt = NULL;
      while(pt) {
        nt = pt->next;
        if (pt->astr && (!aliasf || TESTAFF(pt->astr, ONLYUPCASEFLAG, pt->alen))) op_free(pt->astr);
        op_free(pt);
        pt = nt;
      }
    }
    op_free(tableptr);
  }
  tablesize = 0;

  if (aliasf) {
    for (int j = 0; j < (numaliasf); j++) op_free(aliasf[j]);
    op_free(aliasf);
    aliasf = NULL;
    if (aliasflen) {
      op_free(aliasflen);
      aliasflen = NULL;
    }
  }
  if (aliasm) {
    for (int j = 0; j < (numaliasm); j++) op_free(aliasm[j]);
    op_free(aliasm);
    aliasm = NULL;
  }  

#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
#ifdef HUNSPELL_USE_UTF_TBL
  if (utf8) free_utf_tbl();
#endif
#endif
#endif

  if (enc) op_free(enc);
  if (lang) op_free(lang);
  
  if (ignorechars) op_free(ignorechars);
  if (ignorechars_utf16) op_free(ignorechars_utf16);

#ifdef MOZILLA_CLIENT
    OP_DELETEA(csconv);
#endif
}

// lookup a root word in the hashtable

struct hentry * HashMgr::lookup(const char *word) const
{
    struct hentry * dp;
    if (tableptr) {
       dp = tableptr[hash(word)];
       if (!dp) return NULL;
       for (  ;  dp != NULL;  dp = dp->next) {
          if (op_strcmp(word, dp->word) == 0) return dp;
       }
    }
    return NULL;
}

// add a word to the hash table (private)
int HashMgr::add_word(const char * word, int wbl, int wcl, unsigned short * aff,
    int al, const char * desc, bool onlyupcase)
{
    bool upcasehomonym = false;
    int descl = desc ? (aliasm ? sizeof(short) : op_strlen(desc) + 1) : 0;
    // variable-length hash record with word and optional fields
    struct hentry* hp = 
	(struct hentry *) op_malloc(sizeof(struct hentry) + wbl + descl);
    if (!hp) return 1;
    char * hpw = hp->word;
    op_strcpy(hpw, word);
    if (ignorechars != NULL) {
      if (utf8) {
        remove_ignored_chars_utf(hpw, ignorechars_utf16, ignorechars_utf16_len);
      } else {
        remove_ignored_chars(hpw, ignorechars);
      }
    }
    if (complexprefixes) {
        if (utf8) reverseword_utf(hpw); else reverseword(hpw);
    }

    int i = hash(hpw);

    hp->blen = (unsigned char) wbl;
    hp->clen = (unsigned char) wcl;
    hp->alen = (short) al;
    hp->astr = aff;
    hp->next = NULL;      
    hp->next_homonym = NULL;

    // store the description string or its pointer
    if (desc) {
        hp->var = H_OPT;
        if (aliasm) {
            hp->var += H_OPT_ALIASM;
            store_pointer(hpw + wbl + 1, get_aliasm(op_atoi(desc)));
        } else {
	    op_strcpy(hpw + wbl + 1, desc);
            if (complexprefixes) {
                if (utf8) reverseword_utf(HENTRY_DATA(hp));
                else reverseword(HENTRY_DATA(hp));
            }
        }
	if (op_strstr(HENTRY_DATA(hp), MORPH_PHON)) hp->var += H_OPT_PHON;
    } else hp->var = 0;

       struct hentry * dp = tableptr[i];
       if (!dp) {
         tableptr[i] = hp;
         return 0;
       }
       while (dp->next != NULL) {
         if ((!dp->next_homonym) && (op_strcmp(hp->word, dp->word) == 0)) {
    	    // remove hidden onlyupcase homonym
            if (!onlyupcase) {
		if ((dp->astr) && TESTAFF(dp->astr, ONLYUPCASEFLAG, dp->alen)) {
		    op_free(dp->astr);
		    dp->astr = hp->astr;
		    dp->alen = hp->alen;
		    op_free(hp);
		    return 0;
		} else {
    		    dp->next_homonym = hp;
    		}
            } else {
        	upcasehomonym = true;
            }
         }
         dp=dp->next;
       }
       if (op_strcmp(hp->word, dp->word) == 0) {
    	    // remove hidden onlyupcase homonym
            if (!onlyupcase) {
		if ((dp->astr) && TESTAFF(dp->astr, ONLYUPCASEFLAG, dp->alen)) {
		    op_free(dp->astr);
		    dp->astr = hp->astr;
		    dp->alen = hp->alen;
		    op_free(hp);
		    return 0;
		} else {
    		    dp->next_homonym = hp;
    		}
            } else {
        	upcasehomonym = true;
            }
       }
       if (!upcasehomonym) {
    	    dp->next = hp;
       } else {
    	    // remove hidden onlyupcase homonym
    	    if (hp->astr) op_free(hp->astr);
    	    op_free(hp);
       }
    return 0;
}     

int HashMgr::add_hidden_capitalized_word(char * word, int wbl, int wcl,
    unsigned short * flags, int al, char * dp, int captype)
{
    // add inner capitalized forms to handle the following allcap forms:
    // Mixed caps: OpenOffice.org -> OPENOFFICE.ORG
    // Allcaps with suffixes: CIA's -> CIA'S    
    if (((captype == HUHCAP) || (captype == HUHINITCAP) ||
      ((captype == ALLCAP) && (flags != NULL))) &&
      !((flags != NULL) && TESTAFF(flags, forbiddenword, al))) {
          unsigned short * flags2 = (unsigned short *) op_malloc(sizeof(unsigned short) * (al+1));
	  if (!flags2) return 1;
          if (al) op_memcpy(flags2, flags, al * sizeof(unsigned short));
          flags2[al] = ONLYUPCASEFLAG;
          if (utf8) {
              char st[BUFSIZE]; /* ARRAY OK 2009-02-04 jb */
              w_char w[BUFSIZE]; /* ARRAY OK 2009-02-04 jb */
              int wlen = u8_u16(w, BUFSIZE, word);
              mkallsmall_utf(w, wlen, langnum);
              mkallcap_utf(w, 1, langnum);
              u16_u8(st, BUFSIZE, w, wlen);
              return add_word(st,wbl,wcl,flags2,al+1,dp, true);
           } else {
               mkallsmall(word, csconv);
               mkinitcap(word, csconv);
               return add_word(word,wbl,wcl,flags2,al+1,dp, true);
           }
    }
    return 0;
}

// detect captype and modify word length for UTF-8 encoding
int HashMgr::get_clen_and_captype(const char * word, int wbl, int * captype) {
    int len;
    if (utf8) {
      w_char dest_utf[BUFSIZE];
      len = u8_u16(dest_utf, BUFSIZE, word);
      *captype = get_captype_utf8(dest_utf, len, langnum);
    } else {
      len = wbl;
      *captype = get_captype((char *) word, len, csconv);
    }
    return len;
}

// remove word (personal dictionary function for standalone applications)
int HashMgr::remove(const char * word)
{
    struct hentry * dp = lookup(word);
    while (dp) {
        if (dp->alen == 0 || !TESTAFF(dp->astr, forbiddenword, dp->alen)) {
            unsigned short * flags =
                (unsigned short *) op_malloc(sizeof(short) * (dp->alen + 1));
            if (!flags) return 1;
            for (int i = 0; i < dp->alen; i++) flags[i] = dp->astr[i];
            flags[dp->alen] = forbiddenword;
            dp->astr = flags;
            dp->alen++;
            flag_qsort(flags, 0, dp->alen);
        }
        dp = dp->next_homonym;
    }
    return 0;
}

/* remove forbidden flag to add a personal word to the hash */
int HashMgr::remove_forbidden_flag(const char * word) {
    struct hentry * dp = lookup(word);
    if (!dp) return 1;
    while (dp) {
         if (dp->astr && TESTAFF(dp->astr, forbiddenword, dp->alen)) {
            if (dp->alen == 1) dp->alen = 0; // XXX forbidden words of personal dic.
            else {
                unsigned short * flags2 =
                    (unsigned short *) op_malloc(sizeof(short) * (dp->alen - 1));
                if (!flags2) return 1;
                int i, j = 0;
                for (i = 0; i < dp->alen; i++) {
                    if (dp->astr[i] != forbiddenword) flags2[j++] = dp->astr[i];
                }
                dp->alen--;
                dp->astr = flags2; // XXX allowed forbidden words
            }
         }
         dp = dp->next_homonym;
       }
   return 0;
}

// add a custom dic. word to the hash table (public)
int HashMgr::add(const char * word)
{
    unsigned short * flags = NULL;
    int al = 0;
    if (remove_forbidden_flag(word)) {
        int captype;
        int wbl = op_strlen(word);
        int wcl = get_clen_and_captype(word, wbl, &captype);
        add_word(word, wbl, wcl, flags, al, NULL, false);
        return add_hidden_capitalized_word((char *) word, wbl, wcl, flags, al, NULL, captype);
    }
    return 0;
}

int HashMgr::add_with_affix(const char * word, const char * example)
{
    // detect captype and modify word length for UTF-8 encoding
    struct hentry * dp = lookup(example);
    remove_forbidden_flag(word);
    if (dp && dp->astr) {
        int captype;
        int wbl = op_strlen(word);
        int wcl = get_clen_and_captype(word, wbl, &captype);
	if (aliasf) {
	    add_word(word, wbl, wcl, dp->astr, dp->alen, NULL, false);	
	} else {
    	    unsigned short * flags = (unsigned short *) op_malloc(dp->alen * sizeof(short));
	    if (flags) {
		op_memcpy((void *) flags, (void *) dp->astr, dp->alen * sizeof(short));
		add_word(word, wbl, wcl, flags, dp->alen, NULL, false);
	    } else return 1;
	}
    	return add_hidden_capitalized_word((char *) word, wbl, wcl, dp->astr, dp->alen, NULL, captype);
    }
    return 1;
}

// walk the hash table entry by entry - null at end
// initialize: col=-1; hp = NULL; hp = walk_hashtable(&col, hp);
struct hentry * HashMgr::walk_hashtable(int &col, struct hentry * hp) const
{  
  if (hp && hp->next != NULL) return hp->next;
  for (col++; col < tablesize; col++) {
    if (tableptr[col]) return tableptr[col];
  }
  // null at end and reset to start
  col = -1;
  return NULL;
}

// load a munched word list and build a hash table on the fly
int HashMgr::load_tables(const char * tpath, const char * key)
{
  int al;
  char * ap;
  char * dp;
  char * dp2;
  unsigned short * flags;
  char * ts;

  // open dictionary file
  FileMgr * dict = OP_NEW(FileMgr,(tpath, key));
  if (dict == NULL) return 1;

  // first read the first line of file to get hash table size */
  if (!(ts = dict->getline())) {
    HUNSPELL_WARNING(hunspell_stderr, "error: empty dic file\n");
    OP_DELETE( dict);
    return 2;
  }
  mychomp(ts);

  /* remove byte order mark */
  if (op_strncmp(ts,"\xEF\xBB\xBF",3) == 0) {
    op_memmove(ts, ts+3, op_strlen(ts+3)+1);
    // warning: dic file begins with byte order mark: possible incompatibility with old Hunspell versions
  }

  tablesize = op_atoi(ts);
  if (tablesize == 0) {
    HUNSPELL_WARNING(hunspell_stderr, "error: line 1: missing or bad word count in the dic file\n");
    OP_DELETE( dict);
    return 4;
  }
  tablesize = tablesize + 5 + USERWORD;
  if ((tablesize %2) == 0) tablesize++;

  // allocate the hash table
  tableptr = (struct hentry **) op_malloc(tablesize * sizeof(struct hentry *));
  if (! tableptr) {
    OP_DELETE( dict);
    return 3;
  }
  for (int i=0; i<tablesize; i++) tableptr[i] = NULL;

  // loop through all words on much list and add to hash
  // table and create word and affix strings

  while ((ts = dict->getline())) {
    mychomp(ts);
    // split each line into word and morphological description
    dp = ts;
    while ((dp = op_strchr(dp, ':'))) {
	if ((dp > ts + 3) && (*(dp - 3) == ' ' || *(dp - 3) == '\t')) {
	    for (dp -= 4; dp >= ts && (*dp == ' ' || *dp == '\t'); dp--);
	    if (dp < ts) { // missing word
		dp = NULL;
	    } else {
		*(dp + 1) = '\0';
		dp = dp + 2;
	    }
	    break;
	}
	dp++;
    }

    // tabulator is the old morphological field separator
    dp2 = op_strchr(ts, '\t');
    if (dp2 && (!dp || dp2 < dp)) {
	*dp2 = '\0';
	dp = dp2 + 1;
    }

    // split each line into word and affix char strings
    // "\/" signs slash in words (not affix separator)
    // "/" at beginning of the line is word character (not affix separator)
    ap = op_strchr(ts,'/');
    while (ap) {
        if (ap == ts) {
            ap++;
            continue;
        } else if (*(ap - 1) != '\\') break;
        // replace "\/" with "/"
        for (char * sp = ap - 1; *sp; *sp = *(sp + 1), sp++);
        ap = op_strchr(ap,'/');
    }

    if (ap) {
      *ap = '\0';
      if (aliasf) {
        int index = op_atoi(ap + 1);
        al = get_aliasf(index, &flags, dict);
        if (!al) {
            HUNSPELL_WARNING(hunspell_stderr, "error: line %d: bad flag vector alias\n", dict->getlinenum());
            *ap = '\0';
        }
      } else {
        al = decode_flags(&flags, ap + 1, dict);
        if (al == -1) {
            HUNSPELL_WARNING(hunspell_stderr, "Can't allocate memory.\n");
            OP_DELETE(dict);
            return 6;
        }
        flag_qsort(flags, 0, al);
      }
    } else {
      al = 0;
      ap = NULL;
      flags = NULL;
    }

    int captype;
    int wbl = op_strlen(ts);
    int wcl = get_clen_and_captype(ts, wbl, &captype);
    // add the word and its index plus its capitalized form optionally
	if (wbl > 0 )
	{
		if (add_word(ts,wbl,wcl,flags,al,dp, false) ||
			add_hidden_capitalized_word(ts, wbl, wcl, flags, al, dp, captype)) {

			OP_DELETE( dict);
			return 5;
		}
	}
  }

  OP_DELETE( dict);
  return 0;
}

// the hash function is a simple load and rotate
// algorithm borrowed

int HashMgr::hash(const char * word) const
{
    long  hv = 0;
    for (int i=0; i < 4  &&  *word != 0; i++)
        hv = (hv << 8) | (*word++);
    while (*word != 0) {
      ROTATE(hv,ROTATE_LEN);
      hv ^= (*word++);
    }
    return (unsigned long) hv % tablesize;
}

int HashMgr::decode_flags(unsigned short ** result, char * flags, FileMgr * af) {
    int len;
    if (*flags == '\0') {
        *result = NULL;
        return 0;
    }
    switch (flag_mode) {
      case FLAG_LONG: { // two-character flags (1x2yZz -> 1x 2y Zz)
        len = op_strlen(flags);
        if (len%2 == 1) HUNSPELL_WARNING(hunspell_stderr, "error: line %d: bad flagvector\n", af->getlinenum());
        len /= 2;
        *result = (unsigned short *) op_malloc(len * sizeof(short));
        if (!*result) return -1;
        for (int i = 0; i < len; i++) {
            (*result)[i] = (((unsigned short) flags[i * 2]) << 8) + (unsigned short) flags[i * 2 + 1]; 
        }
        break;
      }
      case FLAG_NUM: { // decimal numbers separated by comma (4521,23,233 -> 4521 23 233)
        int i;
        len = 1;
        char * src = flags; 
        unsigned short * dest;
        char * p;
        for (p = flags; *p; p++) {
          if (*p == ',') len++;
        }
        *result = (unsigned short *) op_malloc(len * sizeof(short));
        if (!*result) return -1;
        dest = *result;
        for (p = flags; *p; p++) {
          if (*p == ',') {
            i = op_atoi(src);
            if (i >= DEFAULTFLAGS) HUNSPELL_WARNING(hunspell_stderr, "error: line %d: flag id %d is too large (max: %d)\n",
              af->getlinenum(), i, DEFAULTFLAGS - 1);
            *dest = (unsigned short) i;
            if (*dest == 0) HUNSPELL_WARNING(hunspell_stderr, "error: line %d: 0 is wrong flag id\n", af->getlinenum());
            src = p + 1;
            dest++;
          }
        }
        i = op_atoi(src);
        if (i >= DEFAULTFLAGS) HUNSPELL_WARNING(hunspell_stderr, "error: line %d: flag id %d is too large (max: %d)\n",
          af->getlinenum(), i, DEFAULTFLAGS - 1);
        *dest = (unsigned short) i;
        if (*dest == 0) HUNSPELL_WARNING(hunspell_stderr, "error: line %d: 0 is wrong flag id\n", af->getlinenum());
        break;
      }    
      case FLAG_UNI: { // UTF-8 characters
        w_char w[BUFSIZE/2];
        len = u8_u16(w, BUFSIZE/2, flags);
        *result = (unsigned short *) op_malloc(len * sizeof(short));
        if (!*result) return -1;
        op_memcpy(*result, w, len * sizeof(short));
        break;
      }
      default: { // Ispell's one-character flags (erfg -> e r f g)
        unsigned short * dest;
        len = op_strlen(flags);
        *result = (unsigned short *) op_malloc(len * sizeof(short));
        if (!*result) return -1;
        dest = *result;
        for (unsigned char * p = (unsigned char *) flags; *p; p++) {
          *dest = (unsigned short) *p;
          dest++;
        }
      }
    }
    return len;
}

unsigned short HashMgr::decode_flag(const char * f) {
    unsigned short s = 0;
    int i;
    switch (flag_mode) {
      case FLAG_LONG:
        s = ((unsigned short) f[0] << 8) + (unsigned short) f[1];
        break;
      case FLAG_NUM:
        i = op_atoi(f);
        if (i >= DEFAULTFLAGS) HUNSPELL_WARNING(hunspell_stderr, "error: flag id %d is too large (max: %d)\n", i, DEFAULTFLAGS - 1);
        s = (unsigned short) i;
        break;
      case FLAG_UNI:
        u8_u16((w_char *) &s, 1, f);
        break;
      default:
        s = (unsigned short) *((unsigned char *)f);
    }
    if (s == 0) HUNSPELL_WARNING(hunspell_stderr, "error: 0 is wrong flag id\n");
    return s;
}

char * HashMgr::encode_flag(unsigned short f) {
    unsigned char ch[10]; // ARRAY OK 2009-02-04 jb 
    if (f==0) return mystrdup("(NULL)");
    if (flag_mode == FLAG_LONG) {
        ch[0] = (unsigned char) (f >> 8);
        ch[1] = (unsigned char) (f - ((f >> 8) << 8));
        ch[2] = '\0';
    } else if (flag_mode == FLAG_NUM) {
        op_sprintf((char *) ch, "%d", f);
    } else if (flag_mode == FLAG_UNI) {
        u16_u8((char *) &ch, 10, (w_char *) &f, 1);
    } else {
        ch[0] = (unsigned char) (f);
        ch[1] = '\0';
    }
    return mystrdup((char *) ch);
}

// read in aff file and set flag mode
int  HashMgr::load_config(const char * affpath, const char * key)
{
  char * line; // io buffers
  int firstline = 1;
 
  // open the affix file
  FileMgr * afflst = OP_NEW(FileMgr,(affpath, key));
  if (!afflst) {
    HUNSPELL_WARNING(hunspell_stderr, "Error - could not open affix description file %s\n",affpath);
    return 1;
  }

    // read in each line ignoring any that do not
    // start with a known line type indicator

    while ((line = afflst->getline())) {
        mychomp(line);

       /* remove byte order mark */
       if (firstline) {
         firstline = 0;
         if (op_strncmp(line,"\xEF\xBB\xBF",3) == 0) op_memmove(line, line+3, op_strlen(line+3)+1);
       }

        /* parse in the try string */
        if ((op_strncmp(line,"FLAG",4) == 0) && op_isspace(line[4])) {
            if (flag_mode != FLAG_CHAR) {
                HUNSPELL_WARNING(hunspell_stderr, "error: line %d: multiple definitions of the FLAG affix file parameter\n", afflst->getlinenum());
            }
            if (op_strstr(line, "long")) flag_mode = FLAG_LONG;
            if (op_strstr(line, "num")) flag_mode = FLAG_NUM;
            if (op_strstr(line, "UTF-8")) flag_mode = FLAG_UNI;
            if (flag_mode == FLAG_CHAR) {
                HUNSPELL_WARNING(hunspell_stderr, "error: line %d: FLAG needs `num', `long' or `UTF-8' parameter\n", afflst->getlinenum());
            }
        }
        if (op_strncmp(line,"FORBIDDENWORD",13) == 0) {
          char * st = NULL;
          if (parse_string(line, &st, afflst->getlinenum())) {
             OP_DELETE( afflst);
             return 1;
          }
          forbiddenword = decode_flag(st);
          op_free(st);
        }
        if (op_strncmp(line, "SET", 3) == 0) {
    	  if (parse_string(line, &enc, afflst->getlinenum())) {
             OP_DELETE( afflst);
             return 1;
          }    	    
    	  if (op_strcmp(enc, "UTF-8") == 0) {
    	    utf8 = 1;
#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
#ifdef HUNSPELL_USE_UTF_TBL
    	    initialize_utf_tbl();
#endif
#endif
#endif
    	  } else csconv = get_current_cs(enc);
    	}
        if (op_strncmp(line, "LANG", 4) == 0) {
    	  if (parse_string(line, &lang, afflst->getlinenum())) {
             OP_DELETE( afflst);
             return 1;
          }    	    
    	  langnum = get_lang_num(lang);
    	}

       /* parse in the ignored characters (for example, Arabic optional diacritics characters */
       if (op_strncmp(line,"IGNORE",6) == 0) {
          if (parse_array(line, &ignorechars, &ignorechars_utf16,
                 &ignorechars_utf16_len, utf8, afflst->getlinenum())) {
             OP_DELETE( afflst);
             return 1;
          }
       }

       if ((op_strncmp(line,"AF",2) == 0) && op_isspace(line[2])) {
          if (parse_aliasf(line, afflst)) {
             OP_DELETE( afflst);
             return 1;
          }
       }

       if ((op_strncmp(line,"AM",2) == 0) && op_isspace(line[2])) {
          if (parse_aliasm(line, afflst)) {
             OP_DELETE( afflst);
             return 1;
          }
       }

       if (op_strncmp(line,"COMPLEXPREFIXES",15) == 0) complexprefixes = 1;
       if (((op_strncmp(line,"SFX",3) == 0) || (op_strncmp(line,"PFX",3) == 0)) && op_isspace(line[3])) break;
    }
    if (csconv == NULL) csconv = get_current_cs(SPELL_ENCODING);
    OP_DELETE( afflst);
    return 0;
}

/* parse in the ALIAS table */
int  HashMgr::parse_aliasf(char * line, FileMgr * af)
{
   if (numaliasf != 0) {
      HUNSPELL_WARNING(hunspell_stderr, "error: line %d: multiple table definitions\n", af->getlinenum());
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numaliasf = op_atoi(piece);
                       if (numaliasf < 1) {
                          numaliasf = 0;
                          aliasf = NULL;
                          aliasflen = NULL;
                          HUNSPELL_WARNING(hunspell_stderr, "error: line %d: bad entry number\n", af->getlinenum());
                          return 1;
                       }
                       aliasf = (unsigned short **) op_malloc(numaliasf * sizeof(unsigned short *));
                       aliasflen = (unsigned short *) op_malloc(numaliasf * sizeof(short));
                       if (!aliasf || !aliasflen) {
                          numaliasf = 0;
                          if (aliasf) op_free(aliasf);
                          if (aliasflen) op_free(aliasflen);
                          aliasf = NULL;
                          aliasflen = NULL;
                          return 1;
                       }
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      numaliasf = 0;
      op_free(aliasf);
      op_free(aliasflen);
      aliasf = NULL;
      aliasflen = NULL;
      HUNSPELL_WARNING(hunspell_stderr, "error: line %d: missing data\n", af->getlinenum());
      return 1;
   } 
 
   /* now parse the numaliasf lines to read in the remainder of the table */
   char * nl;
   for (int j=0; j < numaliasf; j++) {
        if (!(nl = af->getline())) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        aliasf[j] = NULL;
        aliasflen[j] = 0;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (op_strncmp(piece,"AF",2) != 0) {
                                 numaliasf = 0;
                                 op_free(aliasf);
                                 op_free(aliasflen);
                                 aliasf = NULL;
                                 aliasflen = NULL;
                                 HUNSPELL_WARNING(hunspell_stderr, "error: line %d: table is corrupt\n", af->getlinenum());
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            aliasflen[j] = (unsigned short) decode_flags(&(aliasf[j]), piece, af);
                            flag_qsort(aliasf[j], 0, aliasflen[j]);
                            break; 
                          }
                  default: break;
               }
               i++;
           }
           piece = mystrsep(&tp, 0);
        }
        if (!aliasf[j]) {
             op_free(aliasf);
             op_free(aliasflen);
             aliasf = NULL;
             aliasflen = NULL;
             numaliasf = 0;
             HUNSPELL_WARNING(hunspell_stderr, "error: line %d: table is corrupt\n", af->getlinenum());
             return 1;
        }
   }
   return 0;
}

int HashMgr::is_aliasf() {
    return (aliasf != NULL);
}

int HashMgr::get_aliasf(int index, unsigned short ** fvec, FileMgr * af) {
    if ((index > 0) && (index <= numaliasf)) {
        *fvec = aliasf[index - 1];
        return aliasflen[index - 1];
    }
    HUNSPELL_WARNING(hunspell_stderr, "error: line %d: bad flag alias index: %d\n", af->getlinenum(), index);
    *fvec = NULL;
    return 0;
}

/* parse morph alias definitions */
int  HashMgr::parse_aliasm(char * line, FileMgr * af)
{
   if (numaliasm != 0) {
      HUNSPELL_WARNING(hunspell_stderr, "error: line %d: multiple table definitions\n", af->getlinenum());
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numaliasm = op_atoi(piece);
                       if (numaliasm < 1) {
                          HUNSPELL_WARNING(hunspell_stderr, "error: line %d: bad entry number\n", af->getlinenum());
                          return 1;
                       }
                       aliasm = (char **) op_malloc(numaliasm * sizeof(char *));
                       if (!aliasm) {
                          numaliasm = 0;
                          return 1;
                       }
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      numaliasm = 0;
      op_free(aliasm);
      aliasm = NULL;
      HUNSPELL_WARNING(hunspell_stderr, "error: line %d: missing data\n", af->getlinenum());
      return 1;
   } 

   /* now parse the numaliasm lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numaliasm; j++) {
        if (!(nl = af->getline())) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        aliasm[j] = NULL;
        piece = mystrsep(&tp, ' ');
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (op_strncmp(piece,"AM",2) != 0) {
                                 HUNSPELL_WARNING(hunspell_stderr, "error: line %d: table is corrupt\n", af->getlinenum());
                                 numaliasm = 0;
                                 op_free(aliasm);
                                 aliasm = NULL;
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            // add the remaining of the line
                            if (*tp) {
                                *(tp - 1) = ' ';
                                tp = tp + op_strlen(tp);
                            }
                            if (complexprefixes) {
                                if (utf8) reverseword_utf(piece);
                                    else reverseword(piece);
                            }
                            aliasm[j] = mystrdup(piece);
                            if (!aliasm[j]) {
                                 numaliasm = 0;
                                 op_free(aliasm);
                                 aliasm = NULL;
                                 return 1;
                            }
                            break; }
                  default: break;
               }
               i++;
           }
           piece = mystrsep(&tp, ' ');
        }
        if (!aliasm[j]) {
             numaliasm = 0;
             op_free(aliasm);
             aliasm = NULL;
             HUNSPELL_WARNING(hunspell_stderr, "error: line %d: table is corrupt\n", af->getlinenum());
             return 1;
        }
   }
   return 0;
}

int HashMgr::is_aliasm() {
    return (aliasm != NULL);
}

char * HashMgr::get_aliasm(int index) {
    if ((index > 0) && (index <= numaliasm)) return aliasm[index - 1];
    HUNSPELL_WARNING(hunspell_stderr, "error: bad morph. alias index: %d\n", index);
    return NULL;
}
#endif //SPC_USE_HUNSPELL_ENGINE
