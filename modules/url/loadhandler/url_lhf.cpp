/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"
#ifndef NO_FTP_SUPPORT

#include "modules/pi/OpSystemInfo.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/datefun.h"
#include "modules/util/htmlify.h"
#include "modules/encodings/utility/opstring-encodings.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_pd.h"

#include "modules/olddebug/timing.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/ftp.h"
#include "modules/auth/auth_basic.h"
#include "modules/url/tools/arrays.h"
#include "modules/formats/argsplit.h"

#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/decoders/inputconverter.h"

#include "modules/about/opfolderlisting.h"

extern int finfo_compare(const File_Info *e1, const File_Info *e2 );
// Url_lhf.cpp

// URL Load Handler FTP

#define DirectoryTextMaxLen 64

URL_FTP_LoadHandler::URL_FTP_LoadHandler(URL_Rep *url_rep, MessageHandler *msgh)
	: URL_LoadHandler(url_rep, msgh)
	, generator(NULL)
	, bytes_downloaded(0)
{
	ftp = NULL;
	started = FALSE;
	dir_buffer = NULL;
	unconsumed = dir_buffer_size = 0;
	sending = FALSE;
	authenticating = FALSE;
	authentication_done = FALSE;
	is_directory = FALSE;
	converter = NULL;
}


URL_FTP_LoadHandler::~URL_FTP_LoadHandler()
{
	urlManager->StopAuthentication(url);
	if(ftp)
		DeleteComm();
	ftp = NULL;
	if(dir_buffer)
		OP_DELETEA(dir_buffer);
	OP_DELETE(converter);
	OP_DELETE(generator);
}


void URL_FTP_LoadHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	NormalCallCount blocker(this);
									__DO_START_TIMING(TIMING_COMM_LOAD);
	switch(msg)
	{
		/*
		case MSG_COMM_DATA_READY :
			ProcessReceivedData();
			break;
		*/
		case MSG_COMM_LOADING_FINISHED:
			HandleLoadingFinished();
			break;
		case MSG_COMM_LOADING_FAILED:
			HandleLoadingFailed(par1, par2);
			break;
		case MSG_FTP_HEADER_LOADED:
			{
				TRAPD(op_err, HandleHeaderLoadedL(par2));
				if(OpStatus::IsError(op_err))
				{
					g_memory_manager->RaiseCondition(op_err);
					HandleLoadingFailed(Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				}
			}
			break;

	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD);
}

void URL_FTP_LoadHandler::HandleLoadingFinished()
{
	DeleteComm();

	if((!finfo_list.Empty() && !started) || 
		(is_directory && finfo_list.Empty() && !started))
	{
		sending = TRUE;
		while(!finfo_list.Empty() || (is_directory && !started))
		{
			ProcessReceivedData();
		}
	}

	finfo_list.Clear();

	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	URL_LoadHandler::SetProgressInformation(REQUEST_FINISHED,0,
		server_name && server_name->UniName() ? server_name->UniName() : UNI_L(""));
	mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);  
}

void URL_FTP_LoadHandler::HandleHeaderLoadedL(MH_PARAM_2 par2)
{
	URL_DataStorage *url_ds = url->GetDataStorage();

	if(url_ds == NULL)
		return;

	url_ds->SetAttributeL(URL::KMIME_CharSet, NULL);	 
	url_ds->SetAttributeL(URL::KContentType, URL_UNDETERMINED_CONTENT);
	url_ds->SetAttributeL(URL::KMIME_Type, NULL);
	url->SetAttributeL(URL::KIsGeneratedByOpera, FALSE);

	if(par2)
	{
		is_directory = TRUE;
		url_ds->SetAttributeL(URL::KContentType, URL_HTML_CONTENT);
		url_ds->SetAttributeL(URL::KMIME_ForceContentType, "text/html; charset=utf-16");

		dir_message.Empty();
		if(ftp)
			ftp->GetDirMsgCopy(dir_message);

		unconsumed = 0;
		started = FALSE;
		sending = FALSE;

		dir_buffer_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
		dir_buffer = OP_NEWA_L(char, dir_buffer_size+1);
	}
	else
	{
		OpFileLength file_len = ftp->GetSize();
		url_ds->SetAttributeL(URL::KContentSize, &file_len);
		URLContentType tmp_url_ct;

		OpString temp_path;
		ANCHOR(OpString, temp_path);

		LEAVE_IF_ERROR(url->GetAttribute(URL::KUniPath, 0, temp_path));

		LEAVE_IF_ERROR(url_ds->FindContentType(tmp_url_ct, 0, 0, temp_path.CStr()));
		url_ds->SetAttributeL(URL::KContentType, tmp_url_ct);
#ifdef FTP_RESUME
		url_ds->SetAttributeL(URL::KResumeSupported,(ftp->GetSupportResume() ? Probably_Resumable : Not_Resumable));
		if(ftp->MDTM_Date().HasContent())
		{
			if(	url_ds->GetAttribute(URL::KFTP_MDTM_Date).Compare(ftp->MDTM_Date()) != 0)
				OpStatus::Ignore(url_ds->SetAttribute(URL::KFTP_MDTM_Date, ftp->MDTM_Date())); // Not critical;
		}
		if(url_ds->GetAttribute(URL::KReloadSameTarget))
		{
			if(ftp->GetHasResumed())
			{
				url_ds->UnsetCacheFinished();
			}
			else
			{
				url_ds->ResetCache();
			}
		}
#endif
	}

	if((URLContentType) url_ds->GetAttribute(URL::KContentType) != URL_UNDETERMINED_CONTENT)
	{
		url_ds->BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
		url->Access(FALSE);
	}
	// Should this be set at this time?
	url_ds->SetAttributeL(URL::KHeaderLoaded, TRUE);
}


void URL_FTP_LoadHandler::HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	URL_DataStorage *url_ds = url->GetDataStorage();

	if(url_ds == NULL)
		return;

	if (ftp)
		g_main_message_handler->UnsetCallBacks(ftp);

#ifdef _ENABLE_AUTHENTICATE
	if(!authenticating && (par2 == URL_ERRSTR(SI, ERR_FTP_USER_ERROR)
						|| par2 == URL_ERRSTR(SI, ERR_FTP_NEED_PASSWORD)))
	{
		authentication_done = FALSE;
		NormalCallCount blocker(this);
		authenticating = urlManager->HandleAuthentication(url, !ftp->AnoynymousUser());
		if(authenticating && authentication_done)
		{
			authenticating = FALSE;
			return;
		}
	}

	if(authenticating)
		return;
#endif

	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	URL_LoadHandler::SetProgressInformation(REQUEST_FINISHED,0,
		server_name && server_name->UniName() ? server_name->UniName() : UNI_L(""));

	DeleteComm();

	sending = TRUE;
	while(!finfo_list.Empty() || (is_directory && !started))
	{
		ProcessReceivedData();
	}

	finfo_list.Clear();

	if(dir_buffer)
	{
		OP_DELETEA(dir_buffer);
		dir_buffer = NULL;
		dir_buffer_size = 0;
		unconsumed = 0;
	}

	url_ds->SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE);
	mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),par2);  
}

unsigned URL_FTP_LoadHandler::ReadData(char *buffer, unsigned buffer_len)
{
	OP_STATUS stat = OpStatus::OK;
	OpStatus::Ignore(stat);
	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);

	if(is_directory)
	{
		OP_MEMORY_VAR unsigned loaded = 0;
		
		if(ftp)
		{
			loaded = ftp->ReadData(dir_buffer + unconsumed, dir_buffer_size - unconsumed);
	
			SetProgressInformation(LOAD_PROGRESS, loaded, (server_name ? server_name->UniName() : NULL));
		}

		unconsumed += loaded;
		dir_buffer[unconsumed] = '\0';


		BOOL finished = FALSE;
		loaded = 0;
		/*do*/ {
			if(!sending)
				GenerateDirListing();

			finished = TRUE;
			if(sending)
			{
				if(!started)
				{
					OP_ASSERT(!generator);
					URL the_url(url, (char *) NULL);
					generator = OP_NEW(OpFolderListing, (the_url));
					OP_STATUS rc;
					if (!generator)
					{
						rc = OpStatus::ERR_NO_MEMORY;
					}
					else
					{
						rc = generator->GenerateData();
					}
					if (OpStatus::IsRaisable(rc))
					{
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					}

					if(dir_message.HasContent())
					{
						int dirmsg_len = dir_message.Length();
						// Figure out which encoding to interpret the ftp
						// message as
						if (!converter)
						{
							const char * OP_MEMORY_VAR encoding = NULL;

							URL_DataStorage *ds = url->GetDataStorage();
							MessageHandler *mh = ds ? ds->GetFirstMessageHandler() : NULL;
							Window *my_window = mh ? mh->GetWindow() : NULL;
						
							if (my_window)
							{
								encoding = my_window->GetForceEncoding();
							}

							if (NULL == encoding || 0 == *encoding || strni_eq(encoding, "AUTODETECT-", 11))
							{
								CharsetDetector detect(server_name ? server_name->Name() : NULL, my_window);
								detect.PeekAtBuffer(dir_message.CStr(),dirmsg_len);
								const char *detected_encoding = detect.GetDetectedCharset();
								if (detected_encoding)
								{
									encoding = detected_encoding;
								}
								else
								{
									encoding = g_pcdisplay->GetDefaultEncoding();
								}
							}

							InputConverter::CreateCharConverter(encoding, &converter);
						}

						// Convert the message
						if(converter)
						{
							OpString ftp_msg;
							TRAPD(err, SetFromEncodingL(&ftp_msg, converter, dir_message.CStr(), dirmsg_len));
							if (OpStatus::IsError(err))
							{
								// Handle as iso-8859-1 if something went wrong // won't work if OOM
								ftp_msg.Set(dir_message);//FIXME:OOM (unable to report)
							}

							if(ftp_msg.HasContent())
							{
								if (generator)
									generator->WriteWelcomeMessage(ftp_msg);
							}
						}
					}
/*
					if(url->GetAttribute(URL::KPath).Compare("/") != 0 || url->GetAttribute(URL::KHaveAuthentication))
					{
						OpString DirectoryText;
						TRAPD(err, g_languageManager->GetStringL(Str::SI_DIRECTORY_UPTOPARENT_TEXT, DirectoryText));
						URL this_url(url, (char *) NULL);
						if (ftp) {
							OpString path;
							path.Set(ftp->GetParentPath());
							if (path[0] != 0) {
								this_url.WriteDocumentDataUniSprintf(UNI_L("<P><A href=\"%s\">%s</A></P>\r\n"), path.CStr(), DirectoryText.CStr());
							}
						} else {
							this_url.WriteDocumentDataUniSprintf(UNI_L("<P><A href=\"..\">%s</A></P>\r\n"), DirectoryText.CStr());
						}
					}
*/
					started = TRUE;

					finfo_list.Sort_List(finfo_compare);
				}

				stat =
					GenerateDirectoryHTML(generator, finfo_list, finished, TRUE); 
				if(OpStatus::IsMemoryError(stat))
					g_memory_manager->RaiseCondition(stat);

				/*
				loaded += GenerateDirectoryHTML(buffer, buffer_len, pos, count, 
											started, url->Name(PASSWORD_ONLYUSER),
											&finfo, finished, FALSE);
				*/
				if(!finished)
					mh->PostDelayedMessage(MSG_COMM_DATA_READY, Id(),0,100);
			}
		}//while(!finished && !sending);

		if(finished && !ftp)
		{
			OP_ASSERT(generator);
			if (generator)
			{
				generator->EndFolderListing();
			}
		}

		return loaded;
	}

	if(!ftp)
		return 0;
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(!url_ds->GetAttribute(URL::KHeaderLoaded))
	{
		//MessageHandler *msgh= url_ds->GetMessageHandlerList()->FirstMsgHandler();
		
		mh->PostDelayedMessage(MSG_COMM_DATA_READY, Id(),0,100);
		//msgh->SetCallBack(this, MSG_COMM_DATA_READY, ftp->Id(),0);
		return 0;
	}
	unsigned rlen =  ftp->ReadData(buffer, buffer_len);
	bytes_downloaded += rlen;
	if (bytes_downloaded >= ftp->GetSize())
		mh->PostDelayedMessage(MSG_COMM_LOADING_FINISHED, Id(),0, 0);

	SetProgressInformation(LOAD_PROGRESS, rlen, (server_name ? server_name->UniName() : NULL));

	return rlen;

}

void URL_FTP_LoadHandler::EndLoading(BOOL force)
{
	if(ftp)
	{
		if(force)
			ftp->EndLoading();
		else
			ftp->Stop();
	}
}

void URL_FTP_LoadHandler::DeleteComm()
{
	if(ftp)
	{
		g_main_message_handler->RemoveCallBacks(this, ftp->Id());
		g_main_message_handler->UnsetCallBacks(ftp);
		SafeDestruction(ftp);
		ftp = NULL;
	}
}

CommState URL_FTP_LoadHandler::Load()
{
	if(url->GetAttribute(URL::KHostName).IsEmpty())
	{
		url->GetDataStorage()->BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND), MH_LIST_ALL);
		return COMM_REQUEST_FAILED;
	}

	OpString8 pth;

	TRAPD(op_err, url->GetAttributeL(URL::KPath, pth));
	if(OpStatus::IsError(op_err))
	{
		if(OpStatus::IsMemoryError(op_err))
			g_memory_manager->RaiseCondition(op_err);
		return COMM_REQUEST_FAILED;
	}

	const char *OP_MEMORY_VAR temp_path = (pth.HasContent() ? pth.CStr() + 1 : "");
	char *type_str = NULL;

	OP_MEMORY_VAR int typecode = FTP_TYPECODE_NULL;

	type_str = const_cast<char *>(op_strchr(temp_path, ';')); // parameters are only permitted *after* the last pathcomponent

	if(type_str)
	{
		*(type_str++) = '\0';

		ParameterList arguments;
		arguments.SetValue(type_str, PARAM_SEP_SEMICOLON | PARAM_BORROW_CONTENT);

		Parameters *type_param = arguments.GetParameter("type",PARAMETER_ASSIGNED_ONLY);
		if(type_param && type_param->Value())
		{
			switch(*type_param->Value())
			{
			case 'a':
			case 'A':
				typecode = FTP_TYPECODE_A;
				break;
			case 'i':
			case 'I':
				typecode = FTP_TYPECODE_I;
				break;
			case 'd':
			case 'D':
				typecode = FTP_TYPECODE_D;
				break;
			}
		}
	}

	const char * OP_MEMORY_VAR ftp_user = 	url->GetAttribute(URL::KUserName).CStr();
	OpString8 ftp_pass;

	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, url->GetAttributeL(URL::KPassWord, ftp_pass), COMM_REQUEST_FAILED);
	
	AuthElm *auth_elm=NULL;
	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);

	unsigned short _port = url->GetAttribute(URL::KResolvedPort);

	if(ftp_user && *ftp_user) 
	{
		if(ftp_pass.HasContent())
		{
			Basic_AuthElm *temp_elm = OP_NEW(Basic_AuthElm, (_port,AUTH_NO_METH,(URLType) url->GetAttribute(URL::KType)));
			if( temp_elm && OpStatus::IsSuccess(temp_elm->Construct(ftp_user , ftp_user, ftp_pass)) )
			{
				auth_elm = temp_elm;
				server_name->Add_Auth(auth_elm, url->GetAttribute(URL::KPath));
			}
			else
			{
				if( temp_elm )
					OP_DELETE(temp_elm);
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return COMM_REQUEST_FAILED;
			}
		}
		else
		{
			auth_elm = server_name->Get_Auth(NULL, _port, NULL, (URLType) url->GetAttribute(URL::KType), AUTH_SCHEME_FTP, url->GetContextId());
			if(auth_elm)
			{
				if(url->GetAttribute(URL::KUserName).CStr() || url->GetAttribute(URL::KPassWord).CStr())
				{
					if((auth_elm->GetScheme() & AUTH_SCHEME_FTP) != 0)
					{
						mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
						return COMM_LOADING;
					}
				}

				RETURN_VALUE_IF_ERROR(auth_elm->GetAuthString(ftp_pass, NULL), COMM_REQUEST_FAILED);
			}
			else
				ftp_pass.Set("");
			if (ftp_pass.CStr() == NULL)
				return COMM_REQUEST_FAILED;
		}

	}
	else if(!ftp_user || !*ftp_user)
	{
		auth_elm = server_name->Get_Auth("", _port, NULL, (URLType) url->GetAttribute(URL::KType), AUTH_SCHEME_FTP, url->GetContextId());
		if(auth_elm)
		{
			ftp_user = auth_elm->GetUser();
			RETURN_VALUE_IF_ERROR(auth_elm->GetAuthString(ftp_pass, NULL), COMM_REQUEST_FAILED);
		}
	}

	ftp = OP_NEW(FTP_Request, (mh));//FIXME:OOM
	
	if(ftp == NULL)
		return COMM_REQUEST_FAILED;

	if(OpStatus::IsError(ftp->Construct(server_name, _port,temp_path, ftp_user, ftp_pass, typecode)))//FIXME:OOM
	{
		OP_DELETE(ftp);
		ftp = NULL;
		return COMM_REQUEST_FAILED;
	}

#ifdef URL_DISABLE_INTERACTION
	if(url->GetAttribute(URL::KBlockUserInteraction))
		ftp->SetUserInteractionBlocked(TRUE);
#endif
	ftp->Set_NetType((OpSocketAddressNetType) url->GetAttribute(URL::KLimitNetType));
	if (ftp->Get_NetType() != NETTYPE_UNDETERMINED &&
		ftp->Get_NetType() > server_name->GetNetType() && 
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, server_name)
		)
	{
		if (mh)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), GetCrossNetworkError(ftp->Get_NetType(), server_name->GetNetType()));
		return COMM_LOADING; // error message will arrive
	}

	if(ftp_user && *ftp_user)
	{
		url->SetAttribute(URL::KHaveAuthentication, TRUE);
		ftp->SetAuthorizationId(auth_elm ? auth_elm->GetId() : 0);
	}


#ifdef FTP_RESUME
	if(url->GetAttribute(URL::KIsResuming))
	{
		OpFileLength registered_len=0;
		url->GetAttribute(URL::KContentLoaded, &registered_len);

		ftp->SetResume(registered_len);
		ftp->SetMDTM_Date(url->GetAttribute(URL::KFTP_MDTM_Date));
	}
#endif

	SetProgressInformation(SET_SECURITYLEVEL, SECURITY_STATE_NONE);

	RETURN_VALUE_IF_ERROR(ftp->SetCallbacks(this, this), COMM_REQUEST_FAILED);

	ftp->ChangeParent(this);

	return ftp->Load();
}

void URL_FTP_LoadHandler::GenerateDirListing()
{
	if(unconsumed == 0)
		return;

	char *buf_pos = dir_buffer;
	unsigned line_len;

	if (!converter)
	{
		// Figure out which encoding to interpret the file listing as.
		const char * OP_MEMORY_VAR encoding = NULL;

		URL_DataStorage *ds = url->GetDataStorage();
		MessageHandler *mh = ds ? ds->GetFirstMessageHandler() : NULL;
		Window *my_window = mh ? mh->GetWindow() : NULL;

		if (my_window)
		{
			encoding = my_window->GetForceEncoding();
		}

		if (NULL == encoding || 0 == *encoding || strni_eq(encoding, "AUTODETECT-", 11))
		{
			// No forced encoding, try detecting
			ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
			CharsetDetector detect(server ? server->Name() : NULL, my_window);
			detect.PeekAtBuffer(buf_pos, unconsumed);
			const char *detected_encoding = detect.GetDetectedCharset();
			if (detected_encoding)
			{
				encoding = detected_encoding;
			}
			else
			{
				encoding = g_pcdisplay->GetDefaultEncoding();
			}
		}

		InputConverter::CreateCharConverter(encoding, &converter);
	}

	while (*buf_pos)
	{
		char* endline = op_strchr(buf_pos, '\n');
		if (endline)
			line_len = (int)(endline-buf_pos) + 1;
		else
			break;

#ifdef _DEBUG_PARSE_FTP
			PrintfTofile("ftp.txt","parsing %.*s", line_len,tmp);
#endif

			if (ParseFtpLine(buf_pos, line_len))
			{
			}
#ifdef _DEBUG_PARSE_FTP
			else
				PrintfTofile("ftp.txt","Failed\r\n");
#endif
		buf_pos = endline+1;
		while(*buf_pos && (*buf_pos == '\r' || *buf_pos == '\n'))
			buf_pos ++;
	}

	unsigned rest = (unconsumed - (buf_pos -dir_buffer));
	op_memmove(dir_buffer, buf_pos, rest);
	unconsumed = rest;
}

PREFIX_CONST_ARRAY(static, const char*, url_month_array, url)
    CONST_ENTRY("Jan")
    CONST_ENTRY("Feb")
    CONST_ENTRY("Mar")
    CONST_ENTRY("Apr")
    CONST_ENTRY("May")
    CONST_ENTRY("Jun")
    CONST_ENTRY("Jul")
    CONST_ENTRY("Aug")
    CONST_ENTRY("Sep")
    CONST_ENTRY("Oct")
    CONST_ENTRY("Nov")
    CONST_ENTRY("Dec")
CONST_END(url_month_array)

BOOL URL_FTP_LoadHandler::ParseFtpLine(char* buf, int buf_len)
{
	while(buf_len>0 && (buf[buf_len-1] == '\r' ||buf[buf_len-1] == '\n'))
		buf_len--;
	char tmp =buf[buf_len];
	buf[buf_len] = 0;

	char attribute;
	char *mon = (char*)g_memory_manager->GetTempBuf2k();
	unsigned day=0, mon1 = 0;
	unsigned year=0, hour=0, min=0, sec=0;
	char *filename = (char*)g_memory_manager->GetTempBuf();
	char *tempsize_str = (char*)g_memory_manager->GetTempBuf2();
	*tempsize_str = '\0';
	OpFileLength tempsize=0;

	OpStackAutoPtr<File_Info> finfo (OP_NEW(File_Info, ()));//FIXME:OOM
	if(finfo.get() == NULL)
		return FALSE;

	while(*buf && op_isspace((unsigned char) *buf))
	{
		buf++;
		buf_len --;
	}

	if(*buf == '+')
	{
		//EFLP format 
		buf++;
		buf_len --;

		char *pos = buf;
		attribute = '-';

		while(*pos && *pos != '\t')
		{
			switch(*pos)
			{
			case 'r':
				attribute = '-';
				break;
			case '/':
				attribute = 'd';
				break;
			case 'm':
				{
					long f_date;

					if(op_sscanf(pos+1, "%ld", &f_date) == 1 && f_date != 0)
					{
						time_t tmp_date = f_date;
						struct tm *tm_date = op_localtime(&tmp_date);

						if(tm_date)
						{
							sec = tm_date->tm_sec;
							min = tm_date->tm_min;
							hour = tm_date->tm_hour;
							day = tm_date->tm_mday;
							mon1 = tm_date->tm_mon+1;
							year = tm_date->tm_year +1900;

							OP_ASSERT(/*sec>=0 && */ sec <60);
							OP_ASSERT(mon1>=1 && mon1 <=12);
						}
					}
				}
				break;
			case 's':
				if(OpStatus::IsError(StrToOpFileLength(pos+1, &tempsize)))
					tempsize = 0;
				break;

			}

			while(*pos && *pos != ',')
				pos++;

			pos++;
		}

		if(*pos)
		{
			op_strlcpy(filename, pos+1, 4000);
		}
		else
		{
			buf[buf_len] = tmp;
			return FALSE;
		}

		if(attribute != 'd')
			finfo->type = 'i';
	}
	else if(op_sscanf(buf," %c%*s %*s %*s %*s %30[0123456789] %3s %u %u %u:%u:%u %4000[^\r\n]",
		&attribute, tempsize_str, mon, &day,&year, &hour, &min, &sec, filename) == 9)
	{
		if(year < 50)
			year += 2000;
		else if(year < 1900)
			year += 1900;
	}
	else
	if(op_sscanf(buf," %c%*s %*s %*s %*s %30[0123456789] %3s %u %u %u:%u %4000[^\r\n]",
		&attribute, tempsize_str, mon, &day,&year, &hour, &min, filename) == 8)
	{
		sec = 0;
		if(year < 50)
			year += 2000;
		else if(year < 1900)
			year += 1900;
	}
	else
	if(op_sscanf(buf," %c%*s %*s %*s %30[0123456789] %3s %u %u %u:%u:%u %4000[^\r\n]",
		&attribute, tempsize_str, mon, &day,&year, &hour, &min, &sec, filename) == 8)
	{
		if(year < 50)
			year += 2000;
		else if(year < 1900)
			year += 1900;
	}
	else
	{
		year =0;
		if(op_sscanf(buf," %c%*s %*s %*s %*s %30[0123456789] %3s %u %u:%d:%u %4000[^\r\n]",
			&attribute, tempsize_str, mon, &day, &hour, &min, &sec, filename) != 8
			&& op_sscanf(buf," %c%*s %*s %*s %30[0123456789] %3s %u %u:%d:%u %4000[^\r\n]",
			&attribute, tempsize_str, mon, &day, &hour, &min, &sec, filename) != 8)
		{
			sec = 0;
			if(op_sscanf(buf," %c%*s %*s %*s %*s %30[0123456789] %3s %u %u:%d %4000[^\r\n]",
			&attribute, tempsize_str, mon, &day, &hour, &min, filename) != 7 &&
			op_sscanf(buf," %c%*s %*s %*s %30[0123456789] %3s %u %u:%d %4000[^\r\n]",
			&attribute, tempsize_str, mon, &day, &hour, &min, filename) != 7)
		{
			int d1, m1, y1;

			if(op_sscanf(buf,"%u.%u.%u %d:%d%*s %100s %4000[^\r\n]",
				&d1, &m1, &y1,&hour, &min, tempsize_str,  filename) != 7)
			{
				if(op_sscanf(buf,"%u-%u-%u %d:%d%*s %100s %4000[^\r\n]",
					 &m1, &d1, &y1,&hour, &min, tempsize_str,  filename) != 7)
				{
					sec = hour = min = m1 = 0;
					tempsize_str[0] = '\0';
					int dummy;
					if(op_sscanf(buf," %c%*s %d %*s %*s %30[0123456789] %3s %u %u %4000[^\r\n]",
						&attribute, &dummy, tempsize_str, mon, &d1, &y1,  filename) != 7 &&
						op_sscanf(buf," %c%*s %d %*s %*s %30[0123456789] %3s %u %u %4000[^\r\n]",
						&attribute, &dummy, tempsize_str, mon, &d1, &y1,  filename) != 7 &&
						op_sscanf(buf," %c %*s %*s %30[0123456789] %3s %u %u:%d %4000[^\r\n]",
						&attribute, tempsize_str, mon, &day, &hour, &min, filename) != 7 &&
						op_sscanf(buf," %c%*s %*s %*s %30[0123456789] %3s %u %u %4000[^\r\n]",
						&attribute, tempsize_str, mon, &d1, &y1,  filename) != 6 &&
						op_sscanf(buf," %c%*s %*s %30[0123456789] %3s %u %u %4000[^\r\n]",
						&attribute, tempsize_str, mon, &d1, &y1,  filename) != 6)
					{
					buf[buf_len] = tmp;
					return FALSE;
					}
				}
			}
		
			day = d1;
			mon1 = m1;
			year = y1;
			if(year < 50)
				year += 2000;
			else if(year < 1900)
				year += 1900;
			sec = 0;

			if(op_strspn(tempsize_str, " 0123456789,") != op_strlen(tempsize_str))
				attribute = 'd';
		}
		}
	}

	buf[buf_len] = tmp;

	if(tempsize_str[0])
	{
		if(OpStatus::IsError(StrToOpFileLength(tempsize_str, &tempsize)))
			tempsize = 0;
	}

	finfo->size = tempsize;

	BOOL is_symb_link = FALSE;
	if (attribute == 'd')
		finfo->attrib = OpFileInfo::DIRECTORY;
	else if (attribute == 'l')
	{
		finfo->attrib = OpFileInfo::SYMBOLIC_LINK;
		is_symb_link = TRUE;
	}
	else
		finfo->attrib = OpFileInfo::FILE;

	// reset date/time
	finfo->wr_time = 0;
	struct tm filetime;

	op_memset(&filetime,0,sizeof(filetime));

	filetime.tm_mon=0;
	filetime.tm_isdst = -1;

	if(mon1)
	{
		filetime.tm_mon = mon1 - 1;
	}
	else
	{
		mon[3] = 0; // To be on the safe side, op_sscanf may not NUL terminate.
		int j=0;
		for (j=0; j<12; j++)
		{
			if (op_stricmp(mon, g_url_month_array[j]) == 0)
			{
				filetime.tm_mon = j;
				break;
			}
		}
		if (j==12)
			return FALSE;
	}

	// get day;

	if (day > 31)
		day = 31;
	filetime.tm_mday = day;

	// if #month > #thismonth and no specific year it was previous year
	if(year == 0)
	{
		unsigned int month;

		year = GetThisYear(&month);
		if ((unsigned int) filetime.tm_mon + 1 >  month)
			year--;
	}
	filetime.tm_year = year - 1900;

	if (hour > 23)
		hour = 23;
	filetime.tm_hour = hour;
	if (min > 59)
		min = 59;
	filetime.tm_min = min;
	if (sec > 59)
		sec = 59;
	filetime.tm_sec = sec;

	finfo->wr_time = op_mktime(&filetime);

	if (is_symb_link)
	{
		char *link = op_strstr(filename, " ->");
		if(!link)
			link = op_strstr(filename, "->");
		if(link)
			*link = '\0';
	}

	if(op_strcmp(filename, "..") == 0)
		return FALSE;

	OP_STATUS rc;
	if(converter)
	{
		TRAP(rc, SetFromEncodingL(&finfo->name, converter, filename, op_strlen(filename)));
	}
	else
	{
		// If we weren't able to create a converter, just pretend it is
		// iso-8859-1.
		rc = finfo->name.Set(filename);
	}

	// We also need to remember the unconverted name so that we can pass
	// the proper name back to the ftp server when following the link.
	if(OpStatus::IsSuccess(rc))
	{
		rc = finfo->rawname.Set(filename);
	}

	if(OpStatus::IsMemoryError(rc))
	{
		g_memory_manager->RaiseCondition(rc);
	}
	finfo->name.Strip();

	if(finfo->name.IsEmpty())
		return FALSE;

	finfo->Into(&finfo_list);
	finfo.release();
	return TRUE ;
}

void URL_FTP_LoadHandler::SetProgressInformation(ProgressState progress_level, 
											 unsigned long progress_info1, 
											 const void *progress_info2)
{
	switch(progress_level)
	{
	case HEADER_LOADED :
		{
			TRAPD(op_err, HandleHeaderLoadedL(progress_info1));
			if(OpStatus::IsError(op_err))
			{
				g_memory_manager->RaiseCondition(op_err);
				HandleLoadingFailed(Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			}
		}
		break;
	case REQUEST_FINISHED:
		break;
	default:
		URL_LoadHandler::SetProgressInformation(progress_level,progress_info1, progress_info2);
	}
}

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void URL_FTP_LoadHandler::PauseLoading()
{
	if (ftp)
		ftp->PauseLoading();
}

OP_STATUS URL_FTP_LoadHandler::ContinueLoading()
{
	if (ftp)
		return ftp->ContinueLoading();
	return OpStatus::ERR;
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#endif // NO_FTP_SUPPORT
