/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _BITTORRENT_SUPPORT_

# include "adjunct/quick/dialogs/BTConfigDialog.h"
# include "adjunct/quick/dialogs/SimpleDialog.h"
# include "adjunct/quick_toolkit/widgets/OpLabel.h"
# include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
# include "adjunct/quick/windows/DocumentDesktopWindow.h"
# include "modules/hardcore/keys/opkeys.h"
# include "modules/prefs/prefsmanager/prefsmanager.h"
# include "modules/prefs/prefsmanager/collections/pc_network.h"
# include "modules/inputmanager/inputaction.h"
# include "modules/widgets/OpMultiEdit.h"
# include "modules/widgets/OpButton.h"
# include "modules/widgets/OpDropDown.h"
# include "modules/widgets/OpEdit.h"
# include "modules/windowcommander/src/TransferManager.h"
# include "modules/windowcommander/src/WindowCommander.h"
# include "modules/xmlutils/xmlparser.h"
# include "modules/xmlutils/xmltoken.h"
# include "modules/xmlutils/xmltokenhandler.h"
# include "modules/locale/oplanguagemanager.h"

#define BT_TEST_URL	"http://xml.opera.com/checkport/?port="




BTConfigDialog::BTConfigDialog() :
	m_feed_id(0),
	m_port(NULL),
	m_transfer_item(NULL)
{
}


BTConfigDialog::~BTConfigDialog()
{
}

BOOL BTConfigDialog::OnInputAction(OpInputAction *action)
{
	// we need to filter out any non-numeric characters in the port input edit field
	if(action->GetAction() == OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION)
	{
		OpInputAction* child_action = action->GetChildAction();

		if(child_action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED &&
			child_action->GetFirstInputContext() == m_port)
		{
			if( (child_action->GetActionData() >= (uni_char)'a' &&
				 child_action->GetActionData() <= (uni_char) 'z') ||
				(child_action->GetActionData() >= (uni_char)'A' &&
				 child_action->GetActionData() <= (uni_char) 'Z') )
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
	}
	return Dialog::OnInputAction(action);
}

struct BTSpeeds
{
	const uni_char *text;
	int speed;
};

const static struct BTSpeeds btspeeds[] =
{
	{ UNI_L("5"),			5 },
	{ UNI_L("10"),			10},
	{ UNI_L("20"),			20},
	{ UNI_L("40"),			40},
	{ UNI_L("80"),			80},
	{ UNI_L("100"),			100},
	{ UNI_L("150"),			150},
	{ UNI_L("200"),			200},
	{ UNI_L("250"),			250},
	{ UNI_L("300"),			300},
	{ UNI_L("400"),			400},
	{ UNI_L("500"),			500},
	{ NULL, 0 },
};

void BTConfigDialog::OnInit()
{
	int offset;
	int mode = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTBandwidthRestrictionMode);
	int listen_port = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTListenPort);
	int upload_speed = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTMaxUploadRate);
	int download_speed = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BTMaxDownloadRate);

	OpString unlimited;
	g_languageManager->GetString(Str::S_BITTORRENT_UNLIMITED, unlimited);
	if (unlimited.IsEmpty())
		return; // error

	OpCheckBox *checkbox = (OpCheckBox *)GetWidgetByName("Automatic_bandwidth_checkbox");
	if(checkbox != NULL)
	{
		checkbox->SetValue(mode == 1 ? 1 : 0);
	}

	OpDropDown *dlbw = (OpDropDown *)GetWidgetByName("Max_downloadspeed_selector");
	if(dlbw != NULL)
	{
		INT32 index;

		dlbw->SetEnabled(mode == 1 ? FALSE : TRUE);
		dlbw->AddItem(unlimited.CStr(), -1, &index);

		for(offset = 0; btspeeds[offset].text != NULL; offset++)
		{
			dlbw->AddItem(btspeeds[offset].text, -1, &index, btspeeds[offset].speed);
			if(btspeeds[offset].speed == download_speed)
			{
				dlbw->SelectItem(index, TRUE);
			}
		}
	}
	OpDropDown *dluw = (OpDropDown *)GetWidgetByName("Max_uploadspeed_selector");
	if(dluw != NULL)
	{
		INT32 index;

		dluw->SetEnabled(mode == 1 ? FALSE : TRUE);
		dluw->AddItem(unlimited.CStr(), -1, &index);

		for(offset = 0; btspeeds[offset].text != NULL; offset++)
		{
			dluw->AddItem(btspeeds[offset].text, -1, &index, btspeeds[offset].speed);
			if(btspeeds[offset].speed == upload_speed)
			{
				dluw->SelectItem(index, TRUE);
			}
		}
	}
	OpEdit *port = (OpEdit *)GetWidgetByName("Listen_port_edit");
	if(port != NULL)
	{
		OpString port_str;
		port_str.AppendFormat(UNI_L("%ld"), listen_port);

		port->SetMaxLength(5);
		port->SetText(port_str.CStr());

		m_port = port;
	}
	Dialog::OnInit();
}

void BTConfigDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Automatic_bandwidth_checkbox"))
	{
		INT32 state = ((OpCheckBox *)widget)->GetValue();

		if(state)
		{
			OpDropDown *dlbw = (OpDropDown *)GetWidgetByName("Max_downloadspeed_selector");
			if(dlbw != NULL)
			{
				dlbw->SetEnabled(FALSE);
			}
			OpDropDown *dluw = (OpDropDown *)GetWidgetByName("Max_uploadspeed_selector");
			if(dlbw != NULL)
			{
				dluw->SetEnabled(FALSE);
			}
		}
		else
		{
			OpDropDown *dlbw = (OpDropDown *)GetWidgetByName("Max_downloadspeed_selector");
			if(dlbw != NULL)
			{
				dlbw->SetEnabled(TRUE);
			}
			OpDropDown *dluw = (OpDropDown *)GetWidgetByName("Max_uploadspeed_selector");
			if(dlbw != NULL)
			{
				dluw->SetEnabled(TRUE);
			}
		}
	}
	else if(widget->IsNamed("Test_button"))
	{
		BOOL url_exists;
		OpString feed_url;
		OpString port_str;

		OpEdit *port = (OpEdit *)GetWidgetByName("Listen_port_edit");
		if(port != NULL)
		{
			port->GetText(port_str);

			feed_url.Set(BT_TEST_URL);
			feed_url.Append(port_str);

			OpString8 strport;

			strport.Set(port_str.CStr());

			OP_STATUS status = P2PServerConnector::TestAcceptConnections(op_atoi(strport.CStr()), this, m_listensocket);
			// we ignore the return code, we might already have an active binding to that port
			OpStatus::Ignore(status);

			RETURN_VOID_IF_ERROR(g_transferManager->GetTransferItem(&m_transfer_item, feed_url.CStr(), &url_exists));

			if(url_exists == TRUE)
			{
				m_transfer_item->SetTransferListener(NULL);
			}

			URL* url = m_transfer_item->GetURL();
			OP_ASSERT(url != 0);

			url->SetAttribute(URL::KDisableProcessCookies, TRUE);

			MessageHandler *oldHandler = url->GetFirstMessageHandler();
			url->ChangeMessageHandler(oldHandler ? oldHandler : g_main_message_handler, g_main_message_handler);

			URL tmp;
			url->Reload(g_main_message_handler, tmp, TRUE, FALSE);

			m_transfer_item->SetTransferListener(this);

			m_feed_id = url->Id(FALSE);

			widget->SetEnabled(FALSE);
		}
	}
}

void BTConfigDialog::OnCancel()
{
	if(m_transfer_item)
	{
		((TransferManager*)g_transferManager)->ReleaseTransferItem(m_transfer_item);
		m_transfer_item = NULL;
	}
}

UINT32 BTConfigDialog::OnOk()
{
	OP_STATUS status;
	OP_MEMORY_VAR INT32 dl_speed = 0, ul_speed = 0;
	OP_MEMORY_VAR int mode = 1;
	OP_MEMORY_VAR INT32 portno = 0;

	OpCheckBox *checkbox = (OpCheckBox *)GetWidgetByName("Automatic_bandwidth_checkbox");
	if(checkbox != NULL)
	{
		mode = checkbox->GetValue();
	}
	OpDropDown *dlbw = (OpDropDown *)GetWidgetByName("Max_downloadspeed_selector");
	if(dlbw != NULL)
	{
		dl_speed = (INTPTR)dlbw->GetItemUserData(dlbw->GetSelectedItem());
	}
	OpDropDown *dluw = (OpDropDown *)GetWidgetByName("Max_uploadspeed_selector");
	if(dluw != NULL)
	{
		ul_speed = (INTPTR)dluw->GetItemUserData(dluw->GetSelectedItem());
	}
	OpEdit *port = (OpEdit *)GetWidgetByName("Listen_port_edit");
	if(port != NULL)
	{
		OpString strport;
		OpString8 strport8;

		port->GetText(strport);

		strport8.Set(strport.CStr());

		portno = op_atoi(strport8.CStr());
	}
	TRAP(status, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::BTBandwidthRestrictionMode, mode == 1 ? 1 : 2));
	TRAP(status, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::BTListenPort, portno));
	TRAP(status, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::BTMaxDownloadRate, dl_speed));
	TRAP(status, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::BTMaxUploadRate, ul_speed));

	if(m_transfer_item)
	{
		((TransferManager*)g_transferManager)->ReleaseTransferItem(m_transfer_item);
		m_transfer_item = NULL;
	}
	return 0;
}



void BTConfigDialog::OnProgress(OpTransferItem* transferItem, TransferStatus status)
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
					break;

				OP_STATUS err = OpStatus::OK;

				while(has_more)
				{
					OP_MEMORY_VAR unsigned long source_len;
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
				if (OpStatus::IsError(err) || !buffer.DataSize())
					break;

				OpString str;

				str.Append((char *)buffer.Buffer(), buffer.BufferSize());

				OpButton *widget = (OpButton *)GetWidgetByName("Test_button");
				if(widget != NULL)
				{
					widget->SetEnabled(TRUE);
				}
				P2PServerConnector::TestStopConnections(m_listensocket);

				XMLParser *parser = NULL;
				RETURN_VOID_IF_ERROR(XMLParser::Make(parser, this, (MessageHandler*)NULL, this, URL()));
				XMLParser::Configuration xml_config;
				xml_config.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
				parser->SetConfiguration(xml_config);

				err = parser->Parse(str.CStr(), str.Length(), FALSE);
				OP_DELETE(parser);

				if (OpStatus::IsError(err))
				{
					// The parsing failed, make sure that the general corrupt state is used
					m_return_status.Empty();
					m_source_ip_address.Empty();
					break;
				}

				OpString format, title;
				OpString err_msg, end_msg;
				g_languageManager->GetString(Str::D_BITTORRENT_TEST_LABEL, title);
				g_languageManager->GetString(Str::D_BITTORRENT_TEST_RESULTS, format);

				if(!m_return_status.Compare("SUCCESS"))
				{
					// success!
					g_languageManager->GetString(Str::D_BITTORRENT_TEST_RESULTS_SUCCESS, end_msg);
				}
				else
				{
					g_languageManager->GetString(Str::D_BITTORRENT_TEST_RESULTS_FAIL, err_msg);
					end_msg.AppendFormat(err_msg.CStr(), m_return_status.CStr());
				}
				OpString msg;
				msg.AppendFormat(format.CStr(),
							m_source_ip_address.CStr());

				msg.Append("\n\n");
				msg.Append(end_msg.CStr());

				// avoid any misguided callbacks while dialog is waiting
				m_transfer_item->SetTransferListener(NULL);

				SimpleDialog::ShowDialog(WINDOW_NAME_BTCONFIG_OK, this, msg.CStr(), title.CStr(),
					Dialog::TYPE_OK, Dialog::IMAGE_INFO, NULL, "bittorrent.html#test");
			}
			break;

		case TRANSFER_PROGRESS :
			return;

		case TRANSFER_FAILED:
			break;
	}
	((TransferManager*)g_transferManager)->ReleaseTransferItem(m_transfer_item);
	m_transfer_item = NULL;
}

void BTConfigDialog::Continue(XMLParser *parser)
{
}

void BTConfigDialog::Stopped(XMLParser *parser)
{
}

void BTConfigDialog::CharacterDataHandler(const uni_char *s, int len)
{
	m_content_string.Append(s, len);
}

void BTConfigDialog::StartElementHandler(const XMLToken &token)
{
	if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("PORTCHECK"), token.GetName().GetLocalPartLength()))
	{
		if (XMLToken::Attribute *ip_attr = token.GetAttribute(UNI_L("ip"), 2))
		{
			m_source_ip_address.Set(ip_attr->GetValue(), ip_attr->GetValueLength());
			return;
		}
	}
}

void BTConfigDialog::EndElementHandler(const XMLToken &token)
{
	if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("STATUS"), token.GetName().GetLocalPartLength()))
	{
		m_return_status.Set(m_content_string);
		m_return_status.Strip();
		m_content_string.Empty();
	}
}

XMLTokenHandler::Result	BTConfigDialog::HandleToken(XMLToken &token)
{
	switch (token.GetType())
	{
		case XMLToken::TYPE_STag:
			StartElementHandler(token);
			break;

		case XMLToken::TYPE_ETag:
			EndElementHandler(token);
			break;

		case XMLToken::TYPE_Text:
			{
				uni_char *local_copy = NULL;
				const uni_char *text_data = token.GetLiteralSimpleValue();
				if (!text_data)
				{
					local_copy = token.GetLiteralAllocatedValue();
					text_data = local_copy;
				}

				CharacterDataHandler(text_data, token.GetLiteralLength());

				if (local_copy)
					OP_DELETEA(local_copy);
			}
			break;
	}
	return XMLTokenHandler::RESULT_OK;
}

#endif // _BITTORRENT_SUPPORT_
