/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OLDDEBUG_ENABLED
#include "modules/olddebug/olddebug_module.h"
#include "modules/olddebug/timing.h"
#include "modules/util/opfile/opfile.h"

#include "modules/olddebug/tstdump.h"

#include "modules/util/simset.h"
#include "modules/util/str.h"

#ifdef EPOC
# define DEBUG_OUTPUTDIRECTORY   "C:\\System\\Data\\"
#else
# ifndef DEBUG_OUTPUTDIRECTORY
#  ifdef MSWIN
#   define DEBUG_OUTPUTDIRECTORY "C:\\klient\\"
#  else
#   define DEBUG_OUTPUTDIRECTORY "/tmp/"
#  endif
# endif
#endif

struct filelist : public Link
{
	OpString name;
	OpFile file;

	~filelist(){}
};

filelist *FindDebugFile(const OpStringC8 &name)
{
	filelist *item;

	item = (filelist *) g_odbg_debug_files.First();
	while(item)
	{
		if(item->name.Compare(name) == 0)
		{
			item->Out();
			item->Into(&g_odbg_debug_files);
			break;
		}
		item = (filelist *) item->Suc();
	}

	if(!item)
	{
		OpStackAutoPtr<filelist> item1(new filelist);

		if(item1.get())
		{
			RETURN_VALUE_IF_ERROR(item1->name.Set(name), NULL);
			OpString fullname;

			RETURN_VALUE_IF_ERROR(fullname.SetConcat(UNI_L(DEBUG_OUTPUTDIRECTORY),item1->name), NULL);
			RETURN_VALUE_IF_ERROR(item1->file.Construct(fullname), NULL);
			RETURN_VALUE_IF_ERROR(item1->file.Open(OPFILE_APPEND | OPFILE_TEXT), NULL);

			item = item1.release();
			item->Into(&g_odbg_debug_files);
		}
	}

	return item;
}

void CloseDebugFile(const OpStringC8 &name)
{
	filelist *item;

	item = FindDebugFile(name);

	if(item && item->file.IsOpen())
	{
		item->file.Close();
	}
}


void TruncateDebugFiles()
{
	filelist *item;

	item = (filelist *) g_odbg_debug_files.First();

	while(item)
	{
		filelist *item2 = NULL;
		if(item->file.IsOpen())
		{
			item->file.Close();
			if(OpStatus::IsError(item->file.Open(OPFILE_WRITE | OPFILE_TEXT)))
				item2 = item;
		}
		else
		{
			item->file.Delete();
			item2 = item;
		}

		item = (filelist *) item->Suc();
		if(item2)
		{
			item2->Out();
			delete item2;
			item2 = NULL;
		}
	}
}

void TruncateDebugFile(const OpStringC8 &name)
{
	filelist *item;

	item = FindDebugFile(name);
	if(item)
	{
		if(item->file.IsOpen())
		{
			item->file.Close();
			if(OpStatus::IsSuccess(item->file.Open(OPFILE_WRITE | OPFILE_TEXT)))
				item = NULL; // Don't delete
		}
		else
		{
			item->file.Delete();
		}

		if(item)
		{
			item->Out();
			delete item;
		}
	}
}

void CloseDebugFiles()
{
	filelist *item, *temp;

	item = (filelist *) g_odbg_debug_files.First();
	while(item)
	{
		// 		if(item->file) // closed from the destructor.
		// 			fclose(item->file);
		temp = item;
		item = (filelist *) item->Suc();
		temp->Out();
		delete temp;
	}
}

void DumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name)
{
	filelist *debugfile;
	unsigned long pos1;
	unsigned long pos2;

	debugfile = FindDebugFile(name);
	if(debugfile != NULL)
	{
		if(source != NULL)
		{
			debugfile->file.Print("\n %s : %d bytes \n",text.CStr(),(int) len);

			for(pos1 = 0; pos1 <len;pos1+=16)
			{
				debugfile->file.Print("%04.4lx :",pos1);
				for(pos2 = pos1; pos2<pos1+16; pos2++)
				{
					if(pos2 >= len)
						debugfile->file.Print("   ");
					else
						debugfile->file.Print(" %02.2x", source[pos2]);
				}
				debugfile->file.Print("   ");
				for(pos2 = pos1; pos2<pos1+16; pos2++)
				{
					if(pos2 >= len)
						debugfile->file.Print(" ");
					else
						debugfile->file.Print("%c", (op_isprint(source[pos2]) && source[pos2] != '\t'? source[pos2] : '.'));
				}
				debugfile->file.Print("\n");
			}
		}
		else
		{
			debugfile->file.Print("\n %s\n",text.CStr());
		}
		debugfile->file.Flush();
	}
}


void DumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name, int indent_level)
{
	filelist *debugfile;
	unsigned long pos1;
	unsigned long pos2;

	debugfile = FindDebugFile(name);
	if(debugfile != NULL)
	{
		if(source != NULL)
		{
			debugfile->file.Print("\n%*s%s : %d bytes \n",(indent_level ? indent_level*4 : 1), " ", text.CStr(),(int) len);

			for(pos1 = 0; pos1 <len;pos1+=16){
				debugfile->file.Print("%*s%04.4lx :",indent_level*4, " ",pos1);
				for(pos2 = pos1; pos2<pos1+16; pos2++){
					if(pos2 >= len)
						debugfile->file.Print("   ");
					else
						debugfile->file.Print(" %02.2x", source[pos2]);
				}
				debugfile->file.Print("   ");
				for(pos2 = pos1; pos2<pos1+16; pos2++){
					if(pos2 >= len)
						debugfile->file.Print(" ");
					else
						debugfile->file.Print("%c", (op_isprint(source[pos2]) && source[pos2] != '\t'? source[pos2] : '.'));
				}
				debugfile->file.Print("\n");
			}
		}else{
			debugfile->file.Print("\n%*s%s\n",(indent_level ? indent_level*4 : 1), " ",text.CStr());
		}
		debugfile->file.Flush();
	}
}

void DumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &text, const OpStringC8 &text1, const OpStringC8 &name)
{
	filelist *debugfile;
	unsigned char pos1;
	unsigned char pos2;

	debugfile = FindDebugFile(name);
	if(debugfile != NULL)
	{
		if(source != NULL)
		{
			debugfile->file.Print("\n %s %s: %d bytes \n",text.CStr(),text1.CStr(),(int) len);

			for(pos1 = 0; pos1 <len;pos1+=16)
			{
				debugfile->file.Print("%04.4lx :",pos1);
				for(pos2 = pos1; pos2<pos1+16; pos2++)
				{
					if(pos2 >= len)
						debugfile->file.Print("   ");
					else
						debugfile->file.Print(" %02.2x", source[pos2]);
				}
				debugfile->file.Print("   ");
				for(pos2 = pos1; pos2<pos1+16; pos2++)
				{
					if(pos2 >= len)
						debugfile->file.Print(" ");
					else
						debugfile->file.Print("%c", (op_isprint(source[pos2]) && source[pos2] != '\t' ? source[pos2] : '.'));
				}
				debugfile->file.Print("\n");
			}
		}
		else
		{
			debugfile->file.Print("\n %s\n",text.CStr());
		}
		debugfile->file.Flush();
	}
}

void BinaryDumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &name)
{
	filelist *debugfile;

	debugfile = FindDebugFile(name);
	if(debugfile != NULL && source != NULL && len != 0)
	{
		debugfile->file.Write(source, len);
		debugfile->file.Flush();
	}
}


void PrintfTofile(const OpStringC8 &name,const char *fmt, ...)
{
	filelist *debugfile;
	va_list marker;

	va_start(marker,fmt);
	debugfile = FindDebugFile(name);
	if(debugfile != NULL)
	{
		op_vsnprintf(g_odbg_printbuffer, sizeof(g_odbg_printbuffer)-1, fmt, marker);
		g_odbg_printbuffer[sizeof(g_odbg_printbuffer)-1] = '\0';
		debugfile->file.Write(g_odbg_printbuffer, op_strlen(g_odbg_printbuffer));
		debugfile->file.Flush();
	}
	va_end(marker);
}

void PrintvfTofile(const OpStringC8 &name,const OpStringC8 &fmt, va_list args)
{
	filelist *debugfile;

	debugfile = FindDebugFile(name);
	if(debugfile != NULL)
	{
		op_vsnprintf(g_odbg_printbuffer, sizeof(g_odbg_printbuffer)-1, fmt.CStr(), args);
		g_odbg_printbuffer[sizeof(g_odbg_printbuffer)-1] = '\0';
		debugfile->file.Write(g_odbg_printbuffer, op_strlen(g_odbg_printbuffer));
		debugfile->file.Flush();
	}
}

#endif // OLDDEBUG_ENABLED
