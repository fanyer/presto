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

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefsfile/prefsfile.h"
#ifndef PREFS_HOSTOVERRIDE
# include "modules/prefsfile/impl/backend/prefssectioninternal.h"
#endif
#include "modules/url/url_enum.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/util/handy.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_network_c.inl"

PrefsCollectionNetwork *PrefsCollectionNetwork::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcnet)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcnet = OP_NEW_L(PrefsCollectionNetwork, (reader));
	return g_opera->prefs_module.m_pcnet;
}

PrefsCollectionNetwork::~PrefsCollectionNetwork()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);
#endif

#ifndef PREFS_HOSTOVERRIDE
	OP_DELETE(m_use100Continue);
#endif
	g_opera->prefs_module.m_pcnet = NULL;
}

int PrefsCollectionNetwork::GetDefaultIntegerPref(integerpref which) const
{
	// Need to handle special cases here
	switch (which)
	{
	case UseHTTPProxy:
		return m_default_http_proxy_enabled;

	case UseHTTPSProxy:
		return m_default_https_proxy_enabled;

#ifdef FTP_SUPPORT
	case UseFTPProxy:
		return m_default_ftp_proxy_enabled;
#endif

#ifdef GOPHER_SUPPORT
	case UseGopherProxy:
		return m_default_gopher_proxy_enabled;
#endif

#ifdef WAIS_SUPPORT
	case UseWAISProxy:
		return m_default_wais_proxy_enabled;
#endif

#ifdef SOCKS_SUPPORT
	case UseSOCKSProxy:
		return m_default_socks_proxy_enabled;

#endif

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	case AutomaticProxyConfig:
		return m_default_auto_proxy_enabled;
#endif

	case HasNoProxyServers:
		return m_default_use_proxy_exceptions;

	default:
		return m_integerprefdefault[which].defval;
	}
}

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
int PrefsCollectionNetwork::GetDefaultIntegerInternal(int which, const struct integerprefdefault *)
{
	return GetDefaultIntegerPref(integerpref(which));
}
#endif // PREFS_HAVE_STRING_API

const uni_char *PrefsCollectionNetwork::GetDefaultStringPref(stringpref which) const
{
	// Need to handle special cases here
	switch (which)
	{
	case HTTPProxy:
		return m_default_http_proxy.CStr();

	case HTTPSProxy:
		return m_default_https_proxy.CStr();

#ifdef FTP_SUPPORT
	case FTPProxy:
		return m_default_ftp_proxy.CStr();
#endif

#ifdef GOPHER_SUPPORT
	case GopherProxy:
		return m_default_gopher_proxy.CStr();
#endif

#ifdef WAIS_SUPPORT
	case WAISProxy:
		return m_default_wais_proxy.CStr();
#endif

#ifdef SOCKS_SUPPORT
	case SOCKSProxy:
		return m_default_socks_proxy.CStr();
#endif

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	case AutomaticProxyConfigURL:
		return m_default_auto_proxy_url.CStr();
#endif

	case NoProxyServers:
		return m_default_proxy_exceptions.CStr();

	case AcceptLanguage:
		return m_default_accept_language.CStr();

#ifdef ADD_PERMUTE_NAME_PARTS
	case HostNameExpansionPostfix:
		return m_default_hostname_postfix.CStr();
#endif

	default:
		return m_stringprefdefault[which].defval;
	}
}

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
const uni_char *PrefsCollectionNetwork::GetDefaultStringInternal(int which, const struct stringprefdefault *)
{
	return GetDefaultStringPref(stringpref(which));
}
#endif // PREFS_HAVE_STRING_API

void PrefsCollectionNetwork::ReadAllPrefsL(PrefsModule::PrefsInitInfo *info)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);

	/* DSK-124229, DSK-158389: Accept-Language needs a default value, or
	 * else some sites stop working. */
	OpString langlist; ANCHOR(OpString, langlist);
	langlist.SetL(info->m_user_languages);
	if (!langlist.IsEmpty())
	{
		// Change all underscores to hyphens, as per RFC 5646 (BCP 47).
		uni_char *underscore = langlist.CStr();
		while (underscore && (underscore = uni_strchr(underscore + 1, '_')) != NULL)
		{
			*underscore = '-';
		}

		/* We have a comma-separated language list, we need to add quality
		 * values to it. We do not add a quality value to the first
		 * language (q=1.0 is implied), since this breaks some servers. */
		uni_char *p = uni_strtok(langlist.CStr(), UNI_L(","));
		BOOL seen_english = uni_strni_eq_upper(p, "EN", 3);
		m_default_accept_language.SetL(langlist.CStr());
		int quality = 9; // Start at 0.9

		while ((p = uni_strtok(NULL, UNI_L(","))) != NULL)
		{
			LEAVE_IF_ERROR(m_default_accept_language.AppendFormat(UNI_L(",%s;q=0.%d"), p, quality));
			if (uni_strni_eq_upper(p, "EN", 3))
			{
				seen_english = TRUE; // Seen English, do not add to the tail
			}

			/* DSK-374172: If the platform gives us a very long listing, make
			 * sure we never add more than ten languages (stopping at q=0.1).
			 */
			if (quality > 1)
			{
				-- quality;
				if (quality == 1 && !seen_english)
				{
					/* Make space for en;q=0.1 */
					break;
				}
			}
			else
				break;
		}

		/* Always make sure English is listed by default, otherwise some
		 * pages just stop working. Give English a quality of 0.1 less
		 * than the lowest provided language, with 0.1 being the lowest. */
		if (!seen_english)
		{
			LEAVE_IF_ERROR(m_default_accept_language.AppendFormat(UNI_L(",en;q=0.%d"), quality));
		}
	}

	// Default to English
	if (m_default_accept_language.IsEmpty())
	{
		m_default_accept_language.SetL("en");
	}

	// Use the default value if none was set in ini file.
#ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[AcceptLanguage].section),
	                     m_stringprefdefault[AcceptLanguage].key))
#endif
	{
		m_stringprefs[AcceptLanguage].SetL(m_default_accept_language);
	}

#ifdef ADD_PERMUTE_NAME_PARTS
	// Bug#212532: Add the current country name to the list of host expansion
	// suffixes, unless it is "us".
	// Bug#230607: Add additional domains for some countries.
	// Bug#273771: Even more domains to use.

	uni_char countrycode[3]; /* ARRAY OK 2009-02-26 adame */
	g_op_system_info->GetUserCountry(countrycode);

	if (countrycode[0])
	{
		uni_strlwr(countrycode);

/** Pack two chars in an int. */
#define STR2INT16(x,y) ((x << 8) | y)

		switch (STR2INT16(countrycode[0], countrycode[1]))
		{
		case STR2INT16('a','r'): /* Argentina */
			m_default_hostname_postfix.SetL("com.ar,com,gov.ar,ar,");
			break;

		case STR2INT16('a','u'): /* Australia */
			m_default_hostname_postfix.SetL("com.au,com,gov.au,edu.au,net.au,org.au,au,");
			break;

		case STR2INT16('b','d'): /* Bangladesh */
			m_default_hostname_postfix.SetL("com.bd,");
			break;

		case STR2INT16('b','r'): /* Brazil */
			m_default_hostname_postfix.SetL("com.br,org.br,br,com,edu.br,");
			break;

		case STR2INT16('c','a'): /* Canada */
			m_default_hostname_postfix.SetL("ca,qc.ca,");
			break;

		case STR2INT16('c','n'): /* (Mainland) China */
			m_default_hostname_postfix.SetL("cn,com.cn,edu.cn,com,org.cn,gov.cn,net.cn,");
			break;

		case STR2INT16('c','o'): /* Colombia */
			m_default_hostname_postfix.SetL("com.co,");
			break;

		case STR2INT16('e','g'): /* Egypt */
			m_default_hostname_postfix.SetL("com.eg,com,gov.eg,org.eg,eg,");
			break;

		case STR2INT16('g','b'): /* Great Britain */
		case STR2INT16('u','k'): /* just to make sure */
			m_default_hostname_postfix.SetL("co.uk,com,org.uk,gov.uk,");
			break;

		case STR2INT16('i','d'): /* Indonesia */
			m_default_hostname_postfix.SetL("co.id,id,go.id,or.id,");
			break;

		case STR2INT16('i','l'): /* Israel */
			m_default_hostname_postfix.SetL("co.il,gov.il,org.il,ac.il,com,");
			break;

		case STR2INT16('i','n'): /* India */
			m_default_hostname_postfix.SetL("co.in,com,gov.in,org.in,in,");
			break;

		case STR2INT16('j','p'): /* Japan */
			m_default_hostname_postfix.SetL("co.jp,jp,or.jp,ne.jp,");
			break;

		case STR2INT16('k','r'): /* South Korea */
			m_default_hostname_postfix.SetL("co.kr,go.kr,or.kr,");
			break;

		case STR2INT16('m','e'): /* Montenegro */
			m_default_hostname_postfix.SetL("cg.yu,");
			break;

		case STR2INT16('m','x'): /* Mexico */
			m_default_hostname_postfix.SetL("com.mx,com,mx,");
			break;

		case STR2INT16('m','y'): /* Malaysia */
			m_default_hostname_postfix.SetL("com.my,com,gov.my,my,");
			break;

		case STR2INT16('n','g'): /* Nigeria */
			m_default_hostname_postfix.SetL("com.ng,");
			break;

		case STR2INT16('n','z'): /* New Zeeland */
			m_default_hostname_postfix.SetL("co.nz,com,org.nz,govt.nz,nz,");
			break;

		case STR2INT16('p','e'): /* Peru */
			m_default_hostname_postfix.SetL("com.pe,");
			break;

		case STR2INT16('p','h'): /* Phillipines */
			m_default_hostname_postfix.SetL("com.ph,com,gov.ph,ph,");
			break;

		case STR2INT16('p','k'): /* Pakistan */
			m_default_hostname_postfix.SetL("com.pk,");
			break;

		case STR2INT16('p','l'): /* Poland */
			m_default_hostname_postfix.SetL("pl,com.pl,com,net.pl,edu.pl,org.pl,");
			break;

		case STR2INT16('p','t'): /* Portugal */
			m_default_hostname_postfix.SetL("pt,com.pt,web.pt,com,gov.pt,");
			break;

		case STR2INT16('r','s'): /* Serbia */
			m_default_hostname_postfix.SetL("co.yu,");
			break;

		case STR2INT16('s','a'): /* Saudi-Arabia */
			m_default_hostname_postfix.SetL("com.sa,");
			break;

		case STR2INT16('s','g'): /* Singapore */
			m_default_hostname_postfix.SetL("com.pg,");
			break;

		case STR2INT16('t','h'): /* Thailand */
			m_default_hostname_postfix.SetL("co.th,com,go.th,or.th,ac.th,");
			break;

		case STR2INT16('t','r'): /* Turkey */
			m_default_hostname_postfix.SetL("com.tr,net.tr,gen.tr,org.tr,biz.tr,web.tr,");
			break;

		case STR2INT16('t','w'): /* Taiwan */
			m_default_hostname_postfix.SetL("com.tw,net.tw,edu.tw,org.tw,tw,");
			break;

		case STR2INT16('u','a'): /* Ukraine */
			m_default_hostname_postfix.SetL("com.ua,com,ua,");
			break;

		case STR2INT16('u','s'): /* USA */
			/* no additional strings */
			break;

		case STR2INT16('v','e'): /* Venezuela */
			m_default_hostname_postfix.SetL("com.ve,com,co.ve,");
			break;

		case STR2INT16('v','n'): /* Vietnamn */
			m_default_hostname_postfix.SetL("com.vn,com,vn,");
			break;

		case STR2INT16('z','a'): /* South Africa */
			m_default_hostname_postfix.SetL("co.za,com,gov.za,");
			break;

		default:
			/* use country code directly */
			LEAVE_IF_ERROR(m_default_hostname_postfix.SetConcat(countrycode, UNI_L(",")));
			break;
		}

		/* Add com if not already added */
		if (m_default_hostname_postfix.Find("com,") == KNotFound)
			m_default_hostname_postfix.AppendL("com,");
		/* Add net and org */
		m_default_hostname_postfix.AppendL("net,org");
	}
	if (m_default_hostname_postfix.IsEmpty())
	{
		m_default_hostname_postfix.SetL("com");
	}

	// Use the default value if none was set in ini file.
# ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[HostNameExpansionPostfix].section),
	                     m_stringprefdefault[HostNameExpansionPostfix].key))
# endif
	{
		m_stringprefs[HostNameExpansionPostfix].SetL(m_default_hostname_postfix);
	}
#endif

	// Set up things needed for helper functions
	m_accept_types.SetL(m_stringprefs[AcceptTypes].CStr());
	m_accept_language.SetL(m_stringprefs[AcceptLanguage].CStr());

	// Read proxy settings
	ReadProxiesL(FALSE);

#ifndef PREFS_HOSTOVERRIDE
	ReadUse100ContinueL();
#endif
}

void PrefsCollectionNetwork::ReadProxiesL(BOOL broadcast)
{
	// Cache global object
	OpSystemInfo *sysinfo = g_op_system_info;

	// Read system default settings if none were found in the ini file.
	ReadProxyDefaultL(broadcast, sysinfo, UNI_L("http"), HTTPProxy, &m_default_http_proxy, UseHTTPProxy, &m_default_http_proxy_enabled);
	ReadProxyDefaultL(broadcast, sysinfo, UNI_L("https"), HTTPSProxy, &m_default_https_proxy, UseHTTPSProxy, &m_default_https_proxy_enabled);
#ifdef FTP_SUPPORT
	ReadProxyDefaultL(broadcast, sysinfo, UNI_L("ftp"), FTPProxy, &m_default_ftp_proxy, UseFTPProxy, &m_default_ftp_proxy_enabled);
#endif
#ifdef GOPHER_SUPPORT
	ReadProxyDefaultL(broadcast, sysinfo, UNI_L("gopher"), GopherProxy, &m_default_gopher_proxy, UseGopherProxy, &m_default_gopher_proxy_enabled);
#endif
#ifdef WAIS_SUPPORT
	ReadProxyDefaultL(broadcast, sysinfo, UNI_L("wais"), WAISProxy, &m_default_wais_proxy, UseWAISProxy, &m_default_wais_proxy_enabled);
#endif
#ifdef SOCKS_SUPPORT
	ReadProxyDefaultL(broadcast, sysinfo, UNI_L("socks"), SOCKSProxy, &m_default_socks_proxy, UseSOCKSProxy, &m_default_socks_proxy_enabled);
#endif

	// Read settings for the automatic proxy configuration URL.
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	sysinfo->GetAutoProxyURLL(&m_default_auto_proxy_url, &m_default_auto_proxy_enabled);
# ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[AutomaticProxyConfigURL].section),
	                     m_stringprefdefault[AutomaticProxyConfigURL].key))
# endif
	{
		m_stringprefs[AutomaticProxyConfigURL].SetL(m_default_auto_proxy_url);
# ifdef PREFS_WRITE
		if (broadcast)
		{
			BroadcastChange(AutomaticProxyConfigURL, m_stringprefs[AutomaticProxyConfigURL]);
		}
# endif
	}

# ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_integerprefdefault[AutomaticProxyConfig].section),
	                     m_integerprefdefault[AutomaticProxyConfig].key))

# endif
	{
		m_intprefs[AutomaticProxyConfig] = m_default_auto_proxy_enabled != 0;
#ifdef PREFS_WRITE
		if (broadcast)
		{
			OpPrefsCollection::BroadcastChange(AutomaticProxyConfig, m_intprefs[AutomaticProxyConfig]);
		}
#endif
	}
#endif

	// Read the proxy exception list.
	sysinfo->GetProxyExceptionsL(&m_default_proxy_exceptions, &m_default_use_proxy_exceptions);
#ifdef PREFS_VALIDATE
	if (!m_default_proxy_exceptions.IsEmpty())
	{
		OpString *new_proxy_exceptions;
		if (CheckConditionsL(NoProxyServers, m_default_proxy_exceptions, &new_proxy_exceptions, NULL))
		{
			m_default_proxy_exceptions.TakeOver(*new_proxy_exceptions);
			OP_DELETE(new_proxy_exceptions);
		}
	}
#endif

#ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[NoProxyServers].section),
	                     m_stringprefdefault[NoProxyServers].key))
#endif
	{
		m_stringprefs[NoProxyServers].SetL(m_default_proxy_exceptions);
#ifdef PREFS_WRITE
		if (broadcast)
		{
			BroadcastChange(NoProxyServers, m_stringprefs[NoProxyServers]);
		}
#endif
	}

#ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_integerprefdefault[HasNoProxyServers].section),
	                     m_integerprefdefault[HasNoProxyServers].key))

#endif
	{
		m_intprefs[HasNoProxyServers] = m_default_use_proxy_exceptions != 0;
#ifdef PREFS_WRITE
		if (broadcast)
		{
			OpPrefsCollection::BroadcastChange(HasNoProxyServers, m_intprefs[HasNoProxyServers]);
		}
#endif
	}
}

void PrefsCollectionNetwork::ReadProxyDefaultL(BOOL broadcast, class OpSystemInfo *sysinfo, const uni_char *protocol, stringpref defpref, OpString *defstring, integerpref enablepref, BOOL *enableval)
{
	// Read and remember the defaults.
	int enabled;
	sysinfo->GetProxySettingsL(protocol, enabled, *defstring);
	*enableval = enabled != 0;

	// Set the current value unless the user has overridden it
#ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_integerprefdefault[enablepref].section),
	                     m_integerprefdefault[enablepref].key))
#endif
	{
		m_intprefs[enablepref] = enabled != 0;
#ifdef PREFS_WRITE
		if (broadcast)
		{
			OpPrefsCollection::BroadcastChange(enablepref, enabled != 0);
		}
#endif
	}

#ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[defpref].section),
	                     m_stringprefdefault[defpref].key))
#endif
	{
		m_stringprefs[defpref].SetL(*defstring);
#ifdef PREFS_WRITE
		if (broadcast)
		{
			BroadcastChange(defpref, *defstring);
		}
#endif
	}
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionNetwork::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionNetwork::CheckConditionsL(int which, int *value, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case UABaseId:
#ifdef PREFS_HOSTOVERRIDE
		if (host && *host)
		{
			// Allow overrides to override completely
			if (*value < 0 ||
			    (static_cast<enum UA_BaseStringId>(*value) >= UA_Mozilla_478
#ifdef URL_TVSTORE_UA
				&& static_cast<enum UA_BaseStringId>(*value) != UA_TVStore
#endif
				))
				*value = m_integerprefdefault[UABaseId].defval;
		}
		else
#endif // PREFS_HOSTOVERRIDE
		{
			// Do not allow the default UA string to be overridden completely,
			// i.e always require the strings that say "Opera" in them.
			if (*value <= 0 || (*value >= UA_MozillaOnly && *value != UA_OperaDesktop
#ifdef URL_TVSTORE_UA
				&& *value != UA_TVStore
#endif
				))
				*value = m_integerprefdefault[UABaseId].defval;
		}
		break;

	case FollowedLinkExpireHours:
	case FollowedLinkExpireDays:
		// Max value for both hours and days are 99
		if (*value < 0 || *value > 99) *value = 99;
		break;

	case DiskCacheSize:
	case DiskCacheBufferSize:
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	case OperatorDiskCacheSize:
#endif
		if (*value >= 1000000)
			*value = 999999;
		break;

	case MaxConnectionsServer:
		if (*value > MAX_CONNECTIONS_SERVER)
			*value = MAX_CONNECTIONS_SERVER;
		else if (*value < 1)
			*value = 1;
		break;

	case MaxPersistentConnectionsServer:
		if (*value > MAX_CONNECTIONS_SERVER)
			*value = MAX_CONNECTIONS_SERVER;
		else if (*value < 1)
			*value = 1;
		break;

	case MaxConnectionsTotal:
		if (*value > MAX_CONNECTIONS_TOTAL)
			*value = MAX_CONNECTIONS_TOTAL;
		else if (*value < 1)
			*value = 1;
		break;

	case NetworkBufferSize:
		if (*value > 9999)
			*value = 9999;
		else if (*value < 1)
			*value = 1;
		break;

#ifdef _SSL_SUPPORT_
 	case SecurityPasswordLifeTime:
 		if (*value < 0)
 			*value = 5;
		break;
#endif

#ifdef _SSL_SUPPORT_
	case MinimumSecurityLevel:
		// Do not allow to go below the default level
		if (*value < m_integerprefdefault[MinimumSecurityLevel].defval)
			*value = m_integerprefdefault[MinimumSecurityLevel].defval;
		break;
#endif

#ifdef _EMBEDDED_BYTEMOBILE
	case EmbeddedOptimizationLevel:
		if (*value < 0)
			*value = 0;
		else if (*value > 5)
			*value = 5;
#endif // _EMBEDDED_BYTEMOBILE

#ifdef _ASK_COOKIE
	case ShowCookieDomainErr:
	case CookiePathErrMode:
# ifndef PUBSUFFIX_ENABLED
	case DontShowCookieNoIPAddressErr:
# endif
		break; // Nothing to do.
#endif


#ifdef PREFS_HAVE_DISABLED_COOKIE_STATE
	case DisabledCookieState:
#endif
#ifdef _USE_PREAUTHENTICATION_
	case AllowPreSendAuthentication:
#endif
	case TagUrlsUsingPasswordRelatedCookies:
	case EnableCookiesDNSCheck:
	case EnableCookiesV2:
#ifdef ADD_PERMUTE_NAME_PARTS
	case CheckPermutedHostNames:
	case CheckHostOnLocalNetwork:
	case EnableHostNameWebLookup:
#endif
#ifdef FTP_SUPPORT
	case UseAbsoluteFTPPath:
#endif
	case UseUTF8Urls:
#ifdef HTTP_CONNECTION_TIMERS
	case HTTPResponseTimeout:
	case HTTPIdleTimeout:
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	case EnableNTLM:
#endif
#ifdef _SSL_USE_SMARTCARD_
	case StrictSmartCardSecurityLevel:
	case SmartCardTimeOutMinutes:
	case AutomaticallyUseSingleAvailableSmartcard:
	case WarnIfNoSmartCardInReader:
#endif
#ifdef PREFS_HAVE_FORCE_CD
	case ForceCD:
#endif
	case DecodeAll:
#ifdef PREFS_AUTOMATIC_RAMCACHE
	case AutomaticRamCache:
#endif
#ifdef PREFS_HAVE_MAX_COOKIES
	case MaxTotalCookies:
#endif
	case DoNotTrack:
	case NoLocalFile:
	case OfflineMode:
#ifdef PREFS_HAVE_RIM_MDS_BROWSER_MODE
	case RIMMDSBrowserMode:
#endif
	case WarnInsecureFormSubmit:
	case EnableClientPull:
	case EnableClientRefresh:
	case ClientRefreshToSame:
	case TrustServerTypes:
	case CookiesEnabled:
	case ReferrerEnabled:
#ifdef _ASK_COOKIE
	case DisplayReceivedCookies:
	case AcceptCookiesSessionOnly:
#endif //_ASK_COOKIE
#ifdef PREFS_HAVE_UAMODE
	case UAMode:
#endif
	case NoConnectionKeepAlive:
#ifdef SYNCHRONOUS_HOST_RESOLVING
	case SyncDNSLookup:
#endif
	case AlwaysCheckRedirectChanged:
	case AlwaysCheckRedirectChangedImages:
	case AlwaysCheckNeverExpireGetQueries:
	case AlwaysReloadHTTPSInHistory:
	case CacheHTTPSAfterSessions:
	case CheckExpiryHistory:
	case CheckExpiryLoad:
#ifdef SUPPORT_THREADED_FILEWRITER
	case SetFileWriterThreadPriority:
	case FileWriterThreadPriority:
	case FileWriterCacheSize:
#endif
#ifdef DISK_CACHE_SUPPORT
	case CacheToDisk:
	case MultimediaStreamSize:
	case MultimediaStreamAlways:
	case MultimediaStreamRAM:
#endif
	case CacheDocs:
	case CacheFigs:
	case CacheOther:
#ifdef DISK_CACHE_SUPPORT
	case EmptyCacheOnExit:
#endif
	case CheckDocModification:
	case CheckFigModification:
	case CheckOtherModification:
	case DocExpiry:
	case FigsExpiry:
	case OtherExpiry:
	case HasNoProxyServers:
	case HasUseProxyLocalNames:
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	case AutomaticProxyConfig:
#endif
	case EnableHTTP11ForProxy:
	case EnableStartWithHTTP11ForProxy:
	case UseHTTPProxy:
	case UseHTTPSProxy:
#ifdef URL_PER_SITE_PROXY_POLICY
	case EnableProxy:
#endif
#ifdef FTP_SUPPORT
	case UseFTPProxy:
#endif
#ifdef GOPHER_SUPPORT
	case UseGopherProxy:
#endif
#ifdef WAIS_SUPPORT
	case UseWAISProxy:
#endif
#ifdef SOCKS_SUPPORT
	case UseSOCKSProxy:
#endif
	case UseExtraIdleConnections:
	case OpenIdleConnectionOnClose:
	case EnablePipelining:
#ifdef _EMBEDDED_BYTEMOBILE
	case EmbeddedBytemobile:
	case EmbeddedPrediction:
	case EmbeddedImageResize:
#endif
#ifdef _BYTEMOBILE
	case BytemobileOptimizationEnabled:
#endif
#ifdef _SSL_SUPPORT_
	case CryptoMethodOverride:
#ifdef SSL_2_0_SUPPORT
	case EnableSSL2_X:
#endif // SSL_2_0_SUPPORT
	case EnableSSL3_0:
	case EnableSSL3_1:
	case EnableTLS1_1:
#ifdef _SUPPORT_TLS_1_2
	case EnableTLS1_2:
#endif // _SUPPORT_TLS_1_2
	case WarnAboutImportedCACertificates:
#endif
#if defined M2_SUPPORT || defined WAND_SUPPORT
	case UseParanoidMailpassword:
#endif
	case UseOCSPValidation:
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	case UseCRLValidationForSSL:
#endif
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	case UseSSLFalseStart:
#endif
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	case UseStrictTransportSecurity:
#endif
	case CacheHTTPS:
#ifdef PREFS_HOSTOVERRIDE
	case Use100Continue:
#endif
#ifdef SOCKS_SUPPORT
	case RemoteSocksDNS:
#endif
#ifdef _BITTORRENT_SUPPORT_
	case BTListenPort:
	case EnableBitTorrent:
	case BTWarningDialog:
	case BTBandwidthRestrictionMode:
	case BTMaxDownloadRate:
	case BTMaxUploadRate:
#endif
#if defined(VISITED_PAGES_SEARCH) && !defined(HISTORY_SUPPORT)
	case VSMaxIndexSize:
#endif
#ifdef SMILEY_SUPPORT
	case UseSmileyImages:
#endif
#ifdef TRUST_RATING
	case EnableTrustRating:
#endif
#ifdef URL_FILTER
	case EnableContentBlocker:
#endif
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	case StrictEVMode:
#endif // SSL_CHECK_EXT_VALIDATION_POLICY
#ifdef PREFS_HAVE_PREFER_IPV6
	case PreferIPv6:
#endif
	case AllowCrossNetworkNavigation:
#ifdef WEB_TURBO_MODE
	case UseWebTurbo:
#endif
#ifdef MEDIA_PLAYER_SUPPORT
	case MediaCacheSize:
#endif
#ifdef DNS_PREFETCHING
	case DNSPrefetching:
#endif // DNS_PREFETCHING
	case BlockIDNAInvalidDomains:
		break; // Nothing to do.

	case HoldOnFillingPipelines:
		break;

	case IPAddressSecondsToLive:
		if (*value < 0)
			*value = 0;
		break;

#if defined _NATIVE_SSL_SUPPORT_ && defined SELFTEST
	case TestLongbatch:
		break;
#endif
#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
	case TimeOfLastCertUpdateCheck:
		break;
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
#ifdef USE_SPDY
	case SpdyInitialWindowSize:
	case SpdySettingsPersistenceTimeout:
	case SpdyPingTimeout:
		if (*value < 0)
			*value = 0;
		break;
	case SpdyAlwaysForSSL:
	case SpdyWithoutSSL:
	case UseSpdyForHTTPProxy:
		if (*value < 0 || *value > 2)
			*value = 0;
		break;
	case UseSpdy2:
	case UseSpdy3:
	case UseSpdyForTurbo:
	case SpdySettingsPersistence:
	case SpdyPingEnabled:
#endif // USE_SPDY
		break;
	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionNetwork::CheckConditionsL(int which,
                                              const OpStringC &invalue,
                                              OpString **outvalue,
                                              const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case HTTPProxy:
	case HTTPSProxy:
#ifdef FTP_SUPPORT
	case FTPProxy:
#endif
#ifdef GOPHER_SUPPORT
	case GopherProxy:
#endif
#ifdef WAIS_SUPPORT
	case WAISProxy:
#endif
#ifdef SOCKS_SUPPORT
	case SOCKSProxy:
#endif
		return StripServerL(invalue, outvalue);
		break;

	case NoProxyServers:
		return MassageNoProxyServersL(invalue, outvalue);
		break;

	case IdnaWhiteList:
		// Make sure the list is the proper form with initial and
		// trailing colon.
		if (!invalue.IsEmpty() &&
		    (':' != invalue[0] || ':' != invalue[invalue.Length() - 1]))
		{
			OpStackAutoPtr<OpString> newval(OP_NEW_L(OpString, ()));
			if (':' != invalue[0])
				newval->SetL(":");
			newval->AppendL(invalue);
			if (':' != invalue[invalue.Length() - 1])
				newval->AppendL(":");
			*outvalue = newval.release();
			return TRUE;
		}

#ifdef COMPONENT_IN_UASTRING_SUPPORT
	case IspUserAgentId:
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	case AutomaticProxyConfigURL:
#endif
	case AcceptTypes:
#ifdef ADD_PERMUTE_NAME_PARTS
	case HostNameExpansionPrefix:
	case HostNameExpansionPostfix:
	case HostNameWebLookupAddress:
#endif
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	case OutOfCoverageFileName:
	case NeverFlushTrustedServers:
#endif
#ifdef HELP_SUPPORT
	case HelpUrl:
#endif
	case PermittedPorts:
#if defined PREFS_HAVE_EBO_GUID || defined _EMBEDDED_BYTEMOBILE
	case BytemobileGUID:
#endif
		break; // Nothing to do.

#ifdef SOCKS_SUPPORT
	case SOCKSUser:
	case SOCKSPass: // each of username and password are restricted to 100 chars
		if (invalue.Length() > 100)
		{
			OpStackAutoPtr<OpString> newval(OP_NEW_L(OpString, ()));
			newval->SetL(invalue, 100);
			*outvalue = newval.release();
			return TRUE;
		}
		return FALSE;
#endif

	case AcceptLanguage:
#ifdef UPGRADE_SUPPORT
		// Remove underscores, if any (added by Opera 9.00)
		if (invalue.FindFirstOf('_') != KNotFound)
		{
			OpStackAutoPtr<OpString> newval(OP_NEW_L(OpString, ()));
			newval->SetL(invalue);
			uni_char *p;
			while ((p = uni_strchr(newval->CStr(), '_')) != NULL)
			{
				*p = '-';
			}
			*outvalue = newval.release();
			return TRUE;

		}
#endif
		break;
#ifdef WEB_TURBO_MODE
	case WebTurboBypassURLs:
#ifndef _NATIVE_SSL_SUPPORT_
	case MasterPasswordCheckCodeAndSalt:
#endif // !_NATIVE_SSL_SUPPORT_
		break;
#endif
#if defined _NATIVE_SSL_SUPPORT_ && defined SELFTEST
	case LongbatchURL:
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

/** Adjust a server name so that it conforms to validity rules.
  * For information on parameters and return value, see CheckConditionsL
  * documentation.
  */
BOOL PrefsCollectionNetwork::StripServerL(const OpStringC &source,
                                          OpString **target)
{
	int server_len = source.Length();

	if (0 == server_len)
	{
		return FALSE;
	}

	OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
	uni_char *tmp = newvalue->ReserveL(server_len + 1);

	tmp[0] = 0;
	int i = 0;
	int j = 0;

	while (i < server_len)
	{
		if (uni_isspace(source[i]))
		{
			i++;
		}
		else
		{
			if (source[i] == ':' && i + 1 < server_len && source[i+1] == '/')
			{
				while (i < server_len)
				{
					if (source[i] == ':' || source[i] == '/')
						i++;
					else
						break;
				}
				j = 0;
			}
			else
			{
				if (source[i] == '/')
				{
					i++;
				}
				else
				{
					tmp[j++] = source[i++];
				}
			}
		}
	}
	tmp[j] = 0;

	if(tmp[0] == '\0')
	{
		newvalue->Empty();
	}

	*target = newvalue.release();
	return TRUE;
}

/** Perform fix-ups to the list of proxy servers given so that it is in a format
  * suited for internal storage.
  * For information on parameters and return value, see CheckConditionsL
  * documentation.
  */
BOOL PrefsCollectionNetwork::MassageNoProxyServersL(const OpStringC &servers,
                                                    OpString **target)
{
	// Split and re-assemble
	if (!servers.IsEmpty())
	{
		OpStackAutoPtr<OpString> NoProxyServersTmp(OP_NEW_L(OpString, ()));
		NoProxyServersTmp->SetL(servers);

		OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
		newvalue->ReserveL(servers.Length() + 2);

		uni_char *tmp;
		BOOL anything = FALSE;

		const uni_char * const separator = UNI_L(" \n\t,;");
		for (tmp = uni_strtok(NoProxyServersTmp->CStr(), separator); tmp;
		     tmp = uni_strtok(NULL, separator))
		{
			OpString *servername;
			if (StripServerL(tmp, &servername))
			{
				if (!servername->IsEmpty())
				{
					newvalue->AppendL(*servername);
					newvalue->AppendL(",");
					anything = TRUE;
				}

				OP_DELETE(servername);
			}
		}

		// Kill final comma.
		if (anything)
			newvalue->CStr()[newvalue->Length() - 1] = 0;

		*target = newvalue.release();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Helper function used by the "get proxy" functions. Returns the proxy server
 * for the specified protocol if, and only if, it is enabled. Checks for
 * overridden settings.
 *
 * @param host Host name to check proxy for,
 * @param boolpref The protocol's boolean preference (enabled or not).
 * @param proxypref The protocol's string preference (host name and port).
 * @return The proxy host name and port, NULL if none defined or disabled.
 */
const uni_char *PrefsCollectionNetwork::GetProxyServerHelper(
	const uni_char *host, integerpref boolpref, stringpref proxypref)
{
	// Retrieve default value for proxy enabled
	int proxyenabled = m_intprefs[boolpref];

#ifdef PREFS_HOSTOVERRIDE
	if (host)
	{
		OverrideHost *override =
			static_cast<OverrideHost *>(FindOverrideHost(host));

		if (override)
		{
			// Found a matching override
			int overriddenproxyenabled;
			if (override->GetOverridden(boolpref, overriddenproxyenabled))
			{
				proxyenabled = overriddenproxyenabled;
			}

			// If proxies are enabled, either by default or for this host,
			// check if also the proxy host is overridden, and if so return that
			// value.
			if (proxyenabled)
			{
				const OpStringC *overriddenproxyhost;
				if (override->GetOverridden(proxypref, &overriddenproxyhost))
				{
					// Return the overridden proxy host string.
					return overriddenproxyhost->CStr();
				}
			}
		}

		// Fall back to standard code, but with the proxyenabled flag perhaps
		// overridden. This allows a default proxy to be set to disabled by
		// default and only enabled for certain hosts.
	}
#endif // PREFS_HOSTOVERRIDE

	if (proxyenabled)
	{
		// Return standard proxy setting, if enabled.
		return m_stringprefs[proxypref].CStr();
	}
	else
	{
		// Proxy disabled, either globally or for this host.
		return NULL;
	}
}

#ifdef PREFS_WRITE
void PrefsCollectionNetwork::BroadcastChange(int pref, const OpStringC &newvalue)
{
	// Check if we need to do any updates for our helper functions
	switch (stringpref(pref))
	{
	case AcceptTypes:
		OpStatus::Ignore(m_accept_types.Set(newvalue.CStr()));
		break;

	case AcceptLanguage:
		OpStatus::Ignore(m_accept_language.Set(newvalue.CStr()));
		break;
	}

	OpPrefsCollection::BroadcastChange(pref, newvalue);
}
#endif // PREFS_WRITE

#ifndef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionNetwork::GetUse100Continue(const uni_char *host)
{
	// We check our local (fast?) cache.
	OP_ASSERT(m_use100Continue); // Cache should be set up in init

	return m_use100Continue->FindEntry(host) != NULL;
}
#endif // !PREFS_HOSTOVERRIDE

#if !defined PREFS_HOSTOVERRIDE && defined PREFS_WRITE
void PrefsCollectionNetwork::WriteUse100ContinueL(const OpStringC &host, BOOL use)
{
	// When we find a buggy server, we write its name. We do not need to
	// associate the name with a value, since we remove the value if we
	// find it to again be compatible.

	if (use)
	{
# ifdef PREFS_READ
		OP_STATUS rc = m_reader->WriteStringL(UNI_L("Non-Compliant Server 100 Continue"), host, NULL);
		if (OpStatus::IsSuccess(rc))
# endif
		{
			m_use100Continue->SetL(host, NULL);
		}
	}
	else
	{
# ifdef PREFS_READ
		m_reader->DeleteKeyL(UNI_L("Non-Compliant Server 100 Continue"), host);
# endif
		m_use100Continue->DeleteEntry(host);
	}
}
#endif // !PREFS_HOSTOVERRIDE && PREFS_WRITE

#ifndef PREFS_HOSTOVERRIDE
void PrefsCollectionNetwork::ReadUse100ContinueL()
{
# ifdef PREFS_READ
	// Since the 100 continue is checked for each hostname, we cache it
	// in an internal PrefsSection (so we don't have to search through
	// all sections each time, and don't have to keep the file loaded)
	m_use100Continue =
		m_reader->ReadSectionInternalL(UNI_L("Non-Compliant Server 100 Continue"));
# endif

	if (NULL == m_use100Continue)
	{
		// No data was found.
		m_use100Continue = OP_NEW_L(PrefsSectionInternal, ());
		m_use100Continue->ConstructL(UNI_L("100 continue cache"));
	}
}
#endif // !PREFS_HOSTOVERRIDE

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
BOOL PrefsCollectionNetwork::IsAutomaticProxyOn()
{
	if (m_intprefs[AutomaticProxyConfig] && m_APCEnabled)
	{
		if (!m_stringprefs[AutomaticProxyConfigURL].IsEmpty())
		{
			return TRUE;
		}
		m_APCEnabled = FALSE;
	}
	return FALSE;
}
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

time_t PrefsCollectionNetwork::GetFollowedLinksExpireTime()
{
	return ((time_t)m_intprefs[PrefsCollectionNetwork::FollowedLinkExpireDays] * 24 +
	        m_intprefs[PrefsCollectionNetwork::FollowedLinkExpireHours]) * 60 * 60;
}
