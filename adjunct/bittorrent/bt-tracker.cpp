/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
//#include "irc-message.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/misc.h"

#include "modules/encodings/detector/charsetdetector.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/url2.h"
#include "modules/url/tools/url_util.h"

#include "adjunct/bittorrent/bt-util.h"
#include "adjunct/bittorrent/bt-benode.h"
#include "adjunct/bittorrent/bt-info.h"
#include "adjunct/bittorrent/bt-download.h"
#include "adjunct/bittorrent/bt-tracker.h"
#include "adjunct/bittorrent/bt-client.h"

extern OpString* GetSystemIp(OpString* pIp);

//********************************************************************
//
//	BTTracker
//
//********************************************************************

/*
http://www.eurotorrents.net/announce.php?passkey=358208e93cf3e5a440e0a799f0eb3105?
info_hash=%25%A2%29%8Aty%C3%F5%8B%28%94%00%C2%2AMq%60M9%A7
peer_id=%2DOP76%2D71fe2a14d0e75f
port=6881
uploaded=0
downloaded=0
left=123
compact=1
ip=10.20.24.74
event=started
*/

// tracker contructor
BTTracker::BTTracker()
:   // m_feed_id ?
    m_download(NULL),
    // m_process
    // m_scrape_request
    // m_shutting_down
	m_ref_count(0),
	m_tracker_error(FALSE),
	m_transfer_item(NULL)
{
	BT_RESOURCE_ADD("BTTracker", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "BTTracker::~BTTracker()"
#endif

BTTracker::~BTTracker()
{
//	if(!m_shutting_down && m_download)
//	{
//		m_download->ClientConnectors()->Remove(this);
//	}
	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

OP_STATUS BTTracker::Connect(BTDownload *download, OpString& tracker_url, const char *verb, BOOL process, BOOL scrape_request, BOOL shutting_down)
{
	OpString tracker;
	OpString8 strBTH, strPeerID, tmp_tracker;

	m_scrape_request = scrape_request;
	m_shutting_down = shutting_down;
	m_tracker.Set(tracker_url);
	m_feed_id = -1;
	m_process = process;
	m_download = download;

	BTInfoImpl::Escape(&m_download->m_pBTH, strBTH);
	BTInfoImpl::Escape(&m_download->m_peerid, strPeerID);

	tmp_tracker.Set(m_tracker.CStr());

	if(scrape_request)
	{
		int idx = tmp_tracker.FindLastOf('/');

		if(idx != KNotFound)
		{
			idx++;
			OpString scrape_url;
			scrape_url.Set(tmp_tracker.CStr(), idx);

			OpStringC8 announce(tmp_tracker.CStr() + idx);

			if(announce.CompareI("announce", 8) == 0)
			{
				scrape_url.Append("scrape");
				scrape_url.Append(tmp_tracker.CStr() + idx+8);

				OpString8 strURL;
				strURL.AppendFormat("?info_hash=%s",
									strBTH.CStr());

				scrape_url.Append(strURL);

				tracker.TakeOver(scrape_url);

				DEBUGTRACE_TRACKER(UNI_L("** SCRAPE: %s\n"), tracker.CStr());
			}
		}
	}
	else
	{
		char ubuffer[32];
		char dbuffer[32];
		char rbuffer[32];
		OpString8 temp_start;
		ANCHOR(OpString8, temp_start);

		RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString((OpFileLength)m_download->m_torrentUploaded, &temp_start));
		op_strncpy(ubuffer, temp_start.CStr(), sizeof(ubuffer));

		RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString((OpFileLength)m_download->m_downloaded, &temp_start));
		op_strncpy(dbuffer, temp_start.CStr(), sizeof(dbuffer));

		RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString((OpFileLength)m_download->GetVolumeRemaining(), &temp_start));
		op_strncpy(rbuffer, temp_start.CStr(), sizeof(rbuffer));

		OpString8 strURL;
		OpString8 delimiter;

		delimiter.Set("?");

		if(op_strstr(tmp_tracker.CStr(), "?"))
		{
			delimiter.Set("&");
		}

		strURL.AppendFormat("%s%sinfo_hash=%s&peer_id=%s&port=%i&uploaded=%s&downloaded=%s&left=%s&key=%s",
							tmp_tracker.CStr(),
 							delimiter.CStr(),
							strBTH.CStr(),
							strPeerID.CStr(),
							g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort),
							ubuffer,
							dbuffer,
							rbuffer,
							strPeerID.CStr());

		tracker.Append(strURL);

		tracker.Append("&compact=1&numwant=200");

		// one of: "started" (initially sent), "completed", "stopped" or not sent at all on regular
		// updates
		if(verb != NULL )
		{
			tracker.Append("&event=");
			tracker.Append(verb);
		}
		DEBUGTRACE_TRACKER(UNI_L("* TRACKER: %s\n"), tracker.CStr());
	}
	m_tracker.TakeOver(tracker);

	if(m_shutting_down)
	{
		m_download = NULL;
	}
//	else
//	{
//		m_download->ClientConnectors()->Add(this);
//	}

	return Connect(m_tracker, m_feed_id);
}

void BTTracker::Process(BOOL request, OpString& buffer)
{
	OpByteBuffer bytebuf;
	OpString8 tmpbuf;

	tmpbuf.Set(buffer.CStr());

	bytebuf.Append((unsigned char *)tmpbuf.CStr(), tmpbuf.Length());

	Process(request, &bytebuf);
}

void BTTracker::Process(BOOL request, OpByteBuffer *buffer)
{
	if(m_download == NULL || m_shutting_down)
	{
		// we're shutting down, so we don't need to the result
		return;
	}
	if(!g_Downloads->Check(m_download)) return;
	if(m_download->m_tracker_state == BTTracker::TrackerNone) return;

	if(!request)
	{
		m_download->OnTrackerEvent(FALSE, m_tracker);
		return;
	}

	BENode* root = BENode::Decode(buffer);

	if(root->IsType(BENode::beDict))
	{
		if(Process(root))
		{
			if(!m_download->m_trackerError)
				m_download->OnTrackerEvent(TRUE, m_tracker);
		}
		else
		{
			// parse error
			m_download->OnTrackerEvent(FALSE, m_tracker);
		}
	}
	else if(root->IsType(BENode::beString))
	{
		// error
		char *str = root->GetString();

		m_download->OnTrackerEvent(FALSE, m_tracker, str);
	}
	else
	{
		// parse error
		OpString8 str;

		str.Append((char *)buffer->Buffer(), buffer->DataSize());

		m_download->OnTrackerEvent(FALSE, m_tracker, str.CStr());
		m_tracker_error = TRUE;
	}
	OP_DELETE(root);
}

BOOL BTTracker::Process(BENode *root)
{
	if(m_download == NULL || m_shutting_down)
	{
		return TRUE;
	}
	BENode* error = root->GetNode("failure reason");
	if(error != NULL)
	{
		// we have an error in the result from the tracker
		m_download->OnTrackerEvent(FALSE, m_tracker, error->GetString());

		m_download->m_torrentTracker = op_time(NULL) + 60;
		if(m_download->m_tracker_state < BTTracker::TrackerStart)
		{
			m_download->m_tracker_state = BTTracker::TrackerStart;
		}
		if(!m_scrape_request)
		{
			m_tracker_error = TRUE;
		}
		return TRUE;
	}
	if(m_scrape_request)
	{
		BENode *dict = root->GetNode("files");

		if(dict && dict->IsType(BENode::beDict))
		{
			BENode *stats = dict->GetNode(0);
			if(stats != NULL)
			{
				BENode *seeders = stats->GetNode("complete");
				if(seeders && seeders->IsType(BENode::beInt))
				{
					m_download->m_totalSeeders = (UINT32)seeders->GetInt();
				}

				BENode *totalpeers = stats->GetNode("incomplete");
				if(totalpeers && totalpeers->IsType(BENode::beInt))
				{
					m_download->m_totalPeers = (UINT32)totalpeers->GetInt();
				}
			}
		}
	}
	else
	{
		// Interval in seconds that the client should wait between
		// sending regular requests to the tracker
		BENode* interval = root->GetNode("interval");

		if(interval == NULL || !interval->IsType(BENode::beInt))
		{
			// wrong datatype, fail
			return FALSE;
		}
		UINT32 nInterval = (UINT32)interval->GetInt();

		// we really don't need to access the tracker more than every 2nd minute
		nInterval = max( nInterval, 120U );

		// but we don't want to wait too long between each request
		nInterval = min( nInterval, 3600U );

		m_download->m_torrentTracker = op_time(NULL) + nInterval;
		if(m_download->m_tracker_state < BTTracker::TrackerStart)
		{
			m_download->m_tracker_state = BTTracker::TrackerStart;
		}

		BENode *seeders = root->GetNode("complete");
		if(seeders && seeders->IsType(BENode::beInt))
		{
			m_download->m_totalSeeders = (UINT32)seeders->GetInt();
		}

		BENode *totalpeers = root->GetNode("incomplete");
		if(totalpeers && totalpeers->IsType(BENode::beInt))
		{
			m_download->m_totalPeers = (UINT32)totalpeers->GetInt();
		}
		OpString strSystemIP;

		// we don't want to add overself in the list of peers
		GetSystemIp(&strSystemIP);

		// get all peers
		BENode* pPeers = root->GetNode("peers");
		UINT32 nCount = 0;

		if(pPeers->IsType(BENode::beList))
		{
			for(INT32 nPeer = 0 ; nPeer < pPeers->GetCount() ; nPeer++)
			{
				BENode* pPeer = pPeers->GetNode( nPeer );
				if ( ! pPeer->IsType( BENode::beDict ) ) continue;

				BENode* pID = pPeer->GetNode( "peer id" );

				BENode* pIP = pPeer->GetNode( "ip" );
				if ( ! pIP->IsType( BENode::beString ) ) continue;

				BENode* pPort = pPeer->GetNode( "port" );
				if ( ! pPort->IsType( BENode::beInt ) ) continue;

				OpString addr, port, tmpport;
				OpString8 strPort;
				strPort.AppendFormat("%d", g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort));

				tmpport.Set(strPort);

				pIP->GetString(addr);
				pPort->GetString(port);

				if(strSystemIP.Compare(addr) == 0 && tmpport.Compare(port) == 0)
				{
					// it's us! Skip this one :-)
					continue;
				}
				DEBUGTRACE_TRACKER(UNI_L("ADDING PEER: %s\n"), addr.CStr());

				OpString dummy;

				if(pID->IsType( BENode::beString ) && pID->GetStringLength() == sizeof(SHAStruct))
				{
					if(m_download->AddSourceBT((SHAStruct *)pID->GetString(), addr, (WORD)pPort->GetInt(), FALSE, dummy))
					{
						nCount++;
					}
				}
				else
				{
					if(m_download->AddSourceBT(NULL, addr, (WORD)pPort->GetInt(), FALSE, dummy))
					{
						nCount++;
					}
				}
			}
		}
		else if(pPeers->IsType(BENode::beString))
		{
			// Indicates the client accepts a compact response. The peers list is replaced
			// by a peers string with 6 bytes per peer. The first four bytes are the host
			// (in network byte order), the last two bytes are the port (again in network byte order).

			OpString address;
			OpVector<BTCompactPeer> peers;
			UINT32 i;

			pPeers->GetString(address);

			RETURN_VALUE_IF_ERROR(m_download->StringToPeerlist(address, peers), FALSE);

			for(i = 0; i < peers.GetCount(); i++)
			{
				BTCompactPeer *peer = peers.Get(i);
				if(peer)
				{
					if(strSystemIP.Compare(peer->m_peer) == 0 && peer->m_port == (UINT32)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort))
					{
						// it's us! Skip this one :-)
						continue;
					}
					DEBUGTRACE_TRACKER(UNI_L("ADDING PEER: %s\n"), peer->m_peer.CStr());

					OpString dummy;

					if(m_download->AddSourceBT(NULL, peer->m_peer, (WORD)peer->m_port, FALSE, dummy))
					{
						nCount++;
					}
				}
			}
			peers.DeleteAll();
		}
	}
//	m_download->CheckExistingFile();
	m_download->StartRampUp();
	m_download->OnRun();
	return TRUE;
}

void BTTracker::StartClosing()
{
	if (m_transfer_item)
		((TransferManager*)g_transferManager)->ReleaseTransferItem(m_transfer_item);
}

OP_STATUS BTTracker::Connect(const OpStringC& feed_url, INT32& feed_id)
{
	OP_STATUS status;
	const static char useragent[] = "Opera BitTorrent";
	BOOL url_exists = FALSE;
	feed_id = -1;

	if (m_transfer_item)
		((TransferManager*)g_transferManager)->ReleaseTransferItem(m_transfer_item);

	status = g_transferManager->GetTransferItem(reinterpret_cast<OpTransferItem **>(&m_transfer_item), feed_url.CStr(), &url_exists);
	if (OpStatus::IsError(status))
	{
		m_tracker_error = TRUE;
		return status;
	}

	g_transferManager->AddTransferManagerListener(this);

	URL* url = m_transfer_item->GetURL();
	if(url_exists)
	{
		url->StopLoading(g_main_message_handler);
	}

	OP_ASSERT(url != 0);

	url->SetAttribute(URL::KDisableProcessCookies, TRUE);

	URL_Custom_Header header;

	header.name.Set("User-Agent");
	header.value.Set(useragent);

	TRAP(status, url->SetAttributeL(URL::KAddHTTPHeader, &header));

	MessageHandler *oldHandler = url->GetFirstMessageHandler();
	url->ChangeMessageHandler(oldHandler ? oldHandler : g_main_message_handler, g_main_message_handler);

	if(url->Status(0) == URL_UNLOADED)	//hack to work around failed second download.
	{
		URL tmp;
		url->Reload(g_main_message_handler, tmp, TRUE, FALSE);
	}
	m_transfer_item->SetTransferListener(this);
	m_feed_id = url->Id(FALSE);

	return OpStatus::OK;
}

void BTTracker::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	URL *url = transferItem->GetURL();
	const INT32 feed_id = url->Id();

	if(feed_id != m_feed_id)
	{
		return;
	}
	switch (status)
	{
		case TRANSFER_ABORTED : // Should not happen, but happens because of a bug in URL (supposedly)
		case TRANSFER_DONE :
			{
				BOOL has_more = TRUE;
				OpByteBuffer buffer;
				URL_DataDescriptor *data_descriptor = url->GetDescriptor(NULL, TRUE, TRUE);

				if(!data_descriptor)
				{
					if(m_download)
					{
						m_download->OnTrackerEvent(FALSE, m_tracker);
					}
					break;
				}
				m_tracker_error = FALSE;
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

					buffer.Append(src, source_len);
					data_descriptor->ConsumeData(source_len);
				}
				OP_DELETE(data_descriptor);
				if (OpStatus::IsError(err))
					break;

				if(buffer.DataSize() != 0)
				{
					if(buffer.Buffer()[0] == 'd' && buffer.Buffer()[1] == '\0')
					{
						OpString8 charset;

						charset.Set("utf16");

						OutputConverter *conv = NULL;
						OP_STATUS rc = OutputConverter::CreateCharConverter(charset.CStr(), &conv);

						if (OpStatus::IsSuccess(rc) && conv)
						{
							int read = 0, converted = 0;
							OpString opstr;
							char *tmpbuf = OP_NEWA(char, buffer.DataSize() + 1);

							if(tmpbuf == NULL)
							{
								break;
							}
							converted = conv->Convert(buffer.Buffer(), buffer.DataSize(), tmpbuf, buffer.DataSize(), &read);
							tmpbuf[converted] = '\0';

							buffer.Empty();

							buffer.Append((unsigned char *)tmpbuf, converted);

#ifdef _DEBUG
							OpString tmp;

							tmp.Set(tmpbuf);

							DEBUGTRACE8_TRACKER(UNI_L("**** TRACKER RESPONSE: %s\n"), tmp.CStr());
#endif
							OP_DELETEA(tmpbuf);

							Process(TRUE, &buffer);

							OP_DELETE(conv);
						}
					}
					else
					{
#ifdef _DEBUG
						OpString str;

						str.Append((char *)buffer.Buffer(), buffer.BufferSize());

						if(m_scrape_request)
						{
							DEBUGTRACE8_TRACKER(UNI_L("**** SCRAPE RESPONSE: %s\n"), str.CStr());
						}
						else
						{
							DEBUGTRACE8_TRACKER(UNI_L("**** TRACKER RESPONSE: %s\n"), str.CStr());
						}
#endif
						Process(TRUE, &buffer);
					}
				}
			}
			break;

		case TRANSFER_PROGRESS :
			return;

		case TRANSFER_FAILED:
			{
				OpStringC error = url->GetAttribute(URL::KInternalErrorString);

				if(m_download && g_Downloads->Check(m_download) && !m_scrape_request)
				{
					OpString8 err;

					err.Set(error.CStr());

					m_download->OnTrackerEvent(FALSE, m_tracker, err.CStr());
				}
				m_tracker_error = TRUE;
			}
			break;
	}
	((TransferManager*)g_transferManager)->ReleaseTransferItem(transferItem);
}

void BTTracker::OnTransferItemRemoved(OpTransferItem* transfer_item)
{
	if (m_transfer_item == transfer_item)
	{
		g_transferManager->RemoveTransferManagerListener(this);
		m_transfer_item = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////////
//
// Multitracker support code
//
///////////////////////////////////////////////////////////////////////////////////
BTTrackerEntry::BTTrackerEntry(OpString& tracker_url)
	:
	m_current_tracker(FALSE),
	m_tracker(NULL),
	m_tracker_scrape(NULL)
{
	m_tracker_url.Set(tracker_url.CStr());
	m_tracker = OP_NEW(BTTracker, ());
	if (m_tracker)
		m_tracker->AddRef();

	m_tracker_scrape = OP_NEW(BTTracker, ());
	if (m_tracker_scrape)
		m_tracker_scrape->AddRef();

	BT_RESOURCE_ADD("BTTrackerEntry", this);
}

BTTrackerEntry::BTTrackerEntry(BTTrackerEntry *copy_from)
{
	m_tracker_url.Set(copy_from->m_tracker_url.CStr());
	m_current_tracker = copy_from->m_current_tracker;
	m_tracker = copy_from->m_tracker;
	m_tracker_scrape = copy_from->m_tracker_scrape;
	m_tracker->AddRef();
	m_tracker_scrape->AddRef();

	BT_RESOURCE_ADD("BTTrackerEntry", this);
}

BTTrackerEntry::~BTTrackerEntry()
{
	if (m_tracker)
		m_tracker->Release();
	if (m_tracker_scrape)
		m_tracker_scrape->Release();

	BT_RESOURCE_REMOVE(this);
}

//// tracker group

BTTrackerGroup::BTTrackerGroup()
{
	BT_RESOURCE_ADD("BTTrackerGroup", this);
}
BTTrackerGroup::~BTTrackerGroup()
{
/*
	UINT32 cnt;
	for(cnt = 0; cnt < m_trackers.GetCount(); cnt++)
	{
		BTTrackerEntry *tracker = m_trackers.Get(cnt);
		if(tracker)
		{
			tracker->AbortRequest();
		}
	}
*/
	m_trackers.DeleteAll();

	BT_RESOURCE_REMOVE(this);
}

OP_STATUS BTTrackerGroup::CreateTrackerEntry(OpString& tracker_url)
{
	BTTrackerEntry *entry = OP_NEW(BTTrackerEntry, (tracker_url));
	if(entry)
	{
		if (!entry->GetTracker() || !entry->GetScrapeTracker())
		{
			OP_DELETE(entry);
			return OpStatus::ERR_NO_MEMORY;
		}
		if(m_trackers.GetCount() == 0)
		{
			entry->SetAsCurrent(TRUE);
		}
		return m_trackers.Add(entry);
	}
	return OpStatus::ERR_NO_MEMORY;
}

BTTrackerGroup* BTTrackerGroup::Copy()
{
	BTTrackerGroup* tracker_group = OP_NEW(BTTrackerGroup, ());
	if(tracker_group)
	{
		UINT32 cnt;
		BTTrackerEntry *new_entry;

		for(cnt = 0; cnt < m_trackers.GetCount(); cnt++)
		{
			BTTrackerEntry *entry = m_trackers.Get(cnt);
			if(entry)
			{
				new_entry = entry->Copy();
				if(!new_entry || OpStatus::IsError(tracker_group->m_trackers.Add(new_entry)))
				{
					OP_DELETE(new_entry);
					OP_DELETE(tracker_group);
					return NULL;
				}
			}
		}
	}
	return tracker_group;
}

BTTrackerEntry* BTTrackerGroup::FindActiveTracker(BOOL force, BOOL scrape)
{
	UINT32 cnt;
	BTTrackerEntry *tracker_found = NULL;
	BOOL make_next_current = FALSE;

	for(cnt = 0; cnt < m_trackers.GetCount(); cnt++)
	{
		BTTrackerEntry *tracker = m_trackers.Get(cnt);
		if(tracker)
		{
			if(scrape)
			{
				tracker_found = tracker;
				break;
			}
			if(tracker->IsRequestDone())
			{
				if(!tracker->IsWorking())
				{
					continue;
				}
				if(tracker->IsCurrent())
				{
					tracker_found = tracker;
					break;
				}
				else if(make_next_current)
				{
					tracker->SetAsCurrent(TRUE);
					tracker_found = tracker;
					break;
				}
			}
			else
			{
				tracker->AbortRequest();
				if(tracker->IsCurrent())
				{
					tracker->SetAsCurrent(FALSE);
					make_next_current = TRUE;
				}
			}
		}
	}
	if(!tracker_found && m_trackers.GetCount() && force)
	{
		// 2nd try
		tracker_found = m_trackers.Get(0);
		tracker_found->SetAsCurrent(TRUE);
	}
	return tracker_found;
}

OP_STATUS BTTrackerGroup::SendStarted(BTDownload* pDownload)
{
	BTTrackerEntry *tracker = FindActiveTracker();
	if(tracker && tracker->GetTracker())
	{
		tracker->GetTracker()->Connect(pDownload, tracker->m_tracker_url, "started");
		return OpStatus::OK;
	}
	else
	{
		BTTrackerEntry *tracker = FindActiveTracker(TRUE);
		if(tracker && tracker->GetTracker())
		{
			tracker->GetTracker()->Connect(pDownload, tracker->m_tracker_url, "started");
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

OP_STATUS BTTrackerGroup::SendUpdate(BTDownload* pDownload)
{
	BTTrackerEntry *tracker = FindActiveTracker();
	if(tracker)
	{
		tracker->GetTracker()->Connect(pDownload, tracker->m_tracker_url);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS BTTrackerGroup::SendCompleted(BTDownload* pDownload)
{
	BTTrackerEntry *tracker = FindActiveTracker();
	if(tracker)
	{
		tracker->GetTracker()->Connect(pDownload, tracker->m_tracker_url, "completed");
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS BTTrackerGroup::SendStopped(BTDownload* pDownload, BOOL shutting_down)
{
	BTTrackerEntry* tracker = FindActiveTracker();
	if(tracker)
	{
		tracker->GetTracker()->Connect(pDownload, tracker->m_tracker_url, "stopped", TRUE, FALSE, TRUE);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS BTTrackerGroup::SendScrape(BTDownload *pDownload)
{
	BTTrackerEntry *tracker = FindActiveTracker(FALSE, TRUE);
	if(tracker)
	{
		tracker->GetScrapeTracker()->Connect(pDownload, tracker->m_tracker_url, NULL, TRUE, TRUE);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

BTMultiTracker::BTMultiTracker()
{
	BT_RESOURCE_ADD("BTMultiTracker", this);
}

BTMultiTracker::~BTMultiTracker()
{
	m_tracker_groups.DeleteAll();

	BT_RESOURCE_REMOVE(this);
}

BTTrackerGroup* BTMultiTracker::CreateTrackerGroup()
{
	BTTrackerGroup *group = OP_NEW(BTTrackerGroup, ());
	if(group)
	{
		m_tracker_groups.Add(group);
	}
	return group;
}

void BTMultiTracker::SendStarted(BTDownload* pDownload)
{
	OP_STATUS s;
	UINT32 cnt;
	UINT32 max_cnt = m_tracker_groups.GetCount() > 3 ? 3 : m_tracker_groups.GetCount();
	UINT32 cnt_extra = 0;

	for(cnt = 0; cnt < (max_cnt + cnt_extra); cnt++)
	{
		BTTrackerGroup *group = m_tracker_groups.Get(cnt);
		if(group)
		{
			s = group->SendStarted(pDownload);
			if(OpStatus::IsError(s))
			{
				if(max_cnt + cnt_extra < m_tracker_groups.GetCount())
				{
					cnt_extra++;
				}
			}
		}
	}
}

void BTMultiTracker::SendUpdate(BTDownload* pDownload)
{
	OP_STATUS s;
	UINT32 cnt;
	UINT32 max_cnt = m_tracker_groups.GetCount() > 3 ? 3 : m_tracker_groups.GetCount();
	UINT32 cnt_extra = 0;

	for(cnt = 0; cnt < (max_cnt + cnt_extra); cnt++)
	{
		BTTrackerGroup *group = m_tracker_groups.Get(cnt);
		if(group)
		{
			s = group->SendUpdate(pDownload);
			if(OpStatus::IsError(s))
			{
				if(max_cnt + cnt_extra < m_tracker_groups.GetCount())
				{
					cnt_extra++;
				}
			}
		}
	}
}

void BTMultiTracker::SendCompleted(BTDownload* pDownload)
{
	OP_STATUS s;
	UINT32 cnt;
	UINT32 max_cnt = m_tracker_groups.GetCount() > 3 ? 3 : m_tracker_groups.GetCount();
	UINT32 cnt_extra = 0;

	for(cnt = 0; cnt < (max_cnt + cnt_extra); cnt++)
	{
		BTTrackerGroup *group = m_tracker_groups.Get(cnt);
		if(group)
		{
			s = group->SendCompleted(pDownload);
			if(OpStatus::IsError(s))
			{
				if(max_cnt + cnt_extra < m_tracker_groups.GetCount())
				{
					cnt_extra++;
				}
			}
		}
	}
}

void BTMultiTracker::SendStopped(BTDownload* pDownload, BOOL shutting_down)
{
	OP_STATUS s;
	UINT32 cnt;
	UINT32 max_cnt = m_tracker_groups.GetCount() > 3 ? 3 : m_tracker_groups.GetCount();
	UINT32 cnt_extra = 0;

	for(cnt = 0; cnt < (max_cnt + cnt_extra); cnt++)
	{
		BTTrackerGroup *group = m_tracker_groups.Get(cnt);
		if(group)
		{
			s = group->SendStopped(pDownload, shutting_down);
			if(OpStatus::IsError(s))
			{
				if(max_cnt + cnt_extra < m_tracker_groups.GetCount())
				{
					cnt_extra++;
				}
			}
		}
	}
}

void BTMultiTracker::SendScrape(BTDownload *pDownload)
{
	UINT32 cnt;

	if(m_tracker_groups.GetCount() > 1)
	{
		// don't send scrape with multiple trackers available
		return;
	}
	for(cnt = 0; cnt < m_tracker_groups.GetCount(); cnt++)
	{
		BTTrackerGroup *group = m_tracker_groups.Get(cnt);
		if(group)
		{
			group->SendScrape(pDownload);
		}
	}
}

BTMultiTracker* BTMultiTracker::Copy()
{
	BTMultiTracker *multi_tracker = OP_NEW(BTMultiTracker, ());
	if(multi_tracker)
	{
		UINT32 cnt;
		BTTrackerGroup *new_group;

		for(cnt = 0; cnt < m_tracker_groups.GetCount(); cnt++)
		{
			BTTrackerGroup *group = m_tracker_groups.Get(cnt);
			if(group)
			{
				new_group = group->Copy();
				if(!new_group || OpStatus::IsError(multi_tracker->m_tracker_groups.Add(new_group)))
				{
					OP_DELETE(new_group);
					OP_DELETE(multi_tracker);
					return NULL;
				}
			}
		}
	}
	return multi_tracker;
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
