/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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
#ifndef NO_FTP_SUPPORT

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url2.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/ftp.h"

#include "modules/url/url_tools.h"


#ifdef _DEBUG
#ifdef YNP_WORK
#define DEBUG_FTP
#define DEBUG_FTP_HEXDUMP
#endif
#endif

#ifdef _RELEASE_DEBUG_VERSION
#define DEBUG_FTP
#define DEBUG_FTP_HEXDUMP
#endif

#ifdef DEBUG_FTP
#include "modules/olddebug/tstdump.h"
#endif

FTP_Data::FTP_Data(MessageHandler* msg_handler)
	: ProtocolComm(msg_handler, NULL, NULL)
{
	//EnableGracefulClose();
	buffer = NULL;
	buffer_len = 0;
	record_mode = finished = FALSE;
	recordpos= recordlength = /*recordtype =*/ 0;
	unread_block = 0;
	is_marker = FALSE;
	headerpos = 0;
	
}

FTP_Data::~FTP_Data()
{
	OP_DELETEA(buffer);
}

void FTP_Data::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	mh->PostMessage(msg, Id(), par2);
}


unsigned int FTP_Data::ReadData(char* buf, unsigned int blen)
{
	unsigned int bytes_read;
	if(!record_mode)
	{
		if(blen <= 1)
			return 0;
		bytes_read = ProtocolComm::ReadData(buf, blen-1);
	}
	else
	{
		unsigned int br;
		
		bytes_read =0;
		if(buffer == NULL)
		{
			buffer_len = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
			buffer = OP_NEWA(char, buffer_len);
			if(buffer == NULL)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return 0;
			}
		}
		
		br = 0;
		if(buffer_len != unread_block)
			br = ProtocolComm::ReadData(buffer + unread_block, buffer_len- unread_block);
		
#ifdef DEBUG_FTP
		{
			PrintfTofile("ftp.txt","\n[%d] FTP_Data::GetContent() block mode - read %d bytes:\n",Id(),br);
#ifdef DEBUG_FTP_HEXDUMP
			DumpTofile((const unsigned char *) buffer, (unsigned long) br,"","ftp.txt");
#endif
		}
#endif
		
		br += unread_block;
		
		unsigned int i;
		for(i = 0;!finished && i<br;)
		{
			if(headerpos < 3)
			{
				is_marker = FALSE;
				while(i<br && headerpos < 3)
					header[headerpos++] = buffer[i++];
				if(headerpos == 3)
				{
					recordlength = (((unsigned) ((unsigned char) header[1])) << 8) |   ((unsigned char) header[2]);
					recordpos = 0;
					is_marker = ( ( ((unsigned char) header[0]) & 16 ) ? TRUE : FALSE);
				}
				else
					continue;
			}

			if(is_marker)
			{
				if(br-i >= recordlength - recordpos)
				{
					i += recordlength - recordpos;
					recordpos =  recordlength;
				}
				else
				{
					recordpos += br - i;
					i = br;
				}
			}
			else
			{
				unsigned int blocklen = (br-i >= recordlength - recordpos ?
					recordlength - recordpos : br - i);
				if(bytes_read + blocklen > blen-1)
					blocklen = blen- bytes_read-1;

				if(blocklen == 0)
					break;

				op_memcpy(buf+bytes_read,buffer+i,blocklen);
				bytes_read += blocklen;
				i += blocklen;
				recordpos +=  blocklen;
				if(bytes_read >= blen)
					break;
			}
			
			if(recordpos == recordlength)
			{
				if(((unsigned char) header[0]) & 64)
					finished = TRUE;
				headerpos = 0;
			}
		}
		
		unread_block = br -i;
		if(unread_block)
		{
			op_memcpy(buffer, buffer+i, unread_block);
		}

		if(finished)
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(),0);
	}
	buf[bytes_read] = '\0';

	if(ProtocolComm::Closed())
		mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);

#ifdef DEBUG_FTP
	PrintfTofile("ftp.txt","\n[%d] FTP_Data::GetContent() - read %d bytes: %s\n\n",Id(),bytes_read, buf);
#ifdef DEBUG_FTP_HEXDUMP
	DumpTofile((const unsigned char *) buf, (unsigned long) bytes_read,"","ftp.txt");
#endif
#endif
	
	return bytes_read;
}

void FTP_Data::Clear()
{
}


void FTP_Data::ProcessReceivedData()
{
#ifdef DEBUG_FTP
	PrintfTofile("ftp.txt", "\n[%d] FTP_Data::HandleDataReady() - MSG_COMM_DATA_READY\n", Id());
#endif
	ProtocolComm::ProcessReceivedData();
	//mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0);
}


void FTP_Data::Stop()
{
	finished = TRUE;
}

#endif // NO_FTP_SUPPORT
