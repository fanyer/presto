/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#include "SecurityProtocolsDialog.h"

//#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/widgets/OpWidgetFactory.h"
#include "adjunct/desktop_util/prefs/WidgetPrefs.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#ifdef _SSL_SUPPORT_
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/ssldlg.h"
#include "modules/libssl/sslrand.h"
#endif

SecurityProtocolsDialog::SecurityProtocolsDialog() : m_ciphers_model(2)
{
#ifdef SSL_2_0_SUPPORT
	m_ssl2_count = 0;
#endif // SSL_2_0_SUPPORT
}

SecurityProtocolsDialog::~SecurityProtocolsDialog()
{
	if(m_options_manager && m_options_manager->dec_reference() <= 0)
		OP_DELETE(m_options_manager);
}

void SecurityProtocolsDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
}


void SecurityProtocolsDialog::OnInit()
{
	m_options_manager = g_ssl_api->CreateSecurityManager(TRUE);
	if(!m_options_manager)
		return;

	OpStatus::Ignore(WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_SSL_3_checkbox"), PrefsCollectionNetwork::EnableSSL3_0));
	OpStatus::Ignore(WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_TLS_1_checkbox"), PrefsCollectionNetwork::EnableSSL3_1));
	OpStatus::Ignore(WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_TLS_1_1_checkbox"), PrefsCollectionNetwork::EnableTLS1_1));
	OpStatus::Ignore(WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_TLS_1_2_checkbox"), PrefsCollectionNetwork::EnableTLS1_2));

	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Ciphers_treeview");
	if (treeview)
	{
		OpString loc_str;

		g_languageManager->GetString(Str::S_CRYPTO_VERSION, loc_str);
		m_ciphers_model.SetColumnData(0, loc_str.CStr());
		g_languageManager->GetString(Str::S_CRYPTO_CIPHER, loc_str);
		m_ciphers_model.SetColumnData(1, loc_str.CStr());

		treeview->SetTreeModel(&m_ciphers_model);
		treeview->SetCheckableColumn(1);
		treeview->SetColumnWeight(1, 200);

		SSL_ProtocolVersion version;
		OpString ciphername;
		BOOL selected;

#ifdef SSL_2_0_SUPPORT
		OpString ssl[2];
		ssl[0].Set(UNI_L("SSL 2"));
		ssl[1].Set(UNI_L("SSL 3/TLS 1"));
		for (int v = 2; v <= 3; v++)
		{
			version.Set(v,0);

			int i;
			for (i = 0; m_options_manager->GetCipherName(version, i, ciphername, selected); i++)
			{
				int pos = m_ciphers_model.AddItem(ssl[v-2].CStr(), NULL, 0, -1, NULL, 0, OpTypedObject::UNKNOWN_TYPE, selected);
				m_ciphers_model.SetItemData(pos, 1, ciphername.CStr());
			}

			if (v == 2)
			{
				m_ssl2_count = i;
			}
		}
#else
		OpString ssl;
		ssl.Set(UNI_L("SSL 3/TLS 1"));
		version.Set(3,0);

		int i;
		for (i = 0; m_options_manager->GetCipherName(version, i, ciphername, selected); i++)
		{
			int pos = m_ciphers_model.AddItem(ssl.CStr(), NULL, 0, -1, NULL, 0, OpTypedObject::UNKNOWN_TYPE, selected);
			m_ciphers_model.SetItemData(pos, 1, ciphername.CStr());
		}
#endif
	}
	ShowWidget("Advanced_group", FALSE);
}


UINT32 SecurityProtocolsDialog::OnOk()
{
	/* If the options manager is not created, the dialog may be
	 * uninitialized.  Let's not save random garbage to prefs.
	 */
	if(!m_options_manager)
		return FALSE;

	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_SSL_3_checkbox"), PrefsCollectionNetwork::EnableSSL3_0));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_TLS_1_checkbox"), PrefsCollectionNetwork::EnableSSL3_1));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_TLS_1_1_checkbox"), PrefsCollectionNetwork::EnableTLS1_1));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_TLS_1_2_checkbox"), PrefsCollectionNetwork::EnableTLS1_2));

	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Ciphers_treeview");
	if (treeview)
	{
		int total = treeview->GetLineCount();

#ifdef SSL_2_0_SUPPORT
		int *ssl2_list = NULL;
#endif // SSL_2_0_SUPPORT
		int *ssl3_list = NULL;

#ifdef SSL_2_0_SUPPORT
		if (m_ssl2_count)
		{
			ssl2_list = OP_NEWA(int, m_ssl2_count);
			if (!ssl2_list)
				return 0;
		}
#endif // SSL_2_0_SUPPORT
		if (total)
		{
#ifdef SSL_2_0_SUPPORT
			ssl3_list = OP_NEWA(int, total - m_ssl2_count);
#else
			ssl3_list = OP_NEWA(int, total);
#endif // SSL_2_0_SUPPORT
			if (!ssl3_list)
				return 0;
		}

		int i = 0;

#ifdef SSL_2_0_SUPPORT
		int ssl2_pos = 0;

		for (; i < m_ssl2_count; i++)
		{
			if (treeview->IsItemChecked(i))
			{
				ssl2_list[ssl2_pos++] = i;
			}
		}
		i = m_ssl2_count;
#endif // SSL_2_0_SUPPORT

		int ssl3_pos = 0;

		for (; i < total; i++)
		{
			if (treeview->IsItemChecked(i))
			{
				ssl3_list[ssl3_pos++] = i
#ifdef SSL_2_0_SUPPORT
					- m_ssl2_count
#endif // SSL_2_0_SUPPORT
					;
			}
		}

		SSL_ProtocolVersion version;

#ifdef SSL_2_0_SUPPORT
		version.Set(2, 0);
		m_options_manager->SetCiphers(version, ssl2_pos, ssl2_list);
#endif // SSL_2_0_SUPPORT
		version.Set(3, 0);
		m_options_manager->SetCiphers(version, ssl3_pos, ssl3_list);

#ifdef SSL_2_0_SUPPORT
		if (ssl2_list)
			OP_DELETEA(ssl2_list);
#endif // SSL_2_0_SUPPORT
		if (ssl3_list)
			OP_DELETEA(ssl3_list);
	}

	g_ssl_api->CommitOptionsManager(m_options_manager);
	return 0;
}
