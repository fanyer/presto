/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/quick/dialogs/BTStartDownloadDialog.h"

#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/url2.h"

#include "adjunct/bittorrent/bt-util.h"
#include "adjunct/bittorrent/bt-download.h"
#include "adjunct/bittorrent/bt-tracker.h"
#include "adjunct/bittorrent/bt-benode.h"
#include "adjunct/bittorrent/bt-info.h"
#include "adjunct/bittorrent/dl-base.h"
#include "adjunct/bittorrent/bt-globals.h"
#include "adjunct/bittorrent/connection.h"
#include "adjunct/bittorrent/bt-packet.h"

void BTInfo::LoadTorrentFile(OpTransferItem * transfer_item)
{
	m_info_impl->LoadTorrentFile(transfer_item);
}

BOOL BTInfo::LoadTorrentFile(OpString& filename)
{
	return m_info_impl->LoadTorrentFile(filename);
}

void BTInfo::OnTorrentAvailable(URL& url)
{
	OpString loc;
	OpString tmp_storage;
	loc.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage));

	if(loc.IsEmpty())
	{
		/* The return value of the previous GetFolderPathIgnoreErrrors
		 * call is no longer in use, so I can reuse tmp_storage.
		 */
		loc.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage));
	}

	BTStartDownloadDialog *dlg = OP_NEW(BTStartDownloadDialog, ());

	if (dlg)
	{
		BTDownloadInfo *info = OP_NEW(BTDownloadInfo, ());
		if (info)
		{
			info->savepath.TakeOver(loc);
			info->url = url;

			OpString urlname;
			url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped, urlname);
			info->loadpath.Set(urlname);

			info->btinfo = this;

			if (OpStatus::IsSuccess(dlg->Init(g_application->GetActiveDesktopWindow(), info)))
				return;

			OP_DELETE(info);
		}

		OP_DELETE(dlg);

	}
}

void BTInfo::Cancel(BTDownloadInfo * btdinfo)
{
	OP_DELETE(btdinfo);
	//?OnFinished();
}

void BTInfo::InitiateDownload(BTDownloadInfo * btdinfo)
{
	m_info_impl->InitiateDownload(btdinfo);
}

void BTInfoImpl::InitiateDownload(BTDownloadInfo * btdinfo)
{
	// the torrent file:
	BOOL has_more = TRUE;

	m_pSource.Empty();
	URL_DataDescriptor *data_descriptor;

	data_descriptor = btdinfo->url.GetDescriptor(NULL, TRUE, TRUE);

	if(!data_descriptor)
	{
		OP_DELETE(btdinfo);
		return;
	}

	OP_STATUS err = OpStatus::OK;

	while(has_more)
	{
        OP_MEMORY_VAR unsigned long source_len = 0;
		TRAP(err, source_len = data_descriptor->RetrieveDataL(has_more));
		if (OpStatus::IsError(err))
			break;

		unsigned char *src = (unsigned char *)data_descriptor->GetBuffer();
		if (src == NULL || source_len == 0)
			break;

		m_pSource.Append(src, source_len);
		data_descriptor->ConsumeData(source_len);
	}
	OP_DELETE(data_descriptor);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(btdinfo);
		return;
	}

	// where to put the result
	OpString loc;
	loc.TakeOver(btdinfo->savepath);
	if (loc.Length() && loc[loc.Length()-1] != PATHSEPCHAR)
		loc.Append(PATHSEP);

	g_folder_manager->SetFolderPath(OPFILE_SAVE_FOLDER, loc.CStr());

	m_targetdirectory.TakeOver(loc);

	OpString torrentfile;
	torrentfile.Set(m_targetdirectory);

	OpString urlname;
	btdinfo->url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped, urlname);
	int idx = urlname.FindLastOf('/');

	if(idx != -1)
	{
		if (torrentfile.Length() && torrentfile[torrentfile.Length()-1] != PATHSEPCHAR)
			torrentfile.Append(PATHSEP);

		torrentfile.Append(urlname.CStr() + idx+1);
	}
	m_savefilename.TakeOver(torrentfile);

	m_sLoadedFromPath.Set(urlname);
	if(LoadTorrentBuffer(&m_pSource))
	{
		AddTorrentToDownload();
	}

	btdinfo->url.ForceStatus(URL_UNLOADED);

	OP_DELETE(btdinfo);

	if (m_listener)
		m_listener->OnFinished();
}

//////////////////////////////////////////////////////////////////////
// BTInfoImpl construction

BTInfoImpl::BTInfoImpl()
:	m_bValid(FALSE),
	m_bDataSHA1(FALSE),
	m_nTotalSize(0),
	m_nBlockSize(0),
	m_nBlockCount(0),
	m_pBlockSHA1(NULL),
	m_nFiles(0),
	m_pFiles(NULL),
	m_private_torrent(FALSE),
	m_nTestByte(0),
	m_attachtodownload(NULL),
	m_multi_tracker(NULL),
	m_listener(NULL)
{
	BT_RESOURCE_ADD("BTInfoImpl", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTInfoImpl::~BTInfoImpl()"
#endif

BTInfoImpl::~BTInfoImpl()
{
	Clear();

	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

BTInfoImpl::BTFile::BTFile()
{
	m_nSize = 0;
	m_bSHA1 = FALSE;
	m_nOffset = 0;

	BT_RESOURCE_ADD("BTInfoImpl::BTFile", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTInfoImpl::BTFile::~BTFile()"
#endif

BTInfoImpl::BTFile::~BTFile()
{
	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// BTInfo clear

void BTInfoImpl::Clear()
{
	OP_DELETEA(m_pBlockSHA1);
	OP_DELETEA(m_pFiles);
	OP_DELETE(m_multi_tracker);

	m_bValid		= FALSE;
	m_nTotalSize	= 0;
	m_nBlockSize	= 0;
	m_nBlockCount	= 0;
	m_pBlockSHA1	= NULL;
	m_nFiles		= 0;
	m_pFiles		= NULL;
	m_sName.Empty();
}

//////////////////////////////////////////////////////////////////////
// BTInfo copy

void BTInfoImpl::Copy(BTInfoImpl* pSource)
{
	OP_ASSERT(pSource != NULL);
	Clear();

	m_bValid		= pSource->m_bValid;
	m_pInfoSHA1		= pSource->m_pInfoSHA1;
	m_bDataSHA1		= pSource->m_bDataSHA1;
	m_pDataSHA1		= pSource->m_pDataSHA1;
	m_nTotalSize	= pSource->m_nTotalSize;
	m_nBlockSize	= pSource->m_nBlockSize;
	m_nBlockCount	= pSource->m_nBlockCount;
	m_sName.Set(pSource->m_sName);
	m_nFiles		= pSource->m_nFiles;
	m_sLoadedFromPath.Set(pSource->m_sLoadedFromPath);
	m_pSource.Append(pSource->m_pSource.Buffer(), pSource->m_pSource.DataSize());
	pSource->GetTargetDirectory(m_targetdirectory);
	m_savefilename.Set(pSource->m_savefilename);
	m_private_torrent = pSource->m_private_torrent;

	if(pSource->m_pBlockSHA1 != NULL)
	{
		m_pBlockSHA1 = OP_NEWA(SHAStruct, m_nBlockCount);
		if (m_pBlockSHA1)
			memcpy(m_pBlockSHA1, pSource->m_pBlockSHA1,	sizeof(SHAStruct) * (UINT32)m_nBlockCount);
		else
			m_nBlockCount = 0;
	}

	if(pSource->m_pFiles != NULL)
	{
		m_pFiles = OP_NEWA(BTFile, m_nFiles);
		if (m_pFiles)
			for (int nFile = 0 ; nFile < m_nFiles ; nFile++)
				m_pFiles[ nFile ].Copy(&pSource->m_pFiles[ nFile ]);
		else
			m_nFiles = 0;

	}
	m_multi_tracker = pSource->GetTracker()->Copy();
}

//////////////////////////////////////////////////////////////////////
// BTInfo::BTFile copy

void BTInfoImpl::BTFile::Copy(BTFile* pSource)
{
	m_sPath.Append(pSource->m_sPath);
	m_nSize = pSource->m_nSize;
	m_nOffset = pSource->m_nOffset;
	m_bSHA1 = pSource->m_bSHA1;
	m_pSHA1 = pSource->m_pSHA1;
}

void BTInfoImpl::LoadTorrentFile(OpTransferItem * transfer_item)
{
	m_pSource.Empty();

	OpString filename;
	((TransferItem *)transfer_item)->GetMetaFile(filename);

	if(LoadTorrentFile(filename))
	{
		AddTorrentToDownload();
		if(m_listener)
		{
			m_listener->OnFinished();
		}
	}
}

//////////////////////////////////////////////////////////////////////
// BTInfo load .torrent file

BOOL BTInfoImpl::LoadTorrentFile(OpString& filename)
{
	OpFile file;
	OpFileLength nLength;

    if ((file.Construct(filename.CStr()) != OpStatus::OK) ||
         (file.Open(OPFILE_READ) != OpStatus::OK))
    {
        return FALSE;
    }

	if (OpStatus::IsSuccess(file.GetFileLength(nLength)) && nLength > 0)
	{
		if (nLength < 20 * 1024 * 1024)
		{
			unsigned char *buf = OP_NEWA(unsigned char, (int)nLength);
			if (!buf)
				return FALSE;
			OpFileLength bytes_read = 0;
			if(file.Read(buf, nLength, &bytes_read) != OpStatus::OK)
			{
				OP_DELETEA(buf);
				return FALSE;
			}
			m_pSource.Empty();
			m_pSource.Append(buf, (UINT)nLength);

			OP_DELETEA(buf);

			return LoadTorrentBuffer(&m_pSource);
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// BTInfo save .torrent file

BOOL BTInfoImpl::SaveTorrentFile(OpString& targetdirectory)
{
	if(!IsAvailable()) return FALSE;
	if(m_pSource.DataSize() == 0) return FALSE;

	OP_STATUS status = OpStatus::ERR;
	OpFile file;
	OpString path;
	OpString name;
	int seppos;

	path.Set(targetdirectory);

	seppos = path.FindLastOf(PATHSEPCHAR);
	if(seppos != KNotFound)
	{
		if(seppos != path.Length() - 1)
		{
			path.Append(PATHSEP);
		}
	}

	GetName(name);
	path.Append(name);
	path.Append(".torrent");

	status = file.Construct(path.CStr());

	file.Open(OPFILE_SHAREDENYWRITE | OPFILE_READ);

	if(!file.IsOpen())
	{
		status = file.Open(OPFILE_SHAREDENYWRITE | OPFILE_OVERWRITE);

		if(file.IsOpen())
		{
			if(file.Write(m_pSource.Buffer(), m_pSource.DataSize()) != OpStatus::OK)
			{
				status = OpStatus::ERR;
			}
			else
			{
				m_savefilename.Set(path);
			}
			file.Close();
		}
	}
	else
	{
		m_savefilename.Set(path);
		file.Close();
	}
	return status == OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// BTInfo load torrent info from buffer

BOOL BTInfoImpl::LoadTorrentBuffer(OpByteBuffer* pBuffer)
{
	BENode* pNode = BENode::Decode(pBuffer);
	if (pNode == NULL) return FALSE;
	BOOL bSuccess = LoadTorrentTree(pNode);
	OP_DELETE(pNode);
	return bSuccess;
}

void BTInfoImpl::GetInfoHash(SHAStruct *pSHA1)
{
	memcpy(pSHA1, &m_pInfoSHA1, sizeof(SHAStruct));
}

//////////////////////////////////////////////////////////////////////
// BTInfo load torrent info from tree

BOOL BTInfoImpl::LoadTorrentTree(BENode* pRoot)
{
	Clear();

	if(!pRoot->IsType(BENode::beDict))
	{
		return FALSE;
	}
	m_multi_tracker = OP_NEW(BTMultiTracker, ());
	if(!m_multi_tracker)
	{
		return FALSE;
	}

	BENode* pAnnounce;

	pAnnounce = pRoot->GetNode("announce-list");
	if(pAnnounce)
	{
		DEBUGTRACE_TORRENT(UNI_L("BT: announce-list:\n"), 0);

		// multitracker
		if (pAnnounce->IsType(BENode::beList))
		{
			int cnt;

			for(cnt = 0; cnt < pAnnounce->GetCount(); cnt++)
			{
				BENode* tracker = pAnnounce->GetNode(cnt);
				if(tracker)
				{
					OpString tracker_url;
					BTTrackerGroup *tracker_group = m_multi_tracker->CreateTrackerGroup();

					if(tracker->IsType(BENode::beString))
					{
						tracker->GetStringFromUTF8(tracker_url);

						DEBUGTRACE_TORRENT(UNI_L("%s\n"), (uni_char *)tracker_url.CStr());

						if(tracker_group)
						{
							if(OpStatus::IsError(tracker_group->CreateTrackerEntry(tracker_url)))
							{
								return FALSE;
							}
						}
					}
					else if(tracker->IsType(BENode::beList))
					{
						int cnt2;

						for(cnt2 = 0; cnt2 < tracker->GetCount(); cnt2++)
						{
							BENode* tracker_node = tracker->GetNode(cnt2);
							if(tracker_node)
							{
								if(tracker_node->IsType(BENode::beString))
								{
									tracker_node->GetStringFromUTF8(tracker_url);

									if(tracker_group)
									{
										DEBUGTRACE_TORRENT(UNI_L("  %s\n"), (uni_char *)tracker_url.CStr());

										if(OpStatus::IsError(tracker_group->CreateTrackerEntry(tracker_url)))
										{
											return FALSE;
										}
									}
								}
								else
								{
									OP_ASSERT(FALSE);
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		pAnnounce = pRoot->GetNode("announce");

		if (pAnnounce->IsType(BENode::beString))
		{
			OpString tracker_url;
			BTTrackerGroup *tracker_group = m_multi_tracker->CreateTrackerGroup();
			if(!tracker_group)
			{
				return FALSE;
			}
			pAnnounce->GetStringFromUTF8(tracker_url);

			if(OpStatus::IsError(tracker_group->CreateTrackerEntry(tracker_url)))
			{
				return FALSE;
			}
			DEBUGTRACE_TORRENT(UNI_L("BT: announce: %s\n"), tracker_url.CStr());

//			if (m_sTracker.Find(UNI_L("http")) != 0)
//			{
//				m_sTracker.Empty();
//				return FALSE;
//			}
		}
	}
	BENode* pInfo = pRoot->GetNode("info");
	if (! pInfo->IsType(BENode::beDict))
	{
		return FALSE;
	}
	BENode* pName = pInfo->GetNode("name.utf-8");  // prefer utf-8 if it exists: this is a hack for erroneously generated content.
	if (!pName) pName = pInfo->GetNode("name");
	if (pName->IsType(BENode::beString))
	{
		pName->GetStringFromUTF8(m_sName);
		DEBUGTRACE_TORRENT(UNI_L("BT: name: %s\n"), m_sName.CStr());
	}
	BENode *pPrivate = pInfo->GetNode("private");
	if (pPrivate->IsType(BENode::beInt))
	{
		int priv = (int)pPrivate->GetInt();
		DEBUGTRACE_TORRENT(UNI_L("BT: private: %d\n"), priv);
		if(priv)
		{
			m_private_torrent = TRUE;
		}
		else
		{
			m_private_torrent = FALSE;
		}
	}
	else
	{
		m_private_torrent = FALSE;
	}

    if (m_sName.IsEmpty())
	{
		OpString8 str;
		str.AppendFormat("Unnamed_Torrent_%i", (int)rand());
		m_sName.Append(str);
	}
	BENode* pPL = pInfo->GetNode("piece length");
	if(!pPL->IsType(BENode::beInt))
	{
		return FALSE;
	}
	m_nBlockSize = (UINT32)pPL->GetInt();
	if(!m_nBlockSize)
	{
		return FALSE;
	}
	DEBUGTRACE_TORRENT(UNI_L("BT: blocksize: %d\n"), m_nBlockSize);

	BENode* pHash = pInfo->GetNode("pieces");
	if (! pHash->IsType(BENode::beString)) return FALSE;
	if (pHash->GetStringLength() % sizeof(SHAStruct)) return FALSE;
	m_nBlockCount = (UINT32)(pHash->GetStringLength() / sizeof(SHAStruct));
	if (! m_nBlockCount || m_nBlockCount > 209716) return FALSE;

	DEBUGTRACE_TORRENT(UNI_L("BT: pieces: %d\n"), m_nBlockCount);

	m_pBlockSHA1 = OP_NEWA(SHAStruct, m_nBlockCount);

	if (m_pBlockSHA1)
	{
		for (UINT32 nBlock = 0 ; nBlock < m_nBlockCount ; nBlock++)
		{
			SHAStruct* pSource = (SHAStruct*)pHash->GetString();
			memcpy(m_pBlockSHA1 + nBlock, pSource + nBlock, sizeof(SHAStruct));
		}
	}
	else
		m_nBlockCount = 0;

	BENode* pSHA1 = pInfo->GetNode("sha1");
	if (pSHA1)
	{
		if (! pSHA1->IsType(BENode::beString) || pSHA1->GetStringLength() != sizeof(SHAStruct)) return FALSE;
		m_bDataSHA1 = TRUE;
		memcpy(&m_pDataSHA1, pSHA1->GetString(), sizeof(SHAStruct));
	}

	BENode* pFiles = pInfo->GetNode("files");
	BENode* pLength = pInfo->GetNode("length");
	if(pLength)
	{
		if (! pLength->IsType(BENode::beInt)) return FALSE;
		m_nTotalSize = pLength->GetInt();
		if (! m_nTotalSize) return FALSE;

		DEBUGTRACE_TORRENT(UNI_L("BT: length: %d\n"), m_nTotalSize);

		m_pFiles = OP_NEWA(BTFile, 1);
		if (m_pFiles)
		{
			m_nFiles = 1;
			m_pFiles[0].m_sPath.Append(m_sName);
			m_pFiles[0].m_nSize = m_nTotalSize;
			m_pFiles[0].m_nOffset = 0;
			m_pFiles[0].m_bSHA1 = m_bDataSHA1;
			m_pFiles[0].m_pSHA1 = m_pDataSHA1;
		}
	}
	else if (pFiles)
	{
		if (! pFiles->IsType(BENode::beList)) return FALSE;
		m_nFiles = pFiles->GetCount();
		if (! m_nFiles || m_nFiles > 8192) return FALSE;

		m_pFiles = OP_NEWA(BTFile, m_nFiles);
		if (!m_pFiles)
			m_nFiles = 0;

		DEBUGTRACE_TORRENT(UNI_L("BT: files: %d\n"), m_nFiles);

		m_nTotalSize = 0;
		UINT64 offset = 0;

		for (int nFile = 0 ; nFile < m_nFiles ; nFile++)
		{
			BENode* pFile = pFiles->GetNode(nFile);
			if (! pFile->IsType(BENode::beDict)) return FALSE;

			BENode* pLength = pFile->GetNode("length");
			if (! pLength->IsType(BENode::beInt)) return FALSE;
			m_pFiles[ nFile ].m_nSize = pLength->GetInt();
			m_pFiles[ nFile ].m_nOffset = offset;

			BENode* pPath = pFile->GetNode("path.utf-8"); // prefer utf-8 if it exists: this is a hack for erroneously generated content.
			if (!pPath) pPath = pFile->GetNode("path");
			if (! pPath->IsType(BENode::beList)) return FALSE;
			if (pPath->GetCount() > 32) return FALSE;

			// Hack to prefix all
			m_pFiles[ nFile ].m_sPath.Set(m_sName);

			for (int nPath = 0 ; nPath < pPath->GetCount() ; nPath++)
			{
				BENode* pPart = pPath->GetNode(nPath);
				if (! pPart->IsType(BENode::beString)) return FALSE;

				if (m_pFiles[ nFile ].m_sPath.Length())
					m_pFiles[ nFile ].m_sPath.Append(PATHSEP);

				OpString strTmp;

				pPart->GetStringFromUTF8(strTmp);

//				BTInfo::SafeFilename(strTmp);

				m_pFiles[ nFile ].m_sPath.Append(strTmp);

				DEBUGTRACE_TORRENT(UNI_L("BT: filename: %s "), m_pFiles[ nFile ].m_sPath.CStr());
				DEBUGTRACE_TORRENT(UNI_L("(%d)\n"), m_pFiles[ nFile ].m_nSize);

			}
			if (BENode* pSHA1 = pFile->GetNode("sha1"))
			{
				if (! pSHA1->IsType(BENode::beString) || pSHA1->GetStringLength() != sizeof(SHAStruct)) return FALSE;
				m_pFiles[ nFile ].m_bSHA1 = TRUE;
				memcpy(&m_pFiles[ nFile ].m_pSHA1, pSHA1->GetString(), sizeof(SHAStruct));
			}
			m_nTotalSize += m_pFiles[ nFile ].m_nSize;
			offset += m_pFiles[ nFile ].m_nSize;
		}
	}
	else
	{
		return FALSE;
	}

	if((m_nTotalSize + m_nBlockSize - 1) / m_nBlockSize != m_nBlockCount)
		return FALSE;

	if(!CheckFiles()) return FALSE;

	pInfo->GetSHA1(&m_pInfoSHA1);

	m_bValid = TRUE;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// BTInfo load torrent info from tree

BOOL BTInfoImpl::CheckFiles()
{
	OpString * pszPath;
	for (int nFile = 0 ; nFile < m_nFiles ; nFile++)
	{
		m_pFiles[ nFile ].m_sPath.Strip(TRUE, TRUE);

		pszPath = &(m_pFiles[ nFile ].m_sPath);

		if(pszPath->IsEmpty()) return FALSE;
		if(pszPath->Find(":") != KNotFound)  return FALSE;
		if(pszPath->Compare("\\", 1) == 0 || pszPath->Compare("/", 1) == 0) return FALSE;
		if(pszPath->Find("..\\") != KNotFound) return FALSE;
		if(pszPath->Find("../") != KNotFound) return FALSE;
	}
	return(m_nFiles > 0);
}

//////////////////////////////////////////////////////////////////////
// BTInfo block testing

void BTInfoImpl::BeginBlockTest()
{
	OP_ASSERT(IsAvailable());
	OP_ASSERT(m_pBlockSHA1 != NULL);

	m_pTestSHA1.Reset();
	m_nTestByte = 0;
}

void BTInfoImpl::AddToTest(void *pInput, UINT32 nLength)
{
	if (nLength == 0) return;

	OP_ASSERT(IsAvailable());
	OP_ASSERT(m_pBlockSHA1 != NULL);
	OP_ASSERT(m_nTestByte + nLength <= m_nBlockSize);

	m_pTestSHA1.Add(pInput, nLength);
	m_nTestByte += nLength;
}

BOOL BTInfoImpl::FinishBlockTest(UINT32 nBlock)
{
	OP_ASSERT(IsAvailable());
	OP_ASSERT(m_pBlockSHA1 != NULL);

	if (nBlock >= m_nBlockCount) return FALSE;

	SHAStruct pSHA1;

	m_pTestSHA1.Finish();
	m_pTestSHA1.GetHash(&pSHA1);
	return (BOOL)(pSHA1 == m_pBlockSHA1[ nBlock ]);
}

void BTInfoImpl::Escape(SHAStruct *pSHA1, OpString8& strEscaped)
{
	static const char *pszHex = "0123456789ABCDEF";
	char tmpbuf[sizeof(SHAStruct) * 3 + 1];	// max used if every character needs to be escaped + 1 byte for NUL terminator
	char *psz = (char *)tmpbuf;

	for (unsigned int nByte = 0 ; nByte < sizeof(SHAStruct) ; nByte++)
	{
		UINT32 nValue = (UINT32)pSHA1->n[ nByte ];

		if ((nValue >= '0' && nValue <= '9') ||
			(nValue >= 'a' && nValue <= 'z') ||
			(nValue >= 'A' && nValue <= 'Z'))
		{
			*psz++ = (char)nValue;
		}
		else
		{
			*psz++ = '%';
			*psz++ = pszHex[ (nValue >> 4) & 15 ];
			*psz++ = pszHex[ nValue & 15 ];
		}
	}

	*psz = 0;
	strEscaped.Set((char *)tmpbuf);
}

OP_STATUS BTInfoImpl::AddTorrentToDownload()
{
	if(m_attachtodownload != NULL)
	{
		BTDownload *dl = (BTDownload *)m_attachtodownload;

		if(dl->SetTorrent(this) == OpStatus::OK)
		{
			BOOL fail;

			dl->FileDownloadBegin(fail);
			dl->CheckExistingFile();
		}
	}
	else
	{
		SHAStruct SHA1;

		GetInfoHash(&SHA1);

		BTDownload *download = (BTDownload *)g_Downloads->FindByBTH(&SHA1);

		if(download == NULL)
		{
			download = OP_NEW(BTDownload, ());

			if(download != NULL)
			{
				if(download->SetTorrent(this) == OpStatus::OK)
				{
					BOOL fail;

					g_Downloads->Add(download);

					download->FileDownloadBegin(fail);
					download->CheckExistingFile();
				}
				else
				{
					OP_DELETE(download);
				}
			}
		}
		else
		{
//			download->
		}
	}
	return OpStatus::OK;
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
