/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"
#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/util/str.h"
#include "modules/url/url_sn.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/protocol/op_ssl.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/base/sslciphspec.h"

#include "modules/libssl/debug/tstdump2.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#define _DEBUGSSL_REC_FILE_APPIN
#define _DEBUGSSL_REC_FILE_RECV
#define _DEBUGSSL_REC_FILE_SEND
#define _DEBUGSSL_ENCRYPT
#define _DEBUGSSL_EVENT
#define  TST_DUMP
#endif
#elif defined(ERIC_YOUNG_WORK)
#define _DEBUGSSL_REC_FILE_APPIN
#define _DEBUGSSL_REC_FILE_RECV
#define _DEBUGSSL_REC_FILE_SEND
#define _DEBUGSSL_ENCRYPT
//#define _DEBUGSSL_EVENT
#define  TST_DUMP
#elif defined(_RELEASE_DEBUG_VERSION)
#define _DEBUGSSL_REC_FILE_APPIN
#define _DEBUGSSL_REC_FILE_RECV
#define _DEBUGSSL_REC_FILE_SEND
#define _DEBUGSSL_ENCRYPT
//#define _DEBUGSSL_EVENT
#define  TST_DUMP
#endif

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define SSL_ERRSTR(p,x) Str::##p##_##x
#else
#define SSL_ERRSTR(p,x) x
#endif

void SSL_Record_Layer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	Handling_callback++;
	switch(msg)
	{
    case  MSG_SSL_ENCRYPTION_STEP :
		StartEncrypt();
		break;
		/*
    case  MSG_SSL_DECRYPTION_STEP :
		statusinfo.DecryptHandling = TRUE;
#ifdef _DEBUGSSL_EVENT
		PrintfTofile("ssltrans.txt","\nEventDecrypt : Handling decrypt step %x %x %lx\n",
			(int) msg, (MH_PARAM_1) par1, (MH_PARAM_2) par2);
#endif
		EventDecrypt();
		if(!statusinfo.ReadingData)
		{
			if(!Buffer_in.Empty())
				ProtocolComm::ProcessReceivedData();
			if(unconsumed_buffer_length)
				ProcessReceivedData();
		}
		statusinfo.DecryptHandling = FALSE;
		break;
		*/
	case MSG_SSL_DATA_READY:
		if(!Buffer_in.Empty())
			ProtocolComm::ProcessReceivedData();
	}
	Handling_callback --;
}

uint32 SSL_Record_Layer::Handle_Receive_Record(const byte *source, uint32 length)
{
	SSL_Alert err;

	if (length == 0 || source == NULL)
		return 0;

	if(connstate->version_specific == NULL)
	{
		RaiseAlert(SSL_Fatal, SSL_Unexpected_Message);
		return length;
	}

#ifdef LIBSSL_HANDSHAKE_HEXDUMP
	if(g_selftest.running)
	{
		if(!statusinfo.ApplicationActive)
		{
			SSL_secure_varvector32 *data = OP_NEW(SSL_secure_varvector32, ());
			if(data)
			{
				data->SetTag(1); // Receive

				data->Set((unsigned char *)source, length);
				data->Into(&pending_connstate->selftest_sequence);
			}
		}
		else
			pending_connstate->selftest_sequence.Clear();
	}
#endif // LIBSSL_HANDSHAKE_HEXDUMP

	DataStream_SourceBuffer src_data((unsigned char *)source, length);
	src_data.SetIsASegment(TRUE);

	while (src_data.MoreData() && plain_record == NULL)
	{
		if(!IsProcessingInputData())
			break;

		if(statusinfo.ChangeReadCipher)
			break;

		if(loading_record == NULL)
		{
			loading_record = connstate->version_specific->GetRecord((SSL_ENCRYPTMODE) statusinfo.present_recorddecryption);

			if(loading_record == NULL)
			{
				RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
				break;
			}
			loading_record->ForwardTo(this);
			statusinfo.DecryptFinished = FALSE;
		}

		BOOL finished = FALSE;

		if (!statusinfo.DecryptFinished)
		{
			OP_MEMORY_VAR OP_STATUS op_err;
			OP_STATUS op_err1;

			op_err = OpRecStatus::OK;
			TRAP(op_err1, op_err = loading_record->ReadRecordFromStreamL(&src_data));
			finished = (op_err == OpRecStatus::FINISHED || OpStatus::IsError(op_err1));
			OpStatus::Ignore(op_err1); // Errors will be handled elsewhere
		}

		if(!finished)
			continue;

		statusinfo.DecryptFinished = TRUE;

		{
			SSL_ProtocolVersion ver;

			ver = loading_record->GetVersion();
			if(ver.Major()< 3)
			{
				RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
				ver = connstate->version;
			}

			if(ver != connstate->read.version)
			{
				if(!AcceptNewVersion(ver))
				{
					RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
					break;
				}
			}
		}

		if(loading_record->GetType() == SSL_ChangeCipherSpec)
			statusinfo.ChangeReadCipher = TRUE;

		switch ((SSL_ENCRYPTMODE) statusinfo.present_recorddecryption)
		{
		case SSL_RECORD_ENCRYPTED_UNCOMPRESSED:
		case SSL_RECORD_ENCRYPTED_COMPRESSED:
		case SSL_RECORD_COMPRESSED :
            {
				loading_record->Into(&Buffer_in_encrypted);
				loading_record = NULL;
				PerformDecryption();
            }
            break;
		case SSL_RECORD_PLAIN :
			plain_record = loading_record;
			loading_record = NULL;
			plain_record_pos = 0;
			connstate->read.cipher->Sequence_number ++;
			break;
		}

		if(plain_record != NULL && !plain_record->GetHandled())
		{
			plain_record->SetHandled(TRUE);
			Handle_Record(plain_record->GetType());
		}
	}
	return src_data.GetReadPos();
}

void SSL_Record_Layer::ProcessReceivedData()
{
	m_last_socket_operation = LAST_SOCKET_OPERATION_READ;
	uint32 len1, len2=0;

	if(statusinfo.CurrentlyProcessingIncoming)
		return;

	statusinfo.CurrentlyProcessingIncoming = TRUE;

	OP_ASSERT(network_buffer_size >= unconsumed_buffer_length);

	if(network_buffer_size-unconsumed_buffer_length > 0 && !ProtocolComm::Closed())
	{
		len2 = (uint32) ProtocolComm::ReadData((char *) networkbuffer_in+unconsumed_buffer_length, (unsigned int) (network_buffer_size-unconsumed_buffer_length));
	}

#ifdef _DEBUGSSL_REC_FILE_RECV
	if(len2>0)
	{
		OpString8 text;

		text.AppendFormat("ID %d : ReadContent received",Id());
		DumpTofile(networkbuffer_in+unconsumed_buffer_length,len2,text,"ssltrans.txt");
	}
#endif

	len2 += unconsumed_buffer_length;
	len1 = Handle_Receive_Record(networkbuffer_in,len2);

	OP_ASSERT(len2 >= len1);

	unconsumed_buffer_length =  (len2- len1);
	if (unconsumed_buffer_length != 0)
		op_memmove(networkbuffer_in,networkbuffer_in+len1,unconsumed_buffer_length);

	statusinfo.CurrentlyProcessingIncoming = FALSE;
	HandleCallback(MSG_NO_MESSAGE, 0, 0);
}

void SSL_Record_Layer::Perform_ProcessReceivedData()
{
	m_last_socket_operation = LAST_SOCKET_OPERATION_READ;

	if(!Buffer_in.Empty() || !Buffer_in_encrypted.Empty())
		ProtocolComm::ProcessReceivedData();
}

unsigned SSL_Record_Layer::ReadData(char *buf, unsigned buf_len)
{
	m_last_socket_operation = LAST_SOCKET_OPERATION_READ;

	uint32 len2,len1,blen;
	SSL_Record_Base *temp;
	byte *target;

	if(statusinfo.ReadingData)
		return 0;

	statusinfo.ReadingData = TRUE;

	len2 = 0;
	blen = buf_len;
	target = (byte *) buf;

	while (blen > 0  && (temp = Buffer_in.First()) != NULL)
	{
		len1 = temp->ReadDataL(target, blen);
		target += len1;
		blen -= len1;
		len2 += len1;
		if(!temp->MoreData())
		{
			temp->Out();
			OP_DELETE(temp);
		}
	}

	if(ProtocolComm::Closed())
	{
		if(ProcessingFinished(TRUE) || unconsumed_buffer_length == 0)
		{
			if(ProcessingFinished(TRUE))
			{
				long err = 0;
				OpMessage msg;

				if(loading_record != NULL)
				{
					OP_DELETE(loading_record);
					loading_record = NULL;

					msg = MSG_COMM_LOADING_FAILED;
					err = SSL_ERRSTR(SI,ERR_COMM_CONNECTION_CLOSED);
				}
				else
					msg = MSG_COMM_LOADING_FINISHED;

				Stop();

				if(msg && mh != NULL)
					mh->PostMessage(msg, Id(), err);
			}
		}
		else
		{
			if(mh == 0)
				Stop();
		}
	}
	else
	{
		PerformDecryption();
	}
#ifdef _DEBUGSSL_REC_FILE_APPIN
	if(len2>0)
	{
		OpString8 text;

		text.AppendFormat("ID %d : ReadData forwarding application bytes",Id());
		DumpTofile((byte *) buf,len2,text,"sslapin.txt");
	}
#endif

	if(!Buffer_in.Empty())
		mh->PostMessage(MSG_SSL_DATA_READY,Id(),0);

	statusinfo.ReadingData = FALSE;

	return len2;
}

SSL_Record_Base *SSL_Record_Layer::Fragment_message(SSL_secure_varvector32 *item)
{
	if (!item)
		return NULL;

	ResetError();

	SSL_Record_Base * OP_MEMORY_VAR tempbuffer;
	SSL_secure_varvector32 * OP_MEMORY_VAR temp;
	SSL_ContentType datatype;
	uint32 OP_MEMORY_VAR pos;
	OP_MEMORY_VAR uint32 rec_len;

    tempbuffer = connstate->version_specific->GetRecord(SSL_RECORD_PLAIN);

    if(tempbuffer == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return NULL;
    }

	datatype = (SSL_ContentType) item->GetTag();

 	tempbuffer->ForwardTo(this);
	tempbuffer->SetType(datatype);
    tempbuffer->SetVersion(connstate->write.version);

	rec_len = tempbuffer->GetLegalMaxLength();
	if(rec_len > network_buffer_size)
		rec_len = network_buffer_size;
	tempbuffer->SetResizeBlockSize(rec_len);

	 /**
	 * For block ciphers we have to divide the record into two records to avoid an CBC mode attack.
	 * The first record contains 1 byte, the second contains the rest.
	 *
	 * Since the previous cipher text is used as IV for next encryption, sending a 1 byte plaintext
	 * in first record stops the attacker from constructing a plain text from the IV that can be used in
	 * a chosen plain text attack. The IV for the next encryption where the attacker can control the plaintext
	 * is unknown for the attacker.
	 *
	 * Currently block ciphers are always in CBC mode, thus we assume CBC mode for all block ciphers.
	 * If this technique is used on CFB mode in the future, so be it,
	 * as there is no generic way of checking for encryption mode.
	 *
	 * Only SSL 3.0 and TLS 1.0 is vulnerable.
	 */
	if (statusinfo.PerformApplicationDataSplit && datatype == SSL_Application && CipherSuitRequiresRecordSplitting() && item->GetLength() > 1)
	{
		SSL_TRAP_AND_RAISE_ERROR_THIS(tempbuffer->AddContentL(item, 1));
		if(!item->MoreData())
		{
			OP_DELETE(item);
			item = NULL;
		}

		statusinfo.PerformApplicationDataSplit = FALSE;
	}
	else
	{
		temp = item;
		while ((pos = tempbuffer->GetLength()) < rec_len && temp != NULL && (SSL_ContentType) temp->GetTag() == datatype)
		{
			SSL_TRAP_AND_RAISE_ERROR_THIS(tempbuffer->AddContentL(temp, rec_len - pos));
			if(ErrorRaisedFlag)
				break;

			if(!temp->MoreData())
			{
				SSL_secure_varvector32 *temp1 = (SSL_secure_varvector32 *) temp->Suc();
				OP_DELETE(temp);
				temp = temp1;
			}
		}
	}

	if (tempbuffer->GetLength() >= rec_len)
		statusinfo.EncryptedRecordFull = TRUE;


    if((datatype != SSL_PerformChangeCipher
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
    	&& datatype != SSL_Handshake_False_Start_Application
#endif // LIBSSL_ENABLE_SSL_FALSE_START
    	&& tempbuffer->GetLength() == 0) || ErrorRaisedFlag)
	{
		OP_DELETE(tempbuffer);
		return NULL;
	}

	return tempbuffer;
}

void SSL_Record_Layer::Send(SSL_secure_varvector32 *source)
{
	SSL_secure_varvector32 *temp;
	if(source == NULL)
		return;

	source->ResetRead();
	SSL_ContentType type = (SSL_ContentType) source->GetTag();
	switch (type)
	{
    case SSL_AlertMessage :
		if(!unprocessed_ssl_messages_prioritized.Empty())
		{
			temp = unprocessed_ssl_messages_prioritized.First();

			while(temp != NULL && (SSL_ContentType) temp->GetTag() == SSL_AlertMessage)
                temp = (SSL_secure_varvector32 *) temp->Suc();

			if (temp != NULL)
			{
                source->Precede(temp);
                break;
			}
			// Otherwise put in into the end of the list
		}
    case SSL_ChangeCipherSpec :
    case SSL_Handshake :
	case SSL_PerformChangeCipher:
	case SSL_Warning_Alert_Message:
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	// When doing ssl false start, we send application data early, before receiving finish from server.
	case SSL_Handshake_False_Start_Application:
#endif // LIBSSL_ENABLE_SSL_FALSE_START
		source->Into(&unprocessed_ssl_messages_prioritized);
		break;
    case SSL_Application :
		source->Into(&unprocessed_application_data);
		break;
    default :
		OP_DELETE(source);
	}

	StartEncrypt();
}

void SSL_Record_Layer::EventEncrypt()
{
	ProgressEncryptionPipeline();
	statusinfo.PerformApplicationDataSplit = statusinfo.ApplicationActive && statusinfo.AllowApplicationDataSplitting && CipherSuitRequiresRecordSplitting();
}

void SSL_Record_Layer::ProgressEncryptionPipeline()
{
	if(statusinfo.CurrentlyProcessingOutgoing)
	{
		return;
	}
 	statusinfo.CurrentlyProcessingOutgoing = TRUE;


	if(sending_record != NULL)
		SSL_Record_Layer::RequestMoreData();

	if(setting_up_records)
	{
		statusinfo.CurrentlyProcessingOutgoing = FALSE;
		return;
	}

	if (sending_record == NULL)
	{
		SSL_Record_Base *templistitem = NULL;

		if (!unprocessed_ssl_messages_prioritized.Empty())
		{
			// We first see if there are any prioritized messages.
			// We remove the SSL_PerformChangeCipher message as it is only internal and will
			// not be sent out on network
			templistitem = Fragment_message(unprocessed_ssl_messages_prioritized.First());
			while (templistitem && templistitem->GetType() == SSL_PerformChangeCipher)
			{
				Do_ChangeCipher(TRUE);
				OP_DELETE(templistitem);
				templistitem = Fragment_message(unprocessed_ssl_messages_prioritized.First());
			}
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
			if (templistitem && templistitem->GetType() == SSL_Handshake_False_Start_Application)
			{
				OP_DELETE(templistitem);
				if (!unprocessed_application_data.Empty())
					templistitem = Fragment_message(unprocessed_application_data.First());
				else
					templistitem = Fragment_message(unprocessed_ssl_messages_prioritized.First());
			}
#endif // LIBSSL_ENABLE_SSL_FALSE_START
		}

		if (!templistitem && statusinfo.ApplicationActive)
			templistitem = Fragment_message(unprocessed_application_data.First());

		if (templistitem)
		{
		switch ((SSL_ENCRYPTMODE) statusinfo.present_recordencryption)
			{
			case SSL_RECORD_PLAIN :
				if(sending_record == NULL)
				{
					sending_record = templistitem;

					connstate->write.cipher->Sequence_number++;
				}
				break;
			case SSL_RECORD_ENCRYPTED_UNCOMPRESSED:
			case SSL_RECORD_COMPRESSED :
			case SSL_RECORD_ENCRYPTED_COMPRESSED:
				{
					SSL_Record_Base *encrypting_record = templistitem;

					sending_record = encrypting_record->Encrypt(connstate->write.cipher);
					if (sending_record != NULL)
						sending_record->ForwardTo(this);

#ifdef _DEBUGSSL_ENCRYPT
					{
						uint32 pos3;
						char *temp, text[180];

						pos3 =encrypting_record->GetLength();
						op_sprintf(text,"ID %d : Encrypting Record\n",Id());
						DumpTofile(*encrypting_record,pos3,text,"ssltrans.txt");
						switch (encrypting_record->GetType())
						{
						case SSL_ChangeCipherSpec :
							temp = "ChangeCipherSpec";
							break;
						case SSL_AlertMessage :
							temp = "AlertMessage";
							break;
						case SSL_Handshake :
							temp = "Handshake";
							break;
						case SSL_Application :
							temp = "Application";
							break;
						default:
							temp = "Unknown Messagetype";
						}
						op_sprintf(text,"ID %d : EventEncrypt encrypt %d bytes of messagetype %s(%0.2x).\n",Id(),(int) pos3,temp,(int)encrypting_record->GetType());
						DumpTofile(*encrypting_record,pos3,text,"sslcrypt.txt");
					}
#endif
					OP_DELETE(encrypting_record);
				}
				break;
			}
		}
	}

	if(sending_record != NULL)
		SSL_Record_Layer::RequestMoreData();

	statusinfo.CurrentlyProcessingOutgoing = FALSE;
}

BOOL SSL_Record_Layer::CipherSuitRequiresRecordSplitting() const
{
	SSL_GeneralCipher *method = connstate->read.cipher->Method;
	if (method)
	{
		SSL_BulkCipherType cipher_id = method->CipherID();

		return // The attack is only possible for block ciphers in CBC mode.
				// Since there's no generic way of checking for CBC mode,
				// we check a black list for not using record splitting.
				//
				// Any new block cipher will use record splitting.

				method->CipherType() == SSL_BlockCipher &&
			 	cipher_id != SSL_3DES_CFB &&
			 	cipher_id != SSL_AES_128_CFB &&
			 	cipher_id != SSL_AES_192_CFB &&
			 	cipher_id != SSL_AES_256_CFB &&
			 	cipher_id != SSL_CAST5_CFB &&

				// Only do this security measure for SSL 3.0 TLS 1.0. version number 3.2 == TLS 1.1.
				connstate->version.Compare(3, 2) < 0;
	}
	else
		return TRUE; // We should not end up here, but return TRUE in case we do.
}

BOOL SSL_Record_Layer::DataWaitingToBeSent() const
{
	return !unprocessed_ssl_messages_prioritized.Empty() ||
			(statusinfo.ApplicationActive && !unprocessed_application_data.Empty()) ||
			(sending_record && !sending_record->Empty());
}

void SSL_Record_Layer::StartEncrypt()
{
	if (!statusinfo.CurrentlyProcessingOutgoing)
	{
		statusinfo.EncryptedRecordFull = FALSE;
		do
			ProgressEncryptionPipeline();
		while (DataWaitingToBeSent() &&
			!setting_up_records &&
			!ErrorRaisedFlag &&
			!statusinfo.EncryptedRecordFull &&
			!(ProtocolComm::Closed() || ProtocolComm::PendingClose()) &&
			GetOPStatus() != OpStatus::ERR_NO_MEMORY);

		if(DataWaitingToBeSent() || out_buffer.GetLength() > 0)
		{
			// In case there are more data waiting to be sent, we
			g_main_message_handler->PostMessage(MSG_SSL_ENCRYPTION_STEP, Id(), 0);
		}

		statusinfo.PerformApplicationDataSplit = statusinfo.ApplicationActive && statusinfo.AllowApplicationDataSplitting && CipherSuitRequiresRecordSplitting();
	}
}

void SSL_Record_Layer::RequestMoreData()
{
	byte * OP_MEMORY_VAR target=NULL;
	OP_MEMORY_VAR uint32 len;

	if(statusinfo.ApplicationActive)
	{
		DataStream_GenericRecord_Small *item;
		uint32 totsize=0;
		UINT count;

		count = unprocessed_application_data.Cardinal();

		if(count >= 16)
		{
			item = unprocessed_application_data.First();
			totsize = 0;

			while(item)
			{
				totsize += item->GetLength()-item->GetReadPos();
				item = item->Suc();
			}
		}
		if(count < 16 || totsize < 65536)
			ProtocolComm::RequestMoreData();
	}

	if(sending_record == NULL)
		return;

	if(ProtocolComm::Closed())
	{
		OP_DELETE(sending_record);
		sending_record = NULL;
		return;
	}

	SSL_varvector32 target1;
	OP_STATUS op_err = OpStatus::OK;

	TRAP(op_err, sending_record->WriteRecordL(&target1));

	if (OpStatus::IsSuccess(op_err))
	{
		if (out_buffer.GetLength() > 0 || statusinfo.WriteRecordsToOutBuffer)
			TRAP(op_err, out_buffer.AddContentL(target1.GetDirect(), target1.GetLength()));
	}

	if (OpStatus::IsError(op_err))
	{
		RaiseAlert(op_err);
		return;
	}

	OP_DELETE(sending_record);
	sending_record = NULL;

	if(OpStatus::IsSuccess(op_err) && !statusinfo.WriteRecordsToOutBuffer)
	{
		if (out_buffer.GetLength() > 0)
		{
			len = out_buffer.GetLength();
			TRAP(op_err,target = out_buffer.ReleaseL());
		}
		else
		{
			len = target1.GetLength();
			TRAP(op_err,target = target1.ReleaseL());
		}

		if(OpStatus::IsError(op_err))
		{
			OP_DELETEA(target);
			RaiseAlert(op_err);
			return;
		}

#ifdef _DEBUGSSL_REC_FILE_SEND
		{
			OpString8 text;

			text.AppendFormat("ID %d : RequestMoreData sending %d bytes \n",Id(),(int) len);
			DumpTofile(target,len,text,"ssltrans.txt");
		}
#endif

#ifdef LIBSSL_HANDSHAKE_HEXDUMP
		if(g_selftest.running)
		{
			if(!statusinfo.ApplicationActive)
			{
				SSL_secure_varvector32 *data = OP_NEW(SSL_secure_varvector32, ());
				if(data)
				{
					data->SetTag(2); // Send

					data->Set((unsigned char *)target, len);
					data->Into(&pending_connstate->selftest_sequence);
				}
			}
			else
				pending_connstate->selftest_sequence.Clear();
		}
#endif // LIBSSL_HANDSHAKE_HEXDUMP

		m_last_socket_operation = LAST_SOCKET_OPERATION_WRITE;
		buffered_amount_raw_data += len;
		ProtocolComm::SendData((char *) target,(int) len);

	}

	if(m_AllDoneMsgMode != NO_STATE && statusinfo.ApplicationActive && unprocessed_application_data.Empty())
	{
		ProtocolComm::SetAllSentMsgMode(m_AllDoneMsgMode, m_AllDone_requestMsgMode);
		m_AllDoneMsgMode = m_AllDone_requestMsgMode = NO_STATE;
	}
}

void SSL_Record_Layer::Flush()
{
	byte * OP_MEMORY_VAR target = NULL;

	OP_MEMORY_VAR uint32 len = out_buffer.GetLength();
	TRAPD(op_err,target = out_buffer.ReleaseL());
	if(OpStatus::IsError(op_err))
	{
		OP_DELETEA(target);
		RaiseAlert(op_err);
		return;
	}

	buffered_amount_raw_data += len;
	ProtocolComm::SendData((char *) target,(int) len);
}

void SSL_Record_Layer::PerformDecryption()
{
	if ((SSL_ENCRYPTMODE) statusinfo.present_recorddecryption != SSL_RECORD_PLAIN && plain_record == NULL)
	{
		while(plain_record == NULL && !Buffer_in_encrypted.Empty())
		{
			SSL_Record_Base *decrypting_record;

			decrypting_record = Buffer_in_encrypted.First();
			decrypting_record->Out();

			plain_record = decrypting_record->Decrypt(connstate->read.cipher);
			plain_record_pos = 0;

#ifdef TST_DUMP
			PrintfTofile("ssltrans.txt","\nEventDecrypt : decrypting %d type, %d bytes, %s (%d).\n",decrypting_record->GetType(),
				decrypting_record->GetLength(),
				(plain_record != NULL ? "finished" : "not finished"),
				(plain_record != NULL ? plain_record->GetLength() :0));
#endif

			if (decrypting_record->ErrorRaisedFlag)
			{
				RaiseAlert(decrypting_record);
			}
			OP_DELETE(decrypting_record);


			if (plain_record && plain_record->ErrorRaisedFlag)
			{
				RaiseAlert(plain_record);
			}

			if(ErrorRaisedFlag)
			{
				SSL_Alert msg;
				Error(&msg);
				if(msg.GetLevel() != SSL_Warning)
					Buffer_in_encrypted.Clear();

				OP_DELETE(plain_record);
				plain_record = NULL;
				return;
			}

			if(plain_record != NULL)
				plain_record->ForwardTo(this);
		}
	}

	if(plain_record != NULL && !plain_record->GetHandled())
	{
#ifdef TST_DUMP
			PrintfTofile("ssltrans.txt","\nEventDecrypt : Handling %d type, %d bytes\n",plain_record->GetType(),
				plain_record->GetLength());

			OpString8 text;
			text.AppendFormat("ID %d : EventDecrypt decrypted ",Id());
			DumpTofile(*plain_record,plain_record->GetLength(),text,"ssltrans.txt");
#endif
		plain_record->SetHandled(TRUE);
		Handle_Record(plain_record->GetType());
	}
}

void SSL_Record_Layer::RemoveRecord()
{
	OP_DELETE(plain_record);
	plain_record = NULL;
}

void SSL_Record_Layer::MoveRecordToApplicationBuffer()
{
	if (plain_record != NULL)
	{
		if(plain_record->GetLength() > 0)
			plain_record->Into(&Buffer_in);
		else
			OP_DELETE(plain_record);
		plain_record = NULL;
	}
	if(!statusinfo.ReadingData && !Buffer_in.Empty() )
		ProtocolComm::ProcessReceivedData();
}

DataStream *SSL_Record_Layer::GetRecord()
{
	return plain_record;
}

void SSL_Record_Layer::FlushBuffers(BOOL complete)
{
	Buffer_in.Clear();
	Buffer_in_encrypted.Clear();
	out_buffer.Resize(0);

	unprocessed_application_data.Clear();
	unprocessed_ssl_messages_prioritized.Clear();

	if(complete)
	{
		OP_DELETE(loading_record);
		loading_record = NULL;

		OP_DELETE(plain_record);
		plain_record = NULL;

		OP_DELETE(sending_record);
		sending_record = NULL;
	}
}

int SSL_Record_Layer::Init()
{
	loading_record        = NULL;
	sending_record        = NULL;
	plain_record          = NULL;
	plain_record_pos	  = 0;

	unsigned char *buffer = NULL;
	TRAPD(err, buffer = out_buffer.ReleaseL());
	if (OpStatus::IsSuccess(err))
		OP_DELETEA(buffer);

	statusinfo.ProcessingInputData = FALSE;
	statusinfo.PerformApplicationDataSplit = statusinfo.AllowApplicationDataSplitting;
	statusinfo.FirstData = TRUE;

	statusinfo.ChangeReadCipher =
		statusinfo.DecryptFinished =
		statusinfo.ApplicationActive =
		statusinfo.CurrentlyProcessingIncoming = FALSE;

	statusinfo.present_recordencryption =
		statusinfo.present_recorddecryption = SSL_RECORD_PLAIN;

	network_buffer_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;

	if(!network_buffer_size)
		network_buffer_size = 4096; // GENERALOPTIONS.networkbufferlength;

	if(networkbuffer_in != NULL)
		OP_DELETEA(networkbuffer_in);
	networkbuffer_in = OP_NEWA(byte, network_buffer_size+1);
	if(!networkbuffer_in)
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);

	unconsumed_buffer_length = 0;

	Bufferpos = 0;

	PauseApplicationData();
	return 1;
}

void SSL_Record_Layer::Do_ChangeCipher(BOOL writecipher)
{
	struct SSL_ConnectionState::direction_cipher SSL_ConnectionState::*spec;
	SSL_CipherSpec *cipher,*pending_cipher;
	uint64 seq_back;
	SSL_ENCRYPTMODE encryptiontype;
	uint8 major;

	seq_back = 0;

#ifdef _DEBUGSSL_EVENT
		PrintfTofile("ssltrans.txt","[%d] Changing Cipher %s\n", Id(),
			(writecipher ? "for sending" : "for receiving"));
#endif
#ifdef _DEBUGSSL_ENCRYPT
		PrintfTofile("sslcrypt.txt","[%d] Changing Cipher %s\n", Id(),
			(writecipher ? "for sending" : "for receiving"));
#endif

	spec =  (writecipher ?  &SSL_ConnectionState::write :  &SSL_ConnectionState::read);
	cipher = (connstate->*spec).cipher;
	if(cipher != NULL)
	{
		seq_back = cipher->Sequence_number;
		OP_DELETE(cipher);
	}

	connstate->*spec = pending_connstate->*spec;
	cipher = (connstate->*spec).cipher;
	major = (connstate->*spec).version.Major();
	(pending_connstate->*spec).cipher = NULL;
	pending_cipher = pending_connstate->Prepare_cipher_spec(writecipher);// new SSL_CipherSpec;

	if(!writecipher)
		statusinfo.ChangeReadCipher = FALSE;

	if(pending_cipher == NULL)
		return;

	if(cipher->compression != SSL_Null_Compression)
		encryptiontype = (cipher->Method != NULL ? SSL_RECORD_ENCRYPTED_COMPRESSED : SSL_RECORD_COMPRESSED);
	else
		encryptiontype = (cipher->Method != NULL ? SSL_RECORD_ENCRYPTED_UNCOMPRESSED : SSL_RECORD_PLAIN);;

	if(writecipher)
		statusinfo.present_recordencryption = encryptiontype;
	else
		statusinfo.present_recorddecryption = encryptiontype;

	if(major < 3)
		cipher->Sequence_number = seq_back;

	if(!writecipher)
		StartEncrypt();
	else
		ProcessReceivedData();
}

BOOL SSL_Record_Layer::Closed()
{
    if(!ProtocolComm::Closed())
        return FALSE;
    return (ProcessingFinished(TRUE) ? TRUE : FALSE);
}

BOOL SSL_Record_Layer::ProcessingFinished(BOOL ignore_loading_record)
{
	if(trying_to_reconnect)
		return FALSE;

	return (ProtocolComm::Closed() &&
		Buffer_in.Empty() && Buffer_in_encrypted.Empty() &&
		plain_record == NULL &&
		(ignore_loading_record || loading_record  == NULL || unconsumed_buffer_length == 0/*  || !loading_record->FinishedLoading()*/));
}


BOOL SSL_Record_Layer::EmptyBuffers(BOOL ignore_loading_record) const
{
	return (Buffer_in.Empty() && Buffer_in_encrypted.Empty() &&
		unprocessed_application_data.Empty() && unprocessed_ssl_messages_prioritized.Empty() &&
		plain_record == NULL && (ignore_loading_record ||loading_record  == NULL || unconsumed_buffer_length == 0)
		);
}

SSL_ConnectionState *SSL_Record_Layer::Prepare_connstate()
{
	SSL_ConnectionState *tempstate;

#ifdef LIBSSL_HANDSHAKE_HEXDUMP
	tempstate = OP_NEW(SSL_ConnectionState, (server_info, selftest_sequence));
#else
	tempstate = OP_NEW(SSL_ConnectionState, (server_info));
#endif
	if(tempstate != NULL)
	{
		tempstate->ForwardTo(this);

		if(tempstate->Error())
		{
			RaiseAlert(tempstate);
			OP_DELETE(tempstate);
			tempstate = NULL;
		}
	}
	else
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
	return tempstate;
}

SSL_Record_Layer::SSL_Record_Layer(MessageHandler* msg_handler, ServerName* hostname, unsigned short portnumber,
								   BOOL do_record_splitting)
								   : ProtocolComm(msg_handler, NULL, NULL)
{
	InternalInit(hostname, portnumber, do_record_splitting);
}

void SSL_Record_Layer::InternalInit(ServerName* hostname, unsigned short portnumber, BOOL do_record_splitting)
{
	m_last_socket_operation = LAST_SOCKET_OPERATION_NONE;
	Handling_callback = 0;
	buffered_amount_raw_data = 0;
	servername = hostname;
	port = portnumber;
	server_info = servername->GetSSLSessionHandler(port);
	networkbuffer_in = /*networkbuffer_out= */ NULL;
	connstate = pending_connstate = NULL;
	trying_to_reconnect = FALSE;
	setting_up_records = FALSE;
	statusinfo.ReadingData = FALSE;
	statusinfo.AllowApplicationDataSplitting = do_record_splitting;
	statusinfo.CurrentlyProcessingOutgoing = FALSE;
	statusinfo.WriteRecordsToOutBuffer = FALSE;
	m_AllDoneMsgMode = m_AllDone_requestMsgMode = NO_STATE;
	Init();

	if(!g_ssl_api->CheckSecurityManager())
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return;
	}
	else
	{
		SSL_AlertDescription des = SSL_No_Description;
		if(g_securityManager->ComponentsLacking)
			des = SSL_Components_Lacking;
		else if(!g_securityManager->SecurityEnabled)
			des = SSL_Security_Disabled;

		if(des != SSL_No_Description)
		{
			RaiseAlert(SSL_Internal, des);
			return;
		}
	}

	connstate = Prepare_connstate();
	pending_connstate = Prepare_connstate();

	static const OpMessage msglist[] =
	{
		MSG_SSL_ENCRYPTION_STEP,
		//MSG_SSL_DECRYPTION_STEP,
		MSG_SSL_DATA_READY
	};

	OpStatus::Ignore(mh->SetCallBackList(this, Id(), msglist, ARRAY_SIZE(msglist)));
}

CommState SSL_Record_Layer::ConnectionEstablished()
{
	SetProcessingInputData(TRUE);
	return COMM_LOADING;
}

SSL_Record_Layer::~SSL_Record_Layer()
{
	InternalDestruct();
}

void SSL_Record_Layer::InternalDestruct()
{
	FlushBuffers();

	OP_DELETE(loading_record);
	loading_record = NULL;

	OP_DELETE(plain_record);
	plain_record = NULL;

	OP_DELETE(sending_record);
	sending_record = NULL;

	OP_DELETEA(networkbuffer_in);
	networkbuffer_in = NULL;

	OP_DELETE(connstate);
	connstate = NULL;

	OP_DELETE(pending_connstate);
	pending_connstate = NULL;

	if(g_windowManager != NULL && mh != NULL)
		mh->UnsetCallBacks(this);
	if(g_main_message_handler)
		g_main_message_handler->UnsetCallBacks(this);
	//PrintfTofile("ssltrans.txt"," ID %d : destroyed %p %p %p\n", Id(), windowManager, mh, g_main_message_handler);
}

void SSL_Record_Layer::ResumeApplicationData()
{
	statusinfo.ApplicationActive = TRUE;
	statusinfo.PerformApplicationDataSplit = statusinfo.AllowApplicationDataSplitting && CipherSuitRequiresRecordSplitting();
	StartEncrypt();
	//g_main_message_handler->PostMessage(MSG_SSL_ENCRYPTION_STEP, Id(), 0);
}

void SSL_Record_Layer::StartingToSetUpRecord(BOOL flag, BOOL discard_buffered_records)
{
	if(discard_buffered_records)
	{
		unprocessed_application_data.Clear();
		out_buffer.Resize(0);

		DataStream_GenericRecord_Small *tempbuffer, *temp;

		temp = unprocessed_ssl_messages_prioritized.First();
		while(temp)
		{
			tempbuffer = temp;
			temp = temp->Suc();

			if((SSL_ContentType) tempbuffer->GetTag() != SSL_AlertMessage)
			{
				tempbuffer->Out();
				OP_DELETE(tempbuffer);
			}
		}

	}
	setting_up_records = flag;
	if(!flag)
	{
		StartEncrypt();
	}
}

void SSL_Record_Layer::ForceFlushPrioritySendQueue()
{
	while((!unprocessed_ssl_messages_prioritized.Empty() || sending_record) &&
		!(ProtocolComm::Closed() || ProtocolComm::PendingClose()) && 
		GetOPStatus()!=OpStatus::ERR_NO_MEMORY)
		EventEncrypt();
}

void SSL_Record_Layer::SetAllSentMsgMode(ProgressState parm, ProgressState request_parm)
{
	m_AllDoneMsgMode = parm;
	m_AllDone_requestMsgMode = (m_AllDoneMsgMode != NO_STATE ? request_parm : NO_STATE);
	if(m_AllDoneMsgMode != NO_STATE && statusinfo.ApplicationActive && unprocessed_application_data.Empty())
	{
		ProtocolComm::SetAllSentMsgMode(m_AllDoneMsgMode, m_AllDone_requestMsgMode);
		m_AllDoneMsgMode = m_AllDone_requestMsgMode = NO_STATE;
	}
}

void SSL_Record_Layer::RemoveURLReferences()
{
	if (pending_connstate)
	{
		URL emptyURL;
		pending_connstate->ActiveURL = emptyURL;
		if (pending_connstate->key_exchange)
			pending_connstate->key_exchange->SetDisplayURL(emptyURL);
	}
}

#endif // _NATIVE_SSL_SUPPORT_
