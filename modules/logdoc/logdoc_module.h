/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_LOGDOC_LOGDOC_MODULE_H
#define MODULES_LOGDOC_LOGDOC_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/logdoc/link_type.h"
#ifndef HAS_COMPLEX_GLOBALS
# include "modules/logdoc/data/entities_len.h"
# include "modules/logdoc/src/htm_elm_maps.h"
# include "modules/logdoc/src/html5/html5treestate.h"
#endif // HAS_COMPLEX_GLOBALS

class NamespaceManager;
class HTM_Lex;
class TempBuffer;
class URL;
class HTML5EntityStates;
class HTML5NameMapper;
class HTML5EntityNode;
struct HTML5ReplacementCharacter;

#ifdef DNS_PREFETCHING
class DNSPrefetcher;
#endif // DNS_PREFETCHING

const int ImageCPHashCount = 29; ///< number of buckets in the hash m_url_image_content_providers. ONLY CHANGE TO PRIME!

class LogdocModule : public OperaModule
{
public:

#include "modules/logdoc/src/html5/errortablelength.inl" // for kErrorTableLength

	LogdocModule();

	void	InitL(const OperaInitInfo& info);

	void	Destroy();

	HTM_Lex*			m_htm_lex;
	NamespaceManager*	m_ns_manager;
	Head*				m_image_animation_handlers;
	Head*				m_url_image_content_providers;
	MessageHandler*		m_url_image_content_provider_mh;


	// Used by ReplaceEscapes()
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && !defined ENCODINGS_OPPOSITE_ENDIAN
	const unsigned short*	GetWin1252Table();
	unsigned short*		m_win1252;
	BOOL				m_win1252present;
#else
	static const unsigned short*	GetWin1252Table();
#endif
	friend int ReplaceEscapes(uni_char *, BOOL, BOOL, BOOL);
	friend int ReplaceEscapes(uni_char *, int, BOOL, BOOL, BOOL);

#ifdef LINK_SUPPORT
# define LINK_TYPE_MAP_SIZE 37
#else
# define LINK_TYPE_MAP_SIZE 2
#endif
#ifndef HAS_COMPLEX_GLOBALS
	const char*		m_amp_esc_map[AMP_ESC_SIZE];
	const char*		m_attr_value_name_map[81];
	const uni_char*	m_clear_name_map[ClearNameMapLen];
	const uni_char*	m_scroll_name_map[ScrollNameMapLen];
	const uni_char*	m_shape_name_map[ShapeNameMapLen];
	const uni_char*	m_frame_value_name_map[FrameValueNameMapLen];
	const uni_char*	m_rules_name_map[RulesNameMapLen];
	const uni_char*	m_dir_name_map[DirNameMapLen];
	const uni_char*	m_method_name_map[MethodNameMapLen];
	const uni_char*	m_behavior_name_map[BehaviorNameMapLen];
	const uni_char*	m_xml_tag_name_map[4];
	const uni_char* m_html5_tag_names_upper[Markup::kUpperTagNameTableLength];
	const uni_char* m_html5_attr_names_upper[Markup::kUpperAttrNameTableLength];
	const char*		m_html5_error_descs[LogdocModule::kErrorTableLength];
	HTML5TreeState::State*
					m_html5_transitions[HTML5TreeState::kNumberOfTransitionStates];
	HTML5ReplacementCharacter* m_entity_replacement_characters;
# ifdef ACCESS_KEYS_SUPPORT
	const char*		m_accesskey_map[89];
# endif // ACCESS_KEYS_SUPPORT
	LinkMapEntry
						m_link_type_map[LINK_TYPE_MAP_SIZE];
	const char*		m_dtd_strings[62];
	const char*		m_handheld_html_doctypes[9];
	const char*		m_handheld_wml_doctypes[4];

# ifdef WML_WBXML_SUPPORT
	const uni_char*	m_wbxml_wml_tags[37];
	const uni_char*	m_wbxml_wml_starts[96];
	const uni_char*	m_wbxml_wml_values[29];
# endif // WML_WBXML_SUPPORT
# ifdef SI_WBXML_SUPPORT
	const uni_char*	m_wbxml_si_tags[4];
	const uni_char*	m_wbxml_si_starts[14];
	const uni_char*	m_wbxml_si_values[4];
# endif // SI_WBXML_SUPPORT
# ifdef SL_WBXML_SUPPORT
	const uni_char*	m_wbxml_sl_starts[8];
	const uni_char*	m_wbxml_sl_values[4];
# endif // SL_WBXML_SUPPORT
# ifdef PROV_WBXML_SUPPORT
	const uni_char*	m_wbxml_prov_tags[3];
	const uni_char*	m_wbxml_prov_starts[87];
	const uni_char*	m_wbxml_prov_cp1_starts[44];
	const uni_char*	m_wbxml_prov_values[93];
	const uni_char*	m_wbxml_prov_cp1_values[15];
# endif // PROV_WBXML_SUPPORT
# ifdef DRMREL_WBXML_SUPPORT
	const uni_char*	m_wbxml_drmrel_tags[19];
	const uni_char*	m_wbxml_drmrel_starts[3];
	const uni_char*	m_wbxml_drmrel_values[3];
# endif // DRMREL_WBXML_SUPPORT
# ifdef CO_WBXML_SUPPORT
	const uni_char*	m_wbxml_co_tags[3];
	const uni_char*	m_wbxml_co_starts[5];
	const uni_char*	m_wbxml_co_values[4];
# endif // CO_WBXML_SUPPORT
# ifdef EMN_WBXML_SUPPORT
	const uni_char*	m_wbxml_emn_tags[1];
	const uni_char*	m_wbxml_emn_starts[9];
	const uni_char*	m_wbxml_emn_values[4];
# endif // EMN_WBXML_SUPPORT
#endif // HAS_COMPLEX_GLOBALS

	TempBuffer*			GetTempBuffer();
	void				ReleaseTempBuffer(TempBuffer *buf);

#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
	/**
	 * Checks if animated images throttling should be turned on/off (by checking whether core is busy/free).
	 *
	 * @param force - Whether throttling should be forced no matter of current load.
	 *
	 * @returns TRUE if throttling should be turned on.
	 */
	BOOL IsThrottlingNeeded(BOOL force);
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0

	HTML5EntityStates*	m_html5_entity_states;
	HTML5NameMapper*	m_html5_name_mapper;

#ifdef DNS_PREFETCHING
	DNSPrefetcher* m_dns_prefetcher;
#endif // DNS_PREFETCHING

private:

	int				m_tmp_buffers_used;
	TempBuffer*		m_tmp_buffers[3];
#if LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0
	/** Cached value of last core load check (for better performance - depending on configuaration - core load check isn't performed everytime when needed.
	 *  Sometimes this cached value from previos check is returned
	 */
	BOOL			m_cached_throttling_needed;
	/** Last core load check timestamp */
	double			m_last_throttling_check_timestamp;
#endif // LOGDOC_MIN_ANIM_UPDATE_INTERVAL_DURING_LOADING > 0 || LOGDOC_MIN_ANIM_UPDATE_INTERVAL_LOW > 0

	void	InitLinkMapL();
};

#define htmLex			(g_opera->logdoc_module.m_htm_lex)
#define g_ns_manager	(g_opera->logdoc_module.m_ns_manager)

#ifndef HAS_COMPLEX_GLOBALS
# define AMP_ESC		(g_opera->logdoc_module.m_amp_esc_map)
# define ATTR_value_name	(g_opera->logdoc_module.m_attr_value_name_map)
# define ClearNameMap	(g_opera->logdoc_module.m_clear_name_map)
# define ScrollNameMap	(g_opera->logdoc_module.m_scroll_name_map)
# define ShapeNameMap	(g_opera->logdoc_module.m_shape_name_map)
# define FrameValueNameMap	(g_opera->logdoc_module.m_frame_value_name_map)
# define RulesNameMap	(g_opera->logdoc_module.m_rules_name_map)
# define DirNameMap		(g_opera->logdoc_module.m_dir_name_map)
# define MethodNameMap	(g_opera->logdoc_module.m_method_name_map)
# define BehaviorNameMap	(g_opera->logdoc_module.m_behavior_name_map)
# define XML_tag_name	(g_opera->logdoc_module.m_xml_tag_name_map)
# define g_html5_tag_names_upper (g_opera->logdoc_module.m_html5_tag_names_upper)
# define g_html5_attr_names_upper (g_opera->logdoc_module.m_html5_attr_names_upper)
# define g_html5_error_descs (g_opera->logdoc_module.m_html5_error_descs)
# define g_html5_transitions (g_opera->logdoc_module.m_html5_transitions)
# ifdef ACCESS_KEYS_SUPPORT
#  define g_AK2OK_names		(g_opera->logdoc_module.m_accesskey_map)
# endif // ACCESS_KEYS_SUPPORT
# define g_LinkTypeMap	(g_opera->logdoc_module.m_link_type_map)
# define gDTDStrings	(g_opera->logdoc_module.m_dtd_strings)
# define g_handheld_html_doctypes	(g_opera->logdoc_module.m_handheld_html_doctypes)
# define g_handheld_wml_doctypes	(g_opera->logdoc_module.m_handheld_wml_doctypes)

# ifdef WML_WBXML_SUPPORT
#  define WML_WBXML_tag_tokens			(g_opera->logdoc_module.m_wbxml_wml_tags)
#  define WML_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_wml_starts)
#  define WML_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_wml_values)
# endif // WML_WBXML_SUPPORT
# ifdef SI_WBXML_SUPPORT
#  define ServiceInd_WBXML_tag_tokens			(g_opera->logdoc_module.m_wbxml_si_tags)
#  define ServiceInd_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_si_starts)
#  define ServiceInd_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_si_values)
# endif // SI_WBXML_SUPPORT
# ifdef SL_WBXML_SUPPORT
#  define ServiceLoad_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_sl_starts)
#  define ServiceLoad_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_sl_values)
# endif // SL_WBXML_SUPPORT
# ifdef PROV_WBXML_SUPPORT
#  define Provisioning_WBXML_tag_tokens			(g_opera->logdoc_module.m_wbxml_prov_tags)
#  define Provisioning_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_prov_starts)
#  define Provisioning_WBXML_attr_start_tokens_cp1	(g_opera->logdoc_module.m_wbxml_prov_cp1_starts)
#  define Provisioning_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_prov_values)
#  define Provisioning_WBXML_attr_value_tokens_cp1	(g_opera->logdoc_module.m_wbxml_prov_cp1_values)
# endif // PROV_WBXML_SUPPORT
# ifdef DRMREL_WBXML_SUPPORT
#  define RightsExpressionLanguage_WBXML_tag_tokens			(g_opera->logdoc_module.m_wbxml_drmrel_tags)
#  define RightsExpressionLanguage_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_drmrel_starts)
#  define RightsExpressionLanguage_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_drmrel_values)
# endif // DRMREL_WBXML_SUPPORT
# ifdef CO_WBXML_SUPPORT
#  define CacheOperation_WBXML_tag_tokens			(g_opera->logdoc_module.m_wbxml_co_tags)
#  define CacheOperation_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_co_starts)
#  define CacheOperation_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_co_values)
# endif // CO_WBXML_SUPPORT
# ifdef EMN_WBXML_SUPPORT
#  define EmailNotification_WBXML_tag_tokens		(g_opera->logdoc_module.m_wbxml_emn_tags)
#  define EmailNotification_WBXML_attr_start_tokens	(g_opera->logdoc_module.m_wbxml_emn_starts)
#  define EmailNotification_WBXML_attr_value_tokens	(g_opera->logdoc_module.m_wbxml_emn_values)
# endif // EMN_WBXML_SUPPORT
#endif // HAS_COMPLEX_GLOBALS


# define g_html5_entity_states		(g_opera->logdoc_module.m_html5_entity_states)
# define g_html5_name_mapper		(g_opera->logdoc_module.m_html5_name_mapper)

#ifdef DNS_PREFETCHING
# define g_dns_prefetcher			(g_opera->logdoc_module.m_dns_prefetcher)
#endif // DNS_PREFETCHING

#define LOGDOC_MODULE_REQUIRED

#endif // !MODULES_LOGDOC_LOGDOC_MODULE_H
