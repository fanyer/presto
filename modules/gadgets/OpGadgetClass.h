/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpGadgetClass.h
 *
 * (todo: short description)
 *
 * @author lasse@opera.com
*/

#ifndef OP_GADGET_CLASS_H
#define OP_GADGET_CLASS_H


#ifdef GADGET_SUPPORT

#include "modules/gadgets/gadget_utils.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_gadget_representation.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/util/adt/opvector.h"
#include "modules/url/url_enum.h"

#ifdef SIGNED_GADGET_SUPPORT
# include "modules/libcrypto/include/CryptoXmlSignature.h"
#endif // SIGNED_GADGET_SUPPORT

#include "modules/dochand/win.h"

#define GADGET_CONFIGURATION_FILE		UNI_L("config.xml")
#define GADGET_CONFIGURATION_FILE_LEN	10
#define GADGET_INDEX_FILE				UNI_L("index.html")
#define GADGET_INDEX_FILE_LEN			10
#define GADGET_WEBSERVER_TYPE GadgetWebserverType

class XMLFragment;
class OpZipFolder;
class OpGadgetUpdateInfo;
class PrefsFile;
class TempBuffer;
class GadgetSignatureStorage;
class GadgetSignatureVerifier;
class OpGadgetParsers;
class GadgetConfigurationParserElement;

/** @short Enumeration used to specify what the major type of gadget this is.
 *
 * The integer values are powers of two so that they can be used as bit flags.
 */
enum GadgetClass
{
	/** @short Regular widget */
	GADGET_CLASS_WIDGET    = 0x01,
	/** @short Unite application or server */
	GADGET_CLASS_UNITE     = 0x02,
	/** @short Browser extension */
	GADGET_CLASS_EXTENSION = 0x04,

	GADGET_CLASS_ANY = GADGET_CLASS_WIDGET | GADGET_CLASS_UNITE | GADGET_CLASS_EXTENSION
};

/** @short Enumeration used to specify what kind of gadget this is. */
enum GadgetType
{
	/** @short Unknown */
	GADGET_TYPE_UNKNOWN = 0,
	/** @short Chromeless */
	GADGET_TYPE_CHROMELESS,
	/** @short Windowed */
	GADGET_TYPE_WINDOWED,
	/** @short Toolbar */
	GADGET_TYPE_TOOLBAR,
};

/** @short Enumeration used for widget.network attribute. */
enum GadgetNetworkType
{
	/** @short Unknown */
	GADGET_NETWORK_UNKNOWN		= 0,
	/** @short None */
	GADGET_NETWORK_NONE			= 0,
	/** @short Private */
	GADGET_NETWORK_PRIVATE		= 1,
	/** @short Public */
	GADGET_NETWORK_PUBLIC       = 2
};

enum GadgetSignatureState
{
	/** @short Unknown */
	GADGET_SIGNATURE_UNKNOWN = 0,
	/** @short Widget is unsigned */
	GADGET_SIGNATURE_UNSIGNED,
	/** @short Widget is signed and signature is valid */
	GADGET_SIGNATURE_SIGNED,
	/** @short Widget is signed, but signature is invalid */
	GADGET_SIGNATURE_VERIFICATION_FAILED,
	/** @short Widget verification is still in progress, please check later */
	GADGET_SIGNATURE_PENDING
};

/** Widget configuration attributes. Many of these are optional for widgets,
  * and may not be set.
  */
enum GadgetStringAttribute
{
	WIDGET_ATTR_ID,							/**< Valid URI that denotes an identifier for the widget. */
	WIDGET_ATTR_VERSION,					/**< Specifies the version of the widget. */

	WIDGET_NAME_TEXT,						/**< Full human-readable name for a widget. */
	WIDGET_NAME_SHORT,						/**< Short name for a widget. */

	WIDGET_DESCRIPTION_TEXT,				/**< Human-readable description of the widget. */

	WIDGET_AUTHOR_ATTR_HREF,				/**< URI that represents a link associated with the author. */
	WIDGET_AUTHOR_ATTR_EMAIL,				/**< String that represents an email address associated with the author. */
	WIDGET_AUTHOR_ATTR_ORGANIZATION,
	WIDGET_AUTHOR_ATTR_IMG,
	WIDGET_AUTHOR_TEXT,						/**< People or organization attributed with the creation of the widget. */

	WIDGET_LICENSE_TEXT,					/**< Software license under which the widget package is provided. */
	WIDGET_LICENSE_ATTR_HREF,				/**< URI or a path that points to a software and/or content license. */

	WIDGET_CONTENT_ATTR_SRC,				/**< Path that points to a start file within the widget package. */
	WIDGET_CONTENT_ATTR_TYPE,				/**< Media type of the file referenced by WIDGET_CONTENT_ATTR_SRC. */
	WIDGET_CONTENT_ATTR_CHARSET,			/**< Character encoding of the file referenced by WIDGET_CONTENT_ATTR_SRC. */

	WIDGET_UPDATE_ATTR_URI,

	WIDGET_SERVICEPATH_TEXT,				/**< Unite service path. */

	WIDGET_DEFAULT_LOCALE,					/**< Default Locale for the widget */

	WIDGET_EXTENSION_SPEEDDIAL_FALLBACK_URL,/**< Fallback url as originally specified in the extension's config.xml. */
	WIDGET_EXTENSION_SPEEDDIAL_URL,			/**< URL to be used in the extension's speed dial entry, may be changed through the dom api. This is initially the same as the fallback url.*/
	WIDGET_EXTENSION_SPEEDDIAL_TITLE,		/**< Title to be used in the extension's speed dial entry. */

	WIDGET_HOME_PAGE,                       /**< Url to the web page at which the widget is hosted. */

	LAST_STRING_ATTRIBUTE
};

/** Widget configuration attributes. Many of these are optional for widgets,
  * and may not be set.
  */
enum GadgetIntegerAttribute
{
	WIDGET_ATTR_HEIGHT,						/**< Viewport height of the custom start file in CSS pixels. */
	WIDGET_ATTR_WIDTH,						/**< Viewport width of the custom start file in CSS pixels. */
	WIDGET_ATTR_MODE,						/**< View modes supported the widget. @see WindowViewMode */
	WIDGET_ATTR_DEFAULT_MODE,				/**< Default view mode. @see GadgetMode */
	WIDGET_ATTR_TRANSPARENT,
	WIDGET_ATTR_DOCKABLE,

	WIDGET_ACCESS_ATTR_NETWORK,
	WIDGET_ACCESS_ATTR_PLUGINS,
	WIDGET_ACCESS_ATTR_FILES,
	WIDGET_ACCESS_ATTR_JSPLUGINS,
	WIDGET_ACCESS_ATTR_WEBSERVER,
	WIDGET_ACCESS_ATTR_URLFILTER,
	WIDGET_ACCESS_ATTR_GEOLOCATION,

	//WIDGET_BDO_ATTR_DIR,

	WIDGET_WIDGETTYPE,
	WIDGET_WEBSERVER_ATTR_TYPE,				/**< Webserver widget subtype */

	WIDGET_JIL_ATTR_BILLING,
	WIDGET_JIL_ATTR_MAXIMUM_HEIGHT,
	WIDGET_JIL_ATTR_MAXIMUM_WIDTH,
	WIDGET_JIL_ATTR_ACCESS_NETWORK,			/**< Just to mark that we have a JIL access tag. */
	WIDGET_JIL_ACCESS_ATTR_FILESYSTEM,
	WIDGET_JIL_ACCESS_ATTR_REMOTE_SCRIPTS,
	WIDGET_UPDATE_ATTR_JIL_PERIOD,

	WIDGET_EXTENSION_SPEEDDIAL_CAPABLE,		/**< Extension's background process can be rendered in a speed dial entry. */
	WIDGET_EXTENSION_SHARE_URL_CONTEXT,		/**< The extension will use a separate url context as usual, but share the cookies with the main browsing context. */
	WIDGET_EXTENSION_SCREENSHOT_SUPPORT,    /**< The extension will be able to use opera.extension.getScreenshot(). */
	WIDGET_EXTENSION_ALLOW_CONTEXTMENUS,    /**< Allow contextmenu API. */

	LAST_INTEGER_ATTRIBUTE
};

enum GadgetCollectionAttribute
{
	WIDGET_ICONS,
	WIDGET_ACCESS_FEATURES,

	LAST_COLLECTION_ATTRIBUTE
};

/** Certain fields in a widget can be localized for a given language, this
  *  defines which fields are localizable. The same enum can be used to
  *  fetch the chosen locale for each field.
  */
enum GadgetLocalizedAttribute
{
	WIDGET_LOCALIZED_NAME,					/**< Used when name and shortname are localized. */
	WIDGET_LOCALIZED_DESCRIPTION,			/**< Used when description is localized. */
	WIDGET_LOCALIZED_LICENSE,				/**< Used when license is localized. */

	LAST_LOCALIZED_ATTRIBUTE
};

/** @short Enumeration used for feature.webserver.type attribute */
typedef enum
{
	GADGET_WEBSERVER_TYPE_SERVICE,
	GADGET_WEBSERVER_TYPE_APPLICATION
} GadgetWebserverType;

/** @short Enumeration used for WIDGET_EXTENSION_ATTR_PANEL_TYPE */
typedef enum
{
	GADGET_EXTENSION_WINDOW,		///< Panel is for the window.
	GADGET_EXTENSION_TAB			///< Panel is for the tab.
} GadgetExtensionLocation;

/** @short Enumeration used for WIDGET_EXTENSION_ATTR_PANEL_LOCATION */
typedef enum
{
	GADGET_EXTENSION_PANEL_LEFT,
	GADGET_EXTENSION_PANEL_RIGHT,
	GADGET_EXTENSION_PANEL_TOP,
	GADGET_EXTENSION_PANEL_BOTTOM
} GadgetPanelLocation;

enum GadgetNamespace
{
	GADGETNS_UNKNOWN			= 0x01,
	GADGETNS_OPERA_2006			= 0x02,	// "http://xmlns.opera.com/2006/widget"
	GADGETNS_W3C_1_0			= 0x04,	// "http://www.w3.org/ns/widgets"
	GADGETNS_OPERA				= 0x08,	// ""
	GADGETNS_JIL_1_0			= 0x20	// "http://www.jil.org/ns/widgets"
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetFeatureParam
//////////////////////////////////////////////////////////////////////////

class OpGadgetFeatureParam : public Link
{
public:
	static OP_STATUS Make(OpGadgetFeatureParam **new_param, const uni_char *name, const uni_char *value);
	virtual ~OpGadgetFeatureParam() {}

	const uni_char *Name()  { return m_name.CStr(); }
	const uni_char *Value() { return m_value.CStr(); }

private:
	OpGadgetFeatureParam() {};

	OpString m_name;
	OpString m_value;
	BOOL m_required;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetFeature
//////////////////////////////////////////////////////////////////////////

class OpGadgetFeature : public Link
{
public:
	static OP_STATUS Make(OpGadgetFeature **new_feature, const uni_char *uri, BOOL required);
	virtual	~OpGadgetFeature() { m_params.Clear(); }

	const uni_char			*URI() { return m_uri.CStr(); }
	BOOL					IsRequired() const { return m_required; }
	UINT					Count() { return m_params.Cardinal(); }
	OpGadgetFeatureParam	*GetParam(UINT index);
	const uni_char			*GetParamValue(const uni_char *name);
	/** Adds a new param. */
	void				AddParam(OpGadgetFeatureParam *param);
#ifdef GADGET_DUMP_TO_FILE
	void					DumpFeatureL(TempBuffer *buf);
#endif // GADGET_DUMP_TO_FILE

private:
	OpGadgetFeature(BOOL required = FALSE) : m_required(required) {}

	BOOL m_required;
	OpString m_uri;
	Head m_params;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetIcon
//////////////////////////////////////////////////////////////////////////

class OpGadgetIcon : public Link
{
public:

	static OP_STATUS Make(OpGadgetIcon **new_icon, const uni_char *src, INT32 width = -1, INT32 height = -1);
	virtual	~OpGadgetIcon() {}

	const uni_char	*Src() { return m_src.CStr(); }
	const uni_char	*Alt() { return m_alt.CStr(); }
	INT32			Width() { return m_width; }
	INT32			Height() { return m_height; }
	INT32			RealWidth() { return m_real_width; }
	INT32			RealHeight() { return m_real_height; }
	BOOL			IsSVG() { return m_src.FindI(".svg") == m_src.Length() - 4; }

#ifdef GADGET_DUMP_TO_FILE
	OP_STATUS		StoreResolvedPath(const uni_char *path) { return m_resolved_path.Set(path); }
	void			DumpIconL(TempBuffer *buf);
#endif // GADGET_DUMP_TO_FILE
private:
	friend class OpGadgetClass; // TODO try to remopve this friendliness
	OpGadgetIcon(INT32 width = -1, INT32 height = -1) : m_width(width), m_height(height), m_real_width(-1), m_real_height(-1) {}
	OpString m_src;
	OpString m_alt;
	INT32 m_width;
	INT32 m_height;
	INT32 m_real_width;
	INT32 m_real_height;

#ifdef GADGET_DUMP_TO_FILE
	OpString m_resolved_path;
#endif //GADGET_DUMP_TO_FILE
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetPreference
//////////////////////////////////////////////////////////////////////////

class OpGadgetPreference : public Link
{
public:
	static OP_STATUS Make(OpGadgetPreference **new_pref, const uni_char *name, const uni_char *value, BOOL readonly);
	const uni_char *Name() { return m_name.CStr(); }
	const uni_char *Value() { return m_value.CStr(); }
	int NameLength()  { return m_name.Length(); }
	int ValueLength() { return m_value.Length(); }
	BOOL IsReadOnly() { return m_readonly; }

	virtual	~OpGadgetPreference()  {}
private:
	OpGadgetPreference(BOOL readonly = FALSE) : m_readonly(readonly) {}

	OpString m_name;
	OpString m_value;
	BOOL m_readonly;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetAccess
//////////////////////////////////////////////////////////////////////////

class OpGadgetAccess : public Link
{
public:
	static OP_STATUS Make(OpGadgetAccess **new_access, const uni_char *origin, BOOL subdomains);
	const uni_char *Name() { return m_origin.CStr(); }
	BOOL Subdomains() { return m_subdomains; }
private:
	OpGadgetAccess(BOOL subdomains) : m_subdomains(subdomains) {}
	OpString m_origin;
	BOOL m_subdomains;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetClass
//////////////////////////////////////////////////////////////////////////

class OpGadgetClass
	: public Link
	, public MessageObject
{
public:
	virtual	~OpGadgetClass();

	/** Reload widget meta data.
	  * Calling this function will reload and reparse all widget meta data
	  * files such as config.xml
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS Reload();

	/** Retrieve the major type for this gadget class.
	  * @return The type of gadget class.
	  */
	GadgetClass GetClass();

	/** Retrieves the name of the widget.
	  * @param name The resulting name.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetName(OpString& name) { return name.Set(GetAttribute(WIDGET_NAME_TEXT)); }

	/** Retrieves the description of the widget.
	  * @param description The resulting description.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetDescription(OpString& description) { return description.Set(GetAttribute(WIDGET_DESCRIPTION_TEXT)); }

	/** Retrieves the widget author string.
	  * @param author The resulting string.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetAuthor(OpString& author) { return author.Set(GetAttribute(WIDGET_AUTHOR_TEXT)); }

	/** Retrieves the email address of the widget author.
	  * @param author_email receives on output the author's email address.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetAuthorEmail(OpString& author_email) { return author_email.Set(GetAttribute(WIDGET_AUTHOR_ATTR_EMAIL)); }

	/** Retrieves the href attribute of the widget author.
	  * @param author_href receives on output the author's href attribute.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetAuthorHref(OpString& author_href) { return author_href.Set(GetAttribute(WIDGET_AUTHOR_ATTR_HREF)); }

	/** Retrieves the widget id (as specified in config.xml, not the unique id generated by the gadgets module).
	  * @param id The widgets id.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetId(OpString& id) { return id.Set(GetAttribute(WIDGET_ATTR_ID)); }

	/** Get the widgets id.
	  * @return A pointer to a string which is the id or NULL.
	  */
	const uni_char* GetGadgetId()  { return GetAttribute(WIDGET_ATTR_ID); }

	/** Retrieves the widget version.
	  * @param version The widgets version.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetVersion(OpString& version) { return version.Set(GetAttribute(WIDGET_ATTR_VERSION)); }

	/** Get the widgets version.
	  * @return A pointer to a string which is the version or NULL.
	  */
	const uni_char* GetGadgetVersion()  { return GetAttribute(WIDGET_ATTR_VERSION); }

	/** Retrieves the URL for the gadget namespace.
	  * @param url The resulting string.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetNamespaceUrl(OpString& url);

	/** Retrieves the widget update url.
	  * @param url The resulting string.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetUpdateUrl(OpString& url) { return url.Set(GetAttribute(WIDGET_UPDATE_ATTR_URI)); }

	/** Get the widget update url.
	  * @return A pointer to a string which is the update url or NULL.
	  */
	const uni_char* GetGadgetUpdateUrl()  { return GetAttribute(WIDGET_UPDATE_ATTR_URI); }

	/** Retrieves the widget path string.
	  * @param gadget_path Result. The local path to the widget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetPath(OpString& gadget_path) { return gadget_path.Set(m_gadget_path); }

	/** Retrieves the widget filename string.
	  * @param filename Result. The filename of the widget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetFileName(OpString& filename) { return filename.Set(m_gadget_path.SubString(m_gadget_path.FindLastOf(PATHSEPCHAR)+1)); }

	/** Get the local path to the widget.
	  * @return A pointer to a string which is the path to the widget or NULL.
	  */
	const uni_char* GetGadgetPath()  { return m_gadget_path.CStr(); }

	/** Retrieves the widget path root folder. Normally the same as
	  * GetGadgetPath(), but may be a sub-folder for some legacy widgets.
	  * @return The local path to the widget.
	  */
	const uni_char* GetGadgetRootPath() { return m_gadget_root_path.IsEmpty() ? GetGadgetPath() : m_gadget_root_path.CStr(); }

	/** Sets the local path to the widget for this class.
	  * The use of this function is discouraged and is included here only for backward compatibility.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS SetGadgetPath(const OpStringC& gadget_path) { return m_gadget_path.Set(gadget_path); }

#ifdef EXTENSION_SUPPORT
	/** Retrieves the includes folder for the widget. This is only available
	  * for extensions.
	  * @param path[out] The path to the includes folder.
	  * @return OK if the path was returned, ERR if this is not an extension.
	  */
	OP_STATUS GetGadgetIncludesPath(OpString& path);
#endif

	/** Retrieves the url from which this widget was downloaded.
	  * @param url The resulting URL.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetDownloadUrl(OpString& url) { return url.Set(m_gadget_download_url); }

	/** Sets the url from which this widget was downloaded.
	  * @param url The URL where this widget was downloaded from.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS SetGadgetDownloadUrl(const OpStringC& url) { return m_gadget_download_url.Set(url); }

	/** Retrieves the content type of this widget.
	  * @return The content type used for this widget. */
	URLContentType GetContentType() { return m_content_type; }

	/** Sets the content type of this widget.
	  * @param type The content type to use for this widget. */
	void SetContentType(URLContentType type) { m_content_type = type; }

	/** Retrieves the name of the index.html file for this widget class.
	  * @param name Result. The name of the index.html file for this widget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetFile(OpString& name) { return name.Set(m_gadget_file); }

	/** The name of the index.html file for this widget class.
	  * @return A pointer to a string or NULL.
	  */
	const uni_char* GetGadgetFile()  { return m_gadget_file.CStr(); }

	/** Retrieves the full path to the config file for this widget class.
	  * @param[out] path The path to the config file.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetConfigFile(OpString& path) { return path.Set(m_config_file); }

	/** Returns the number of icons included in this widget.
	  * @return The number of icons in this widget.
	  */
	UINT32 GetGadgetIconCount() { return OpStatus::IsSuccess(ValidateIcons()) ? m_icons.Cardinal() : 0; }

	/** Retrieves the path, specified_width and specified_height of an icon in this widget.
	  * @param index Index of the icon to retrieve information about (0-based).
	  * @param icon_path Result. The local path to the icon file.
	  * @param specified_width Result. Width of the icon as specified in config.xml.
	  * @param specified_height Result. Height of the icon as specified in config.xml
	  * @param full_path If TRUE the full path is returned, if FALSE only the part specified in config.xml is returned (src attr).
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors. If index is out of range ERR is returned.
	  */
	OP_STATUS GetGadgetIcon(UINT32 index, OpString& icon_path, INT32& specified_width, INT32& specified_height, BOOL full_path = TRUE);

	/** Retrieves a rendition of an icon in the icon array.
	  * @param iconBmp Result. The decoded image is returned here.
	  * @param index Index of the icon to decode.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors. If index is out of range ERR is returned.
	  */
	OP_STATUS GetGadgetIcon(OpBitmap** iconBmp, UINT32 index);

	/** Retrieves a rendition of an icon that *best* matches the supplied width and height requirements.
	  * If there exists an icon that perfectly matches the request then it is returned.
	  * Otherwise if set of icons bigger than width*height is not empty then the smallest
	  * icon from that set is returned.
	  * Otherwise if set of icons smaller than width*height is not empty then the largest
	  * icon from that set is returned.
	  * @param iconBmp Result. The decoded image is returned here.
	  * @param width The requested width of the icon.
	  * @param height The requested height of the icon.
	  * @param resize TRUE if function should resize icon to requested size, FALSE if icon should be rendered in its normal size.
	  * @param allow_size_check TRUE if function can read icons to get missing dimensions (i.e. not specified in config.xml), FALSE otherwise.
	  * @return OK if successful, ERR_NO_SUCH_RESOURCE if there are no icons, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS GetGadgetIcon(OpBitmap** iconBmp, INT32 width, INT32 height, BOOL resize = TRUE, BOOL allow_size_check = TRUE);

	/** Returns the width of the widget that was specified in the config.xml file.
	  * @return Width in pixels.
	  */
	int InitialWidth () { return GetAttribute(WIDGET_ATTR_WIDTH); }

	/** Returns the height of the widget that was specified in the config.xml file.
	  * @return Height in pixels.
	  */
	int InitialHeight() { return GetAttribute(WIDGET_ATTR_HEIGHT); }

	/** Returns what kind of widget this is.
	  * @return Type of widget.
	  */
	GadgetType GetGadgetType() { return (GadgetType) GetAttribute(WIDGET_WIDGETTYPE); }

	/** Returns the default mode of the widget.
	  * @return Default mode.
	  */
	WindowViewMode GetDefaultMode() { return (WindowViewMode) GetAttribute(WIDGET_ATTR_DEFAULT_MODE); }

	/** Returns whether widgets of this class are allowed to be opened.
	  *
	  * Whether a widget is allowed to open depends on the signature state
	  * (MAYBE is returned until the signature verification is complete)
	  * and other widget properties.
	  */
	BOOL3 OpenAllowed();

	/** Returns whether this widget has access to the DOM file API or not.
	  * @return TRUE if access is granted or FALSE if denied.
	  */
	BOOL HasFileAccess();

	/** Returns whether this widget has JIL access to filesystem.
	  * @return TRUE if access is granted or FALSE if denied.
	  */
	BOOL HasJILFilesystemAccess() { return !!GetAttribute(WIDGET_JIL_ACCESS_ATTR_FILESYSTEM); }

	/** Returns whether this widget has access to geolocation apis.
	  *
	  * Only regular widgets have access to geolocation and only if
	  * the appropriate feature is declared in config.xml.
	  *
	  * @return TRUE if access is granted or FALSE if denied.
	  */
	BOOL HasGeolocationAccess() { return !!GetAttribute(WIDGET_ACCESS_ATTR_GEOLOCATION); }

	/** Returns whether this widget has access to getUserMedia API.
	  */
	BOOL HasGetUserMediaAccess() { return FALSE; }

	/** Returns whether this widget is dockable or not.
	  * @return TRUE if dockable or FALSE if not.
	  */
	BOOL IsDockable() { return !!GetAttribute(WIDGET_ATTR_DOCKABLE); }

	/**
	 * Check whether the widget class supports <feature> tags.
	 */
	BOOL HasFeatureTagSupport() const { return (m_namespace == GADGETNS_W3C_1_0) || (m_namespace == GADGETNS_JIL_1_0); }

	/** Checks if the widget requires the specified feature
	  * @return TRUE if the widget has requested the feature and requires it to run, FALSE if it may run without it.
	  */
	BOOL IsFeatureRequired(const uni_char *feature);
	/** Checks if the widget requires the specified feature
	  * @return TRUE if the widget has requested the feature and requires it to run, FALSE if it may run without it.
	  */
	BOOL IsFeatureRequired(const char *feature);

	/** Checks if the widget requested the specified feature.
	  * @return TRUE if the widget has requested the feature, FALSE if not.
	  */
	BOOL IsFeatureRequested(const uni_char *feature)	{ return GetFeature(feature) != NULL; }
	/** Checks if the widget requested the specified feature.
	  * @return TRUE if the widget has requested the feature, FALSE if not.
	  */
	BOOL IsFeatureRequested(const char *feature)		{ return GetFeature(feature) != NULL; }

	/** Returns TRUE if widget object should expose extended opera javscript APIs.
	 */
	BOOL SupportsOperaExtendedAPI();

#ifdef WEBSERVER_SUPPORT
	/** Returns whether this is an alien widget or not.
	  * @return TRUE if it's an alien widget or FALSE if it's not.
	  */
	BOOL IsSubserver() { return !!GetAttribute(WIDGET_ACCESS_ATTR_WEBSERVER); }
#else
	BOOL IsSubserver() { return FALSE; }
#endif

#ifdef WEBSERVER_SUPPORT
	/** Returns the subserver uri for this alien widget.
	  * @param subserver_uri The resulting uri.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS GetGadgetSubserverUri(OpString& subserver_uri) { return subserver_uri.Set(GetAttribute(WIDGET_SERVICEPATH_TEXT)); }

	/** Returns the type of webserver gadget this is. Service or application */
	GadgetWebserverType GetWebserverType() { return (GadgetWebserverType) GetAttribute(WIDGET_WEBSERVER_ATTR_TYPE); }
#endif // WEBSERVER_SUPPORT

#ifdef EXTENSION_SUPPORT
	/** Returns wheter this is an extension widget or not.
	  * @return TRUE if it's an extension widget or FALSE if it's not.
	  */
	BOOL IsExtension() { return URL_EXTENSION_INSTALL_CONTENT == m_content_type; }
#else
	BOOL IsExtension() { return FALSE; }
#endif

#if defined(URL_FILTER) && defined(EXTENSION_SUPPORT)
	/** Returns whether this widget can access the URL filter.
	  * @return TRUE if this widget can access the URL filter or FALSE if it cannot.
	  */
	BOOL HasURLFilterAccess() { return !!GetAttribute(WIDGET_ACCESS_ATTR_URLFILTER); }
#else
	BOOL HasURLFilterAccess() { return FALSE; }
#endif

	/** Returns whether this widget has transparent features or not.
	  * @return TRUE if it has or FALSE if not.
	  */
	BOOL HasTransparentFeatures() { return !!GetAttribute(WIDGET_ATTR_TRANSPARENT); }

	/** Returns whether this widget is persistent or not.
	  * @return TRUE if widget it persistent, FALSE if not.
	  */
	BOOL IsPersistent() { return m_persistent; }

	/** Sets whether this widget should be persistent or not.
	  * Persistent widgets can not be deleted.
	  * @param persistent TRUE if widget should be persistent, FALSE if not.
	  */
	void SetPersistent(BOOL persistent) { m_persistent = persistent; }

	/** Returns whether the widget is enabled or not.
	  * Disabled widgets should not be displayed to the end user,
	  * instead they should be informed that it is disabled and
	  * the reason take from GetDisabledDetails().
	  * @return TRUE if widget is enabled, FALSE if not.
	  */
	BOOL IsEnabled() const { return m_is_enabled; }

	/** Returns the detailed description of why the widget is disabled.
	  * @return Reference to description string or NULL if there is no
	  *         description or widget is enabled.
	  */
	OP_STATUS GetDisabledDetails(OpString &d) const { return d.Set(m_disabled_details); }

	/** Returns a boolean specifying if the class is in the
	  * given namespace. A widget can be in several namespaces.
	  * @param gadgetNamespace a bitmask of GadgetNamespace's to check
	  * @return boolean TRUE or FALSE
	  */
	BOOL SupportsNamespace(int gadgetNamespace) const { return (m_namespace & gadgetNamespace) != 0; }

	/** Sets whether the widget is enabled or not.
	  * @note The disabled details will be emptied.
	  * @param is_enabled If TRUE it will enable the widget, if FALSE it will disable it.
	  */
	OP_STATUS SetIsEnabled(BOOL is_enabled);

	/** Sets whether the widget is enabled or not.
	  * @param is_enabled If TRUE it will enable the widget, if FALSE it will disable it.
	  * @param details Detailed description which is needed when the widget is disabled.
	                   This information is persisted and can be displayed to the end user.
	  */
	OP_STATUS SetIsEnabled(BOOL is_enabled, const OpStringC &details);

	/** Request the latest update information from the update server.
	  * When the update information is found it calls the current gadget
	  * listener set in OpWindowCommanderManager::GetGadgetListener().
	  */
	OP_STATUS RequestUpdateInfo();

	/** Returns the current update info object, or NULL if there is
	  * no update information available.
	  */
	OpGadgetUpdateInfo *GetUpdateInfo() const { return  m_update_info; }

	/** Sets the new update info object to @a update_info.
	  * If an existing update info object was set in the class
	  * it will be deleted.
	  * @note OpGadgetClass takes ownership of the object.
	  * @param update_info The update info object to be set, can be NULL.
	  */
	void SetUpdateInfo(OpGadgetUpdateInfo *update_info);

	/** Returns whether this widget has access to JS plugins or not.
	  * @return TRUE if widget has access, FALSE if not.
	  */
	BOOL HasJSPluginsAccess()  { return !!GetAttribute(WIDGET_ACCESS_ATTR_JSPLUGINS); }

	/** Checks if the widget has access to the specified feature
	  * @return TRUE if the widget has requested and got access, FALSE if not.
	  */
	BOOL HasFeature(const uni_char *feature)	{ return GetFeature(feature) ? TRUE : FALSE; }
	BOOL HasFeature(const char *feature)		{ return GetFeature(feature) ? TRUE : FALSE; }

	/** Returns the number of features specified in this widget. */
	UINT FeatureCount() { return m_features.Cardinal(); }

	/** Retrieve a given feature.
	  * @param index The 0-based index of the feature to retrieve.
	  * @result Pointer to the feature requested or NULL.
	  */
	OpGadgetFeature *GetFeature(UINT index);

	/** Get the Head to all the features.
	  * Useful when you want to traverse every feature
	  * @result Pointer to Head that contains list of features
	  */
	Head* GetFeatures() { return &m_features; }

	/** Returns whether this widget has a signature and is verified.
	  * @return TRUE if widget is signed.
	  */
	GadgetSignatureState SignatureState() { return m_sign_state; }

#ifdef GADGET_PRIVILEGED_SIGNING_SUPPORT
	/**
	 * Returns whether the widget has been signed with a privileged certificate.
	 * Checking this only makes sense when SignatureState returns GADGET_SIGNATURE_SIGNED.
	 */
	BOOL IsSignedWithPrivilegedCert() const { return m_is_signed_with_privileged_cert; }
#endif // GADGET_PRIVILEGED_SIGNING_SUPPORT

	/** Sets whether this widget has a signature and is verified.
	  * Signed widgets uses a different security model.
	  * @param sig_state Signing state for this widget.
	  */
	void SetSignatureState(GadgetSignatureState sig_state) { m_sign_state = sig_state; }

	const uni_char *SignatureId() { return m_signature_id.CStr(); }

	void SetSecurity(GadgetSecurityPolicy* policy) { OP_DELETE(m_security_policy); m_security_policy = policy; }
	GadgetSecurityPolicy* GetSecurity() { return m_security_policy; }

	DEPRECATED(const uni_char *GetLocale());

	/** Check if folder-based localization is enabled *and* we have a
	  * "locales" folder.
	  * @return FALSE if there is a "locales" folder, or folder-based
	  *   localization is disabled (non-W3C widgets), TRUE otherwise.
	  */
	BOOL HasLocalesFolder();

	// deprecated functions
	DEPRECATED(OP_STATUS GetGadgetUrl(OpString& url, BOOL include_index_file = TRUE, BOOL include_protocol = TRUE));
	DEPRECATED(OP_STATUS GetGadgetIcon(OpString& icon_path));
	DEPRECATED(OP_STATUS GetGadgetIcon(OpBitmap** iconBmp));
	DEPRECATED(OP_STATUS GetDefaultMode(WindowViewMode* mode));
	DEPRECATED(BOOL3 HasIntranetAccess());
	DEPRECATED(void HasIntranetAccess(BOOL3 intranet));

	OP_STATUS SetAttribute(GadgetIntegerAttribute attr, UINT32 value, BOOL override = FALSE);
	OP_STATUS SetAttribute(GadgetStringAttribute attr, const uni_char* value, BOOL override = FALSE);
	OP_STATUS SetLocaleForAttribute(GadgetLocalizedAttribute attr, const uni_char* value);
	UINT32 GetAttribute(GadgetIntegerAttribute attr, BOOL *set = NULL);
	const uni_char* GetAttribute(GadgetStringAttribute attr, BOOL *set = NULL) const;
	const uni_char* GetLocaleForAttribute(GadgetLocalizedAttribute attr, BOOL *set = NULL);

#ifdef GADGET_DUMP_TO_FILE
	static void DumpStringL(TempBuffer *buf, const uni_char* name, const uni_char *str);
	static void DumpStringL(TempBuffer *buf, const uni_char *str);
	OP_STATUS DumpConfiguration(TempBuffer *buf);
	void DumpConfigurationL(TempBuffer *buf);
	void DumpAttributeL(TempBuffer *buf, const uni_char* name, GadgetStringAttribute attr);
	void DumpAttributeL(TempBuffer *buf, const uni_char* name, GadgetIntegerAttribute attr);
	void DumpFeaturesL(TempBuffer *buf);
	void DumpIconsL(TempBuffer *buf);
	void DumpPreferencesL(TempBuffer *buf);
	void DumpAccessL(TempBuffer *buf);
	void DumpLocalesL(TempBuffer *buf);
#endif // GADGET_DUMP_TO_FILE

	OpGadgetAccess *GetFirstAccess() { return static_cast<OpGadgetAccess*>(m_access.First()); }
	OpGadgetPreference *GetFirstPref() { return static_cast<OpGadgetPreference*>(m_default_preferences.First()); }

#ifdef SELFTEST
	void SetBundledGlobalPolicy(GadgetSecurityPolicy *policy) { OP_DELETE(m_global_sec_policy); m_global_sec_policy = policy; }
	GadgetSecurityPolicy* GetBundledGlobalPolicy() const { return m_global_sec_policy; }
#endif // SELFTEST

protected:
	friend class OpGadgetUpdateObject;

	void SetIsUpdating(BOOL updating) { m_is_updating = updating; }
	BOOL IsUpdating() { return m_is_updating; }

	// from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	BOOL HasPreference(const uni_char *pref);

	OpGadgetFeature * GetFeature(const uni_char *feature);
	OpGadgetFeature * GetFeature(const char *feature);

private:
	friend class OpGadgetManager;
	friend class OpGadgetParsers;
#ifdef SELFTEST
	friend class ST_gadgetsgadgetclass;
	friend class ST_gadgetsgadgetparsers;
#endif

	OpGadgetClass();

	OP_GADGET_STATUS Construct(const OpStringC& gadget_path, URLContentType type);
	OP_GADGET_STATUS ApplyDefaults();

	OP_GADGET_STATUS LoadInfoFromInfoFile(OpString& error_reason);

#ifdef SIGNED_GADGET_SUPPORT
	OP_GADGET_STATUS VerifySignature();
#endif // SIGNED_GADGET_SUPPORT
	OP_GADGET_STATUS ProcessXMLContent(XMLFragment* f, OpString& error_reason);
	
	OP_GADGET_STATUS PostProcess(OpString& error_reason);
	OP_GADGET_STATUS ProcessFeature(OpGadgetFeature *feature, OpString& error_reason);
	OP_GADGET_STATUS ProcessIcon(OpGadgetIcon *gadget_icon);

	OP_STATUS SaveToFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix);
	OP_STATUS LoadFromFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix);

	void ClearSetAttributes() { m_string_attributes_set = 0; m_integer_attributes_set = 0; m_localized_attributes_set = 0; }
	BOOL IsAttributeSet(GadgetStringAttribute attr) { return (m_string_attributes_set & (1 << (UINT32)attr)) != 0; }
	BOOL IsAttributeSet(GadgetIntegerAttribute attr) { return (m_integer_attributes_set & (1 << (UINT32)attr)) != 0; }
	BOOL IsLocaleForAttributeSet(GadgetLocalizedAttribute attr) { return (m_localized_attributes_set & (1 << (UINT32)attr)) != 0; }

	OP_STATUS SetAttribute(GadgetStringAttribute attr, XMLFragment *f, BOOL override = FALSE);
	OP_STATUS SetAttribute(GadgetStringAttribute attr, XMLFragment *f, const uni_char *attr_str, BOOL override = FALSE, uni_char bidi_marker = 0);
	OP_STATUS SetAttribute(GadgetIntegerAttribute attr, XMLFragment *f, const uni_char *attr_str, BOOL override = FALSE);
	OP_STATUS SetAttributeBoolean(GadgetIntegerAttribute attr, XMLFragment* f, const uni_char *attr_str_arg, BOOL dval, BOOL override = FALSE);
	OP_STATUS SetAttributeElementString(GadgetStringAttribute attr, XMLFragment *f, BOOL override = FALSE);
	OP_STATUS SetAttributeElementStringBIDI(GadgetStringAttribute attr, XMLFragment *f, BOOL normalize = TRUE, BOOL override = FALSE);
	OP_STATUS GetAttributeString(XMLFragment *f, const uni_char *attr_str_arg, OpString &string);
	OP_STATUS GetAttributeIRI(XMLFragment *f, const uni_char *attr_str_arg, OpString &string, BOOL allow_wildcard = FALSE);
	OP_STATUS GetAttributeBoolean(XMLFragment *f, const uni_char *attr_str_arg, BOOL &result, BOOL dval = FALSE);
	OP_STATUS SetAttributeIRI(GadgetStringAttribute attr, XMLFragment *f, const uni_char *attr_str, BOOL override = FALSE, BOOL allow_wildcard = FALSE);

	uni_char GetBIDIMarker(const uni_char *dir_attribute);
	OP_STATUS CollectBIDIString(TempBuffer &buffer, XMLFragment *f, uni_char bidi_marker = 0);
	OP_STATUS GetGadgetIconPath(OpGadgetIcon *icon, BOOL full_path, OpString &icon_path);
	OP_STATUS GetGadgetIconInternal(OpBitmap** iconBmp, OpString& icon_path, BOOL resize, INT32 width, INT32 height);
	OP_STATUS PeekGadgetIconDimension(OpGadgetIcon *icon);

	/**
	 * Checks if icon with a given src already is in the icon list.
	 *
	 * @note : this method doesn't check validity of icons!
	 */
	BOOL HasIconWithSrc(const uni_char* src);

	/** Validates all the icons declared in configuration document and removes all the invalid ones.
	 *
	 * If the result is an empty list then it tries to find a default icon.
	 * All the processing in Validate icons should only occur once(m_icons_checked flag is set to prevent additional processing).
	 */
	OP_STATUS ValidateIcons();
	/** Validates one icon checking if the apropriate file exists and is an image.
	 *
	 * @param src - non-localized icon path inside the widget package.
	 * @param valid[out] - set to TRUE if the icon for the specified path is valid, or FALSE otherwise.
	 * @param resolved_path[out] - set to resolved full path of the icon file.
	 */
	OP_STATUS ValidateIcon(const uni_char* src, BOOL& valid, OpString& resolved_path);
	void Clear();
	/** Composes an error message to be posted on the console from error code and optional error reason.
	 *  @param error error code.
	 *  @param error_reason details about error. Optional - can be NULL.
	 *  @param message[out] composed error message shown to the user.
	 */
	static void GetGadgetErrorMessage(OP_GADGET_STATUS error, const uni_char* error_reson, OpString& message);
	////////////////////////////////////////
	// data from widget manifest; config.xml
	////////////////////////////////////////

	OpString m_localized_attributes[LAST_LOCALIZED_ATTRIBUTE]; ///< Locale of attributes. E.g: en-gb or nb-no
	OpString m_string_attributes[LAST_STRING_ATTRIBUTE];
	UINT32 m_integer_attributes[LAST_INTEGER_ATTRIBUTE];
	UINT32 m_localized_attributes_set:LAST_LOCALIZED_ATTRIBUTE;
	UINT32 m_string_attributes_set:LAST_STRING_ATTRIBUTE;
	UINT32 m_integer_attributes_set:LAST_INTEGER_ATTRIBUTE;

	Head m_icons;
	Head m_features;	///< List features that are declared in config.xml *and* supported by the implementation
	Head m_access;
	Head m_default_preferences;

	////////////////////////////////////////

	OpString m_config_file;				///< Path to config file. E.g: widget.wgt/config.xml
	OpString m_gadget_file;				///< Path to start file. E.g: index.html
	OpString m_gadget_path;				///< Path to widget archive. E.g: /somefolder/widget.wgt
	OpString m_gadget_root_path;		///< Path to root of legacy widget. E.g: /somefolder/widget.wgt/widget
	OpString m_gadget_download_url;
	OpString m_disabled_details;
	OpString m_signature_id;

	BOOL m_is_updating;
	BOOL m_persistent;
	BOOL m_is_enabled;
	BOOL3 m_has_locales_folder;			///< "locales" folder is present in widget; MAYBE means we haven't actually checked yet
	BOOL m_icons_checked;
	GadgetSignatureState m_sign_state;
#ifdef GADGET_PRIVILEGED_SIGNING_SUPPORT
	BOOL m_is_signed_with_privileged_cert;
#endif // GADGET_PRIVILEGED_SIGNING_SUPPORT
	URLContentType m_content_type;

	OpGadgetUpdateInfo* m_update_info;
	GadgetSecurityPolicy* m_security_policy;
#ifdef SELFTEST
	GadgetSecurityPolicy* m_global_sec_policy;
#endif // SELFTEST

	int m_namespace;
	uni_char m_default_bidi_marker;		///< Default marker for start of BIDI string. Defaults to 0 which is LTR without pop
#ifdef SIGNED_GADGET_SUPPORT
	GadgetSignatureStorage*  m_signature_storage;
	GadgetSignatureVerifier* m_signature_verifier;
#endif // SIGNED_GADGET_SUPPORT
};

inline OP_STATUS OpGadgetClass::GetGadgetIcon(OpString& icon_path) { INT32 dummy1, dummy2; return GetGadgetIcon(0, icon_path, dummy1, dummy2); }
inline OP_STATUS OpGadgetClass::GetGadgetIcon(OpBitmap **iconBmp) { return GetGadgetIcon(iconBmp, 0); }
inline OP_STATUS OpGadgetClass::GetDefaultMode(WindowViewMode* mode) { *mode = (WindowViewMode) GetAttribute(WIDGET_ATTR_DEFAULT_MODE); return OpStatus::OK; }
inline OP_STATUS OpGadgetClass::GetGadgetUrl(OpString& url, BOOL include_index_file, BOOL include_protocol) { OP_ASSERT(FALSE); return OpStatus::OK; }
inline BOOL3 OpGadgetClass::HasIntranetAccess() { return MAYBE; }
inline void OpGadgetClass::HasIntranetAccess(BOOL3 intranet) {};

// Deprecated functions
inline const uni_char *OpGadgetClass::GetLocale() { return GetLocaleForAttribute(WIDGET_LOCALIZED_NAME); }

#endif // GADGET_SUPPORT
#endif // !OP_GADGET_CLASS_H
