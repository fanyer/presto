// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton, Huib Kleinhout
//

#ifndef __SPEEDDIAL_MANAGER_H__
#define __SPEEDDIAL_MANAGER_H__

#ifdef SUPPORT_SPEED_DIAL

#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"
#include "adjunct/quick/speeddial/DesktopSpeedDial.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"
#include "adjunct/quick_toolkit/widgets/OpImageBackground.h"

#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/util/adt/oplisteners.h"

//used for the undoing changes in speeddial
#include "adjunct/quick/speeddial/SpeedDialUndo.h"

#include "modules/hardcore/timer/optimer.h"

#define g_speeddial_manager (SpeedDialManager::GetInstance())
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Speed dial preference information
#define SPEEDDIAL_SECTION_STR					"Speed Dial"
#define SPEEDDIAL_SECTION						"Speed Dial %d"
#define SPEEDDIAL_KEY_TITLE						"Title"
#define SPEEDDIAL_KEY_CUSTOM_TITLE				"Custom Title"
#define SPEEDDIAL_KEY_URL						"Url"
#define SPEEDDIAL_KEY_DISPLAY_URL				"Display Url"
// Extension local ID as per widgets.dat, or the global identifier as per config.xml if extension not installed (e.g., came from Link)
#define SPEEDDIAL_KEY_EXTENSION_ID				"Extension ID"
#define SPEEDDIAL_KEY_RELOAD					"Reload Enabled"
#define SPEEDDIAL_KEY_RELOAD_INTERVAL			"Reload Interval"
#define SPEEDDIAL_KEY_RELOAD_ONLY_IF_EXPIRED	"Reload Only If Expired"
#define SPEEDDIAL_KEY_RELOAD_POLICY				"Reload Policy"
#define SPEEDDIAL_KEY_PARTNER_ID				"Partner ID"
#define SPEEDDIAL_KEY_ID						"ID"

// Background image information
#define SPEEDDIAL_IMAGE_SECTION					"Background"
#define SPEEDDIAL_KEY_IMAGENAME					"Filename"
#define SPEEDDIAL_KEY_IMAGELAYOUT				"Layout"
#define SPEEDDIAL_KEY_IMAGE_ENABLED				"Enabled"

// File format and version upgrade information
#define SPEEDDIAL_VERSION_SECTION				"Version"
/* The "Force Reload" flag is set to true (1) if all speed dials
 * should be reloaded at the first opportunity (at which point it
 * should be removed from the ini file again).
 */
#define SPEEDDIAL_KEY_FORCE_RELOAD				"Force Reload"

// extra prefs
#define SPEEDDIAL_UI_SECTION					"UI"
#define SPEEDDIAL_UI_OPACITY					"Opacity"

// partner stuff
#define SPEEDDIAL_DELETED_ITEMS_SECTION			"Deleted Items"

class OpSpeedDialView;

// Defining this here because it's important that SpeedDialPageContent and SpeedDialPlusContent
// both use the same image skin (or at least the same sized image skins) in order to get layout
// right. (This was from work toward fixing DSK-330333.)
#define SPEEDDIAL_THUMBNAIL_IMAGE_SKIN          "Speed Dial Thumbnail Image Skin"

class OpString_list;
class PrefsFile;

#ifdef SPEEDDIAL_SVG_SUPPORT
class SpeedDialSVGBackground: public OpLoadingListener
{
public:
	SpeedDialSVGBackground();
	virtual ~SpeedDialSVGBackground();

	class SpeedDialImageLoadListener
	{
	public:
		virtual ~SpeedDialImageLoadListener() {}

		virtual void OnImageLoaded() = 0;
	};

	// OpLoadingListener interface
	virtual void OnUrlChanged(OpWindowCommander* commander, URL& url) {} // NOTE: This will be removed from OpLoadingListener soon (CORE-35579), don't add anything here!
	virtual void OnUrlChanged(OpWindowCommander* commander, const uni_char* url) {}
	virtual void OnStartLoading(OpWindowCommander* commander) {}
	virtual void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) {}
	virtual void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) {}
	virtual void OnAuthenticationCancelled(OpWindowCommander* commander, URL_ID authid) {}
	virtual void OnStartUploading(OpWindowCommander* commander) {}
	virtual void OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) {}
	virtual BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url) { return FALSE; }
	virtual void OnXmlParseFailure(OpWindowCommander*) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	virtual void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif

# ifdef EMBROWSER_SUPPORT
	virtual void OnUndisplay(OpWindowCommander* commander) {}
	virtual void OnLoadingCreated(OpWindowCommander* commander) {}
# endif // EMBROWSER_SUPPORT

	void SetListener(SpeedDialImageLoadListener *listener) { m_listener = listener; }
	OP_STATUS LoadSVGImage(OpString& filename);

	Image GetImage(UINT32 width, UINT32 height);

private:

	SpeedDialImageLoadListener*	m_listener;
	OpWindow*					m_window;					// used for SVG rendering
	OpWindowCommander*			m_windowcommander;			// used for SVG rendering
	OpString					m_image_name;				// filename of the background image
	Image						m_image;
	UINT32						m_width;					// ideal width
	UINT32						m_height;					// ideal height
	bool						m_dirty;					// true if a new image is being loaded
};
#endif // SPEEDDIAL_SVG_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SpeedDialManager
//      is a Singleton
//
//	Only one manager exists for all speed dials
//
//

class SpeedDialManager
	: public DesktopManager<SpeedDialManager>
	, public OpPrefsListener
#ifdef SPEEDDIAL_SVG_SUPPORT
	, public SpeedDialSVGBackground::SpeedDialImageLoadListener
#endif // SPEEDDIAL_SVG_SUPPORT
  	, public OpTimerListener
	, public OpDelayedTriggerListener
{
public:
	enum State
	{
		Folded,
		Shown,
		ReadOnly,
		Disabled
	};
	enum ImageType
	{
		NONE,
		BITMAP,
		SVG
	};

	static const double DefaultZoomLevel;
	static const double MinimumZoomLevel;
	static const double MaximumZoomLevel;
	static const double MinimumZoomLevelForAutoZooming;
	static const double MaximumZoomLevelForAutoZooming;
	static const double ScaleDelta;

	enum ReloadPurpose
	{
		PurposeReload,
		PurposeZoom,
		PurposePurge
	};

	static const UINT32	AdaptZoomToWindow = 0;

	SpeedDialManager();
	~SpeedDialManager();

	OP_STATUS Init();

	// Loading and saving of speed dial information from the speeddial.ini
	OP_STATUS	Load(bool copy_default_file = false);
	OP_STATUS	Save();

	DesktopSpeedDial* GetSpeedDial(int pos) const { return m_speed_dials.Get(pos); }
	const DesktopSpeedDial* GetSpeedDial(const OpInputAction& action) const { return GetSpeedDial(const_cast<OpInputAction&>(action).GetActionData() - 1); }
	// get the position of the given unique id speed dial
	DesktopSpeedDial* GetSpeedDialByID(const uni_char* unique_id) const;

	/**
	 * @param sd a Speed Dial entry
	 * @return action data to set in an action pertaining @a sd
	 */
	INT32 GetSpeedDialActionData(const SpeedDialData* sd) const { return FindSpeedDial(sd) + 1; }

	bool  SpeedDialExists(const OpStringC& url);

	const DesktopSpeedDial* GetSpeedDialByUrl(const OpStringC& url) const;

	OP_STATUS	InsertSpeedDial(int pos, const SpeedDialData* sd, bool use_undo = true);

	OP_STATUS	RemoveSpeedDial(int pos, bool use_undo = true);
	OP_STATUS	RemoveSpeedDial(const SpeedDialData* sd, bool use_undo = true) { return RemoveSpeedDial(FindSpeedDial(sd), use_undo); }

	OP_STATUS	ReplaceSpeedDial(int pos, const SpeedDialData* new_sd, bool from_sync = false);
	OP_STATUS	ReplaceSpeedDial(const SpeedDialData* old_sd, const SpeedDialData* new_sd, bool from_sync = false) { return ReplaceSpeedDial(FindSpeedDial(old_sd), new_sd, from_sync); }

	OP_STATUS	MoveSpeedDial(int from_pos, int to_pos);

	// Get a list of the default URLs from the default standard_speeddial.ini (not the current)
	// Will return an error if the default speedial ini file is not found. On success, the string list
	// should be filled the default URLs.
	OP_STATUS GetDefaultURLList(OpString_list& url_list, OpString_list& title_list);

	// Get state information about speed dial
	State		GetState() const;
	void		SetState(State state);

	bool		IsPlusButtonShown() const;

	// Undo or Redo last user action
	OP_STATUS	Undo(OpSpeedDialView* sd);
	OP_STATUS	Redo(OpSpeedDialView* sd);
	bool		IsUndoAvailable() { return m_undo_stack.IsUndoAvailable(); }
	bool		IsRedoAvailable() { return m_undo_stack.IsRedoAvailable(); }

	void		AnimateThumbnailOutForUndoOrRedo(int pos);

	// Find the position in the array of a speeddial
	inline int FindSpeedDial(const SpeedDialData* sd) const;
	inline int FindSpeedDial(const OpInputAction& action) const;
	
	/**
	 * Find the position in the speeddial's array using WUID as a seach key
	 *
	 * @param wuid wuid of extension we are looking for
	 * @return extension's position if extension with given wuid is found
	 *         -1 otherwise
	 */
	INT32 FindSpeedDialByWuid(const OpStringC& wuid) const;

	/**
	 * Find the position in the speeddial's array using url string as a search key
	 *
	 * @param url Used as a key to find matching speed-dial entry
	 * @param include_display_url Flag to include display URL during search
	 * @return SpeeddDial's position if speeddial with given url is found
	 *         -1 otherwise
	 */
	INT32 FindSpeedDialByUrl(const OpStringC& url, bool include_display_url) const;


	/** Return true if the given address is a speed dial number for a dial that is non-empty.
		If it is, it will also set speed_dial_address to that speed dial url. */
	bool		IsAddressSpeedDialNumber(const OpStringC &address, OpString &speed_dial_address, OpString &speed_dial_title);

	// Count of Speed dial pages that are currently showing
	int			GetSpeedDialTabsCount() { return m_speed_dial_tabs_count; }
	void		AddTab()				{ m_speed_dial_tabs_count++; }
	void		RemoveTab()				{ m_speed_dial_tabs_count--; OP_ASSERT(m_speed_dial_tabs_count >= 0); }

	UINT32		GetTotalCount()			{ return m_speed_dials.GetCount(); }		// returns number of speed dials

	// Functions to add as listener for configuration changes to speed dial
	OP_STATUS AddConfigurationListener(SpeedDialConfigurationListener* listener);
	OP_STATUS RemoveConfigurationListener(SpeedDialConfigurationListener* listener);

	OP_STATUS AddListener(SpeedDialListener& listener) { return m_listeners.Add(&listener); }
	OP_STATUS RemoveListener(SpeedDialListener& listener) { return m_listeners.Remove(&listener); }

	// OpPrefsListener interface
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	// access the deleted items list
	UINT32 GetDeletedSpeedDialIDCount()					{ return m_deleted_ids.GetCount(); }
	const uni_char* GetDeletedSpeedDialID(UINT32 index) { return m_deleted_ids.Get(index)->CStr(); }

#ifdef SPEEDDIAL_SVG_SUPPORT
	virtual void OnImageLoaded();
#endif // SPEEDDIAL_SVG_SUPPORT

	/**
	 * Indicates tha speed dial configuration or add dialog has started from a window.
	 *
	 * @param window the window "hosting" the configuration dialog
	 */
	void StartConfiguring(const DesktopWindow& window);

	/**
	 * @return the current Speed Dial scale
	 */
	double GetThumbnailScale() const { return m_scale; }
	int    GetIntegerThumbnailScale() const { return (int)(GetThumbnailScale() * 100 + 0.5); }

	/**
	 * Sets the Speed Dial scale.
	 *
	 * @param scale the new scale
	 * @return true if scale actually changed
	 */
	bool SetThumbnailScale(double scale);

	/**
	 * Scales the Speed Dial up or down.
	 *
	 * @param scale_up scaling direction
	 */
	void ChangeScaleByDelta(bool scale_up);

	void SetAutomaticScale(bool is_automatic_scale);
	bool IsScaleAutomatic() const { return m_automatic_scale; }

	/**
	 * Puts the Speed Dial into a "scaling" state.
	 *
	 * In this state, the Speed Dial will choose not to reload the thumbnails
	 * on scale change.  They will only be scaled.
	 *
	 * @see EndScaling
	 */
	void StartScaling();

	/**
	 * Clears the Speed Dial's "scaling" state.
	 *
	 * @see StartScaling
	 */
	void EndScaling();

	unsigned GetThumbnailWidth()  const;
	unsigned GetThumbnailHeight() const;

	//======================================================================
	// Now only used by the Speed Dial Link support
	int			GetSpeedDialPositionFromURL(const OpStringC& url) const;

	void		GetMinimumAndMaximumCellSizes(unsigned& min_width, unsigned& min_height, unsigned& max_width, unsigned& max_height) const;
	unsigned    GetDefaultCellWidth() const;

	// add an ID to the deleted items array
	OP_STATUS AddToDeletedIDs(const uni_char *id, bool notify);
	bool      IsInDeletedList(const OpStringC& partner_id);
	bool      RemoveFromDeletedIDs(const OpStringC& partner_id, bool notify);

	void OnColumnLayoutChanged();
	bool IsBackgroundImageEnabled() { return m_background_enabled; }
	void SetBackgroundImageEnabled(bool enabled);
	bool HasBackgroundImage();
	Image GetBackgroundImage(UINT32 width, UINT32 height);
	OP_STATUS GetBackgroundImageFilename(OpString& filename) { return filename.Set(m_background_image_name); }
	OP_STATUS LoadBackgroundImage(OpString& filename);
	OpImageBackground::Layout GetImageLayout() const { return m_image_layout; }
	void SetImageLayout(OpImageBackground::Layout layout);
	bool HasUIOpacity() { return m_ui_opacity != 0 && m_ui_opacity != 100; }
	UINT32 GetUIOpacity() { return m_ui_opacity; }

	void ConfigureThumbnailManager();

	// cancel any possible pending save operation, typically called due to failure
	void CancelUpdate() { SetDirty(false); }
	void PostSaveRequest() { SetDirty(true); }

	/**
	 * Returns true if Load has been called at least once.
	 */
	bool HasLoadedConfig() const { return m_has_loaded_config; }

private:
	void UpdateScale();
	void SetDirty( bool state );

	// OpDelayedTriggerListener
	virtual void OnTrigger(OpDelayedTrigger*);

	// Relaods all the speed dials (used when the privacy manager blanks all the cache)
	// NEVER set TRUE unless if is directly after the Purge call for the thumbnail manager
	void ReloadAll(ReloadPurpose purpose);

	// Should we keep this speed dial entry or should the default override it
	OP_STATUS MergeSpeedDialEntries(bool merge_entries, PrefsFile& default_sd_file, PrefsFile& sd_file, bool& prefs_changed, bool add_extra_items);

	// OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

	void NotifySpeedDialAdded(const DesktopSpeedDial& sd);
	void NotifySpeedDialRemoving(const DesktopSpeedDial& sd);
	void NotifySpeedDialReplaced(const DesktopSpeedDial& old_sd, const DesktopSpeedDial& new_sd);
	void NotifySpeedDialMoved(int from_pos, int to_pos);
	void NotifySpeedDialPartnerIDAdded(const uni_char* partner_id);
	void NotifySpeedDialPartnerIDDeleted(const uni_char* partner_id);
	void NotifySpeedDialsLoaded();

	OpAutoVector<DesktopSpeedDial>		m_speed_dials;				// Array holding the individual speeddial information
	State						m_state;					// Holds the state of the SpeedDial Page itself
															// when you don't want to save it to prefs i.e. kioskmode
	int							m_speed_dial_tabs_count;	// Hold the currently open number of speed dial tabs
	SpeedDialUndo				m_undo_stack;				// Undo information of changes made by the user

	DesktopWidgetWindow*        m_widget_window;
	OpString					m_background_image_name;	// filename of the background image
	OpImageBackground::Layout		m_image_layout;				// how to paint the picture
	SimpleImageReader			m_image_reader;				// image reader for the background image
	bool						m_background_enabled;		// background image enabled
	OpVector<SpeedDialConfigurationListener> m_config_listeners;	// list of listeners for configuration changes
	OpListeners<SpeedDialListener> m_listeners;
	UINT32						m_ui_opacity;				// Transparency used on UI elements
#ifdef SPEEDDIAL_SVG_SUPPORT
	SpeedDialSVGBackground		m_svg_background;
#endif // SPEEDDIAL_SVG_SUPPORT
	ImageType					m_image_type;
	double						m_scale;
	bool						m_scaling;
	OpVector<OpString>			m_deleted_ids;				// IDs of parter speed dials that have been deleted
	bool						m_force_reload;				// Whether to reload all speed dials as soon as possible

	OpTimer						m_zoom_timer;	
	bool						m_automatic_scale;

	OpSpeedDialView*				m_undo_animating_in_speed_dial;

	bool						m_is_dirty;					// if set, data needs to be save
	OpDelayedTrigger			m_delayed_trigger;			// the trigger used to signal when data should be saved

	bool						m_has_loaded_config;		// Set to true when Load() has been called at least once. Save will not be permitted unless this is true.
};



int SpeedDialManager::FindSpeedDial(const SpeedDialData *sd) const
{
	return m_speed_dials.Find(static_cast<DesktopSpeedDial*>(const_cast<SpeedDialData*>(sd)));
}

int SpeedDialManager::FindSpeedDial(const OpInputAction& action) const
{
	int pos = const_cast<OpInputAction&>(action).GetActionData() - 1;
	if (pos < 0 || pos >= int(m_speed_dials.GetCount()))
		pos = -1;
	return pos;
}

#endif // SUPPORT_SPEED_DIAL
#endif // __SPEEDDIAL_MANAGER_H__
