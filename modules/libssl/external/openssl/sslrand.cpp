/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"

#include "modules/libssl/sslrand.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/util/str.h"

#include "modules/util/cleanse.h"
#include "modules/pi/OpSystemInfo.h"

uni_char *BuildFileName(const uni_char *directory, const uni_char *filename);
OP_STATUS SSL_RND_Save();

#ifdef _SSL_SEED_FROMMESSAGE_LOOP_

/*
Requirement for SSL seeder: It must add data about the timestamp or the time between keysdown and/ mouse move events
The delta must be at least millisecond accuracy, but microsecond may be better.

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	_SSLSeedFromMessageLoop
//	___________________________________________________________________________
//
//ifdef _SSL_SEED_FROMMESSAGE_LOOP_
void _SSLSeedFromMessageLoop( const MSG *msg)
{
	extern void SSL_Process_Feeder();

	UINT message = msg->message;
	if(g_SSL_RND_feeder_len && ( message == WM_MOUSEMOVE || message == WM_KEYDOWN) )
	{
		if(g_SSL_RND_feeder_pos +2 > g_SSL_RND_feeder_len)
			SSL_Process_Feeder();

		g_SSL_RND_feeder_data[g_SSL_RND_feeder_pos++] = op_GetTickCount();
		if( message == WM_MOUSEMOVE)
		{
			g_SSL_RND_feeder_data[g_SSL_RND_feeder_pos++] = msg->lParam;
		}
	}
}
//#endif

*/

void SSL_Process_Feeder();
#else
// !!!! DO NOT DEACTIVATE THIS CODE!!!! FIX THE PLATFORM CODE INSTEAD !!!
#error "You MUST either activate _SSL_SEED_FROMMESSAGE_LOOP_ or implement your own random entropy feeder, similar to _SSLSeedFromMessageLoop (above)."
// !!!! DO NOT DEACTIVATE THIS CODE!!!! FIX THE PLATFORM CODE INSTEAD !!!
#endif

OP_STATUS SSL_TidyUp_Random()
{
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	SSL_Process_Feeder();
#endif
	OP_STATUS op_err = SSL_RND_Save();
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	OP_DELETEA(g_SSL_RND_feeder_data);
	g_SSL_RND_feeder_data = NULL;
	g_SSL_RND_feeder_pos = 0;
	g_SSL_RND_feeder_len = 0;
#endif

	return op_err;
}

#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
void SSL_Process_Feeder()
{
#ifdef _DEBUG
	// !!!! DO NOT DEACTIVATE THIS CODE!!!! FIX THE PLATFORM CODE INSTEAD !!!
	// This code checks to see if the entropy feeder code is functional, if not it starts throwing OP_ASSERTS.
	// The entropy feeder is neede to improve randomness in the encryption code
	// !!!! DO NOT DEACTIVATE THIS CODE!!!! FIX THE PLATFORM CODE INSTEAD !!!
	if(
#ifdef SELFTEST
		!g_selftest.running &&
#endif // SELFTEST
		!g_been_fed)
	{
		if(!g_SSL_RND_feeder_len || g_SSL_RND_feeder_pos == 0)
		{
			g_idle_feed_count ++;
			if(g_idle_feed_count > 32)
			{
				// !!!! DO NOT DEACTIVATE THIS CODE!!!! FIX THE PLATFORM CODE INSTEAD !!!
				OP_ASSERT(0); // WARNING!!! It looks like  the feed array is not being updated. Make sure that the _SSLSeedFromMessageLoop (or similar) is in working order
				// !!!! DO NOT DEACTIVATE THIS CODE!!!! FIX THE PLATFORM CODE INSTEAD !!!
				g_been_fed = TRUE; // allright, you've been warned, no more warnings this run. BUT ACTIVATE THAT SEEDER!!!
			}
		}
		else if(g_SSL_RND_feeder_len && g_SSL_RND_feeder_pos > 0 )
		{
			if(++g_feed_count > 2)
				g_been_fed = TRUE;
		}
	}
#endif

	if(g_SSL_RND_feeder_len && g_SSL_RND_feeder_pos && g_SSL_RND_feeder_data)
	{
		SSL_SEED_RND((byte *) g_SSL_RND_feeder_data, g_SSL_RND_feeder_pos*sizeof(DWORD));
		g_SSL_RND_feeder_pos = 0;
	}
}
#endif

void SSL_Rand_seed_file(OpFileFolder folder, const OpStringC &filename)
{
	OpFile file;
	RETURN_VOID_IF_ERROR(file.Construct(filename, folder));

	unsigned char *buffer = (unsigned char *) g_memory_manager->GetTempBuf2k();
	OpFileLength buffer_len = g_memory_manager->GetTempBuf2kLen();
	OpFileLength read_len;

	RETURN_VOID_IF_ERROR(file.Open(OPFILE_READ));

#ifdef VALGRIND
    op_memset(buffer, 0, buffer_len);
#endif
	RAND_seed(buffer, (int) buffer_len);
	while(!file.Eof())
	{
		if (OpStatus::IsError(file.Read(buffer, buffer_len,&read_len)))
			break;
		RAND_seed(buffer, (int) read_len);
	}
	OPERA_cleanse_heap(buffer, (unsigned int) buffer_len);

	file.Close();
}

OP_STATUS FeedTheRandomAnimal()
{

#ifdef DIRECTORY_SEARCH_SUPPORT
#define RANDOM_FILE_SEED_COUNT 32
	struct random_inputlist_st{
		OpString name;
		OpFileLength size;

		random_inputlist_st() : size(0){};
		~random_inputlist_st(){};
	} tops[RANDOM_FILE_SEED_COUNT+1]; 
	
	int count = 0;
	OpFileLength minimum = LONG_MAX;
	int i;
	OpFileLength file_size;


	OpAutoPtr<OpFolderLister> dir(OpFile::GetFolderLister(OPFILE_CACHE_FOLDER, UNI_L("*,*")));

	if(dir.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	while(dir->Next())
	{
		file_size = dir->GetFileLength();

		if(file_size && count < RANDOM_FILE_SEED_COUNT)
		{
            RETURN_IF_ERROR(tops[count].name.Set(dir->GetFileName()));
			tops[count].size = file_size;
			count ++;
			minimum = (minimum > file_size ? file_size : minimum);
		}
		else if(file_size >minimum)
		{
			for(i= 0;i<RANDOM_FILE_SEED_COUNT && tops[i].size != minimum;i++)
			{
				// Scan for first minimum entry.
			}
			if(i<RANDOM_FILE_SEED_COUNT)
			{
				RETURN_IF_ERROR(tops[i].name.Set(dir->GetFileName()));
				minimum = tops[i].size = file_size;
			}
			for(i= 0;i<RANDOM_FILE_SEED_COUNT;i++)
			{
				minimum = (minimum > tops[i].size ? tops[i].size : minimum);	
			}	
		}
	}

	for(i=0;i<count;i++)
	{
		if (tops[i].name.HasContent())
			SSL_Rand_seed_file(OPFILE_CACHE_FOLDER, tops[i].name);
	}

    dir.reset();
#endif // DIRECTORY_SEARCH_SUPPORT

	return OpStatus::OK;
}


OP_STATUS SSL_RND_Init()
{
	if(g_SSL_RND_Initialized)
		return OpStatus::OK;

	g_SSL_RND_Initialized = TRUE;

#ifndef SSL_NO_USE_OPERA 
	SSL_Rand_seed_file(OPFILE_HOME_FOLDER, UNI_L("oprand.dat"));
#else
	SSL_Rand_seed_file(UNI_L("."), UNI_L("oprand.dat"));
#endif

#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	g_SSL_RND_feeder_pos = 0;
	g_SSL_RND_feeder_len = 1024;
	g_SSL_RND_feeder_data = OP_NEWA(DWORD, g_SSL_RND_feeder_len);
	if(g_SSL_RND_feeder_data != NULL)
	{
#ifdef VALGRIND
        op_memset(g_SSL_RND_feeder_data, 0, sizeof(DWORD)*g_SSL_RND_feeder_len);
#endif        
		g_SSL_RND_feeder_pos = g_SSL_RND_feeder_len;
		SSL_Process_Feeder();
		g_SSL_RND_feeder_pos = 0;
	}
	else
		g_SSL_RND_feeder_len = 0;
#endif
	return OpStatus::OK;
}

OP_STATUS SSL_RND_Save()
{
	if(g_SSL_RND_Initialized)
	{
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
		SSL_Process_Feeder();
#endif
		OpFile file;
		RETURN_IF_ERROR(file.Construct(UNI_L("oprand.dat"), OPFILE_HOME_FOLDER));
		
		unsigned char *buffer = (unsigned char *) g_memory_manager->GetTempBuf2k();
		OpFileLength buffer_len = g_memory_manager->GetTempBuf2kLen();
		
		if (buffer_len < 1024)
			buffer_len = 1024;
		
		RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
		
#ifdef VALGRIND
        op_memset(buffer,0,buffer_len);
#endif
		RAND_bytes(buffer, (int) buffer_len);
		file.Write(buffer, buffer_len);
		
		OPERA_cleanse_heap(buffer, (unsigned int) buffer_len);

		file.Close();
	}

	return OpStatus::OK;
}

void SSL_RND(SSL_varvector32 &target,uint32 len)
{
	if(len > 0)
		target.Resize(len);
	SSL_RND(target.GetDirect(),target.GetLength());
}

void SSL_RND(byte *target,uint32 len)
{
	time_t gmt_unix_time;
	
	if(!g_SSL_RND_Initialized)
		SSL_RND_Init();
    
#ifdef VALGRIND
    op_memset(target, 0, len);
#endif
	gmt_unix_time = (uint32) op_time(NULL);
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	SSL_Process_Feeder();
#endif
	RAND_seed((unsigned char *) &gmt_unix_time,sizeof(gmt_unix_time)); 
	RAND_bytes(target,(size_t) len);

}

void SSL_SEED_RND(byte *source,uint32 len)
{
	time_t gmt_unix_time;
	
	if(!g_SSL_RND_Initialized)
		SSL_RND_Init();
	
	gmt_unix_time = (uint32) op_time(NULL);
	RAND_seed((unsigned char *) &gmt_unix_time,sizeof(gmt_unix_time)); 
	RAND_seed(source,(size_t) len); 
}

#endif // _NATIVE_SSL_SUPPORT_
