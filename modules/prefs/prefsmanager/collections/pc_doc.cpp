/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/doc/css_mode.h"
#include "modules/util/gen_str.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/util/opstrlst.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opfolder.h"

#include "modules/prefs/prefsmanager/collections/pc_doc_c.inl"

// Internal flags used for truested external protocols
#define PCDOC_FLAG_USEDEFAULTAPP	0x01
#define PCDOC_FLAG_OPENINTERMINAL	0x02
#define PCDOC_FLAG_USECUSTOMAPP		0x04
#define PCDOC_FLAG_USEINTERNALAPP	0x08
#define PCDOC_FLAG_USEWEBAPP		0x010
#define PCDOC_FLAG_USERDEFINED		0x0100

PrefsCollectionDoc *PrefsCollectionDoc::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcdoc)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcdoc = OP_NEW_L(PrefsCollectionDoc, (reader));
	return g_opera->prefs_module.m_pcdoc;
}

PrefsCollectionDoc::~PrefsCollectionDoc()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcdoc = NULL;
}

#ifdef PREFS_HAVE_STRING_API
int PrefsCollectionDoc::GetDefaultIntegerInternal(int which, const struct integerprefdefault *)
{
	return GetDefaultIntegerPref(integerpref(which));
}
#endif // PREFS_HAVE_STRING_API

void PrefsCollectionDoc::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);

#ifdef HAVE_HANDLERS_FILE
	OpFile handlers_file;
	ANCHOR(OpFile, handlers_file);
	g_pcfiles->GetFileL(PrefsCollectionFiles::HandlersDataFile, handlers_file);

	m_handlers_file.ConstructL();
	m_handlers_file.SetFileL(&handlers_file);
#ifdef PREFSFILE_CASCADE
	OpFile global_fixed_file;
	OpFile global_file;
	LEAVE_IF_ERROR(global_file.Construct(UNI_L("handlersrc"), OPFILE_GLOBALPREFS_FOLDER));
	LEAVE_IF_ERROR(global_fixed_file.Construct(UNI_L("handlersrc.fixed"), OPFILE_FIXEDPREFS_FOLDER));

	m_handlers_file.SetGlobalFileL(&global_file);
	m_handlers_file.SetGlobalFixedFileL(&global_fixed_file);
#endif // PREFSFILE_CASCADE
	m_handlers_file.LoadAllL();
#endif // HAVE_HANDLERS_FILE
	ReadTrustedProtocolsL();
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionDoc::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionDoc::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
#ifdef _AUTO_WIN_RELOAD_SUPPORT_
	case LastUsedAutoWindowReloadTimeout:
		if (*value < 0)
			*value = DEFAULT_AUTO_WINDOW_TIMEOUT;
		break;
#endif

	case ShowScrollbars:
#ifdef PAGED_MEDIA_SUPPORT
	case ShowPageControls:
#endif // PAGED_MEDIA_SUPPORT
	case DisplayLinkTitle:
	case ShowWinSize:
#ifdef SHORTCUT_ICON_SUPPORT
	case AlwaysLoadFavIcon:
#endif
	case CompatModeOverride:
	case ExternalImage:
	case IgnoreTarget:
	case SuppressExternalEmbeds:
	case NewWindow:
	case ShowAnimation:
#ifdef MEDIA_HTML_SUPPORT
	case AllowAutoplay:
#endif
#ifdef SAVE_SUPPORT
	case TxtCharsPerLine:
	case SaveUseSubfolder:
#endif
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	case UseEntitiesInForms:
#endif
	case RenderingMode:
#ifdef TEXT_SELECTION_SUPPORT
	case DisableTextSelect:
#endif
	case XMLLoadExternalEntities:
#ifdef FORMS_LIMIT_FORM_SIZE
	case MaximumBytesFormPost:
	case MaximumBytesFormGet:
#endif
#ifdef FORMS_LIMIT_INPUT_SIZE
	case MaximumBytesInputText:
	case MaximumBytesInputPassword:
#endif
	case SingleWindowBrowsing:
	case AlwaysReloadInterruptedImages:
	case AllowScriptToNavigateInHistory:
	case AllowFileXMLHttpRequest:
#ifdef WEBSOCKETS_SUPPORT
	case EnableWebSockets:
#endif //WEBSOCKETS_SUPPORT
	case AllowAutofocusFormElement:
#ifdef IMAGE_TURBO_MODE
	case TurboMode:
#endif //IMAGE_TURBO_MODE
		break; // Nothing to do.

	case ShowImageState:
		if (*value < 1 || *value > 3)
			*value = 3;
		break;

	case HistoryNavigationMode:
		if (*value < 1 || *value > 3)
			*value = 1;
		break;
#ifdef VISITED_PAGES_SEARCH
	case PageContentSearch:
#endif
	case ReflowDelayLoad:
	case ReflowDelayScript:
	case WaitForStyles:
	case WaitForWebFonts:
	case PrefetchWebFonts:
#ifdef _SPAT_NAV_SUPPORT_
	case EnableSpatialNavigation:
#endif // _SPAT_NAV_SUPPORT_
		break;
#ifdef UPGRADE_SUPPORT
	case TrustedProtocolsVersion:
		break;
#endif // UPGRADE_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	case MediaPlaybackTimeUpdateInterval:
                if(*value <= 0)
                        *value = DEFAULT_MEDIA_TIMEUPDATE_INTERVAL;
                break;
#endif //MEDIA_HTML_SUPPORT
#ifdef CORE_THUMBNAIL_SUPPORT
	case ThumbnailAspectRatioX:
	case ThumbnailAspectRatioY:
		if (*value < 1)
			*value = 1;
		break;
#endif
#ifdef THUMBNAILS_LOGO_TUNING_VIA_PREFS
	case ThumbnailLogoScoreThreshold:
		if (*value < 0)
			*value = 0;
		else if (*value > 99999)
			*value = 99999;
		break;
	case ThumbnailLogoScoreX:
	case ThumbnailLogoScoreY:
	case ThumbnailLogoScoreLogoURL:
	case ThumbnailLogoScoreLogoALT:
	case ThumbnailLogoScoreSiteURL:
	case ThumbnailLogoScoreSiteALT:
	case ThumbnailLogoScoreSiteLink:
	case ThumbnailLogoScoreBanner:
		if (*value < 0)
			*value = 0;
		else if (*value > 1024)
			*value = 1024;
		break;
	case ThumbnailLogoSizeMinX:
	case ThumbnailLogoSizeMinY:
	case ThumbnailLogoSizeMaxX:
	case ThumbnailLogoSizeMaxY:
	case ThumbnailLogoPosMaxX:
	case ThumbnailLogoPosMaxY:
	case ThumbnailIconMinW:
	case ThumbnailIconMinH:
		if (*value < 1)
			*value = 1;
		else if (*value > 2048)
			*value = 2048;
		break;
#endif // THUMBNAILS_LOGO_TUNING_VIA_PREFS
#ifdef CORE_THUMBNAIL_SUPPORT
	case ThumbnailRequestHeader:
		break;
#endif // CORE_THUMBNAIL_SUPPORT
	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionDoc::CheckConditionsL(int which,
                                          const OpStringC &invalue,
                                          OpString **outvalue,
                                          const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
#ifdef SEARCH_TEXT_SUPPORT
	case DefaultSearchType:
		break;
#endif

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

BOOL PrefsCollectionDoc::GetTrustedProtocolInfoL(int index, OpString &protocol, OpString &filename, OpString &description, ViewerMode &viewer_mode, BOOL &in_terminal, BOOL &user_defined)
{
	TrustedProtocolData data;
	BOOL success = GetTrustedProtocolInfo(index, data);
	if (success)
	{
		protocol.SetL(data.protocol);
		filename.SetL(data.filename);
		description.SetL(data.description);
		viewer_mode = data.viewer_mode;
		in_terminal = data.in_terminal;
		user_defined = data.user_defined;
	}

	return success;
}

int PrefsCollectionDoc::GetTrustedApplicationL(const OpStringC &protocol, OpString &filename, OpString &description, BOOL &in_terminal, BOOL &user_defined)
{
	TrustedProtocolData data;
	int index = PrefsCollectionDoc::GetTrustedProtocolInfo(protocol, data);
	if (index != -1)
	{
		filename.SetL(data.filename);
		description.SetL(data.description);
		in_terminal = data.in_terminal;
		user_defined = data.user_defined;
	}

	return index;
}

BOOL PrefsCollectionDoc::SetTrustedProtocolInfoL(int index,
	const OpStringC &protocol, const OpStringC &filename,
	const OpStringC &description, ViewerMode handler_mode, BOOL in_terminal, BOOL user_defined /* = FALSE */)
{

	TrustedProtocolData data;
	data.flags = TrustedProtocolData::TP_Protocol | TrustedProtocolData::TP_Filename | TrustedProtocolData::TP_Description | TrustedProtocolData::TP_ViewerMode | TrustedProtocolData::TP_InTerminal | TrustedProtocolData::TP_UserDefined;

	data.protocol = protocol;
	data.filename = filename;
	data.description = description;
	data.viewer_mode = handler_mode;
	data.in_terminal = in_terminal;
	data.user_defined = user_defined;

	return SetTrustedProtocolInfoL(index, data);
}

BOOL PrefsCollectionDoc::SetTrustedProtocolInfoL(int index, const TrustedProtocolData& data)
{
	int count = static_cast<int>(m_trusted_applications.GetCount());
	if (index < 0 || index > count)
		return FALSE;
	else if (index == count)
	{
		// Creating an entry without a protcol name is not allowed
		if (data.protocol.IsEmpty())
			return FALSE;

		trusted_apps *trusted_app = OP_NEW(trusted_apps, ());
		if (!trusted_app)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		trusted_app->flags = 0;
		OpAutoPtr<trusted_apps> ap_trusted_app(trusted_app);
		RETURN_VALUE_IF_ERROR(m_trusted_applications.Add(trusted_app), FALSE);
		ap_trusted_app.release();
	}
	else
	{
		// Modifying to empty protocol name is not allowed
		if ((data.flags & TrustedProtocolData::TP_Protocol) && data.protocol.IsEmpty())
			return FALSE;
	}

	if (data.flags & TrustedProtocolData::TP_Protocol)
		m_trusted_applications.Get(index)->protocol.SetL(data.protocol);
	if (data.flags & TrustedProtocolData::TP_Filename)
		m_trusted_applications.Get(index)->filename.SetL(data.filename);
	if (data.flags & TrustedProtocolData::TP_WebHandler)
		m_trusted_applications.Get(index)->webhandler.SetL(data.web_handler);
	if (data.flags & TrustedProtocolData::TP_Description)
		m_trusted_applications.Get(index)->description.SetL(data.description);
	if (data.flags & TrustedProtocolData::TP_ViewerMode)
	{
		m_trusted_applications.Get(index)->flags &=
			~(PCDOC_FLAG_USEDEFAULTAPP | PCDOC_FLAG_USECUSTOMAPP | PCDOC_FLAG_USEINTERNALAPP | PCDOC_FLAG_USEWEBAPP);
		switch (data.viewer_mode)
		{
		case UseDefaultApplication:
			m_trusted_applications.Get(index)->flags |= PCDOC_FLAG_USEDEFAULTAPP;
			break;

		case UseCustomApplication:
			m_trusted_applications.Get(index)->flags |= PCDOC_FLAG_USECUSTOMAPP;
			break;

		case UseInternalApplication:
			m_trusted_applications.Get(index)->flags |= PCDOC_FLAG_USEINTERNALAPP;
			break;

		case UseWebService:
			m_trusted_applications.Get(index)->flags |= PCDOC_FLAG_USEWEBAPP;
			break;
		}
	}
	if (data.flags & TrustedProtocolData::TP_InTerminal)
	{
		m_trusted_applications.Get(index)->flags &= ~PCDOC_FLAG_OPENINTERMINAL;
		if (data.in_terminal)
			m_trusted_applications.Get(index)->flags |= PCDOC_FLAG_OPENINTERMINAL;
	}
	if (data.flags & TrustedProtocolData::TP_UserDefined)
	{
		m_trusted_applications.Get(index)->flags &= ~PCDOC_FLAG_USERDEFINED;
		if (data.user_defined)
			m_trusted_applications.Get(index)->flags |= PCDOC_FLAG_USERDEFINED;
	}

	return TRUE;
}

BOOL PrefsCollectionDoc::GetTrustedProtocolInfo(int index, TrustedProtocolData& data)
{
	if (index < 0 || index >= static_cast<int>(m_trusted_applications.GetCount()))
		return FALSE;

	data.protocol = m_trusted_applications.Get(index)->protocol;
	data.filename = m_trusted_applications.Get(index)->filename;
	data.web_handler = m_trusted_applications.Get(index)->webhandler;
	data.description = m_trusted_applications.Get(index)->description;
	if (m_trusted_applications.Get(index)->flags & PCDOC_FLAG_USEDEFAULTAPP)
		data.viewer_mode = UseDefaultApplication;
	else if (m_trusted_applications.Get(index)->flags & PCDOC_FLAG_USECUSTOMAPP)
		data.viewer_mode = UseCustomApplication;
	else if (m_trusted_applications.Get(index)->flags & PCDOC_FLAG_USEWEBAPP)
		data.viewer_mode = UseWebService;
	else
		data.viewer_mode = UseInternalApplication;
	data.in_terminal = (m_trusted_applications.Get(index)->flags & PCDOC_FLAG_OPENINTERMINAL) != 0;
	data.user_defined = (m_trusted_applications.Get(index)->flags & PCDOC_FLAG_USERDEFINED) != 0;

	return TRUE;
}

int PrefsCollectionDoc::GetTrustedProtocolInfo(const OpStringC& protocol, TrustedProtocolData& data)
{
	int index = GetTrustedProtocolIndex(protocol);
	if (index >= 0 && GetTrustedProtocolInfo(index, data))
		return index;
	return -1;
}


int PrefsCollectionDoc::GetTrustedProtocolIndex(const OpStringC& protocol)
{
	for (UINT32 i = 0; i < m_trusted_applications.GetCount(); i ++)
	{
		if(0 == m_trusted_applications.Get(i)->protocol.CompareI(protocol))
			return i;
	}
	return -1;
}

void PrefsCollectionDoc::RemoveTrustedProtocolInfo(int index)
{
	if (index < 0 || index >= static_cast<int>(m_trusted_applications.GetCount()))
		return;

	m_trusted_applications.Delete(index);
}

void PrefsCollectionDoc::RemoveTrustedProtocolInfoL(const OpStringC& protocol, BOOL write /* = TRUE */)
{
	int idx = g_pcdoc->GetTrustedProtocolIndex(protocol);

	g_pcdoc->RemoveTrustedProtocolInfo(idx);

	if (write)
	{
#if defined HAVE_HANDLERS_FILE && defined PREFSFILE_WRITE
		g_pcdoc->WriteTrustedProtocolsL(GetNumberOfTrustedProtocols());
#endif // defined HAVE_HANDLERS_FILE && defined PREFSFILE_WRITE
	}
}

#define TRUSTED_PROTOCOLS_VERSION 1

#if defined HAVE_HANDLERS_FILE && defined PREFSFILE_WRITE
void PrefsCollectionDoc::WriteTrustedProtocolsL(int num_trusted_protocols)
{
	// Remove all the old information first
#if defined UPGRADE_SUPPORT
	unsigned short version = GetIntegerPref(PrefsCollectionDoc::TrustedProtocolsVersion);
	if (version > 0)
#endif // UPGRADE_SUPPORT
	{
		OpString_list sections;
		ANCHOR(OpString_list, sections);

		m_handlers_file.ReadAllSectionsL(sections);
		unsigned long sections_cnt = 0;
		while (sections_cnt < sections.Count())
		{
			OpStackAutoPtr<PrefsSection>
			section(m_handlers_file.ReadSectionL(sections.Item(sections_cnt)));

			if (section.get() && section->Get(UNI_L("Type")) && (uni_stri_eq(section->Get(UNI_L("Type")), "protocol")))
			{
				m_handlers_file.DeleteSectionL(sections.Item(sections_cnt));
			}

			++sections_cnt;
		}
	}

	for (int i = 0; i < num_trusted_protocols; i ++)
	{
		const uni_char *section = m_trusted_applications.Get(i)->protocol.CStr();

		m_handlers_file.WriteStringL(section, UNI_L("Type"), UNI_L("Protocol"));
		m_handlers_file.WriteStringL(section, UNI_L("Handler"), m_trusted_applications.Get(i)->filename);
		m_handlers_file.WriteStringL(section, UNI_L("Webhandler"), m_trusted_applications.Get(i)->webhandler);
		m_handlers_file.WriteStringL(section, UNI_L("Description"), m_trusted_applications.Get(i)->description);
		OpString value;
		value.Empty();
		value.AppendFormat("%d", m_trusted_applications.Get(i)->flags);
		m_handlers_file.WriteStringL(section, UNI_L("Flags"), value);
	}

#if defined UPGRADE_SUPPORT
	if (version == 0)
	{
		WriteIntegerL(PrefsCollectionDoc::TrustedProtocolsVersion,  TRUSTED_PROTOCOLS_VERSION);
		m_reader->DeleteSectionL(UNI_L("Trusted Protocols"));
		g_prefsManager->CommitL();
	}
#endif // UPGRADE_SUPPORT

	m_handlers_file.CommitL(FALSE, FALSE);
}
#endif // defined HAVE_HANDLERS_FILE && defined PREFSFILE_WRITE

void PrefsCollectionDoc::ReadTrustedProtocolsL()
{
	// Delete current entries if any. We may read this file more than once
	m_trusted_applications.DeleteAll();

#ifdef UPGRADE_SUPPORT
	unsigned short version = GetIntegerPref(PrefsCollectionDoc::TrustedProtocolsVersion);

	// backward compatibility
	if (version == 0)
	{
#ifdef PREFS_READ
		// Read trusted protocol information
		OpStackAutoPtr<PrefsSection>
			section(m_reader->ReadSectionL(UNI_L("Trusted Protocols")));
		/*DOC
		 *section=Trusted Protocols
		 *name=n/a
		 *key=protocol
		 *type=string
		 *value=usedefault,openinterminal,path
		 *description=Handler for trusted external protocol
		 */
		if (section.get())
		{
			const PrefsEntry *entry = section->Entries();
			while (entry)
			{
				const uni_char *key = entry->Key();
				OpString value; ANCHOR(OpString, value);
				value.SetL(entry->Value());
				if (key && *key && value.CStr())
				{
					uni_char *tokens[3];
					int tokensread = GetStrTokens(value.CStr(), UNI_L(","), UNI_L(" \t"), tokens, 3);
					if (tokensread == 3 || tokensread == 2)
					{
						trusted_apps *trusted_app = OP_NEW_L(trusted_apps, ());
						OpAutoPtr<trusted_apps> ap_trusted_app(trusted_app);
						trusted_app->protocol.SetL(key);
						trusted_app->filename.SetL(tokens[2]);
						trusted_app->flags = 0;
						if (uni_str_eq(tokens[0], "1"))
							trusted_app->flags |= PCDOC_FLAG_USEDEFAULTAPP;
						else if (uni_str_eq(tokens[0], "2"))
							trusted_app->flags |= PCDOC_FLAG_USEINTERNALAPP;
						else /* if (uni_str_eq(tokens[0], "0")) */
							trusted_app->flags |= PCDOC_FLAG_USECUSTOMAPP;
						if (uni_str_eq(tokens[1], "1"))
							trusted_app->flags |= PCDOC_FLAG_OPENINTERMINAL;

						LEAVE_IF_ERROR(m_trusted_applications.Add(trusted_app));
						ap_trusted_app.release();
					}
				}
				entry = entry->Suc();
			}
		}
#endif // PREFS_READ
	}
	else
#endif // UPGRADE_SUPPORT
	{
#ifdef HAVE_HANDLERS_FILE
		OpString_list sections;
		ANCHOR(OpString_list, sections);

		m_handlers_file.ReadAllSectionsL(sections);
		unsigned long sectons_cnt = 0;
		while (sectons_cnt < sections.Count())
		{
			OpStackAutoPtr<PrefsSection>
			section(m_handlers_file.ReadSectionL(sections.Item(sectons_cnt)));

			if (!section.get() || !section->Get(UNI_L("Type")) || (!uni_stri_eq(section->Get(UNI_L("Type")), "protocol")))
			{
				++sectons_cnt;
				continue;
			}

			trusted_apps *trusted_app = OP_NEW_L(trusted_apps, ());
			OpAutoPtr<trusted_apps> ap_trusted_app(trusted_app);

			trusted_app->protocol.SetL(sections.Item(sectons_cnt));
			trusted_app->filename.SetL(section->Get(UNI_L("Handler")));
			trusted_app->webhandler.SetL(section->Get(UNI_L("Webhandler")));
			trusted_app->description.SetL(section->Get(UNI_L("Description")));
			trusted_app->flags = section->Get(UNI_L("Flags")) ? uni_atoi(section->Get(UNI_L("Flags"))) : 0;
			LEAVE_IF_ERROR(m_trusted_applications.Add(trusted_app));
			ap_trusted_app.release();
			++sectons_cnt;
		}
#endif // HAVE_HANDLERS_FILE
	}
}
