// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton, Huib Kleinhout
//

#ifndef __SPEEDDIAL_DATA_H__
#define __SPEEDDIAL_DATA_H__

#ifdef SUPPORT_SPEED_DIAL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Default Auto Reload Params
#define SPEED_DIAL_RELOAD_ENABLED_DEFAULT				FALSE
// This must match "never reload" in menu.ini (not the best solution...)
#define SPEED_DIAL_RELOAD_INTERVAL_DEFAULT				2147483646
#define SPEED_DIAL_RELOAD_ONLY_IF_EXPIRED_DEFAULT		FALSE

#define SPEED_DIAL_ENABLED_RELOAD_INTERVAL_DEFAULT		60

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SpeedDialData is a container class to hold Speed Dial information for a single speed dial. It can be
// used to save or keep track of the speed dials
//
class SpeedDialData
{
public:
	enum ReloadPolicy
	{
		Reload_NeverSoft=0,    // Do not reload automatically, but change to PageDefined if page supports so
		Reload_UserDefined,    // Reload automatically after interval defined by user
		Reload_PageDefined,    // Reload automatically after interval defined by page
		Reload_NeverHard,      // Never reload because user has said so
	};

public:
	//constructor/destructor
	SpeedDialData();
	virtual ~SpeedDialData() {};

	//Take over the settings of another SpeedDialData class
	//If retain_uid is true then unique ID of original is not copied
	virtual OP_STATUS Set(const SpeedDialData& original, bool retain_uid = false);

	//get or set URL, title or partner id
	URL_ID GetCoreUrlID() const { return m_core_url.Id(); }
	const OpStringC &GetURL() const { return m_url; }
	const OpStringC &GetDisplayURL() const { return m_display_url.HasContent() ? m_display_url : m_url; }
	const OpStringC &GetTitle() const { return m_title; }
	const OpStringC &GetPartnerID() const { return m_partner_id; }	// returns the partner id, if available. Partner IDs are used in the default speed dial ini files
	const OpStringC &GetUniqueID() const { return m_unique_id; }	// returns the unique id for this speed dial

	BOOL HasDisplayURL() const { return m_display_url.HasContent(); }

	/**
	 * Gets the global ID of a Speed Dial extension.
	 *
	 * All extension-type speed dials, and only those, have one.
	 *
	 * @return the global ID of the Speed Dial extension
	 */
	OpStringC GetExtensionID() const;

	/**
	 * Gets the local, Opera-internal ID (WUID) of a Speed Dial extension.
	 *
	 * An extension-type speed dial has a WUID if the extension is actually
	 * installed.
	 *
	 * @return the WUID of the Speed Dial extension
	 */
	OpStringC GetExtensionWUID() const;

	BOOL IsCustomTitle() const { return m_is_custom_title; }

	virtual OP_STATUS SetURL(const OpStringC& url);
	virtual OP_STATUS SetDisplayURL(const OpStringC& display_url)			{ return m_display_url.Set(display_url); }
	virtual OP_STATUS SetTitle(const OpStringC& title, BOOL custom)			{ m_is_custom_title = custom; return m_title.Set(title); }

	/**
	 * @param id the WUID of the Speed Dial extension if known (extension
	 * 		installed already), or the global ID otherwise
	 */
	OP_STATUS SetExtensionID(const OpStringC& id) { return m_extension_id.Set(id); }
	OP_STATUS SetPartnerID(const OpStringC& partner_id)	{ return m_partner_id.Set(partner_id); }
	OP_STATUS SetUniqueID(const OpStringC& id)			{ return m_unique_id.Set(id); }

	// Acesss to the Auto Reload data
	OP_STATUS SetReload(const ReloadPolicy policy, const int timeout = 0, const BOOL only_if_expired = FALSE);

	// Get Auto Reload settings
	ReloadPolicy GetReloadPolicy() const	{ return m_reload_policy; }
	BOOL	GetReloadEnabled() const	{ return m_reload_timer_running; }
	int		GetReloadTimeout() const	{ return m_timeout; }
	BOOL	GetReloadOnlyIfExpired() const { return m_only_if_expired; }

	/**
	 * Returns the preview refresh timeout in seconds.
	 * This is a value provided by the page
	 *
	 * @return The timeout. A value less or equal to zero should be
	 *         treated as if a timeout has not been defined by the page
	 */
	int	GetPreviewRefreshTimeout() const { return m_preview_refresh; }

	// Is the speed dial set to something?
	BOOL	IsEmpty() const { return m_url.IsEmpty() && m_extension_id.IsEmpty(); }

	// Returns the number of bytes that are used to store this object in memory
	// (used for to limit the size of the speed dial undo stack)
	size_t	BytesUsed();

	BOOL	IsPartnerEntry() { return m_partner_id.HasContent(); }

	/**
	 * Generate a unique ID for the item, if needed.  If use_hash is TRUE, generate a id from the URL, 
	 * otherwise generate a GUID. Position must be set of use_hash is TRUE.
	 *
	 * @param force if TRUE, always generate an ID
	 * @param use_hash If TRUE, generate an ID based on the URL and position together
	 * @param position If use_hash is TRUE, use this position in the md5 hash generated
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GenerateIDIfNeeded(BOOL force = FALSE, BOOL use_hash = FALSE, INT32 position = 0);

protected:
	OpString	 m_title;				// Title on the speed dial
	OpString	 m_url;					// URL the speed dial links to
	OpString	 m_display_url;			// URL the speed dial appears to link to when editing
	OpString	 m_partner_id;			// If this is a partner speed dial, this is the partner id
	OpString	 m_unique_id;			// Unique ID for this speed dial
	OpString	 m_extension_id;		// the WUID of the Speed Dial extension if known (extension installed already), or the global ID otherwise
	int			 m_timeout;				// sec. between each reload
	int			 m_preview_refresh;     // Preview refresh timeout in seconds (provided by page)
	ReloadPolicy m_reload_policy;       //  how and when to reload the content
	BOOL	 	 m_only_if_expired;		// only reload if "expired/changed"
	BOOL		 m_reload_timer_running;	// is the auto reload enabled?
	BOOL		 m_is_custom_title;		//  Flag identifies whether the title is set by the user or not. TRUE means set by the user.
	URL			 m_core_url;
};


#ifdef _DEBUG
class Debug;
Debug& operator<<(Debug& dbg, const SpeedDialData& sd);
#endif // _DEBUG

#endif // SUPPORT_SPEED_DIAL
#endif // __SPEEDDIAL_DATA_H__
