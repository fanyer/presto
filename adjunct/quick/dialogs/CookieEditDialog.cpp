/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "CookieEditDialog.h"

#include "modules/cookies/url_cm.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpLocale.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"

#include <time.h>


CookieEditDialog::CookieEditDialog(const uni_char* server_name, Cookie* cookie, BOOL readonly)
{
	m_server_name.Set(server_name);
	m_cookie = cookie;
	m_readonly = readonly;

	ServerManager::RequireServerManager();
	SetListener(g_server_manager);
	g_server_manager->SetListener(this);
}


CookieEditDialog::~CookieEditDialog()
{
	g_server_manager->SetListener(0);
	ServerManager::ReleaseServerManager();
}


void CookieEditDialog::OnInit()
{
	if( m_cookie )
	{
		OpString yes, no;
		g_languageManager->GetString(Str::DI_IDYES, yes);
		g_languageManager->GetString(Str::DI_IDNO, no);

		SetWidgetText("Server_edit", m_server_name.CStr() );
		SetWidgetText("Name_edit", m_cookie->Name().CStr() );
		SetWidgetText("Value_edit", m_cookie->Value().CStr() );

		if (m_cookie->Expires())
		{
			SetTime( (OpEdit*)GetWidgetByName("Expire_date_edit"), m_cookie->Expires(), TRUE );
			SetTime( (OpEdit*)GetWidgetByName("Expire_time_edit"), m_cookie->Expires(), FALSE );
		}
		if (m_cookie->GetLastUsed())
		{
			SetTime( (OpEdit*)GetWidgetByName("Last_visited_date_edit"), m_cookie->GetLastUsed(), TRUE );
			SetTime( (OpEdit*)GetWidgetByName("Last_visited_time_edit"), m_cookie->GetLastUsed(), FALSE );
		}
		SetWidgetText("To_creator_edit", m_cookie->SendOnlyToServer() ? yes.CStr() : no.CStr() );
		SetWidgetText("Secure_edit", m_cookie->Secure() ? yes.CStr() : no.CStr() );
		OpString temp_var;
		temp_var.AppendFormat(UNI_L("%d"), m_cookie->Version());
		SetWidgetText("Version_edit", temp_var.CStr() );

		OpEdit* edit = (OpEdit*)GetWidgetByName("Server_edit");
		if( edit )
			edit->SetReadOnly(TRUE);
		edit = (OpEdit*)GetWidgetByName("To_creator_edit");
		if( edit )
			edit->SetReadOnly(TRUE);
		edit = (OpEdit*)GetWidgetByName("Secure_edit");
		if( edit )
			edit->SetReadOnly(TRUE);
		edit = (OpEdit*)GetWidgetByName("Version_edit");
		if( edit )
			edit->SetReadOnly(TRUE);

		edit = (OpEdit*)GetWidgetByName("Name_edit");
		if( edit )
		{
			edit->SetFocus(FOCUS_REASON_OTHER);
			edit->SetCaretPos( edit->GetTextLength() );
		}
	}

	if(m_readonly)
	{
		SetWidgetReadOnly("Server_edit", TRUE);
		SetWidgetReadOnly("Name_edit", TRUE);
		SetWidgetReadOnly("Value_edit", TRUE);
		SetWidgetReadOnly("Expire_date_edit", TRUE);
		SetWidgetReadOnly("Expire_time_edit", TRUE);
		SetWidgetReadOnly("Last_visited_date_edit", TRUE);
		SetWidgetReadOnly("Last_visited_time_edit", TRUE);
		SetWidgetReadOnly("To_creator_edit", TRUE);
		SetWidgetReadOnly("Secure_edit", TRUE);
		SetWidgetReadOnly("Version_edit", TRUE);
		SetWidgetReadOnly("Server_edit", TRUE);
	}
}


void CookieEditDialog::OnCookieRemoved(Cookie* cookie)
{
	if( m_cookie == cookie )
	{
		m_cookie = 0;
	}
}


void CookieEditDialog::SetTime( OpEdit* edit, time_t t, BOOL date )
{
	if( edit )
	{
//		time_t tmp = t - g_op_system_info->GetTimezone();
		uni_char buf[200];

		g_oplocale->op_strftime(buf, 199,
								date ? /*UNI_L("%F")*/UNI_L("%Y-%m-%d") : UNI_L("%H:%M:%S"),
								localtime(&t) );

		edit->SetText(buf);
	}
}


BOOL CookieEditDialog::GetTime( OpEdit* date_edit, OpEdit* time_edit, time_t &t )
{
	if( date_edit && time_edit )
	{
		OpString date_string, time_string;

		date_edit->GetText(date_string);
		time_edit->GetText(time_string);

		if( date_string.CStr() && time_string.CStr() )
		{
			int year=0;
			int month=0;
			int day=0;
			int hour=0;
			int minute=0;
			int second=0;

			int num1 = uni_sscanf(date_string.CStr(), UNI_L("%d-%d-%d"), &year, &month, &day );
			int num2 = uni_sscanf(time_string.CStr(), UNI_L("%d:%d:%d"), &hour, &minute, &second );
			if( num1 == 3 && num2 == 3 )
			{
				if( (year >= 1970 && year <= 2036) && (month >= 1 && month <= 12) && (day >= 1 && day <= 31) &&
					(hour >= 0 && hour <= 23) && (minute >= 0 && minute <= 59) && (second >= 0 && second <= 59) )
				{
					int isdst = 0;
					for( int i=0; i<2; i++ )
					{
						struct tm mytm;
						mytm.tm_sec   = second;
						mytm.tm_min   = minute;
						mytm.tm_hour  = hour;
						mytm.tm_mday  = day;
						mytm.tm_mon   = month - 1;
						mytm.tm_year  = year - 1900;
						mytm.tm_wday  = 0;
						mytm.tm_yday  = 0;
						mytm.tm_isdst = i==0 ? -1 : isdst; // -1 means: Let mktime() try itself

						t = mktime( &mytm );

						if( mytm.tm_isdst == -1 && i == 0 )
						{
							// If mktime() cannot determine the daylight saving time
							// using tzset() itself, then do a rough test that will
							// work in 99.99% of the cases.

							struct tm *tM = localtime( &t );
							if( tM->tm_isdst != -1 )
							{
								isdst = tM->tm_isdst;
								continue;
							}
						}
						return TRUE;
					}
				}
			}


		}
	}

	return FALSE;
}


BOOL CookieEditDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		if( m_cookie )
		{
			OpString name, value, old_name, old_value;
			OpString8 tmp;

			old_name.Set( m_cookie->Name() );
			old_value.Set( m_cookie->Value() );

			GetWidgetText("Name_edit", name );
			GetWidgetText("Value_edit", value );

			tmp.Set(name.CStr());
			m_cookie->SetName(tmp.CStr());
			tmp.Set(value.CStr());
			m_cookie->SetValue(tmp.CStr());


			time_t t;
			if( GetTime((OpEdit*)GetWidgetByName("Expire_date_edit"), (OpEdit*)GetWidgetByName("Expire_time_edit"), t) )
			{
				m_cookie->SetExpires( t );
			}

			if( GetTime((OpEdit*)GetWidgetByName("Last_visited_date_edit"), (OpEdit*)GetWidgetByName("Last_visited_time_edit"), t) )
			{
				m_cookie->SetLastUsed( t );
			}

			if( m_listener )
			{
				m_listener->OnCookieChanged( m_cookie, old_name.CStr(), old_value.CStr() );
			}

		}
	}
	return Dialog::OnInputAction(action);
}
