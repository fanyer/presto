/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Tord Akerbæk
 */
#include "core/pch.h"

#ifdef PREFS_DOWNLOAD

#include "modules/prefsloader/prefsloadmanager.h"
#include "modules/prefsloader/prefsloader.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"

# ifdef DOM_BROWSERJS_SUPPORT
#  include "modules/dom/src/userjs/browserjs_key.h"
#  include "modules/libssl/tools/signed_textfile.h"
# endif // DOM_BROWSERJS_SUPPORT


XMLTokenHandler::Result PrefsLoader::HandleToken(XMLToken &token)
{
	switch (token.GetType())
	{
	case XMLToken::TYPE_STag:
	case XMLToken::TYPE_ETag:
	case XMLToken::TYPE_EmptyElemTag:
		if (token.GetType() != XMLToken::TYPE_ETag)
		{
			HandleStartElement(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength(), token.GetAttributes(), token.GetAttributesCount());
		}
		if (token.GetType() != XMLToken::TYPE_STag)
			HandleEndElement(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength());
	}

    return RESULT_OK;
}

PrefsLoader::PrefsLoader(OpEndChecker *end)
:	m_descriptor(NULL),
	m_prl_parser(NULL),
	m_parseno(0),			// Run a primary parsing to check the hostnames and verify signature
	m_clean_all_update(FALSE),
	m_checker(end),
	m_parse_blocked(FALSE)
{
	m_hostnames.Empty();

	if (!m_checker)
        m_parseno = 1; // Skip the primary parsing when there is no signature and no confirm dialog
}

OP_STATUS PrefsLoader::Construct(URL url)
{
	m_serverURL.SetURL(url);

	// This may be an external prefs server. Some kind of validity checker should be present.
	// In wich case the parsed material should be checked

	return OpStatus::OK;
}

OP_STATUS PrefsLoader::Construct(const OpStringC &host)
{
	OpString serverURLstr;

	RETURN_IF_ERROR(serverURLstr.Set(g_pccore->GetStringPref(PrefsCollectionCore::PreferenceServer)));
	if (!host.IsEmpty())
	{
		RETURN_IF_ERROR(serverURLstr.Append("?host="));
		RETURN_IF_ERROR(serverURLstr.Append(host));
	}

	URL url = g_url_api->GetURL(serverURLstr.CStr());
	if (url.IsEmpty())
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_serverURL.SetURL(url);

	// This is an internal prefs server. We can assume that the content is safe and the validity checker is not compulsory.

	return OpStatus::OK;
}

PrefsLoader::~PrefsLoader()
{
	if (!IsDead())
	{
		URL_ID url_id = m_serverURL->Id();
		FinishLoading(url_id);
	}
}

OP_STATUS PrefsLoader::StartLoading()
{
	MessageHandler *mmh = g_main_message_handler;
	CommState stat = m_serverURL->Load(mmh, URL(),FALSE,FALSE,FALSE,FALSE);

	if(stat == COMM_REQUEST_FAILED)
		return OpStatus::ERR;

	URL_ID url_id = m_serverURL->Id();

	OP_STATUS rc = OpStatus::OK;
	if (OpStatus::IsError(rc = mmh->SetCallBack(this, MSG_URL_DATA_LOADED, url_id)) ||
		OpStatus::IsError(rc = mmh->SetCallBack(this, MSG_URL_MOVED, url_id)) ||
		OpStatus::IsError(rc = mmh->SetCallBack(this, MSG_URL_LOADING_FAILED, url_id)))
	{
		mmh->UnsetCallBacks(this);
	}
	return rc;
}

void PrefsLoader::LoadData(URL_ID url_id)
{
	if(m_parse_blocked)
		return;
	if (!m_descriptor)
		m_descriptor = m_serverURL->GetDescriptor(g_main_message_handler, TRUE);
	if (!m_descriptor)
		return;
	m_parse_blocked = TRUE;

	BOOL more(TRUE);
	unsigned long buf_len = 0;

	while (more && ((buf_len = m_descriptor->RetrieveData(more)) > 0))
	{
		const uni_char *data_buf =
			reinterpret_cast<const uni_char *>(m_descriptor->GetBuffer());

		ParseResponse(data_buf, UNICODE_DOWNSIZE(buf_len), more?TRUE:FALSE);
		m_descriptor->ConsumeData(buf_len);
	}
	m_parse_blocked = FALSE;
	if (m_serverURL->Status(TRUE) == URL_LOADED)
	{
		if(m_parseno == 0)
		{
			if (m_clean_all_update)
			{
				// If the clean_all attribute is used on the preferences element
				// then any existing hostname override can be affected by this update.
				// Since the m_hostnames string we pass to ->IsEnd() might end up in
				// a "Do you want to install updated overrides for these hosts?"-type
				// of dialog with APPROVE/DENY buttons, it's important that we signal
				// the scope of the update correctly.
				m_hostnames.Set("*");
			}
			// if endchecker is present, it is responsible for stopping or continuing the parser.
			if(m_checker && m_checker->IsEnd(m_hostnames.CStr()))
				;
			else
			{
				// Parse one more time to retrieve the preference values
				m_parseno++;
				delete m_descriptor;
				m_descriptor = m_serverURL->GetDescriptor(g_main_message_handler, TRUE);
				if (!m_descriptor)
					return;
				delete m_prl_parser;
				m_prl_parser = NULL;

				more = TRUE;
				buf_len = 0;

				while (more && ((buf_len = m_descriptor->RetrieveData(more)) > 0))
				{
					const uni_char *data_buf =
						reinterpret_cast<const uni_char *>(m_descriptor->GetBuffer());

					ParseResponse(data_buf, UNICODE_DOWNSIZE(buf_len), more?TRUE:FALSE);
					m_descriptor->ConsumeData(buf_len);
				}
			}
        	}
		FinishLoading(url_id);
	}
}

OP_STATUS PrefsLoader::FinishLoading(URL_ID url_id)
{
	delete m_descriptor;
	m_descriptor = NULL;
	g_main_message_handler->RemoveCallBacks(this, url_id);
	MarkDead();
	Commit();
	if(m_checker) m_checker->Dispose();
	m_checker = NULL;
	m_serverURL.UnsetURL();
	delete m_prl_parser;
	m_prl_parser = NULL;

	return OpStatus::OK;
}


BOOL PrefsLoader::ParseResponse(const uni_char* buffer, int length, BOOL more)
{
	if (!m_prl_parser)
	{
		while (length && uni_isspace(*buffer))
		{
			buffer++;
			length--;
		}

		URL empty_url;
		XMLParser::Make(m_prl_parser, this, g_main_message_handler, this, empty_url);
		m_cur_level = 0;
	}

	m_parse_error = FALSE;
	if (!m_prl_parser->IsFinished() && !m_parse_error)
	{
		m_prl_parser->Parse(buffer, length, more);
		if (m_prl_parser->IsFailed())
			m_parse_error = TRUE;
	}

	if (!more)
	{
		delete m_prl_parser;
		m_prl_parser = NULL;
	}
	return FALSE;
}

void PrefsLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_URL_LOADING_FAILED:
		FinishLoading(par2);
		break;

	case MSG_URL_DATA_LOADED:
		LoadData(par1);
		break;

	case MSG_URL_MOVED:
		{
			MessageHandler *mmh = g_main_message_handler;
			mmh->UnsetCallBack(this, MSG_URL_DATA_LOADED);
			mmh->UnsetCallBack(this, MSG_URL_LOADING_FAILED);
			mmh->UnsetCallBack(this, MSG_URL_MOVED);
			mmh->SetCallBack(this, MSG_URL_DATA_LOADED, par2);
			mmh->SetCallBack(this, MSG_URL_LOADING_FAILED, par2);
			mmh->SetCallBack(this, MSG_URL_MOVED, par2);
		}
		break;
	}
}

void
PrefsLoader::HandleStartElement(const uni_char *name, int name_len, XMLToken::Attribute *atts, int atts_len)
{
	OP_STATUS rc = OpStatus::OK;

	XMLToken::Attribute *atts_end = atts + atts_len;
	ElementType type = GetElmType(name, name_len);

	if(m_parseno == 0) // primary parsing
	{
		if(type == ELM_HOST)
		{
			// Store hostname for now
 			while (atts < atts_end)
 			{
				if(GetAttType(atts) == ATT_NAME)
				{
					if (!m_hostnames.IsEmpty())
					{
						if (OpStatus::IsError(rc = m_hostnames.Append(" ", 1)))
							break;
					}
					rc = m_hostnames.Append(atts->GetValue(), atts->GetValueLength());
					if (OpStatus::IsError(rc)) break;
				}
				atts++;
			}
		}
		else if (type == ELM_PREFERENCES)
		{
			while (atts < atts_end)
			{
				if (GetAttType(atts) == ATT_CLEAN_ALL && atts->GetValueLength() == 1 && atts->GetValue()[0] == '1')
					m_clean_all_update = TRUE;
				atts++;
			}
		}
	}
	else // secondary parsing
	{
        switch (type)
        {
        case ELM_HOSTNAMES:
            m_elm_type[++m_cur_level] = ELM_HOSTNAMES;
            // This element is deprecated
            break;

		case ELM_PREFERENCES:
			while (atts < atts_end)
			{
				if (GetAttType(atts) == ATT_CLEAN_ALL && atts->GetValueLength() == 1 && atts->GetValue()[0] == '1')
				{
					OP_MEMORY_VAR BOOL removed_any = FALSE;
					TRAPD(rc, removed_any = g_prefsManager->RemoveOverridesAllHostsL(/* from_user== */ FALSE));
					if (OpStatus::IsSuccess(rc) && removed_any)
						m_has_changes = TRUE;
				}
				atts++;
			}
			break;

        case ELM_HOST:
            {
                BOOL clean = FALSE;
                BOOL has_name = FALSE;
                m_elm_type[++m_cur_level] = ELM_HOST;
                while (atts < atts_end)
                {
                    if(GetAttType(atts) == ATT_NAME)
                    {
                        rc = m_current_host.Set(atts->GetValue(), atts->GetValueLength());
                        if (OpStatus::IsError(rc))
                        {
                            break;
                        }
                        has_name = TRUE;
                    }
                    else if(GetAttType(atts) == ATT_CLEAN)
                    {
                        if (atts->GetValueLength() == 1 && atts->GetValue()[0] == '1')
						{
                            clean = TRUE;
						}
                    }
                    atts++;
                }
                // When the attributes are read,
                if(clean && has_name && OpStatus::IsSuccess(rc))
                {
                    // Remove all overrides on this host.
                    OP_MEMORY_VAR BOOL success = FALSE;
                    TRAP(rc, success = g_prefsManager->RemoveOverridesL(m_current_host.CStr(), FALSE));
		    // m_has_changes == TRUE ensures that the PLoader base class will CommitL our changes to g_prefsManager when Commit() is called.
                    m_has_changes = m_has_changes || success;
                }
            }
            break;

        case ELM_SECTION:
            m_elm_type[++m_cur_level] = ELM_SECTION;
            while (atts < atts_end)
            {
                if(GetAttType(atts) == ATT_NAME)
                {
                    rc = m_current_section.Set(atts->GetValue(), atts->GetValueLength());
                    if (OpStatus::IsError(rc))
                    {
                        break;
                    }
                }
                atts++;
            }
            break;

        case ELM_PREF:
            {
                OpString8 pref_name;
                OpString pref_value;
                while (atts < atts_end)
                {
                    if(GetAttType(atts) == ATT_NAME)
                    {
                        if(IsAcceptedPref(atts))
                            rc = pref_name.Set(atts->GetValue(), atts->GetValueLength());
                        else
                            rc = OpStatus::ERR;
                    }
                    else if(GetAttType(atts) == ATT_VALUE)
                    {
                        rc = pref_value.Set(atts->GetValue(), atts->GetValueLength());
                    }
                    else
                        rc = OpStatus::ERR;

                    if(OpStatus::IsError(rc))
                    {
                        break;
                    }
                    atts++;
                }

                // Record the new pref
                if(OpStatus::IsSuccess(rc))
                {
                    OP_MEMORY_VAR BOOL success = FALSE;
                    if(m_current_host.IsEmpty())
					{
                        TRAP(rc, success = g_prefsManager->WritePreferenceL(m_current_section.CStr(),pref_name.CStr(),pref_value));
					}
                    else
					{
                        TRAP(rc, success = g_prefsManager->OverridePreferenceL(m_current_host.CStr(),m_current_section.CStr(),pref_name.CStr(),pref_value,FALSE));
					}
		    // m_has_changes == TRUE ensures that the PLoader base class will CommitL our changes to g_prefsManager when Commit() is called.
                    m_has_changes = m_has_changes || success;
                }
                break;
            }

#ifdef PREFS_FILE_DOWNLOAD
        case ELM_FILE:
            {
                OpString8 pref_name;
                OpString file_from, file_to, name_to;
                while (atts < atts_end)
                {
                    if(GetAttType(atts) == ATT_NAME)
                    {
                        if(IsAcceptedPref(atts))
                            rc = pref_name.Set(atts->GetValue(), atts->GetValueLength());
                        else
                            rc = OpStatus::ERR;
                    }
                    else if(GetAttType(atts) == ATT_FROM)
                    {
                        rc = file_from.Set(atts->GetValue(), atts->GetValueLength());
                    }
                    else if((GetAttType(atts) == ATT_TO) || (GetAttType(atts) == ATT_VALUE))
                    {
                        rc = file_to.Set(atts->GetValue(), atts->GetValueLength());
                    }
                    else
                        rc = OpStatus::ERR;

                    if(OpStatus::IsError(rc))
                    {
                        break;
                    }
                    atts++;
                }

                if (OpStatus::IsSuccess(rc))
                {
                    if(IsAcceptedFile(file_to))  // Download the file
                        rc = g_PrefsLoadManager->InitFileLoader(m_current_host, m_current_section, pref_name, file_from, file_to);
                }
                break;
            }
        case ELM_DELETE_FILE:
            {
                OpString file_name;
                while (atts < atts_end)
                {
                    if(GetAttType(atts) == ATT_VALUE)
                    {
                        rc = file_name.Set(atts->GetValue(), atts->GetValueLength());
                    }
                    else
                        rc = OpStatus::ERR;

                    if(OpStatus::IsError(rc))
                    {
                        break;
                    }
                    atts++;
                }
                if(!IsAcceptedFile(file_name))
                    rc = OpStatus::ERR;
                // Delete the file
                if (OpStatus::IsSuccess(rc))
                {
                    OpFile f;
                    if(OpStatus::IsSuccess(rc = f.Construct(file_name.CStr(),OPFILE_PREFSLOAD_FOLDER)))
                        rc = f.Delete();
                }
                break;
            }
#endif // PREFS_FILE_DOWNLOAD
        } // switch
    }  // secondary parsing

	// Signal errors globally, as we cannot do much here.
	// FIXME: OOM: Or can we?
	if (OpStatus::IsRaisable(rc))
	{
		g_memory_manager->RaiseCondition(rc);
	}
}

void
PrefsLoader::HandleEndElement(const uni_char *name, int name_len)
{
	ElementType type = GetElmType(name, name_len);

    switch (type)
    {
    case ELM_SECTION:
        m_current_section.Empty();
        break;
    case ELM_HOST:
        m_current_host.Empty();
        break;
    }

	if (m_cur_level > 0 && m_elm_type[m_cur_level] == type) --m_cur_level;
}

PrefsLoader::ElementType
PrefsLoader::GetElmType(const uni_char *name, int name_len)
{
	switch (name_len)
	{
	case 4:
		if (uni_strni_eq_upper(name, "HOST", 4))
			return ELM_HOST;
		else if (uni_strni_eq_upper(name, "PREF", 4))
			return ELM_PREF;
		else if (uni_strni_eq_upper(name, "FILE", 4))
			return ELM_FILE;
		break;

	case 7:
		if (uni_strni_eq_upper(name, "SECTION", 7))
			return ELM_SECTION;
		break;

	case 9:
		if (uni_strni_eq_upper(name, "HOSTNAMES", 9)) // deprecated
			return ELM_HOSTNAMES;
		break;

	case 11:
		if (uni_strni_eq_upper(name, "DELETE-FILE", 11))
			return ELM_DELETE_FILE;
		else if (uni_strni_eq_upper(name, "PREFERENCES", 11))
			return ELM_PREFERENCES;
		break;
	}

    return ELM_UNKNOWN;
}

PrefsLoader::AttributeType
PrefsLoader::GetAttType(XMLToken::Attribute *att)
{
	const uni_char *name = att->GetName().GetLocalPart();
	switch (att->GetName().GetLocalPartLength())
	{
	case 2:
		if (uni_strni_eq_upper(name, "TO", 2))
			return ATT_TO;
		break;

	case 4:
		if (uni_strni_eq_upper(name, "NAME", 4))
			return ATT_NAME;
		else if (uni_strni_eq_upper(name, "FROM", 4))
			return ATT_FROM;
		break;

	case 5:
		if (uni_strni_eq_upper(name, "VALUE", 5))
			return ATT_VALUE;
		else if (uni_strni_eq_upper(name, "CLEAN", 5))
			return ATT_CLEAN;
		break;
	case 9:
		if (uni_strni_eq_upper(name, "CLEAN_ALL", 9))
			return ATT_CLEAN_ALL;
		break;
	}

	return ATT_UNKNOWN;
}

BOOL PrefsLoader::IsAcceptedPref(XMLToken::Attribute *att)
{
	/* Whitelist of preferences accepted for download. Should only
	 * be changed after consulting with QA.
	 */
	const uni_char *name = att->GetValue();
	switch (att->GetValueLength())
	{
#ifdef SELFTEST
	case 5:
		return uni_strni_eq_upper(name, "SCALE", 5);
#endif

#ifdef USE_SPDY
	case 9:
		return uni_strni_eq_upper(name, "USE SPDY2", 9) || uni_strni_eq_upper(name, "USE SPDY3", 9);
#endif

	case 13:
		return uni_strni_eq_upper(name, "DOCUMENT MODE", 13);

	case 14:
		return uni_strni_eq_upper(name, "LOCAL CSS FILE", 14);

	case 16:
		return uni_strni_eq_upper(name, "BROWSER CSS FILE", 16);

	case 17:
		return uni_strni_eq_upper(name, "MINIMUM FONT SIZE", 17) ||
		       uni_strni_eq_upper(name, "ENABLE PIPELINING", 17);

	case 18:
		return uni_strni_eq_upper(name, "SPOOF USERAGENT ID", 18);

	case 19:
		return uni_strni_eq_upper(name, "COMPATMODE OVERRIDE", 19);

	case 22:
		return uni_strni_eq_upper(name, "MINIMUM SECURITY LEVEL", 22);

	case 24:
		return uni_strni_eq_upper(name, "DELAYED SCRIPT EXECUTION", 24);

	case 29:
		return uni_strni_eq_upper(name, "AUTOCOMPLETEOFF DISABLES WAND", 29);

	default:
		return FALSE;
	}
}

BOOL PrefsLoader::IsAcceptedFile(OpString &name)
{
    if(name.Find("..") == KNotFound) return TRUE;
    return FALSE;
}

#endif
