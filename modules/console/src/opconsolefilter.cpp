/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
** Marius Blomli
*/

#include "core/pch.h"

#ifdef OPERA_CONSOLE

#include "modules/console/opconsoleengine.h"
#include "modules/console/opconsolefilter.h"
#include "modules/formats/argsplit.h"
#include "modules/url/url_api.h"
#include "modules/url/url_sn.h"
#include "modules/util/opstrlst.h"

/** Severity to use to indicate that the default value should be used. */
static const OpConsoleEngine::Severity use_default_severity =
	static_cast<OpConsoleEngine::Severity>(-1);

OpConsoleFilter::OpConsoleFilter(OpConsoleEngine::Severity default_severity)
	: m_default_severity(default_severity),
	  m_lowest_id(0)
{
	for (int i = 0; i < static_cast<int>(OpConsoleEngine::SourceTypeLast); ++ i)
	{
		m_severities[i] = use_default_severity;
	}
}

#ifdef CON_ENABLE_SERIALIZATION
OP_STATUS OpConsoleFilter::SetFilter(const OpStringC &serialized, OpConsoleEngine::Severity default_severity)
{
	// Overwrite existing filter values.
	OpString copy;
	OP_STATUS rc = copy.Set(serialized.CStr());
	if (OpStatus::IsError(rc))
		return rc;

	// Cache global pointers
#ifdef PREFS_HOSTOVERRIDE
	URL_API *url_api = g_url_api;
#endif

	// Parse the string list
	UniParameterList filterlist;
	filterlist.SetValue(serialized.CStr(), PARAM_SEP_COMMA);

	// Iterate over the parsed items
	UniParameters *filter = filterlist.First();
	while (filter)
	{
		// Keyword or domain
		OpStringC8 name(filter->GetName());

		// Severity
		OpStringC8 param(filter->GetValue());
		OpConsoleEngine::Severity sev = default_severity;
		if (!param.IsEmpty())
		{
			sev = static_cast<OpConsoleEngine::Severity>(op_atoi(param.CStr()));
			if (sev > OpConsoleEngine::DoNotShow)
			{
				sev = default_severity;
			}
		}

#ifdef PREFS_HOSTOVERRIDE
		if ('@' == name[0])
		{
			// Host override. Convert name back to Unicode and remember.
			OP_STATUS rc;
			ServerName *sn = url_api->GetServerName(name.CStr() + 1, TRUE);
			if (sn)
			{
				rc = SetDomain(sn->UniName(), sev);
			}
			else
			{
				rc = OpStatus::ERR_NO_MEMORY;
			}
			if (OpStatus::IsError(rc))
			{
				return rc;
			}
		}
		else
#endif
		{
			// Normal entry
			for (int i = 0; i < static_cast<int>(OpConsoleEngine::SourceTypeLast); ++ i)
			{
				if (0 == name.Compare(g_console->GetSourceKeyword(static_cast<OpConsoleEngine::Source>(i))))
				{
					// Found the keyword in the index.
					OpConsoleEngine::Source src =
						static_cast<OpConsoleEngine::Source>(i);
					m_severities[src] = sev;
					break;
				}
			}
		}

		filter = filter->Suc();
	}

	return rc;
}
#endif // CON_ENABLE_SERIALIZATION

#ifdef CON_ENABLE_SERIALIZATION
OP_STATUS OpConsoleFilter::GetFilter(OpString &target, BOOL severities) const
{
	OP_STATUS rc = OpStatus::OK;
	target.Empty();

	// Loop through the active filters and record their status
	for (int i = 0; i < static_cast<int>(OpConsoleEngine::SourceTypeLast); ++ i)
	{
		if (m_severities[i] != use_default_severity)
		{
			if (!target.IsEmpty())
			{
				rc = target.Append(",");
				if (OpStatus::IsError(rc))
				{
					break;
				}
			}

			OP_ASSERT(g_console->GetSourceKeyword(static_cast<OpConsoleEngine::Source>(i)) != NULL);
			rc = target.Append(g_console->GetSourceKeyword(static_cast<OpConsoleEngine::Source>(i)));

			if (OpStatus::IsSuccess(rc) && severities)
			{
				OP_ASSERT(m_severities[i] >= 0 && m_severities[i] <= 9);
				char tailstring[3] =
				{ '=', '0' + static_cast<int>(m_severities[i]) };

				rc = target.Append(tailstring, 2);
			}

			// Fail on error
			if (OpStatus::IsError(rc))
			{
				break;
			}
		}
	}

#ifdef PREFS_HOSTOVERRIDE
	ConsoleOverrideHost *p =
		static_cast<ConsoleOverrideHost *>(m_overrides.First());
	while (p)
	{
		rc = target.AppendFormat(UNI_L("%s@%s=%d"),
			(target.IsEmpty() ? UNI_L("") : UNI_L(",")),
			p->Host(),
			static_cast<int>(p->Get()));

		// Fail on error
		if (OpStatus::IsError(rc))
		{
			break;
		}

		p = static_cast<ConsoleOverrideHost *>(p->Suc());
	}
#endif

	return rc;
}
#endif // CON_ENABLE_SERIALIZATION

OP_STATUS OpConsoleFilter::Duplicate(const OpConsoleFilter *filter)
{
	// Recreate all the contents from the other filter
	op_memcpy(m_severities, filter->m_severities, sizeof m_severities);

	// Copy other properties
	m_default_severity = filter->GetDefaultSeverity();
	m_lowest_id = filter->GetLowestId();

#ifdef PREFS_HOSTOVERRIDE
	// Copy overrides
	ConsoleOverrideHost *p =
		static_cast<ConsoleOverrideHost *>(filter->m_overrides.First());
	while (p)
	{
		RETURN_IF_ERROR(SetDomain(p->Host(), p->Get()));
		p = static_cast<ConsoleOverrideHost *>(p->Suc());
	}
#endif

	return OpStatus::OK;
}

OP_STATUS OpConsoleFilter::SetOrReplace(OpConsoleEngine::Source src, OpConsoleEngine::Severity sev)
{
	if (src >= OpConsoleEngine::SourceTypeLast)
		return OpStatus::ERR_OUT_OF_RANGE;

	OP_ASSERT(sev >= 0 && sev <= 9);
	m_severities[static_cast<int>(src)] = sev;
	return OpStatus::OK;
}

void OpConsoleFilter::SetAllExistingSeveritiesTo(OpConsoleEngine::Severity new_severity)
{
	for (int i = 0; i < static_cast<int>(OpConsoleEngine::SourceTypeLast); ++ i)
	{
		if (m_severities[i] != use_default_severity)
		{
			m_severities[i] = new_severity;
		}
	}
}

#ifdef PREFS_HOSTOVERRIDE
OP_STATUS OpConsoleFilter::SetDomain(const uni_char *domain, OpConsoleEngine::Severity sev)
{
	// Find out if there already is a setting for this domain
	ConsoleOverrideHost *override = NULL;
	TRAPD(rc, override = static_cast<ConsoleOverrideHost *>(FindOrCreateOverrideHostL(domain, TRUE)));
	if (OpStatus::IsError(rc) || !override)
	{
		return rc;
	}

	// Set the value
	override->Set(sev);
	return OpStatus::OK;
}
#endif

#ifdef PREFS_HOSTOVERRIDE
OP_STATUS OpConsoleFilter::RemoveDomain(const uni_char *domain)
{
	TRAPD(rc, RemoveOverridesL(domain));
	return rc;
}
#endif

#ifdef PREFS_HOSTOVERRIDE
OpConsoleFilter::ConsoleOverrideHost *OpConsoleFilter::FindOverrideHost(const OpConsoleEngine::Message *message) const
{
	OP_ASSERT(message);
	int colonslashslash;
	if (message && !message->url.IsEmpty() && !m_overrides.Empty()
	    && (colonslashslash = message->url.Find("://")) != KNotFound)
	{
		// Found a domain name
		const uni_char *p = message->url.CStr() + colonslashslash + 3;
		const uni_char *end = uni_strchr(p, '/');
		size_t len = end ? end - p : uni_strlen(p);

		MemoryManager *memman = g_memory_manager;
		uni_char *domain = reinterpret_cast<uni_char *>(memman->GetTempBuf2k());
		uni_strlcpy(domain, p, MIN(len + 1, UNICODE_DOWNSIZE(memman->GetTempBuf2kLen())));
		return static_cast<ConsoleOverrideHost *>(OpOverrideHostContainer::FindOverrideHost(domain, FALSE));
	}

	return NULL;
}
#endif

#ifdef PREFS_HOSTOVERRIDE
OpConsoleEngine::Severity OpConsoleFilter::Get(const uni_char *domain, BOOL exact) const
{
	ConsoleOverrideHost *override =
		static_cast<ConsoleOverrideHost *>
		(OpOverrideHostContainer::FindOverrideHost(domain, exact));
	return override ? override->Get() : m_default_severity;
}
#endif

void OpConsoleFilter::Clear()
{
	for (int i = 0; i < static_cast<int>(OpConsoleEngine::SourceTypeLast); ++ i)
	{
		m_severities[i] = use_default_severity;
	}
}

OpConsoleEngine::Severity OpConsoleFilter::Get(OpConsoleEngine::Source src) const
{
	if (src < OpConsoleEngine::SourceTypeLast &&
	    m_severities[static_cast<int>(src)] != use_default_severity)
	{
		return m_severities[static_cast<int>(src)];
	}

	return m_default_severity;
}

BOOL OpConsoleFilter::DisplayMessage(unsigned int id, const OpConsoleEngine::Message *message) const
{
#ifdef PREFS_HOSTOVERRIDE
	if (id <= m_lowest_id)
	{
		return FALSE;
	}

	ConsoleOverrideHost *override = FindOverrideHost(message);
	return (override ? override->Get() : Get(message->source)) <= message->severity;
#else
	// KISS
	return id > m_lowest_id && Get(message->source) <= message->severity;
#endif
}

#ifdef PREFS_HOSTOVERRIDE
OpString_list *OpConsoleFilter::GetOverriddenHosts()
{
	// Check the size of the table. No need to do anything if it is empty.
	UINT entries = m_overrides.Cardinal();
	if (entries)
	{
		OpString_list *list = OP_NEW(OpString_list, ());
		if (list && OpStatus::IsSuccess(list->Resize(entries)))
		{
			// Populate with the list of overridden hosts.
			ConsoleOverrideHost *p =
				static_cast<ConsoleOverrideHost *>(m_overrides.First());

			OP_STATUS rc = OpStatus::OK;
			unsigned long i = 0;
			while (p && OpStatus::IsSuccess(rc))
			{
				rc = list->Item(i ++).Set(p->Host());
				p = static_cast<ConsoleOverrideHost *>(p->Suc());
			}
			if (OpStatus::IsSuccess(rc))
			{
				// Everything worked fine
				return list;
			}
		}

		// Error handling.
		OP_DELETE(list);
	}
	return NULL;
}
#endif

#ifdef PREFS_HOSTOVERRIDE
OpOverrideHost *OpConsoleFilter::CreateOverrideHostObjectL(const uni_char *host, BOOL from_user)
{
	OpStackAutoPtr<ConsoleOverrideHost> newobj(OP_NEW_L(ConsoleOverrideHost, ()));
	newobj->ConstructL(host, from_user);
	return newobj.release();
}
#endif

#endif
