/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
** Marius Blomli
*/

#if !defined OPCONSOLEFILTER_H && defined OPERA_CONSOLE
#define OPCONSOLEFILTER_H

#include "modules/console/opconsoleengine.h"
#ifdef PREFS_HOSTOVERRIDE
# include "modules/prefs/prefsmanager/hostoverride.h"
#endif

class OpString_list;

/**
 * Class that defines the filtering of the console messages. The class is
 * responsible for keeping track of the settings for each source, and
 * do comparisons when necessary.
 *
 * The internals of the class is a list holding severity levels for each
 * message source, and a default severity. If a source with a severity
 * level is added, the filter will use that severity for that source
 * all other sources will have the default severity level.
 */
class OpConsoleFilter
#ifdef PREFS_HOSTOVERRIDE
	: protected OpOverrideHostContainer
#endif
{
public:
	/**
	 * Constructor.
	 * @param default_severity The default severity that is given to all sources
	 * that are not explicitly set.
	 */
	OpConsoleFilter(OpConsoleEngine::Severity default_severity = OpConsoleEngine::Critical);

#ifdef CON_ENABLE_SERIALIZATION
	/**
	 * Set filter from preference string (unserialize). The string
	 * is a comma separated list of sources, optionally with a
	 * specified severity. If the severity is unspecified, it will
	 * use the severity specified in the default_severity parameter.
	 *
	 * @param serialized A Serialized filter.
	 * @param default_severity Default severity for filters.
	 * @return Status of the operation.
	 */
	OP_STATUS SetFilter(const OpStringC &serialized,
	                    OpConsoleEngine::Severity default_severity = OpConsoleEngine::Information);

	/**
	 * Get filter preference string from current settings (serialize).
	 * This output string represents the current filter settings in a
	 * way that can be used with SetFilter().
	 *
	 * @param target Output string. 
	 * @param severities Whether to include severities in the string.
	 * @return Status of the operation.
	 */
	OP_STATUS GetFilter(OpString &target, BOOL severities = TRUE) const;
#endif // CON_ENABLE_SERIALIZATION

	/** 
	 * Gets the severity limit for the source. If the source has not been
	 * registered, the default severity level is returned.
	 *
	 * @param src The source whose severity level we want to retrieve.
	 * @return The severity for that source.
	 */
	OpConsoleEngine::Severity Get(OpConsoleEngine::Source src) const;

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Gets the severity limit for the domain. If the domain has not been
	 * registered, the default severity level is returned.
	 *
	 * @param domain The domain whose severity level we want to retrieve.
	 * @param exact Whether the matching has to be exact, or if subdomains
	 *              are allowed.
	 * @return The severity for that domain.
	 */
	OpConsoleEngine::Severity Get(const uni_char *domain, BOOL exact) const;
#endif

	/**
	 * Make this filter a duplicate of another filter.
	 * 
	 * @param filter The filter to duplicate.
	 * @return Status of the operation.
	 */
	OP_STATUS Duplicate(const OpConsoleFilter *filter);

	/** 
	 * Sets a specific source to a specific severity. 
	 * If the source is already set, it replaces it.
	 *
	 * @param src The source to insert.
	 * @param sev The severity of that source.
	 * @return Status of the operation.
	 */
	OP_STATUS SetOrReplace(OpConsoleEngine::Source src, OpConsoleEngine::Severity sev);

	/** 
	 * Set all the severities in the list to the given severity. 
	 * Makes it easy to set all severities that are registered in
	 * the filter to a specific severity. The severities that are
	 * not registered will be left with the default severity.
	 *
	 * Useful in the Quick code when the user selects a source and
	 * a severity.
	 *
	 * @param new_severity The severity that all the severities of the 
	 * filter are to be set to.
	 */
	void SetAllExistingSeveritiesTo(OpConsoleEngine::Severity new_severity);

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Set a domain-specific filter (host override). This overrides the
	 * severity setting for all sources for this domain. Only console
	 * messages that contain a URL are affected by the per-domain rules.
	 *
	 * @param domain
	 * @param sev The severity of that source
	 * @return Status of the operation.
	 */
	OP_STATUS SetDomain(const uni_char *domain, OpConsoleEngine::Severity sev);
#endif

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Remove a domain-specific filter (host override). This removes the 
	 * special severity setting for that domain and return it to using 
	 * the general setting.
	 *
	 * @param domain
	 * @return Status of the operation.
	 */
	OP_STATUS RemoveDomain(const uni_char *domain);
#endif

	/**
	 * Clear all individually specified severities, setting them to
	 * return the default.
	 */
	void Clear();

	/**
	 * Set the lowest message number accepted by this filter. This is used
	 * when the user clears a partial view from the UI, since the console
	 * engine will retain the messages.
	 *
	 * @param lowest_id The lowest message number to display.
	 */
	inline void SetLowestId(unsigned int lowest_id)
	{ m_lowest_id = lowest_id; }

	/**
	 * Get the lowest message accepted by this filter.
	 */
	inline unsigned int GetLowestId() const
	{ return m_lowest_id; }

	/** 
	 * Check whether the filter will allow the message through.
	 *
	 * If the source that sent the message is registered in the
	 * filter, the severity level of the message is compared to the
	 * severity level set. If not the severity level of the message
	 * is compared to the default severity level.
	 *
	 * @param id Number of message.
	 * @param message The message that is checked.
	 * @return TRUE if the message passes through the filter, FALSE otherwise.
	 */
	BOOL DisplayMessage(unsigned int id, const OpConsoleEngine::Message *message) const;

	/** Get the default severity. */
	inline OpConsoleEngine::Severity GetDefaultSeverity() const
	{ return m_default_severity; }

	/** Change the default severity. */
	inline void SetDefaultSeverity(const OpConsoleEngine::Severity new_default_severity)
	{ m_default_severity = new_default_severity; }

#ifdef PREFS_HOSTOVERRIDE
	/** Retrieve a list of overridden hosts. The returned value is a
	  * OpString_list listing the host names that have override data.
	  *
	  * If there are no overridden hosts, or an out-of-memory error
	  * occurs, this function will return a NULL pointer.
	  *
	  * @return List of overridden hosts. The pointer must be deleted
	  * by the caller.
	  */
	OpString_list *GetOverriddenHosts();
#endif

private:
	/**
	 * Filter that defines what the severity limit is for messages coming
	 * from each source.
	 */
	OpConsoleEngine::Severity m_severities[static_cast<int>(OpConsoleEngine::SourceTypeLast)];

	/**
	 * Default severity that is returned for sources that are not in the array.
	 */
	OpConsoleEngine::Severity m_default_severity;

	unsigned int m_lowest_id; ///< Lowest id to display

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Class for representing domain settings.
	 */
	class ConsoleOverrideHost : public OpOverrideHost
	{
	public:
		/** First-phase constructor. */
		ConsoleOverrideHost() {};
		virtual ~ConsoleOverrideHost() {};

		void Set(OpConsoleEngine::Severity sev) { m_severity = sev; }
		OpConsoleEngine::Severity Get() const { return m_severity; }
		inline const uni_char *Host() { return m_host; }

	private:
		OpConsoleEngine::Severity m_severity;
	};

	ConsoleOverrideHost *FindOverrideHost(const OpConsoleEngine::Message *) const;

	// Inherited interfaces; OpOverrideHostContainer
	virtual OpOverrideHost *CreateOverrideHostObjectL(const uni_char *host, BOOL from_user);
	virtual void BroadcastOverride(const uni_char *) {};
#endif
};

#endif
