/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/logdoc_module.h"
#include "modules/logdoc/link.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/namespace.h"
#include "modules/logdoc/src/html5/html5entities.h"
#include "modules/logdoc/html5namemapper.h"
#include "modules/layout/cssprops.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
#ifdef OPERA_CONSOLE
# include "modules/locale/oplanguagemanager.h"
#endif // OPERA_CONSOLE
#include "modules/encodings/tablemanager/optablemanager.h"
#ifdef DNS_PREFETCHING
#include "modules/logdoc/dnsprefetcher.h"
#endif // DNS_PREFETCHING

#ifndef HAS_COMPLEX_GLOBALS
extern void init_AMP_ESC();
extern void init_ATTR_value_name();
extern void init_ClearNameMap();
extern void init_ScrollNameMap();
extern void init_ShapeNameMap();
extern void init_FrameValueNameMap();
extern void init_RulesNameMap();
extern void init_DirNameMap();
extern void init_MethodNameMap();
extern void init_BehaviorNameMap();
extern void init_XML_tag_name();
extern void init_g_html5_tag_names_upper();
extern void init_g_html5_attr_names_upper();
extern void init_g_html5_error_descs();
extern void HTML5TransitionsInitL();
#ifdef ACCESS_KEYS_SUPPORT
extern void init_g_AK2OK_names();
#endif // ACCESS_KEYS_SUPPORT
extern void init_LinkTypeMapL();
extern void init_gDTDStrings();
extern void init_g_handheld_html_doctypes();
extern void init_g_handheld_wml_doctypes();
# ifdef WML_WBXML_SUPPORT
extern void init_WML_WBXML_tag_tokens();
extern void init_WML_WBXML_attr_start_tokens();
extern void init_WML_WBXML_attr_value_tokens();
# endif // WML_WBXML_SUPPORT
# ifdef SI_WBXML_SUPPORT
extern void init_ServiceInd_WBXML_tag_tokens();
extern void init_ServiceInd_WBXML_attr_start_tokens();
extern void init_ServiceInd_WBXML_attr_value_tokens();
# endif // SI_WBXML_SUPPORT
# ifdef SL_WBXML_SUPPORT
extern void init_ServiceLoad_WBXML_attr_start_tokens();
extern void init_ServiceLoad_WBXML_attr_value_tokens();
# endif // SL_WBXML_SUPPORT
# ifdef PROV_WBXML_SUPPORT
extern void init_Provisioning_WBXML_tag_tokens();
extern void init_Provisioning_WBXML_attr_start_tokens();
extern void init_Provisioning_WBXML_attr_start_tokens_cp1();
extern void init_Provisioning_WBXML_attr_value_tokens();
extern void init_Provisioning_WBXML_attr_value_tokens_cp1();
# endif // PROV_WBXML_SUPPORT
# ifdef DRMREL_WBXML_SUPPORT
extern void init_RightsExpressionLanguage_WBXML_tag_tokens();
extern void init_RightsExpressionLanguage_WBXML_attr_start_tokens();
extern void init_RightsExpressionLanguage_WBXML_attr_value_tokens();
# endif // DRMREL_WBXML_SUPPORT
# ifdef CO_WBXML_SUPPORT
extern void init_CacheOperation_WBXML_tag_tokens();
extern void init_CacheOperation_WBXML_attr_start_tokens();
extern void init_CacheOperation_WBXML_attr_value_tokens();
# endif // CO_WBXML_SUPPORT
# ifdef EMN_WBXML_SUPPORT
extern void init_EmailNotification_WBXML_tag_tokens();
extern void init_EmailNotification_WBXML_attr_start_tokens();
extern void init_EmailNotification_WBXML_attr_value_tokens();
# endif // EMN_WBXML_SUPPORT
#endif // HAS_COMPLEX_GLOBALS

LogdocModule::LogdocModule() :
	m_htm_lex(NULL)
	, m_ns_manager(NULL)
	, m_image_animation_handlers(NULL)
	, m_url_image_content_providers(NULL)
	, m_url_image_content_provider_mh(NULL)
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && !defined ENCODINGS_OPPOSITE_ENDIAN
	, m_win1252(NULL)
	, m_win1252present(FALSE)
#endif // ENCODINGS_HAVE_TABLE_DRIVEN && !ENCODINGS_OPPOSITE_ENDIAN
	, m_html5_entity_states(NULL)
	, m_html5_name_mapper(NULL)
#ifdef DNS_PREFETCHING
	, m_dns_prefetcher(NULL)
#endif // DNS_PREFETCHING
	, m_tmp_buffers_used(0)
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
	, m_cached_throttling_needed(FALSE)
	, m_last_throttling_check_timestamp(0.0)
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
{
#ifndef HAS_COMPLEX_GLOBALS
	for (unsigned j = 0; j < HTML5TreeState::kNumberOfTransitionStates; j++)
		m_html5_transitions[j] = NULL;
#endif // !HAS_COMPLEX_GLOBALS

	// set to NULL to avoid problems if allocation fails
	for (int i = 0; i < 3; i++)
		m_tmp_buffers[i] = NULL;
}

void
LogdocModule::InitL(const OperaInitInfo& info)
{
	CONST_ARRAY_INIT(AMP_ESC);
	CONST_ARRAY_INIT(ATTR_value_name);
	CONST_ARRAY_INIT(ClearNameMap);
	CONST_ARRAY_INIT(ScrollNameMap);
	CONST_ARRAY_INIT(ShapeNameMap);
	CONST_ARRAY_INIT(FrameValueNameMap);
	CONST_ARRAY_INIT(RulesNameMap);
	CONST_ARRAY_INIT(DirNameMap);
	CONST_ARRAY_INIT(MethodNameMap);
	CONST_ARRAY_INIT(BehaviorNameMap);
	CONST_ARRAY_INIT(XML_tag_name);
	CONST_ARRAY_INIT(g_html5_tag_names_upper);
	CONST_ARRAY_INIT(g_html5_attr_names_upper);
	CONST_ARRAY_INIT(g_html5_error_descs);
#ifdef ACCESS_KEYS_SUPPORT
	CONST_ARRAY_INIT(g_AK2OK_names);
#endif // ACCESS_KEYS_SUPPORT
	CONST_ARRAY_INIT(LinkTypeMapL);
	CONST_ARRAY_INIT(gDTDStrings);
	CONST_ARRAY_INIT(g_handheld_html_doctypes);
	CONST_ARRAY_INIT(g_handheld_wml_doctypes);
#ifndef HAS_COMPLEX_GLOBALS
# ifdef WML_WBXML_SUPPORT
	CONST_ARRAY_INIT(WML_WBXML_tag_tokens);
	CONST_ARRAY_INIT(WML_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(WML_WBXML_attr_value_tokens);
# endif // WML_WBXML_SUPPORT
# ifdef SI_WBXML_SUPPORT
	CONST_ARRAY_INIT(ServiceInd_WBXML_tag_tokens);
	CONST_ARRAY_INIT(ServiceInd_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(ServiceInd_WBXML_attr_value_tokens);
# endif // SI_WBXML_SUPPORT
# ifdef SL_WBXML_SUPPORT
	CONST_ARRAY_INIT(ServiceLoad_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(ServiceLoad_WBXML_attr_value_tokens);
# endif // SL_WBXML_SUPPORT
# ifdef PROV_WBXML_SUPPORT
	CONST_ARRAY_INIT(Provisioning_WBXML_tag_tokens);
	CONST_ARRAY_INIT(Provisioning_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(Provisioning_WBXML_attr_start_tokens_cp1);
	CONST_ARRAY_INIT(Provisioning_WBXML_attr_value_tokens);
	CONST_ARRAY_INIT(Provisioning_WBXML_attr_value_tokens_cp1);
# endif // PROV_WBXML_SUPPORT
# ifdef DRMREL_WBXML_SUPPORT
	CONST_ARRAY_INIT(RightsExpressionLanguage_WBXML_tag_tokens);
	CONST_ARRAY_INIT(RightsExpressionLanguage_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(RightsExpressionLanguage_WBXML_attr_value_tokens);
# endif // DRMREL_WBXML_SUPPORT
# ifdef CO_WBXML_SUPPORT
	CONST_ARRAY_INIT(CacheOperation_WBXML_tag_tokens);
	CONST_ARRAY_INIT(CacheOperation_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(CacheOperation_WBXML_attr_value_tokens);
# endif // CO_WBXML_SUPPORT
# ifdef EMN_WBXML_SUPPORT
	CONST_ARRAY_INIT(EmailNotification_WBXML_tag_tokens);
	CONST_ARRAY_INIT(EmailNotification_WBXML_attr_start_tokens);
	CONST_ARRAY_INIT(EmailNotification_WBXML_attr_value_tokens);
# endif // EMN_WBXML_SUPPORT

	for (unsigned j = 0; j < HTML5TreeState::kNumberOfTransitionStates; j++)
		m_html5_transitions[j] = OP_NEWA_L(HTML5TreeState::State, HTML5TreeState::kNumberOfTransitionSubStates);
	HTML5TransitionsInitL();
#endif // HAS_COMPLEX_GLOBALS

	m_htm_lex = OP_NEW_L(HTM_Lex, ());
	m_htm_lex->InitL();

	m_ns_manager = OP_NEW_L(NamespaceManager, ());
    m_ns_manager->InitL(256);

	m_image_animation_handlers = OP_NEW_L(Head, ());
	m_url_image_content_providers = OP_NEWA_L(Head, ImageCPHashCount);
	m_url_image_content_provider_mh = OP_NEW_L(MessageHandler, (NULL));

	for (int i = 0; i < 3; i++)
	{
		m_tmp_buffers[i] = OP_NEW_L(TempBuffer, ());
		m_tmp_buffers[i]->SetCachedLengthPolicy(TempBuffer::TRUSTED);
	}

	m_tmp_buffers[0]->Expand(PREALLOC_BUFFER_SIZE);
	m_tmp_buffers[1]->Expand(PREALLOC_BUFFER_SIZE);
	m_tmp_buffers[2]->Expand(PREALLOC_BUFFER_SIZE);

	HTML5EntityStates::InitL();
	m_html5_name_mapper = OP_NEW_L(HTML5NameMapper, ());
	m_html5_name_mapper->InitL();

#ifdef DNS_PREFETCHING
	m_dns_prefetcher = OP_NEW_L(DNSPrefetcher, ());
#endif // DNS_PREFETCHING
}

void
LogdocModule::Destroy()
{
#ifndef HAS_COMPLEX_GLOBALS
	for (unsigned j = 0; j < HTML5TreeState::kNumberOfTransitionStates; j++)
		OP_DELETEA(m_html5_transitions[j]);
#endif // !HAS_COMPLEX_GLOBALS

	OP_DELETEA(m_url_image_content_providers);
	m_url_image_content_providers = NULL;

	OP_DELETE(m_url_image_content_provider_mh);
	m_url_image_content_provider_mh = NULL;

	OP_DELETE(m_image_animation_handlers);
	m_image_animation_handlers = NULL;

	OP_DELETE(m_ns_manager);
	m_ns_manager = NULL;

	OP_DELETE(m_htm_lex);
	m_htm_lex = NULL;

	for (int i = 0; i < 3; i++)
		OP_DELETE(m_tmp_buffers[i]);

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && !defined ENCODINGS_OPPOSITE_ENDIAN
	if (m_win1252)
	{
		g_table_manager->Release(m_win1252);
		m_win1252 = NULL;
	}
#endif // ENCODINGS_HAVE_TABLE_DRIVEN && !ENCODINGS_OPPOSITE_ENDIAN

	HTML5EntityStates::Destroy();
	OP_DELETE(m_html5_name_mapper);
	m_html5_name_mapper = NULL;

#ifdef DNS_PREFETCHING
	OP_DELETE(m_dns_prefetcher);
	m_dns_prefetcher = NULL;
#endif // DNS_PREFETCHING
}

const unsigned short*
LogdocModule::GetWin1252Table()
{
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && !defined ENCODINGS_OPPOSITE_ENDIAN

	// Fetch the windows-1252 table from the table manager. We will keep
	// this in memory until we exit Opera, since it is referenced every
	// time we call ReplaceEscapes() to resolve numerical character
	// references.
	if (!m_win1252 && !m_win1252present)
	{
		long tablesize;
		m_win1252 = (unsigned short *) g_table_manager->Get("windows-1252", tablesize);

		// Make sure we got the right table, so that we can't index outside
		// allocated memory.
		if (tablesize != 256)
		{
			// This was no good, ignore the table, and set a flag so that we
			// won't try loading it again later on.
			if (m_win1252)
			{
				g_table_manager->Release(m_win1252);
			}
			m_win1252 = NULL;
			m_win1252present = TRUE;
		}
	}

	return m_win1252;

#else

	// This is the part of the windows-1252 table we actually need for the
	// ReplaceEscapes() function.
	static const unsigned short win1252_table[160 - 128] =
	{
		8364, NOT_A_CHARACTER, 8218, 402, 8222, 8230, 8224, 8225,
		710, 8240, 352, 8249, 338, NOT_A_CHARACTER, 381, NOT_A_CHARACTER,
		NOT_A_CHARACTER, 8216, 8217, 8220, 8221, 8226, 8211, 8212,
		732, 8482, 353, 8250, 339, NOT_A_CHARACTER, 382, 376
	};

	return win1252_table;

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && !ENCODINGS_OPPOSITE_ENDIAN
}

TempBuffer*
LogdocModule::GetTempBuffer()
{
	for (int i = 0; i < 3; i++)
		if ((m_tmp_buffers_used & (1 << i)) == 0)
		{
			m_tmp_buffers_used |= (1 << i);
			m_tmp_buffers[i]->Clear();
			return m_tmp_buffers[i];
		}

	OP_ASSERT(!"We need to restructure the code that uses this or add more buffers.");
	// returning the first instead of NULL to avoid crash, but things will fail.
	m_tmp_buffers[0]->Clear();
	return m_tmp_buffers[0];
}

void
LogdocModule::ReleaseTempBuffer(TempBuffer *buf)
{
	for (int i = 0; i < 3; i++)
		if (m_tmp_buffers[i] == buf)
		{
			m_tmp_buffers_used &= ~(1 << i);
			if (buf->GetCapacity() > 4 * PREALLOC_BUFFER_SIZE)
			{
				// Something made the buffer quite big so make it return to default size to avoid wasting memory.
				buf->FreeStorage();
				OpStatus::Ignore(buf->Expand(PREALLOC_BUFFER_SIZE)); // If it fails we'll grow it when needed instead.
			}
			break;
		}
}

#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
BOOL
LogdocModule::IsThrottlingNeeded(BOOL force)
{
	double curr_time = g_op_time_info->GetRuntimeMS();
	if (curr_time > m_last_throttling_check_timestamp + g_pccore->GetIntegerPref(PrefsCollectionCore::SwitchAnimationThrottlingInterval))
	{
		m_cached_throttling_needed = force || (g_message_dispatcher->GetAverageLag() >= g_pccore->GetIntegerPref(PrefsCollectionCore::LagThresholdForAnimationThrottling));
		m_last_throttling_check_timestamp = curr_time;
	}

	return m_cached_throttling_needed;
}
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
