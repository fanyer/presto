/*	Erronious GPL licence text and copyright removed.
 *	See lth's comment to https://cvs.intern.opera.no/viewcvs/viewcvs.cgi/Opera/lib/Mail/liboe.cpp?hideattic=0&r1=2.6&r2=2.7
 *	"Removed an incorrect copyright notice at the instruction of VPE."
 */

#include "core/pch.h"

#ifdef M2_SUPPORT
# if !defined(_UNIX_DESKTOP_) && !defined(_MACINTOSH_)

# include "adjunct/desktop_util/opfile/desktop_opfile.h"
# include "modules/util/opfile/opfile.h"

  /* The use of fpos_t here where one assumes it to be some int type
   * is not portable. A suggestion for increased portability would be
   * off_t or long int and using ftell/ftello and fseek rfz 20040702
   */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>

# define OE_CANNOTREAD 1
# define OE_NOTOEBOX   2
# define OE_POSITION   3
# define OE_NOBODY     4
# define OE_PANIC      5

/* #define DEBUG -- uncomment to get some DEBUG output to stdout */


/* TABLE STRUCTURES 
   -- tables store pointers to message headers and other tables also
   containing pointers to message headers and other tables -- */

struct oe_table_header { /* At the beginning of each table */
  int self,   /* Pointer to self (filepos) */
    unknown1, /* Unknown */
    list,     /* Pointer to list */
    next,     /* Pointer to next */
    unknown3, /* Unknown */
    unknown4; /* Unknown */
};
typedef struct oe_table_header oe_table_header;

struct oe_table_node { /* Actual table entries */
  int message, /* Pointer to message | 0 */
    list,   /* Pointer to another table | 0 */
    unknown; /* Unknown */
};
typedef struct oe_table_node oe_table_node;

struct oe_list { /* Internal use only */
  fpos_t pos;
  struct oe_list *next;
};
typedef struct oe_list oe_list;



/* MESSAGE STRUCTURES
   -- OE uses 16-byte segment headers inside the actual messages. These 
   are meaningful, as described below, but were not very easy to hack
   correctly -- note that a message may be composed of segments located
   anywhere in the mailbox file, some times far from each other. */

struct oe_msg_segmentheader {
  int self,  /* Pointer to self (filepos) */
    increase, /* Increase to next segment header (not in msg, in file!) */
    include, /* Number of bytes to include from this segment */
    next,  /* Pointer to next message segment (in msg) (filepos) */
    usenet; /* Only used with usenet posts */
};
typedef struct oe_msg_segmentheader oe_msg_segmentheader;




/* INTERAL STRUCTURES */
struct oe_internaldata{
  void (*oput)(char*);
  FILE *oe;
  oe_list *used;
  int success, justheaders, failure;
  int errcode;
  int msgcount;
  struct stat *stat;
};
typedef struct oe_internaldata oe_data;



/* LIST OF USED TABLES */

void oe_posused(oe_data *data, fpos_t pos) {
  oe_list *n = OP_NEW(oe_list, ());
  n->pos = pos;
  n->next = data->used;
  data->used = n;
}

int oe_isposused(oe_data *data, fpos_t pos) {
  oe_list *n = data->used;
  while (n!=NULL) {
    if (pos==n->pos) return 1;
    n = n->next;
  }
  return 0;
}

void oe_freeposused(oe_data *data) {
  oe_list *n;
  while (data->used!=NULL) {n=data->used->next; OP_DELETE(data->used); data->used=n;}
}


/* ACTUAL MESSAGE PARSER */

int oe_readmessage(oe_data *data, 
		   fpos_t pos, 
		   int newsarticle) {
  int segheadsize = sizeof(oe_msg_segmentheader)-4; /*+(newsarticle<<2);*/
  oe_msg_segmentheader sgm;
  char buff[65];
  int endofsegment, headerwritten = 0;
//  fsetpos(data->oe,&pos);
  while (1) {
    fsetpos(data->oe,&pos);
    fread(&sgm,4,4,data->oe);
	if (pos!=sgm.self) { // No body found
#ifdef DEBUG
      printf("- Fail reported at %.8x (%.8x)\n",pos,sgm.self); 
#endif
      data->failure++; 
      return OE_NOBODY;
    } 
    pos+=segheadsize;
    endofsegment = ((int)pos)+sgm.include;

    if (!headerwritten) {
#ifdef DEBUG
      printf("%.8x : \n",pos-segheadsize);
#endif
      data->oput("From importer@localhost Fri Oct 25 12:00:00 2002\n");
      headerwritten = 1;
    }

    while (pos<endofsegment)
	{
		int rLen = endofsegment - (int)pos;
		if (rLen>64) rLen=64;
		rLen = fread(&buff,1,rLen,data->oe);
		if (!rLen)
			break;
		buff[rLen] = 0;
		data->oput(buff);
		pos += rLen;
    }
    pos = sgm.next;
    if (pos==0) 
		break;

    fsetpos(data->oe, &pos);
  }
  data->oput("\n");

  data->success++;
  return 0;
}


/* PARSES MESSAGE HEADERS */

int oe_readmessageheader(oe_data *data, fpos_t pos) {
  int segheadsize = sizeof(oe_msg_segmentheader)-4;
  oe_msg_segmentheader *sgm;
  int self=1, msgpos = 0, newsarticle = 0;

  if (oe_isposused(data,pos)) 
	  return 0; 
  else 
	  oe_posused(data,pos);

  fsetpos(data->oe,&pos);
  sgm = OP_NEW(oe_msg_segmentheader, ());
  fread(sgm,segheadsize,1,data->oe);
  
  if (pos!=sgm->self) 
  { 
	  OP_DELETE(sgm);
	  return OE_POSITION; /* ERROR */ 
  }

  OP_DELETE(sgm);


  fread(&self,4,1,data->oe); 
  self=1;
  
  while ((self & 0x7F)>0) 
  {
	  fread(&self,4,1,data->oe);
	  if ((self & 0xFF) == 0x84) /* 0x80 = Set, 04 = Index */
		  if (msgpos==0)
			  msgpos = self >> 8; 
		  if ((self & 0xFF) == 0x83) /* 0x80 = Set, 03 = News  */
			  newsarticle = 1;
  }
  if (msgpos)
  {
	  data->msgcount++;
	  oe_readmessage(data,msgpos,newsarticle);
  }
  else  
  { 
	  fread(&self,4,1,data->oe);
	  fread(&msgpos,4,1,data->oe);
	  if (oe_readmessage(data,msgpos,newsarticle)) 
	  { 
		  if (newsarticle) 
		  {
			  data->justheaders++; 
			  data->failure--; 
		  }
	  }
  }
  return 0;
}


/* PARSES MAILBOX TABLES */

int oe_readtable(oe_data *data, fpos_t pos) {
  oe_table_header thead;
  oe_table_node tnode;
  int quit = 0;

  if (oe_isposused(data,pos)) return 0;

  fsetpos(data->oe,&pos);

  fread(&thead,sizeof(oe_table_header),1,data->oe);
  if (thead.self != pos) return OE_POSITION;
  oe_posused(data,pos);
  pos+=sizeof(oe_table_header);

  oe_readtable(data,thead.next);
  oe_readtable(data,thead.list);
  fsetpos(data->oe,&pos); 

  while (!quit) {
    //int tmp = 
	  fread(&tnode,sizeof(oe_table_node),1,data->oe);
    int sizTableNode = sizeof(oe_table_node);
	pos+=sizTableNode;
    if ( (tnode.message > data->stat->st_size) && 
	 (tnode.list > data->stat->st_size) ) 
      return 0xF0; /* PANIC */
    if ( (tnode.message == tnode.list) &&  (tnode.message == 0) ) /* Neither message nor list==quit */ 
	{
		 quit = 1; 
	}
	else 
	{
	   oe_readmessageheader(data,tnode.message);
	   oe_readtable(data,tnode.list);
	}
    fsetpos(data->oe,&pos);
  }

  return 0;
}

void oe_readdamaged(oe_data *data) { 
  /* If nothing else works (needed this to get some mailboxes 
     that even OE couldn't read to work. Should generally not 
     be needed, but is nice to have in here */
  fpos_t pos = 0x7C;
  int i,check, lastID;
#ifdef DEBUG
  printf("  Trying to construct internal mailbox structure\n");
#endif
  fsetpos(data->oe,&pos);
  fread(&pos,sizeof(int),1,data->oe); 
  if (pos==0) return; /* No, sorry, didn't work */
  fsetpos(data->oe,&pos);
  fread(&i,sizeof(int),1,data->oe);
  if (i!=pos) return; /* Sorry */
  fread(&pos,sizeof(int),1,data->oe);
  i+=((int)(pos))+8;
  pos = i+4;
  fsetpos(data->oe,&pos);
#ifdef DEBUG
  printf("  Searching for %.8x\n",i);
#endif
  lastID=0;
  while (pos<data->stat->st_size) {
    /* Read through file, notice markers, look for message (gen. 2BD4)*/
    fread(&check,sizeof(int),1,data->oe); 
    if (check==pos) 
		lastID=((int)(pos));
    pos+=4;
    if ((check==i) && (lastID)) {
#ifdef DEBUG
      printf("Trying possible table at %.8x\n",lastID);
#endif
      oe_readtable(data,lastID);
      fsetpos(data->oe,&pos);
    }
  }
}

void oe_readbox_oe4(oe_data *data) {
  fpos_t pos = 0x54, endpos=0, i;
  oe_msg_segmentheader header;
  char *cb = OP_NEWA(char, 4), *sfull = OP_NEWA(char, 65536), *s = sfull;
  fsetpos(data->oe,&pos); 
  while (pos<data->stat->st_size) {
    fsetpos(data->oe,&pos);
    fread(&header,16,1,data->oe);
    data->oput("From importer@localhost Sat Jun 17 01:08:25 2000\n");
    endpos = pos + header.include;
    if (endpos>data->stat->st_size) endpos=data->stat->st_size;
    pos+=4;
    while (pos<endpos) {
      fread(cb,1,4,data->oe);
      for (i=0;i<4;i++,pos++) 
	if (*(cb+i)!=0x0d) {
	  *s++ = *(cb+i);
	  if (*(cb+i) == 0x0a) {
	    *s = '\0';
	    data->oput(sfull);
	    s = sfull;
	  }
	}
    }
    data->success++;
    if (s!=sfull) { *s='\0'; data->oput(sfull); s=sfull; }
    data->oput("\n");
    pos=endpos;
  }
  OP_DELETEA(sfull);
  OP_DELETEA(cb);
}

/* CALL THIS ONE */

oe_data* oe_readbox(const uni_char* filename,void (*oput)(char*)) {
  UINT32 signature[4];
  fpos_t i;
  oe_data *data = OP_NEW(oe_data, ());
  data->success=data->failure=data->justheaders=data->errcode=data->msgcount=0;
  data->used = NULL;
  data->oput = oput;
  /* FIXME: this was replaced by windows specific code temporarily, to be able to remove the old uni_fopen. It should be rewritten using OpFile. */
  data->oe = _wfopen(filename, UNI_L("rb"));
  if (data->oe==NULL) {
    data->errcode = OE_CANNOTREAD;
    return data;
  }

  /* SECURITY (Yes, we need this, just in case) */
  data->stat = OP_NEW(struct stat, ());
  OpFile box_file;
  box_file.Construct(filename);
  DesktopOpFileUtils::Stat(&box_file, data->stat);
  
  /* SIGNATURE */
  fread(&signature,16,1,data->oe); 
  if ((signature[0]!=0xFE12ADCF) || /* OE 5 & OE 5 BETA SIGNATURE */
      (signature[1]!=0x6F74FDC5) ||
      (signature[2]!=0x11D1E366) ||
      (signature[3]!=0xC0004E9A)) {
    if ((signature[0]==0x36464D4A) &&
	(signature[1]==0x00010003)) /* OE4 SIGNATURE */ {
      oe_readbox_oe4(data);
      fclose(data->oe);
      OP_DELETE(data->stat);
      return data;
    }
    fclose(data->oe);
    OP_DELETE(data->stat);
    data->errcode = OE_NOTOEBOX;
    return data;
  }

  /* ACTUAL WORK */
  i = 0x30;
  if( fsetpos(data->oe, &i) != 0)
  {
	  data->errcode=errno;
	  data->errcode=OE_PANIC; //not able to set file position!!!
  }
  else
  {
	  fread(&i,4,1,data->oe);
	  if (!i) i=0x1e254;
	  i = oe_readtable(data,i); /* Reads the box */
	  if (i & 0xF0) {
		oe_readdamaged(data);
		data->errcode=OE_PANIC;
	  }
	  oe_freeposused(data);
  }
  /* CLOSE DOWN */
  fclose(data->oe);
  OP_DELETE(data->stat);
  return data;
}

#endif // !_UNIX_DESKTOP_ && !_MACINTOSH_
#endif //M2_SUPPORT
