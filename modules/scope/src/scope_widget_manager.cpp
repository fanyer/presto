/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Jan Borsodi, Anders Ruud
*/

#include "core/pch.h"

#ifdef SCOPE_WIDGET_MANAGER_SUPPORT

#include "modules/scope/src/scope_widget_manager.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/gadgets/OpGadgetInstallObject.h"
#include "modules/gadgets/OpGadgetUpdate.h"

/* Utility functions */

OP_STATUS SetRelativePath(OpString &dest, const OpString &base, const OpString &path)
{
	if (path.Find(base) == 0)
	{
		int length = base.Length();
		int path_length = path.Length();
		while (length < path_length && path[length] == PATHSEPCHAR)
			++length;
		return dest.Set(path.CStr() + length);
	}
	else
		return dest.Set(path);
}

/* OpScopeWidgetManager::Installer */

OP_STATUS
OpScopeWidgetManager::InstallerObject::Construct(unsigned id, const OpString &wid, GadgetClass install_type)
{
	this->id = id;
	RETURN_IF_ERROR(widget_id.Set(wid));
	OpGadgetInstallObject *obj;
	RETURN_IF_ERROR(OpGadgetInstallObject::Make(obj, install_type));
	install_object.reset(obj);
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::InstallerObject::Open()
{
	return install_object->Open();
}

OP_STATUS
OpScopeWidgetManager::InstallerObject::Close()
{
	return install_object->Close();
}

BOOL
OpScopeWidgetManager::InstallerObject::IsValidWidget()
{
	return install_object->IsValidWidget();
}

OP_STATUS
OpScopeWidgetManager::InstallerObject::Update()
{
	return install_object->Update();
}

OP_STATUS
OpScopeWidgetManager::InstallerObject::AppendData(const ByteBuffer &data)
{
	return install_object->AppendData(data);
}

/* OpScopeWidgetManager */

OpScopeWidgetManager::OpScopeWidgetManager()
	: OpScopeWidgetManager_SI()
	, id_counter(1)
{
}

/* virtual */
OpScopeWidgetManager::~OpScopeWidgetManager()
{
}

/* virtual */
OP_STATUS
OpScopeWidgetManager::OnServiceEnabled()
{
	g_gadget_manager->AttachListener(static_cast<OpGadgetListener *>(this));
	g_gadget_manager->AttachListener(static_cast<OpGadgetInstallListener *>(this));

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeWidgetManager::OnServiceDisabled()
{
	install_objects.DeleteAll();
	download_objects.Clear();
	uninstall_requests.Clear();
	id_counter = 1;
	if (g_gadget_manager)
	{
		g_gadget_manager->DetachListener(static_cast<OpGadgetInstallListener *>(this));
		g_gadget_manager->DetachListener(static_cast<OpGadgetListener *>(this));
	}

	return OpStatus::OK;
}

/*virtual*/ void
OpScopeWidgetManager::HandleGadgetInstallEvent(GadgetInstallEvent& evt)
{
	if (IsEnabled())
		OpStatus::Ignore(OnWidgetInstallResult(evt.event, evt.download_object));
}

/*virtual*/
void
OpScopeWidgetManager::OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data)
{
	if (IsEnabled())
	{
		OpStatus::Ignore(OnWidgetUpgraded(data.gadget_class, data.gadget));
		SendWidgetUpgradedResult(data.gadget_class, WidgetUpdateResult::RESULT_UPDATE_SUCCESSFUL);
	}
}

/*virtual*/
void
OpScopeWidgetManager::OnGadgetInstanceCreated(const OpGadgetInstanceData& data)
{
	if (IsEnabled())
		OpStatus::Ignore(OnWidgetInstalled(data.gadget));
}

/*virtual*/
void
OpScopeWidgetManager::OnGadgetInstanceRemoved(const OpGadgetInstanceData& data)
{
	if (IsEnabled())
		OpStatus::Ignore(OnWidgetUninstalled(data.gadget));
}

/*virtual*/
void
OpScopeWidgetManager::OnGadgetUpdateReady(const OpGadgetUpdateData& data)
{
	if (!IsEnabled())
		return;

	// Don't notify if there is no new version available.
	if (data.type == OpGadgetListener::GADGET_NO_UPDATE)
		return;

	if (OpGadgetClass* klass = data.gadget_class)
	{
		const uni_char* gadgetid = klass->GetGadgetId();
		if (!gadgetid)
			return;

		Widget widget;
		if (OpStatus::IsSuccess(ExportWidget(widget, klass)))
			OpStatus::Ignore(SendOnWidgetUpgradeAvailable(widget));
	}
}

/*virtual*/
void
OpScopeWidgetManager::OnGadgetUpdateError(const OpGadgetErrorData& data)
{
	if (!IsEnabled())
		return;

	if (data.gadget_class)
		SendWidgetUpgradedResult(data.gadget_class, WidgetUpdateResult::RESULT_UPDATE_FAILED);
}

OP_STATUS
OpScopeWidgetManager::ExportWidget(Widget &widget, OpGadgetClass *klass)
{
	BOOL is_set = FALSE;
	switch (klass->GetClass())
	{
	case GADGET_CLASS_EXTENSION: widget.SetType(Widget::TYPE_EXTENSION); break;
	case GADGET_CLASS_UNITE: widget.SetType(Widget::TYPE_UNITE); break;
	default: OP_ASSERT(!"Unknown major type for widget, add type to definition and a new case entry");
	case GADGET_CLASS_WIDGET: widget.SetType(Widget::TYPE_WIDGET); break;
	}

	is_set = FALSE;
	const uni_char *version = klass->GetAttribute(WIDGET_ATTR_VERSION, &is_set);
	if (is_set)
		RETURN_IF_ERROR(widget.SetVersion(version));
	is_set = FALSE;
	const uni_char *description = klass->GetAttribute(WIDGET_DESCRIPTION_TEXT, &is_set);
	if (is_set)
	{
		Widget::Description *description_msg = OP_NEW(Widget::Description, ());
		RETURN_OOM_IF_NULL(description_msg);
		widget.SetDescription(description_msg);

		RETURN_IF_ERROR(description_msg->SetText(description));

		BOOL is_set = FALSE;
		const uni_char *lang = klass->GetLocaleForAttribute(WIDGET_LOCALIZED_DESCRIPTION, &is_set);
		if (is_set)
			RETURN_IF_ERROR(description_msg->SetLocale(lang));
	}

	// Name
	is_set = FALSE;
	const uni_char *nameText = klass->GetAttribute(WIDGET_NAME_TEXT, &is_set);
	if (is_set)
	{
		Widget::Name *name = OP_NEW(Widget::Name, ());
		RETURN_OOM_IF_NULL(name);
		widget.SetName(name);

		RETURN_IF_ERROR(name->SetText(nameText));

		is_set = FALSE;
		const uni_char *shortname = klass->GetAttribute(WIDGET_NAME_SHORT, &is_set);
		if (is_set)
			RETURN_IF_ERROR(name->SetShortName(shortname));

		is_set = FALSE;
		const uni_char *lang = klass->GetLocaleForAttribute(WIDGET_LOCALIZED_NAME, &is_set);
		if (is_set)
			RETURN_IF_ERROR(name->SetLocale(lang));
	}

	Widget::WidgetType type;
	switch (klass->GetGadgetType())
	{
	case GADGET_TYPE_CHROMELESS: type = Widget::WIDGETTYPE_CHROMELESS; break;
	case GADGET_TYPE_WINDOWED: type = Widget::WIDGETTYPE_WINDOWED; break;
	case GADGET_TYPE_TOOLBAR: type = Widget::WIDGETTYPE_TOOLBAR; break;
	case GADGET_TYPE_UNKNOWN: type = Widget::WIDGETTYPE_UNKNOWN; break;
	default:
		OP_ASSERT(!"No type set on gadget, should not happen");
		type = Widget::WIDGETTYPE_UNKNOWN;
	}
	widget.SetWidgetType(type);

	// A numeric value greater than 0 indicates the preferred viewport width/height
	// http://www.w3.org/TR/widgets/#the-width-attribute
	if (klass->InitialWidth() > 0)
		widget.SetWidth(klass->InitialWidth());
	if (klass->InitialHeight() > 0)
		widget.SetHeight(klass->InitialHeight());

	// Author
	is_set = FALSE;
	const uni_char *text = klass->GetAttribute(WIDGET_AUTHOR_TEXT, &is_set);
	if (is_set)
	{
		Widget::Author *author = OP_NEW(Widget::Author, ());
		RETURN_OOM_IF_NULL(author);
		widget.SetAuthor(author);

		RETURN_IF_ERROR(author->SetText(text));

		is_set = FALSE;
		const uni_char *href = klass->GetAttribute(WIDGET_AUTHOR_ATTR_HREF, &is_set);
		if (is_set)
			RETURN_IF_ERROR(author->SetHref(href));

		is_set = FALSE;
		const uni_char *email = klass->GetAttribute(WIDGET_AUTHOR_ATTR_EMAIL, &is_set);
		if (is_set)
			RETURN_IF_ERROR(author->SetEmail(email));

		is_set = FALSE;
		const uni_char *org = klass->GetAttribute(WIDGET_AUTHOR_ATTR_ORGANIZATION, &is_set);
		if (is_set)
			RETURN_IF_ERROR(author->SetOrganization(org));
	}

	// namespace
	RETURN_IF_ERROR(klass->GetGadgetNamespaceUrl(widget.GetNamespaceRef()));

	// UpdateInfo
	is_set = FALSE;
	const uni_char *update_url = klass->GetAttribute(WIDGET_UPDATE_ATTR_URI, &is_set);
	if (is_set)
		RETURN_IF_ERROR(widget.SetUpdateURL(update_url));

	OpGadgetUpdateInfo *update_info = klass->GetUpdateInfo();
	if (update_info)
	{
		OpAutoPtr<Widget::UpdateInfo> update_msg(OP_NEW(Widget::UpdateInfo, ()));
		RETURN_OOM_IF_NULL(update_msg.get());
		Widget::UpdateInfo::UpdateType update_type = Widget::UpdateInfo::UPDATETYPE_UNKNOWN;
		switch (update_info->GetType())
		{
			case OpGadgetUpdateInfo::GADGET_UPDATE_UPDATE: update_type = Widget::UpdateInfo::UPDATETYPE_UPDATE; break;
			case OpGadgetUpdateInfo::GADGET_UPDATE_DISABLE: update_type = Widget::UpdateInfo::UPDATETYPE_DISABLE; break;
			case OpGadgetUpdateInfo::GADGET_UPDATE_ACTIVATE: update_type = Widget::UpdateInfo::UPDATETYPE_ACTIVATE; break;
			case OpGadgetUpdateInfo::GADGET_UPDATE_DELETE: update_type = Widget::UpdateInfo::UPDATETYPE_DELETE; break;
			default:
				OP_ASSERT(!"No type specified in update info, should not happen");
		}
		update_msg->SetUpdateType(update_type);
		RETURN_IF_ERROR(update_msg->SetCustomWidgetID(update_info->GetID()));
		RETURN_IF_ERROR(update_msg->SetSrc(update_info->GetSrc()));
		RETURN_IF_ERROR(update_msg->SetVersion(update_info->GetVersion()));
		update_msg->SetSize(update_info->GetByteCount());
		update_msg->SetIsMandatory(update_info->IsMandatory());
		RETURN_IF_ERROR(update_msg->SetDetails(update_info->GetDetails()));
		RETURN_IF_ERROR(update_msg->SetDocumentUri(update_info->GetHref()));
		update_msg->SetLastModified(static_cast<double>(update_info->GetLastModified()));

		widget.SetUpdateInfo(update_msg.release());
	}

	// Icon
	unsigned icon_count = klass->GetGadgetIconCount();
	for (unsigned i = 0; i < icon_count; ++i)
	{
		Widget::Icon *icon = widget.AppendNewIconList();
		RETURN_OOM_IF_NULL(icon);

		INT32 icon_width, icon_height;
		OpString tmp_path, icon_path, gadget_path;
		RETURN_IF_ERROR(klass->GetGadgetIcon(i, tmp_path, icon_width, icon_height));
		RETURN_IF_ERROR(gadget_path.Set(klass->GetGadgetPath()));
		RETURN_IF_ERROR(SetRelativePath(icon_path, gadget_path, tmp_path));
		RETURN_IF_ERROR(icon->SetSrc(icon_path));
		if (icon_width > 0)
			icon->SetWidth(icon_width);
		if (icon_height > 0)
			icon->SetHeight(icon_height);
	}

	// Content
	{
		is_set = FALSE;
		const uni_char *src = klass->GetAttribute(WIDGET_CONTENT_ATTR_SRC, &is_set);
		if (is_set)
		{
			Widget::Content *content = OP_NEW(Widget::Content, ());
			RETURN_OOM_IF_NULL(content);
			widget.SetContent(content);

			RETURN_IF_ERROR(content->SetSrc(src));

			is_set = FALSE;
			const uni_char *type = klass->GetAttribute(WIDGET_CONTENT_ATTR_TYPE, &is_set);
			if (is_set)
				RETURN_IF_ERROR(content->SetType(type));

			is_set = FALSE;
			const uni_char *charset = klass->GetAttribute(WIDGET_CONTENT_ATTR_CHARSET, &is_set);
			if (is_set)
				RETURN_IF_ERROR(content->SetEncoding(charset));
		}
	}

	// Preference
	OpGadgetPreference *pref = klass->GetFirstPref();
	for (; pref; pref = static_cast<OpGadgetPreference *>(pref->Suc()))
	{
		Widget::Preference *pref_msg = widget.AppendNewPreferenceList();
		RETURN_OOM_IF_NULL(pref_msg);
		RETURN_IF_ERROR(pref_msg->SetName(pref->Name()));
		RETURN_IF_ERROR(pref_msg->SetValue(pref->Value()));
		pref_msg->SetIsReadOnly(pref->IsReadOnly());
	}

	// Properties
	{
		Widget::Properties &properties = widget.GetPropertiesRef();
		properties.SetHasFileAccess(klass->HasFileAccess());
		properties.SetIsDockable(klass->IsDockable());
		properties.SetHasJSPluginsAccess(klass->HasJSPluginsAccess());
		properties.SetIsPersistent(klass->IsPersistent());
		properties.SetHasTransparentFeatures(klass->HasTransparentFeatures());
		properties.SetIsEnabled(klass->IsEnabled());
		properties.SetHasJILFilesystemAccess(klass->HasJILFilesystemAccess());
		properties.SetHasFeatureTagSupport(klass->HasFeatureTagSupport());
		if (!klass->IsEnabled())
			RETURN_IF_ERROR(klass->GetDisabledDetails(properties.GetDisabledDetailsRef()));

		RETURN_IF_ERROR(properties.SetPath(klass->GetGadgetPath()));
		RETURN_IF_ERROR(klass->GetGadgetFileName(properties.GetFileNameRef()));
		RETURN_IF_ERROR(klass->GetGadgetDownloadUrl(properties.GetDownloadURLRef()));

		OpString tmp_path, config_path;
		RETURN_IF_ERROR(klass->GetGadgetConfigFile(tmp_path));
		RETURN_IF_ERROR(SetRelativePath(config_path, properties.GetPath(), tmp_path));
		RETURN_IF_ERROR(properties.SetConfigFilePath(config_path));

		INT32 network = klass->GetAttribute(WIDGET_ACCESS_ATTR_NETWORK);
		properties.SetPublicNetwork((network & GADGET_NETWORK_PUBLIC) != 0);
		properties.SetPrivateNetwork((network & GADGET_NETWORK_PRIVATE) != 0);

		const uni_char* startfile = klass->GetGadgetFile();

		OpString resolved_path, locale;
		BOOL path_found = FALSE;
		g_gadget_manager->Findi18nPath(klass, startfile, klass->HasLocalesFolder(), resolved_path, path_found, &locale);
		if (path_found)
		{
			RETURN_IF_ERROR(properties.SetResolvedStartPath(resolved_path.CStr()));
			RETURN_IF_ERROR(properties.SetStartfileLocale(locale.CStr()));
		}

	}

	// License
	{
		BOOL is_text_set = FALSE, is_href_set = FALSE;
		const uni_char *license_text = klass->GetAttribute(WIDGET_LICENSE_TEXT, &is_text_set);
		const uni_char *license_href = klass->GetAttribute(WIDGET_LICENSE_ATTR_HREF, &is_href_set);
		if (is_text_set || is_href_set)
		{
			Widget::License *license = OP_NEW(Widget::License, ());
			RETURN_OOM_IF_NULL(license);
			widget.SetLicense(license);

			RETURN_IF_ERROR(license->SetText(license_text));

			is_set = FALSE;
			const uni_char *lang = klass->GetLocaleForAttribute(WIDGET_LOCALIZED_LICENSE, &is_set);
			if (is_set)
				RETURN_IF_ERROR(license->SetLocale(lang));

			if (is_href_set)
				RETURN_IF_ERROR(license->SetHref(license_href));
		}
	}

	// Feature
	for (unsigned f_idx = 0; f_idx < klass->FeatureCount(); ++f_idx)
	{
		OpGadgetFeature *feature = klass->GetFeature(f_idx);
		Widget::Feature *widget_feature = widget.AppendNewFeatureList();
		RETURN_OOM_IF_NULL(widget_feature);
		// The name of the feature is a URI, for details see:
		// see http://www.w3.org/TR/widgets/#the-feature-element-and-its-attributes
		RETURN_IF_ERROR(widget_feature->SetName(feature->URI()));
		widget_feature->SetIsRequired(feature->IsRequired());
		for (unsigned p_idx = 0; p_idx < feature->Count(); ++p_idx)
		{
			Widget::Feature::Param *param = widget_feature->AppendNewParamList();
			RETURN_OOM_IF_NULL(param);
			RETURN_IF_ERROR(param->SetName(feature->GetParam(p_idx)->Name()));
			RETURN_IF_ERROR(param->SetValue(feature->GetParam(p_idx)->Value()));
		}
	}

	// NetworkAccess
	{
		OpGadgetAccess *access = klass->GetFirstAccess();
		for (; access; access = static_cast<OpGadgetAccess *>(access->Suc()))
		{
			Widget::Network *network = widget.AppendNewNetworkAccessList();
			RETURN_OOM_IF_NULL(network);
			RETURN_IF_ERROR(network->SetOrigin(access->Name()));
			network->SetAllowSubdomains(access->Subdomains());
		}
	}

	// Security Access
	{
		GadgetSecurityPolicy *policy = klass->GetSecurity();
		if (policy)
		{
			GadgetSecurityDirective *directive = static_cast<GadgetSecurityDirective *>(policy->access->entries.First());
			for (; directive; directive = static_cast<GadgetSecurityDirective *>(directive->Suc()))
			{
				OpAutoPtr<Widget::SecurityAccess> access(OP_NEW(Widget::SecurityAccess, ()));
				RETURN_OOM_IF_NULL(access.get());

				// Protocols
				GadgetActionUrl *action_url = directive->protocol.First();
				for (; action_url; action_url = action_url->Suc())
				{
					if (action_url->GetFieldType() != GadgetActionUrl::FIELD_PROTOCOL)
						continue;
					OpString *protocol = access->AppendNewProtocolList();
					RETURN_OOM_IF_NULL(protocol);
					RETURN_IF_ERROR(action_url->GetProtocol(*protocol));
				}

				// Hosts
				action_url = directive->host.First();
				for (; action_url; action_url = action_url->Suc())
				{
					if (action_url->GetFieldType() != GadgetActionUrl::FIELD_HOST)
						continue;
					OpString *host = access->AppendNewHostList();
					RETURN_OOM_IF_NULL(host);
					RETURN_IF_ERROR(action_url->GetHost(*host));
				}

				// Ports
				action_url = directive->port.First();
				for (; action_url; action_url = action_url->Suc())
				{
					if (action_url->GetFieldType() != GadgetActionUrl::FIELD_PORT)
						continue;

					Widget::SecurityAccess::Port *port = access->AppendNewPortList();
					RETURN_OOM_IF_NULL(port);
					unsigned low, high;
					RETURN_IF_ERROR(action_url->GetPort(low, high));
					port->SetLow(low);
					port->SetHigh(high);
				}

				// Paths
				action_url = directive->path.First();
				for (; action_url; action_url = action_url->Suc())
				{
					if (action_url->GetFieldType() != GadgetActionUrl::FIELD_PATH)
						continue;
					OpString *path = access->AppendNewPathList();
					RETURN_OOM_IF_NULL(path);
					RETURN_IF_ERROR(action_url->GetPath(*path));
				}

				if (access->GetProtocolList().GetCount() > 0 ||
					access->GetHostList().GetCount() > 0 ||
					access->GetPortList().GetCount() > 0 ||
					access->GetPathList().GetCount() > 0)
				{
					RETURN_IF_ERROR(widget.GetSecurityAccessListRef().Add(access.get()));
					access.release();
				}
			}
		}
	}

	// iriIdentifier
	is_set = FALSE;
	const uni_char *iri_id = klass->GetAttribute(WIDGET_ATTR_ID, &is_set);
	if (is_set)
		RETURN_IF_ERROR(widget.SetIriIdentifier(iri_id));

	// Server information is not enabled yet
#ifdef WEBSERVER_SUPPORT
	if (klass->GetClass() == GADGET_CLASS_UNITE)
	{
		Widget::UniteInfo *unite_info = OP_NEW(Widget::UniteInfo, ());
		RETURN_OOM_IF_NULL(unite_info);
		widget.SetUniteInfo(unite_info);
		Widget::UniteInfo::Type unite_type = Widget::UniteInfo::TYPE_UNKNOWN;
		switch (klass->GetWebserverType())
		{
		case GADGET_WEBSERVER_TYPE_SERVICE: unite_type = Widget::UniteInfo::TYPE_SERVICE; break;
		case GADGET_WEBSERVER_TYPE_APPLICATION: unite_type = Widget::UniteInfo::TYPE_APPLICATION; break;
		default:
			OP_ASSERT(!"Unknown webserver type, please add new enum entry to widget_manager.proto file and update code");
		}
		unite_info->SetType(unite_type);
		is_set = FALSE;
		const uni_char *service_path = klass->GetAttribute(WIDGET_SERVICEPATH_TEXT, &is_set);
		if (is_set)
			RETURN_IF_ERROR(unite_info->SetServicePath(service_path));
	}
#endif // WEBSERVER_SUPPORT

#ifdef EXTENSION_SUPPORT
	if (klass->GetClass() == GADGET_CLASS_EXTENSION)
	{
		Widget::ExtensionInfo *ext_info = OP_NEW(Widget::ExtensionInfo, ());
		RETURN_OOM_IF_NULL(ext_info);
		widget.SetExtensionInfo(ext_info);
		OpString includes_path, gadget_path, tmp_path;
		RETURN_IF_ERROR(gadget_path.Set(klass->GetGadgetPath()));
		RETURN_IF_ERROR(SetRelativePath(includes_path, gadget_path, tmp_path));
		RETURN_IF_ERROR(klass->GetGadgetIncludesPath(tmp_path));
		RETURN_IF_ERROR(ext_info->SetIncludesPath(includes_path));
	}

	// UserJSList
	{
		OpGadgetExtensionUserJS *ujs = g_gadget_manager->GetFirstActiveExtensionUserJS();
		while (ujs)
		{
			Widget::UserJS *newuserjs = widget.AppendNewUserJSList();
			RETURN_OOM_IF_NULL(newuserjs);
			RETURN_IF_ERROR(newuserjs->SetFilename(ujs->userjs_filename));
			ujs = ujs->Suc();
		}

	}
#endif // EXTENSION_SUPPORT


	// Signature
	{
		if (klass->SignatureState() != GADGET_SIGNATURE_UNSIGNED)
		{
			Widget::Signature *signature = OP_NEW(Widget::Signature, ());
			RETURN_OOM_IF_NULL(signature);
			widget.SetSignature(signature);

			RETURN_IF_ERROR(signature->SetId(klass->SignatureId()));
			Widget::Signature::State signature_state;
			switch (klass->SignatureState())
			{
			case GADGET_SIGNATURE_UNSIGNED: signature_state = Widget::Signature::STATE_UNSIGNED; break;
			case GADGET_SIGNATURE_SIGNED: signature_state = Widget::Signature::STATE_SIGNED; break;
			case GADGET_SIGNATURE_VERIFICATION_FAILED: signature_state = Widget::Signature::STATE_VERIFICATION_FAILED; break;
			case GADGET_SIGNATURE_PENDING: signature_state = Widget::Signature::STATE_PENDING; break;
			case GADGET_SIGNATURE_UNKNOWN:
			default:
				signature_state = Widget::Signature::STATE_UNKNOWN;
			}
			signature->SetState(signature_state);
#ifdef GADGET_PRIVILEGED_SIGNING_SUPPORT
			if (signature_state == Widget::Signature::STATE_SIGNED)
				signature->SetIsPrivilegedCert(klass->IsSignedWithPrivilegedCert());
#endif // GADGET_PRIVILEGED_SIGNING_SUPPORT
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::ExportWidget(Widget &widget, OpGadget *gadget)
{
	OpGadgetClass *klass = gadget->GetClass();
	RETURN_IF_ERROR(ExportWidget(widget, klass));

#ifdef EXTENSION_SUPPORT
	if (klass->GetClass() == GADGET_CLASS_EXTENSION)
	{
		Widget::ExtensionInfo *ext_info = widget.GetExtensionInfo();
		OP_ASSERT(ext_info);
		ext_info->SetIsRunning(gadget->IsRunning());
	}
#endif // EXTENSION_SUPPORT

	// Never send negative values for width and height
	if (gadget->GetWidth() >= 0)
		widget.SetCurrentWidth(gadget->GetWidth());
	if (gadget->GetHeight() >= 0)
		widget.SetCurrentHeight(gadget->GetHeight());

	struct {WindowViewMode gadget_mode; Widget::ViewMode widget_mode;} modes[] = {
		{WINDOW_VIEW_MODE_UNKNOWN, Widget::VIEWMODE_UNKNOWN},
		{WINDOW_VIEW_MODE_WIDGET, Widget::VIEWMODE_WIDGET},
		{WINDOW_VIEW_MODE_FLOATING, Widget::VIEWMODE_FLOATING},
		{WINDOW_VIEW_MODE_DOCKED, Widget::VIEWMODE_DOCKED},
		{WINDOW_VIEW_MODE_APPLICATION, Widget::VIEWMODE_APPLICATION},
		{WINDOW_VIEW_MODE_FULLSCREEN, Widget::VIEWMODE_FULLSCREEN},
		{WINDOW_VIEW_MODE_MAXIMIZED, Widget::VIEWMODE_MAXIMIZED},
		{WINDOW_VIEW_MODE_MINIMIZED, Widget::VIEWMODE_MINIMIZED},
		{WINDOW_VIEW_MODE_WINDOWED, Widget::VIEWMODE_WINDOWED}
	};
	// defaultViewMode
	widget.SetDefaultViewMode(Widget::VIEWMODE_WIDGET);
	WindowViewMode mode = static_cast<WindowViewMode>(klass->GetAttribute(WIDGET_ATTR_DEFAULT_MODE));
	for (unsigned i = 0; i < ARRAY_SIZE(modes); ++i)
	{
		if ((mode & modes[i].gadget_mode) != 0)
		{
			widget.SetDefaultViewMode(modes[i].widget_mode);
			break;
		}
	}
	// currentViewMode
	mode = gadget->GetMode();
	widget.SetCurrentViewMode(Widget::VIEWMODE_WIDGET);
	for (unsigned i = 0; i < ARRAY_SIZE(modes); ++i)
	{
		if ((mode & modes[i].gadget_mode) != 0)
		{
			widget.SetCurrentViewMode(modes[i].widget_mode);
			break;
		}
	}
	// viewModeList
	mode = static_cast<WindowViewMode>(klass->GetAttribute(WIDGET_ATTR_MODE));
	for (unsigned i = 0; i < ARRAY_SIZE(modes); ++i)
	{
		if ((mode & modes[i].gadget_mode) != 0)
			widget.AppendToViewModeList(modes[i].widget_mode);
	}

	RETURN_IF_ERROR(gadget->GetIdentifier(widget.GetWidgetIDRef()));
	widget.GetPropertiesRef().SetIsActive(gadget->GetIsActive());

	// Window
	Window *window = gadget->GetWindow();
	if (window)
		widget.GetPropertiesRef().SetWindowID(window->Id());

	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoListWidgets(WidgetList &out)
{
	UINT32 count = g_gadget_manager->NumGadgets();
	for (unsigned i = 0; i < count; ++i)
	{
		OpGadget *gadget = g_gadget_manager->GetGadget(i);
		OP_ASSERT(gadget);
		Widget *widget = out.AppendNewWidgetList();
		RETURN_OOM_IF_NULL(widget);
		RETURN_IF_ERROR(ExportWidget(*widget, gadget));
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoOpenWidget(const OpenWidgetArg &in)
{
	OpGadget *gadget = g_gadget_manager->FindGadget(in.GetWidgetID());
	SCOPE_BADREQUEST_IF_NULL(gadget, this, UNI_L("Widget not found"));

	RETURN_IF_ERROR(g_gadget_manager->OpenGadget(gadget));

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	gadget->SetAllowGlobalPolicy(in.GetAllowMutablePolicy());

	if (in.HasGlobalPolicy())
	{
		SCOPE_RESPONSE_IF_FALSE(in.GetAllowMutablePolicy(), this, OpScopeTPHeader::BadRequest, UNI_L("Global policy not mutable"));
		RETURN_IF_ERROR(OpSecurityManager_Gadget::SetGlobalGadgetPolicy(gadget, in.GetGlobalPolicy().CStr()));
	}
#endif // GADGETS_MUTABLE_POLICY_SUPPORT
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoCloseWidget(const CloseWidgetArg &in)
{
	OpGadget *gadget = g_gadget_manager->FindGadget(in.GetWidgetID());
	SCOPE_BADREQUEST_IF_NULL(gadget, this, UNI_L("Widget not found"));

	// Close the window immediately
	BOOL async = FALSE;
	RETURN_IF_ERROR(g_gadget_manager->CloseGadget(gadget, async));
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoCreateInstaller(const CreateInstallerArg &in, Installer &out)
{
	if (!in.GetWidgetID().IsEmpty())
	{
		for (unsigned i = 0; i < install_objects.GetCount(); ++i)
		{
			int issame = install_objects.Get(i)->GetWidgetID().Compare(in.GetWidgetID()) == 0;
			SCOPE_RESPONSE_IF_FALSE(!issame, this, OpScopeTPHeader::BadRequest, UNI_L("An upload object with the same widget ID already exists"));
		}
	}

	GadgetClass installer_type = GADGET_CLASS_WIDGET;
	if (in.HasWidgetType())
	{
		switch (in.GetWidgetType())
		{
		case Widget::TYPE_UNITE:
#ifdef WEBSERVER_SUPPORT
			{
				installer_type = GADGET_CLASS_UNITE;
				break;
			}
#else // WEBSERVER_SUPPORT
				return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("This Opera instance does not have support for Unite Apps"));
#endif // WEBSERVER_SUPPORT

		case Widget::TYPE_EXTENSION:
#ifdef EXTENSION_SUPPORT
			{
				installer_type = GADGET_CLASS_EXTENSION;
				break;
			}
#else // EXTENSION_SUPPORT
			return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("This Opera instance does not have support for Extensions"));
#endif // EXTENSION_SUPPORT

		case Widget::TYPE_WIDGET:
			installer_type = GADGET_CLASS_WIDGET;
			break;
		default:
			OP_ASSERT(!"Unknown widget type, should not happen");
			return OpStatus::ERR;
		}
	}
	InstallerObject *installer_ptr = OP_NEW(InstallerObject, ());
	RETURN_OOM_IF_NULL(installer_ptr);
	OpAutoPtr<InstallerObject> installer(installer_ptr);
	RETURN_IF_ERROR(installer->Construct(id_counter++, in.GetWidgetID(), installer_type));

	if (!in.GetWidgetID().IsEmpty())
	{
		OpString tmp;
		UINT32 count = g_gadget_manager->NumGadgets();
		for (unsigned i = 0; i < count; ++i)
		{
			OpGadget *gadget = g_gadget_manager->GetGadget(i);
			RETURN_IF_ERROR(gadget->GetIdentifier(tmp));
			if (tmp.Compare(in.GetWidgetID()) == 0)
			{
				installer->GetInstallObject()->SetGadget(gadget);
				break;
			}
		}
	}

	unsigned installer_id = installer->GetID();
	RETURN_IF_ERROR(installer->Open());
	RETURN_IF_ERROR(install_objects.Add(installer.release()));

	out.SetInstallerID(installer_id);
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoUpdateInstaller(const UpdateInstallerArg &in)
{
	InstallerObject *installer = FindInstaller(in.GetInstallerID());
	SCOPE_BADREQUEST_IF_NULL(installer, this, UNI_L("No installer found with given installer ID"));

	return installer->AppendData(in.GetByteData());
}

OP_STATUS
OpScopeWidgetManager::DoRemoveInstaller(const RemoveInstallerArg &in)
{
	InstallerObject *installer_ptr = ReleaseInstaller(in.GetInstallerID());
	SCOPE_BADREQUEST_IF_NULL(installer_ptr, this, UNI_L("No installer found with given installer ID"));

	OP_DELETE(installer_ptr);
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoInstallWidget(const InstallWidgetArg &in, WidgetInstallResult &out)
{
	InstallerObject *installer_ptr = ReleaseInstaller(in.GetInstallerID());
	SCOPE_BADREQUEST_IF_NULL(installer_ptr, this, UNI_L("No installer found with given installer ID"));

	OpAutoPtr<InstallerObject> installer(installer_ptr);
	RETURN_IF_ERROR(installer->Close());

	SCOPE_RESPONSE_IF_FALSE(installer->IsValidWidget(), this, OpScopeTPHeader::BadRequest, UNI_L("The installer does not contain a valid widget, cannot install"));

	// Make sure it gets a better filename which reflects the name of the widget
	RETURN_IF_ERROR(installer->Update());

	OpGadgetInstallObject *obj = installer->GetInstallObject();
	OpGadget *upgrade_gadget = obj->GetGadget();
	if (upgrade_gadget)
		RETURN_IF_ERROR(g_gadget_manager->Upgrade(obj));
	else
		RETURN_IF_ERROR(g_gadget_manager->Install(obj));
	OpGadgetClass *gadget_class = obj->GetGadgetClass();

	out.SetResult(gadget_class ? WidgetInstallResult::RESULT_INSTALL_SUCCESSFUL : WidgetInstallResult::RESULT_INSTALL_FAILED);
	return OpStatus::OK;
}

#ifdef EXTENSION_SUPPORT
static OpGadget::ExtensionFlag
ConvertToGadgetFlag(OpScopeWidgetManager::ExtensionFlag::FlagType flag)
{
	switch (flag)
	{
		case OpScopeWidgetManager::ExtensionFlag::FLAGTYPE_ALLOW_USERJS_HTTPS:
			return OpGadget::AllowUserJSHTTPS;
		default:
			OP_ASSERT(!"Add new type.");
			//fallthrough just to set a value to prevent a compiler warning
		case OpScopeWidgetManager::ExtensionFlag::FLAGTYPE_ALLOW_USERJS_PRIVATE:
			return OpGadget::AllowUserJSPrivate;
	}
}
#endif //EXTENSION_SUPPORT

OP_STATUS
OpScopeWidgetManager::DoSetExtensionFlag(const SetExtensionFlagArg &in)
{
#ifdef EXTENSION_SUPPORT
	OpGadget *gadget = g_gadget_manager->FindGadget(in.GetWidgetID());
	SCOPE_BADREQUEST_IF_NULL(gadget, this, UNI_L("Widget not found"));

	gadget->SetExtensionFlag(ConvertToGadgetFlag(in.GetFlag().GetFlag()), in.GetFlag().GetValue());

	return OpStatus::OK;
#else
	return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("This Opera instance does not have support for Extensions"));
#endif //EXTENSION_SUPPORT
}

OP_STATUS
OpScopeWidgetManager::DoGetExtensionFlag(const GetExtensionFlagArg &in, ExtensionFlag &out)
{
#ifdef EXTENSION_SUPPORT
	OpGadget *gadget = g_gadget_manager->FindGadget(in.GetWidgetID());
	SCOPE_BADREQUEST_IF_NULL(gadget, this, UNI_L("Widget not found"));

	out.SetFlag(in.GetFlag().GetFlag());

	out.SetValue(gadget->GetExtensionFlag(ConvertToGadgetFlag(in.GetFlag().GetFlag())));

	return OpStatus::OK;
#else
	return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("This Opera instance does not have support for Extensions"));
#endif //EXTENSION_SUPPORT

}

OP_STATUS
OpScopeWidgetManager::DoUpdateWidget(const UpdateWidgetArg &in, unsigned int async_tag)
{
	OpGadget *gadget = g_gadget_manager->FindGadget(in.GetWidgetID());
	SCOPE_BADREQUEST_IF_NULL(gadget, this, UNI_L("Widget not found"));

	OpAutoPtr<CheckForUpdateObject> updateobj(OP_NEW(CheckForUpdateObject, ()));
	RETURN_OOM_IF_NULL(updateobj.get());

	RETURN_IF_ERROR(updateobj->Construct(gadget->GetGadgetId(), async_tag));
	RETURN_IF_ERROR(check_update_requests.Add(updateobj.get()));
	updateobj.release();
	return gadget->Update();
}

OP_STATUS
OpScopeWidgetManager::DoInstallWidgetByURL(const InstallWidgetByURLArg &in, unsigned tag)
{
	SCOPE_RESPONSE_IF_FALSE(!(in.GetUrl().IsEmpty()), this, OpScopeTPHeader::BadRequest, UNI_L("url field must not be empty"));

	const uni_char *url_text = in.GetUrl().CStr();
	URL url = urlManager->GetURL(url_text);
	SCOPE_RESPONSE_IF_FALSE(url.IsValid(), this, OpScopeTPHeader::BadRequest, UNI_L("url field does not contain a valid URL"));
	OpGadgetDownloadObject *obj;
	RETURN_IF_ERROR(OpGadgetDownloadObject::Make(obj, g_main_message_handler, url_text, NULL, NULL));

	RETURN_IF_ERROR(download_objects.Add(DownloadObject(obj, tag)));

	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoUninstallWidget(const UninstallWidgetArg &in, unsigned tag)
{
	OpGadget *gadget = g_gadget_manager->FindGadget(in.GetWidgetID());
	SCOPE_BADREQUEST_IF_NULL(gadget, this, UNI_L("Widget not found"));

	OpString path;
	RETURN_IF_ERROR(gadget->GetGadgetPath(path));
	RETURN_IF_ERROR(uninstall_requests.Add(UninstallRequest(gadget, tag)));
	RETURN_IF_ERROR(g_gadget_manager->CloseGadget(gadget, FALSE));
	RETURN_IF_ERROR(g_gadget_manager->RemoveGadget(path));
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::DoGetSettings(Settings &out)
{
	out.SetMaxChunkSize(SCOPE_WIDGET_MANAGER_MAX_CHUNK_SIZE);
	OpGadgetManager::GadgetLocaleIterator locale_iterator = g_gadget_manager->GetBrowserLocaleIterator();
	const uni_char* next;
	while ((next = locale_iterator.GetNextLocale()) != NULL)
	{
		OpString *locale = out.AppendNewLocaleList();
		RETURN_OOM_IF_NULL(locale);
		RETURN_IF_ERROR(locale->Set(next));
	}
	OpString *locale = out.AppendNewLocaleList();
	RETURN_OOM_IF_NULL(locale);
	RETURN_IF_ERROR(locale->Set(UNI_L("*")));

	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::OnWidgetInstallResult(OpGadgetInstallListener::Event evt, OpGadgetDownloadObject *obj)
{
	if (!IsEnabled())
		return OpStatus::OK;

	WidgetInstallResult::Result result = WidgetInstallResult::RESULT_UNKNOWN;
	switch (evt)
	{
	case OpGadgetInstallListener::GADGET_INSTALL_INSTALLED: result = WidgetInstallResult::RESULT_INSTALL_SUCCESSFUL; break;
	case OpGadgetInstallListener::GADGET_INSTALL_PROCESS_FINISHED: result = WidgetInstallResult::RESULT_INSTALL_SUCCESSFUL; break;
	case OpGadgetInstallListener::GADGET_INSTALL_DOWNLOAD_FAILED: result = WidgetInstallResult::RESULT_DOWNLOAD_FAILED; break;
	case OpGadgetInstallListener::GADGET_INSTALL_NOT_ENOUGH_SPACE: result= WidgetInstallResult::RESULT_NOT_ENOUGH_SPACE; break;
	case OpGadgetInstallListener::GADGET_INSTALL_INSTALLATION_FAILED: result = WidgetInstallResult::RESULT_INSTALL_FAILED; break;
	}

	WidgetInstallResult msg;
	msg.SetResult(result);
	const uni_char *download_url = obj ? obj->DownloadUrlStr() : NULL;
	if (download_url && *download_url != '\0')
		RETURN_IF_ERROR(msg.SetUrl(download_url));

	// Send event if there is a failure
	if (evt == OpGadgetInstallListener::GADGET_INSTALL_DOWNLOAD_FAILED ||
		evt == OpGadgetInstallListener::GADGET_INSTALL_NOT_ENOUGH_SPACE ||
		evt == OpGadgetInstallListener::GADGET_INSTALL_INSTALLATION_FAILED)
		RETURN_IF_ERROR(SendOnWidgetInstallFailed(msg));

	// Check if this is a response to a previous InstallWidgetByURL command
	unsigned count = download_objects.GetCount();
	for (unsigned i = 0; i < count; ++i)
	{
		DownloadObject download = download_objects.Get(i);
		if (download.GetDownloadObject() == obj)
		{
			// We wait until we get a successful install or failure.
			if (evt == OpGadgetInstallListener::GADGET_INSTALL_DOWNLOADED)
				return OpStatus::OK;

			unsigned tag = download.GetTag();
			RETURN_IF_ERROR(download_objects.Remove(i));
			return SendInstallWidgetByURL(msg, tag);
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeWidgetManager::OnWidgetInstalled(OpGadget *gadget)
{
	if (!IsEnabled())
		return OpStatus::OK;
	OP_ASSERT(gadget);

	Widget msg;
	RETURN_IF_ERROR(ExportWidget(msg, gadget));
	return SendOnWidgetInstalled(msg);
}

OP_STATUS
OpScopeWidgetManager::OnWidgetUpgraded(OpGadgetClass * /*original_klass*/, OpGadget *gadget)
{
	if (!IsEnabled())
		return OpStatus::OK;
	OP_ASSERT(gadget);

	Widget msg;
	RETURN_IF_ERROR(ExportWidget(msg, gadget));

	return SendOnWidgetUpgraded(msg);
}

OP_STATUS
OpScopeWidgetManager::OnWidgetUninstalled(OpGadget *gadget)
{
	if (!IsEnabled())
		return OpStatus::OK;
	OP_ASSERT(gadget);

	WidgetUninstalled msg;
	OpString &id = msg.GetWidgetIDRef();
	RETURN_IF_ERROR(gadget->GetIdentifier(id));

	// Send event
	RETURN_IF_ERROR(SendOnWidgetUninstalled(msg));

	// Check if it matches a response
	unsigned count = uninstall_requests.GetCount();
	for (unsigned i = 0; i < count; ++ i)
	{
		UninstallRequest req = uninstall_requests.Get(i);
		if (req.GetGadget() == gadget)
		{
			uninstall_requests.Remove(i);
			return SendUninstallWidget(req.GetTag());
		}
	}
	return OpStatus::OK;
}

OpScopeWidgetManager::InstallerObject *
OpScopeWidgetManager::FindInstaller(unsigned installer_id)
{
	unsigned count = install_objects.GetCount();
	for (unsigned i = 0; i < count; ++i)
	{
		InstallerObject *installer = install_objects.Get(i);
		if (installer->GetID() == installer_id)
			return installer;
	}
	return NULL;
}

OpScopeWidgetManager::InstallerObject *
OpScopeWidgetManager::ReleaseInstaller(unsigned installer_id)
{
	unsigned count = install_objects.GetCount();
	for (unsigned i = 0; i < count; ++i)
	{
		InstallerObject *installer = install_objects.Get(i);
		if (installer->GetID() == installer_id)
		{
			install_objects.Remove(i);
			return installer;
		}
	}
	return NULL;
}

OpScopeWidgetManager::CheckForUpdateObject *
OpScopeWidgetManager::FindAndRelaseUpdateObject(const uni_char* widget_id)
{
	unsigned count = check_update_requests.GetCount();
	for (unsigned i = 0; i < count; ++i)
	{
		CheckForUpdateObject *updateobject = (check_update_requests.Get(i));
		if (uni_stri_eq(updateobject->GetGadgetID(), widget_id))
		{
			check_update_requests.Remove(i);
			return updateobject;
		}
	}
	return NULL;
}

void
OpScopeWidgetManager::SendWidgetUpgradedResult(OpGadgetClass* klass, WidgetUpdateResult::Result res)
{
	const uni_char* gadgetid = klass->GetGadgetId();
	if (!gadgetid)
		return;

	CheckForUpdateObject *update_object = FindAndRelaseUpdateObject(gadgetid);
	if (!update_object)
		return;

	unsigned tag = update_object->GetTag();
	OP_DELETE(update_object);

	WidgetUpdateResult msg;

	msg.SetResult(res);

	OpStatus::Ignore(SendUpdateWidget(msg, tag));
}


#endif // SCOPE_WIDGET_MANAGER_SUPPORT
