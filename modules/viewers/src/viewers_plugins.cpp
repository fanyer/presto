/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(_PLUGIN_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/excepts.h"
#include "modules/util/OpHashTable.h"
#include "modules/dochand/winman.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"
#include "modules/pi/OpNS4PluginAdapter.h"
#endif // NS4P_COMPONENT_PLUGINS

# include "modules/viewers/plugins.h"
# include "modules/viewers/viewers.h"


OP_STATUS PluginContentTypeDetails::SetContentType(const OpStringC& content_type, const OpStringC& description)
{
	if (content_type.IsEmpty())
		return OpStatus::ERR;

	if (m_plugin && m_plugin->SupportsContentType(content_type))
		return OpStatus::ERR; //Already exists in list

	//Disconnect from old content-type viewer
	if (g_viewers && m_plugin && m_plugin->m_in_plugin_viewer_list && m_content_type.HasContent() && m_content_type.CompareI(content_type) != 0)
	{
		//Todo: Check for mathing extension, and only disconnect if no match found?
		if (Viewer* disconnect_from_viewer = g_viewers->FindViewerByMimeType(m_content_type))
			OpStatus::Ignore(disconnect_from_viewer->DisconnectFromPlugin(m_plugin));
	}

	OP_STATUS ret;
	if ((ret=m_content_type.Set(content_type)) != OpStatus::OK ||
	    (ret=m_description.Set(description)) != OpStatus::OK)
	{
		return ret;
	}

	//Connect to new content-type viewer
	if (g_viewers && m_plugin && m_plugin->m_in_plugin_viewer_list)
	{
		//Connect plugin to viewer
		if (Viewer* connect_to_viewer = g_viewers->FindViewerByMimeType(content_type))
			OpStatus::Ignore(connect_to_viewer->ConnectToPlugin(m_plugin));
	}

	return OpStatus::OK;
}

OP_STATUS PluginContentTypeDetails::GetContentType(OpString& content_type, OpString& description)
{
	OP_STATUS ret;
	if ((ret=content_type.Set(m_content_type))!=OpStatus::OK ||
	    (ret=description.Set(m_description))!=OpStatus::OK)
	{
		return ret;
	}
	return OpStatus::OK;
}

OP_STATUS PluginContentTypeDetails::AddExtension(const OpStringC& extension, const OpStringC& extension_description)
{
	if (extension.IsEmpty())
		return OpStatus::ERR;

	if (SupportsExtension(extension))
		return OpStatus::OK; //Already exists in list

	PluginExtensionDetails* extension_details = OP_NEW(PluginExtensionDetails, ());
	if (!extension_details)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret;
	if (OpStatus::IsError(ret=extension_details->m_extension.Set(extension)) ||
	    OpStatus::IsError(ret=extension_details->m_extension_description.Set(extension_description)) ||
		OpStatus::IsError(ret=m_supported_extension_list.Add(extension_details)))
	{
		OP_DELETE(extension_details);
		return ret;
	}

	//Connect plugin to viewers
	if (g_viewers && m_plugin && m_plugin->m_in_plugin_viewer_list)
	{
		OpVector<Viewer> connect_to_viewer_array;
		if (OpStatus::IsSuccess(g_viewers->FindAllViewersByExtension(extension, &connect_to_viewer_array)))
		{
			for (UINT i=0; i<connect_to_viewer_array.GetCount(); i++)
				OpStatus::Ignore(connect_to_viewer_array.Get(i)->ConnectToPlugin(m_plugin));
		}
	}
	return OpStatus::OK;
}

OP_STATUS PluginContentTypeDetails::AddExtensions(const OpStringC& extensions, const OpStringC& extensions_separator_chars,
                                                  const OpStringC& extensions_descriptions, const OpStringC& extensions_descriptions_separator_chars)
{
	if (extensions.IsEmpty())
		return OpStatus::OK;

	OpAutoVector<OpString> tmp_extension_array;

	OP_STATUS ret;
	if (extensions_separator_chars.HasContent())
	{
		RETURN_IF_ERROR(Viewer::ParseExtensions(extensions, extensions_separator_chars, &tmp_extension_array));
	}
	else
	{
		OpString* extension_item = OP_NEW(OpString, ());
		if (!extension_item)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(ret=extension_item->Set(extensions)) ||
			OpStatus::IsError(ret=tmp_extension_array.Add(extension_item)))
		{
			OP_DELETE(extension_item);
			return ret;
		}
	}

	OpAutoVector<OpString> tmp_description_array;

	if (extensions_descriptions.HasContent() && extensions_descriptions_separator_chars.HasContent())
	{
		RETURN_IF_ERROR(Viewer::ParseExtensions(extensions_descriptions, extensions_descriptions_separator_chars, &tmp_description_array));
	}
	else
	{
		OpString* extension_description_item = OP_NEW(OpString, ());
		if (!extension_description_item)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(ret=extension_description_item->Set(extensions_descriptions)) ||
			OpStatus::IsError(ret=tmp_description_array.Add(extension_description_item)))
		{
			OP_DELETE(extension_description_item);
			return ret;
		}
	}

	UINT description_count = tmp_description_array.GetCount();

	OpString dummy_description_item;
	OpString* tmp_description_item = &dummy_description_item;

	if (description_count == 1)
		tmp_description_item = tmp_description_array.Get(0);

	for (UINT i=0; i<tmp_extension_array.GetCount(); i++)
	{
		if (description_count > 1)
			tmp_description_item = i<description_count ? tmp_description_array.Get(i) : &dummy_description_item;

		OpString *tmp_extension = tmp_extension_array.Get(i);
		if (tmp_extension->Compare("*"))	// npdsplay.dll has this for example
		{
			RETURN_IF_ERROR(AddExtension(*tmp_extension, *tmp_description_item));
		}
	}
	return OpStatus::OK;
}

OP_STATUS PluginContentTypeDetails::RemoveExtension(const OpStringC& extension)
{
	PluginExtensionDetails* extension_item = GetExtensionDetails(extension);
	if (!extension_item)
		return OpStatus::OK; //Nothing to do

	m_supported_extension_list.Delete(extension_item);

	//Disconnect plugin from viewers
	OpVector<Viewer> disconnect_from_viewer_array;
	if (g_viewers && m_plugin && m_plugin->m_in_plugin_viewer_list && g_viewers->FindAllViewersByExtension(extension, &disconnect_from_viewer_array)==OpStatus::OK)
	{
		for (UINT i=0; i<disconnect_from_viewer_array.GetCount(); i++)
			OpStatus::Ignore(disconnect_from_viewer_array.Get(i)->DisconnectFromPlugin(m_plugin));
	}
	return OpStatus::OK;
}

BOOL PluginContentTypeDetails::SupportsExtension(const OpStringC& extension) const
{
	return GetExtensionDetails(extension)!=NULL;
}

OP_STATUS PluginContentTypeDetails::ConnectToViewers()
{
	if (!g_viewers || !m_plugin || !m_plugin->m_in_plugin_viewer_list)
		return OpStatus::OK; //Nothing to do

	//Connect plugin to viewers by content-type
	Viewer* connect_to_viewer = g_viewers->FindViewerByMimeType(m_content_type);

	BOOL is_new_viewer = !connect_to_viewer;

	if (is_new_viewer)
	{
		// Viewer for mimetype doesn't exist yet
		connect_to_viewer = OP_NEW(Viewer, ());
		if (!connect_to_viewer)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<Viewer> viewer(connect_to_viewer);
		connect_to_viewer->SetAction(VIEWER_PLUGIN);
		RETURN_IF_ERROR(connect_to_viewer->SetContentType(m_content_type));
		RETURN_IF_ERROR(g_viewers->AddViewer(connect_to_viewer));
		viewer.release();
	}

	OpStatus::Ignore(connect_to_viewer->ConnectToPlugin(m_plugin));

	for (UINT i=0; i<m_supported_extension_list.GetCount(); i++)
	{
		PluginExtensionDetails* extension_details = m_supported_extension_list.Get(i);

		if (is_new_viewer)
		{
			// Connect plugin to viewers by extensions, only if the content type was not already known, see bug 307918
			OpVector<Viewer> connect_to_viewer_array;

			RETURN_IF_ERROR(g_viewers->FindAllViewersByExtension(extension_details->m_extension, &connect_to_viewer_array));

			for (UINT j=0; j<connect_to_viewer_array.GetCount(); j++)
				OpStatus::Ignore(connect_to_viewer_array.Get(j)->ConnectToPlugin(m_plugin));
		}
		RETURN_IF_ERROR(connect_to_viewer->AddExtension(extension_details->m_extension));
	}
	return OpStatus::OK;
}

OP_STATUS PluginContentTypeDetails::DisconnectFromViewers()
{
	if (!g_viewers || !m_plugin || !m_plugin->m_in_plugin_viewer_list)
		return OpStatus::OK; //Nothing to do

	//Disconnect plugin from viewers by content-type
	if (Viewer* disconnect_from_viewer = g_viewers->FindViewerByMimeType(m_content_type))
		OpStatus::Ignore(disconnect_from_viewer->DisconnectFromPlugin(m_plugin));

	//Disconnect plugin from viewers by extensions
	for (UINT i=0; i<m_supported_extension_list.GetCount(); i++)
	{
		OpVector<Viewer> disconnect_from_viewer_array;
		if (g_viewers->FindAllViewersByExtension(m_supported_extension_list.Get(i)->m_extension, &disconnect_from_viewer_array)==OpStatus::OK)
		{
			for (UINT j=0; j<disconnect_from_viewer_array.GetCount(); j++)
				OpStatus::Ignore(disconnect_from_viewer_array.Get(j)->DisconnectFromPlugin(m_plugin));
		}
	}
	return OpStatus::OK;
}

PluginExtensionDetails* PluginContentTypeDetails::GetExtensionDetails(const OpStringC& extension) const
{
	if (extension.IsEmpty())
		return NULL;

	for (UINT i=0; i<m_supported_extension_list.GetCount(); i++)
	{
		PluginExtensionDetails* extension_item = m_supported_extension_list.Get(i);
		if (extension_item->m_extension.CompareI(extension)==0) //Note to self: Are any platforms using case-sensitive extensions?!?
			return extension_item; //We've found a match
	}
	return NULL;
}

PluginViewer::PluginViewer()
: m_enabled(TRUE),
  m_in_plugin_viewer_list(FALSE)
{
}

PluginViewer::~PluginViewer()
{
	if (g_viewers && m_in_plugin_viewer_list)
	{
		//Disconnect plugin from viewers
		Viewer* tmp_viewer;
		OpHashIterator* iter = g_viewers->m_viewer_list.GetIterator();
		OP_STATUS ret = iter->First();
		while(ret == OpStatus::OK)
		{
			if ((tmp_viewer=reinterpret_cast<Viewer*>(iter->GetData()))!= NULL)
				OpStatus::Ignore(tmp_viewer->DisconnectFromPlugin(this));

			ret = iter->Next();
		}
		OP_DELETE(iter);
	}
}

OP_STATUS PluginViewer::Construct(const OpStringC& path, const OpStringC& product_name, const OpStringC& description, const OpStringC& version, OpComponentType component_type)
{
	OP_STATUS ret;
	if ((ret=m_path.Set(path))!=OpStatus::OK ||
		(ret=m_product_name.Set(product_name))!=OpStatus::OK ||
		(ret=m_description.Set(description))!=OpStatus::OK ||
		(ret=m_version.Set(version))!=OpStatus::OK)
	{
		return ret;
	}
	m_component_type = component_type;

	//Connect plugin to viewers by path
	if (g_viewers && m_in_plugin_viewer_list)
	{
		Viewer* tmp_viewer;
		OpHashIterator* iter = g_viewers->m_viewer_list.GetIterator();
		ret = iter->First();
		while(ret == OpStatus::OK)
		{
			if ((tmp_viewer=reinterpret_cast<Viewer*>(iter->GetData()))!=NULL && tmp_viewer->m_preferred_plugin_path.HasContent() && m_path.HasContent())
			{
				// FIXME: OOM
				BOOL match;
				if (OpStatus::IsSuccess(g_op_system_info->PathsEqual(tmp_viewer->m_preferred_plugin_path.CStr(), m_path.CStr(), &match)) && match)
					OpStatus::Ignore(tmp_viewer->ConnectToPlugin(this));
			}
			ret = iter->Next();
		}
		OP_DELETE(iter);
	}
	return OpStatus::OK;
}

OP_STATUS PluginViewer::AddContentType(PluginContentTypeDetails* content_type)
{
	if (!content_type)
		return OpStatus::ERR_NULL_POINTER;

	content_type->m_plugin = this; //This magic is not public available..
	content_type->ConnectToViewers();

	 return m_supported_content_type_list.Add(content_type);
}

OP_STATUS PluginViewer::RemoveContentType(const OpStringC& content_type)
{
	PluginContentTypeDetails* content_type_details = GetContentTypeDetails(content_type);
	if (!content_type_details)
		return OpStatus::OK; //Nothing to do

	content_type_details->DisconnectFromViewers();
	m_supported_content_type_list.Delete(content_type_details);

	return OpStatus::OK;
}

BOOL PluginViewer::SupportsContentType(const OpStringC& content_type) const
{
	return GetContentTypeDetails(content_type) != NULL;
}

PluginContentTypeDetails* PluginViewer::GetContentTypeDetails(const OpStringC& content_type) const
{
	if (content_type.IsEmpty())
		return NULL;

	for (UINT i=0; i<m_supported_content_type_list.GetCount(); i++)
	{
		PluginContentTypeDetails* content_type_details_item = m_supported_content_type_list.Get(i);
		if (content_type_details_item->m_content_type.CompareI(content_type)==0)
			return content_type_details_item; //We've found a match
	}
	return NULL;
}

BOOL PluginViewer::SupportsExtension(const OpStringC& extension) const
{
	if (extension.IsEmpty())
		return FALSE;

	for (UINT i=0; i<m_supported_content_type_list.GetCount(); i++)
	{
		PluginContentTypeDetails* content_type_item = m_supported_content_type_list.Get(i);
		if (content_type_item->SupportsExtension(extension))
			return TRUE; //We've found a match
	}
	return FALSE;
}

OP_STATUS PluginViewer::ConnectToViewers()
{
	m_in_plugin_viewer_list = TRUE;

	for (UINT i=0; i<m_supported_content_type_list.GetCount(); i++)
		m_supported_content_type_list.Get(i)->ConnectToViewers();

	return OpStatus::OK;
}

OP_STATUS PluginViewer::DisconnectFromViewers()
{
	if (!m_in_plugin_viewer_list)
		return OpStatus::OK; //Nothing to do

	for (UINT i=0; i<m_supported_content_type_list.GetCount(); i++)
		m_supported_content_type_list.Get(i)->DisconnectFromViewers();

	m_in_plugin_viewer_list = FALSE;
	return OpStatus::OK;
}

OP_STATUS PluginViewer::GetTypeDescription(const OpStringC& content_type, OpString& description) const
{
	// Find Content-Type in type list
	const PluginContentTypeDetails* cnt_details = GetContentTypeDetails(content_type);
	if (cnt_details)
	{
		// Just get the entry for the first extension listed
		const PluginExtensionDetails* ext_details = cnt_details->GetFirstExtensionDetails();
		if (ext_details)
		{
			// We *should* have a string on the form "Type (*.ext)" here; cut off the
			// " (*.ext)" part.
			int len = ext_details->m_extension_description.FindLastOf('(');
			if (len != KNotFound)
			{
				OP_STATUS rc = description.Set(ext_details->m_extension_description, len);
				description.Strip();
				return rc;
			}
			else
			{
				// Ooops, weird string. Just return what we have
				return description.Set(ext_details->m_extension_description);
			}
		}
	}
	return OpStatus::OK;
}

#ifdef NS4P_COMPONENT_PLUGINS
/**
 * A message filter allowing messages to be dispatched to only one channel
 * in the component the channel lives.
 */
class NarcissismFilter
	: public OpTypedMessageSelection
{
public:
	NarcissismFilter(const OpMessageAddress& address)
		: m_address(address) {}

	virtual BOOL IsMember(OpTypedMessage* message) {
		return message->GetDst().co != m_address.co
			|| message->GetDst().ch == m_address.ch;
	}

protected:
	OpMessageAddress m_address;
};

class PluginProber;

/**
 * A class encapsulating detection control of a remote plug-in component.
 *
 * The method DetectPlugins() will appear synchronous. This is to limit
 * ripples throughout core.
 *
 * Changing the viewers API to be asynchronous is scheduled for out of
 * process phase 2 (CORE-40424).
 */
class PluginDetector : public OpMessageListener
{
public:
	PluginDetector(OpPluginDetectionListener* listener);
	~PluginDetector();

	inline bool IsDetecting();
	OP_STATUS AddDetectionDoneListener(PluginViewers::OpDetectionDoneListener* listener);
	OP_STATUS RemoveDetectionDoneListener(PluginViewers::OpDetectionDoneListener* listener);

	OP_STATUS DetectPlugins(const OpStringC& suggested_path);
	OP_STATUS HandleProbeResponse(OpPluginProbeResponseMessage* message, OpComponentType type);

	OP_STATUS StartDetection(const OpStringC& suggested_path);
	void StopDetection();
	void OnDetectionFinished(const PluginProber* probe);

	// Implement OpMessageListener

	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

protected:
	bool m_detecting;
	OpChannel* m_plugin_component;
	OpPluginDetectionListener* m_listener;
	PluginProber* m_probes[COMPONENT_LAST];
	UniString m_plugins_path;
	OpVector<PluginViewers::OpDetectionDoneListener> m_detection_done_listeners;

	void OnPeerDisconnected();
	OP_STATUS OnPluginEnumerateResponse(OpPluginEnumerateResponseMessage* message);
	void StartEnumeration();
};

/**
 * This class performs the actual probing of plug-ins, asynchronously.
 *
 * Probing starts by adding OpPluginProbeMessages to this instance and calling
 * Start(). Note that messages must be of type \type (see OpComponentType).
 * This will be correctly handled by PluginDetector::StartDetection().
 *
 * Upon Start(), the probe will request a component channel, set itself as
 * message listener, and return. Soon after, it will receive a PeerConnected
 * and call Start() again. This time around, it will start sending one message.
 * As long as a component channel is established and there are messages in the
 * queue, Start() will always send the last message in the queue. The next
 * message is only sent when a proper response is returned or a
 * PeerDisconnected. In any case, the message is popped from the queue, and
 * Start() is called again to send the next message. This is repeated until the
 * queue is empty.
 */
class PluginProber : public OpMessageListener, public OpTimerListener
{
public:
	/**
	 * Create a plug-in prober using the native component.
	 * @param component  Native plug-in component.
	 * @param detector   The instigating PluginDetector.
	 */
	inline PluginProber(OpChannel* component, PluginDetector* detector);

	/**
	 * Create a plug-in prober for a particular type.
	 * @param type      Component type, determining architecture. See OpComponentType.
	 * @param detector  The instigating PluginDetector.
	 */
	inline PluginProber(OpComponentType type, PluginDetector* detector);
	inline ~PluginProber();

	/**
	 * Initialise the probe.
	 *
	 * This function currently only adds itself as message listener.
	 */
	OP_STATUS Init();

	/// Add probe message to be sent.
	inline OP_STATUS Add(OpPluginProbeMessage* message);

	/// Clear message queue.
	void Clear();

	/// Start sending messages. If the component channel is missing, it will
	/// be created. Messages aren't sent until we receive a PeerConnected.
	OP_STATUS Start();

	// Implement OpMessageListener

	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

	// Implement OpTimerListener

	virtual void OnTimeOut(OpTimer* timer);

private:
	OpComponentType m_type;
	OpChannel* m_component;
	bool m_connected;
	PluginDetector* m_detector;
	OpVector<OpPluginProbeMessage> m_messages;  ///< Ideally, we'd use a queue/stack here.
	OpTimer m_timer;

	/// Handle component crashing.
	void OnPeerDisconnected();

	/// Handle a probe response message.
	inline void OnPluginProbeResponse(OpPluginProbeResponseMessage* message);
};

PluginDetector::PluginDetector(OpPluginDetectionListener* listener)
	: m_detecting(false), m_plugin_component(NULL), m_listener(listener)
{
	op_memset(m_probes, 0, sizeof(m_probes));
}

PluginDetector::~PluginDetector()
{
	OP_DELETE(m_plugin_component);
	for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
		OP_DELETE(m_probes[i]);
}

bool PluginDetector::IsDetecting()
{
	return m_detecting;
}

OP_STATUS PluginDetector::AddDetectionDoneListener(PluginViewers::OpDetectionDoneListener* listener)
{
	return m_detection_done_listeners.Add(listener);
}

OP_STATUS PluginDetector::RemoveDetectionDoneListener(PluginViewers::OpDetectionDoneListener* listener)
{
	return m_detection_done_listeners.RemoveByItem(listener);
}

OP_STATUS PluginDetector::DetectPlugins(const OpStringC& suggested_path)
{
	m_plugins_path.Clear();
	if (suggested_path.HasContent())
		RETURN_IF_ERROR(m_plugins_path.SetConstData(suggested_path.CStr()));
	OpAutoPtr<OpPluginEnumerateResponseMessage> response;

	OpAutoPtr<OpSyncMessenger> enumerate_sync(OP_NEW(OpSyncMessenger, (NULL)));
	RETURN_OOM_IF_NULL(enumerate_sync.get());

	/* Create plug-in component. */
	OpChannel* component;
	RETURN_IF_ERROR(g_opera->RequestComponent(&component, COMPONENT_PLUGIN));
	enumerate_sync->GivePeer(component);

	/* Send enumeration message with a timeout. */
	RETURN_VALUE_IF_ERROR(enumerate_sync->SendMessage(
	                      OpPluginEnumerateMessage::Create(m_plugins_path),
	                      false, VIEWERS_PLUGIN_ENUMERATION_TIMEOUT), OpStatus::OK);

	response = OpPluginEnumerateResponseMessage::Cast(enumerate_sync->TakeResponse());

	/* Reuse enumerate process for native plugin probing (i.e. for probing of 64-bit plugins on 64-bit platforms. */
	OpAutoArray<OpSyncMessenger*> messengers(OP_NEWA(OpSyncMessenger*, COMPONENT_LAST));
	RETURN_OOM_IF_NULL(messengers.get());
	op_memset(messengers.get(), 0, COMPONENT_LAST * sizeof(OpSyncMessenger*));
	messengers.get()[COMPONENT_PLUGIN] = enumerate_sync.release();

	for (UINT32 i = 0; i < response->GetLibraryPaths().GetCount(); ++i)
	{
		OpComponentType type = static_cast<OpComponentType>(response->GetLibraryPaths().Get(i)->GetType());
		OpSyncMessenger*& sync = messengers.get()[type];

		/* Check if we have this component available. */
		if (!sync)
		{
			OpAutoPtr<OpSyncMessenger> new_sync (OP_NEW(OpSyncMessenger, (NULL)));
			if (!new_sync.get())
			{
				for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
					OP_DELETE(messengers.get()[i]);
				return OpStatus::ERR_NO_MEMORY;
			}
			sync = new_sync.release();
		}
		if (!sync->HasPeer())
		{
			/* Create plug-in component of requested type. */
			OpChannel* component;
			OP_STATUS status = g_opera->RequestComponent(&component, type);
			if (OpStatus::IsError(status))
			{
				for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
					OP_DELETE(messengers.get()[i]);
				return status;
			}
			sync->GivePeer(component);
		}

		/* Probe library with a timeout. */
		OP_STATUS s = sync->SendMessage(OpPluginProbeMessage::Create(response->GetLibraryPathsRef().Get(i)->GetPath()),
		                                false, VIEWERS_PLUGIN_PROBE_TIMEOUT);
		if (OpStatus::IsSuccess(s))
			HandleProbeResponse(OpPluginProbeResponseMessage::Cast(sync->GetResponse()), type);
		/* We purposely don't propagate OpStatus::IsError(s) upwards here because want
		 * to keep loading more plugins even though probing failed for a single one. */
		else if (sync->HasPeer())
			/* Probing failed. This may be a failure to start the wrapper, or a failure of the
			 * wrapper. In any case, we discard the process. */
			OP_DELETE(sync->TakePeer());
	}
	for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
		OP_DELETE(messengers.get()[i]);

	return OpStatus::OK;
}

OP_STATUS PluginDetector::StartDetection(const OpStringC& suggested_path)
{
	OP_NEW_DBG("PluginDetector::StartDetection", "ns4p");
	OP_ASSERT(!m_plugin_component
	          || !"PluginDetector::StartDetection: The component channel should've been deleted already");
	OP_ASSERT(!m_detecting || !"PluginDetector::StartDetection: Detection already in progress");
	OP_DBG(("Asynchronous plugin detection started."));

	m_plugins_path.Clear();
	if (suggested_path.HasContent())
		RETURN_IF_ERROR(m_plugins_path.SetCopyData(suggested_path.CStr()));

	/* Create plug-in component. */
	RETURN_IF_ERROR(g_opera->RequestComponent(&m_plugin_component, COMPONENT_PLUGIN));
	OP_STATUS status = m_plugin_component->AddMessageListener(this);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_plugin_component);
		m_plugin_component = NULL;
	}
	else
		m_detecting = true;

	return status;
}

void PluginDetector::StopDetection()
{
	if (IsDetecting())
	{
		for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
		{
			OP_DELETE(m_probes[i]);
			m_probes[i] = NULL;
		}
		m_detecting = false;
	}
}

void PluginDetector::OnDetectionFinished(const PluginProber* probe)
{
	OP_NEW_DBG("PluginDetector::OnDetectionFinished", "ns4p");
	UINT32 count = 0;
	for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
	{
		if (probe == m_probes[i])
		{
			OP_DBG(("prober with id == %d is done."));
			OP_DELETE(probe);
			m_probes[i] = NULL;
		}
		else if (m_probes[i])
			++count;
	}
	if (count == 0)
	{
		m_detecting = false;
		OP_DBG(("all probers are done, will now notify %d detection done listeners.", m_detection_done_listeners.GetCount()));
		/* Tell other parts of core that plug-in detection is done. */
		for (UINT32 i = 0; i < m_detection_done_listeners.GetCount(); ++i)
			m_detection_done_listeners.Get(i)->DetectionDone();
		m_detection_done_listeners.Clear();
		OP_DBG(("detection done listeners notified."));
	}
}

OP_STATUS PluginDetector::ProcessMessage(const OpTypedMessage* message)
{
	if (!m_detecting)
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	switch (message->GetType())
	{
		case OpStatusMessage::Type:
			status = OpStatusMessage::Cast(message)->GetStatus();
			break;
		case OpPeerConnectedMessage::Type:
			StartEnumeration();
			break;
		case OpPeerDisconnectedMessage::Type:
			OnPeerDisconnected();
			break;
		case OpPluginEnumerateResponseMessage::Type:
			status = OnPluginEnumerateResponse(OpPluginEnumerateResponseMessage::Cast(message));
			break;
		default:
			OP_ASSERT(!"PluginDetector::ProcessMessage: Received unhandled/unknown message");
			break;
	}
	if (OpStatus::IsError(status))
	{
		for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
		{
			if (m_probes[i])
			{
				OP_DELETE(m_probes[i]);
				m_probes[i] = NULL;
			}
		}
		m_detecting = false;
	}
	return status;
}

void PluginDetector::OnPeerDisconnected()
{
	OP_DELETE(m_plugin_component);
	m_plugin_component = NULL;
	m_detecting = false;
}

OP_STATUS PluginDetector::OnPluginEnumerateResponse(OpPluginEnumerateResponseMessage* message)
{
	// Reuse native plug-in component created in ::StartDetection().
	m_probes[COMPONENT_PLUGIN] = OP_NEW(PluginProber, (m_plugin_component, this));
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (m_probes[COMPONENT_PLUGIN])
		status = m_probes[COMPONENT_PLUGIN]->Init();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_plugin_component);
		m_plugin_component = NULL;
		return status;
	}
	m_plugin_component->RemoveMessageListener(this);
	m_plugin_component = NULL;

	for (UINT32 i = 0; i < message->GetLibraryPaths().GetCount(); ++i)
	{
		typedef OpNs4pluginsMessages_MessageSet::PluginEnumerateResponse_LibraryPath LibraryPath;
		LibraryPath* libpath = message->GetLibraryPaths().Get(i);

		if (OpNS4PluginAdapter::IsBlacklistedFilename(libpath->GetPath()))
			continue;

		OpComponentType type = static_cast<OpComponentType>(libpath->GetType());
		PluginProber*& probe = m_probes[type];

		if (!probe)
		{
			probe = OP_NEW(PluginProber, (type, this));
			RETURN_OOM_IF_NULL(probe);
		}
		RETURN_IF_ERROR(probe->Add(OpPluginProbeMessage::Create(libpath->GetPath())));
	}
	for (UINT32 i = 0; i < COMPONENT_LAST; ++i)
	{
		if (m_probes[i])
			m_probes[i]->Start();
	}
	return OpStatus::OK;
}

OP_STATUS PluginDetector::HandleProbeResponse(OpPluginProbeResponseMessage* message, OpComponentType type)
{
	typedef OpNs4pluginsMessages_MessageSet::PluginProbeResponse_ContentType ContentType;

	/* Register plug-in library. */
	UniString path = message->GetPath();
	UniString name = message->GetName();
	UniString description = message->GetDescription();
	UniString version = message->GetVersion();

	void* token;
	RETURN_IF_ERROR(m_listener->OnPrepareNewPlugin(path.Data(TRUE), name.Data(TRUE),
												   description.Data(TRUE), version.Data(TRUE),
												   type, TRUE, token));

	/* Register supported content-types. */
	for (unsigned int type = 0; type < message->GetContentTypes().GetCount(); type++)
	{
		ContentType* content_type = message->GetContentTypes().Get(type);
		UniString content_type_string = content_type->GetContentType();
		UniString content_type_description = content_type->GetDescription();

		RETURN_IF_ERROR(m_listener->OnAddContentType(token, content_type_string.Data(TRUE)));

		for (unsigned int i = 0; i < content_type->GetExtensions().GetCount(); i++)
		{
			const UniString &extension_string = content_type->GetExtensions().Get(i);

			RETURN_IF_ERROR(m_listener->OnAddExtensions(token, content_type_string.Data(TRUE),
														extension_string.Data(TRUE),
														content_type_description.Data(TRUE)));
		}
	}

	RETURN_IF_ERROR(m_listener->OnCommitPreparedPlugin(token));

	return OpStatus::OK;
}

void PluginDetector::StartEnumeration()
{
	OP_ASSERT(m_plugin_component || !"PluginDetector::OnPeerConnectedMessage: Missing component");
	OP_STATUS status = m_plugin_component->SendMessage(OpPluginEnumerateMessage::Create(m_plugins_path));
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_plugin_component);
		m_plugin_component = NULL;
		m_detecting = false;
	}
}

PluginProber::PluginProber(OpChannel* component, PluginDetector* detector) :
	m_type(COMPONENT_PLUGIN), m_component(component), m_connected(component && component->IsConnected()), m_detector(detector)
{
	m_timer.SetTimerListener(this);
}

PluginProber::PluginProber(OpComponentType type, PluginDetector* detector) :
	m_type(type), m_component(NULL), m_connected(false), m_detector(detector)
{
	m_timer.SetTimerListener(this);
}

PluginProber::~PluginProber()
{
	Clear();
	OP_DELETE(m_component);
}

OP_STATUS PluginProber::Init()
{
	return m_component->AddMessageListener(this, true);
}

OP_STATUS PluginProber::Add(OpPluginProbeMessage* message)
{
	return m_messages.Add(message);
}

void PluginProber::Clear()
{
	for (UINT32 i = 0; i < m_messages.GetCount(); ++i)
		OP_DELETE(m_messages.Get(i));
	m_messages.Clear();
}

OP_STATUS PluginProber::Start()
{
	if (m_messages.GetCount() == 0)
	{
		m_detector->OnDetectionFinished(this);
		return OpStatus::OK;
	}

	if (!m_component)
	{
		/**
		 * Create a component if we're missing one. This can occur at start,
		 * because we haven't created one yet, or when our peer has been
		 * disconnected due to a faulty plug-in.
		 */
		OP_STATUS status = g_opera->RequestComponent(&m_component, m_type);
		if (OpStatus::IsError(status))
		{
			m_detector->OnDetectionFinished(this);
			return status;
		}
		status = m_component->AddMessageListener(this);
		if (OpStatus::IsError(status))
		{
			m_detector->OnDetectionFinished(this);
			return status;
		}
	}
	else
	{
		/**
		 * Send the next message. This occurs on PeerConnected, when we've
		 * created a component, or PluginProbeResponse after successfully
		 * probing a plug-in. If we're reusing a component (see
		 * PluginDetector::OnPluginEnumerateResponse), we would also skip
		 * right to this step.
		 */
		OP_STATUS status = m_component->SendMessage(m_messages.Get(0));
		m_messages.Remove(0);
		if (OpStatus::IsError(status))
		{
			if (status == OpStatus::ERR_NO_MEMORY)
			{
				m_detector->OnDetectionFinished(this);
				return status;
			}
			OnPeerDisconnected();
		}
		else
			m_timer.Start(VIEWERS_PLUGIN_ENUMERATION_TIMEOUT);
	}

	return OpStatus::OK;
}

OP_STATUS PluginProber::ProcessMessage(const OpTypedMessage* message)
{
	m_timer.Stop();
	switch (message->GetType())
	{
		case OpStatusMessage::Type:
			// A plug-in failed to load. Continue...
		case OpPeerConnectedMessage::Type:
			m_connected = true;
			Start();
			break;
		case OpPeerDisconnectedMessage::Type:
			OnPeerDisconnected();
			break;
		case OpPluginProbeResponseMessage::Type:
			OnPluginProbeResponse(OpPluginProbeResponseMessage::Cast(message));
			break;
		case OpPluginEnumerateResponseMessage::Type:
			// Already handled by PluginDetector.
			break;
		default:
			OP_ASSERT(!"PluginProber::ProcessMessage: Received unhandled/unknown message");
			break;
	}
	return OpStatus::OK;
}

void PluginProber::OnTimeOut(OpTimer*)
{
	// Message timed out. We're going to assume PeerDisconnected.
	OnPeerDisconnected();
}

void PluginProber::OnPeerDisconnected()
{
	// The component channel is dead.
	OP_DELETE(m_component);
	m_component = NULL;

	if (m_connected)
	{
		// if we were connected, re-establish connection and send the remaining messages.
		m_connected = false;
		Start();
	}
	else
	{
		// There was an error in creating this component type, don't try again.
		m_detector->OnDetectionFinished(this);
	}
}

void PluginProber::OnPluginProbeResponse(OpPluginProbeResponseMessage* message)
{
	m_detector->HandleProbeResponse(message, m_type);

	// The message was successfully processed. Send the next one.
	Start();
}

#else // !NS4P_COMPONENT_PLUGINS

void PluginViewers::HandleCallback(OpMessage msg, MH_PARAM_1 param1, MH_PARAM_2)
{
	if (msg == MSG_VIEWERS_DETECT_PLUGINS)
	{
		g_main_message_handler->UnsetCallBacks(this);
		m_has_set_callback = FALSE;

		OpString path;
		if (param1)
		{
			path.Set(*reinterpret_cast<OpString*>(param1));
		}

		OpStatus::Ignore(RefreshPluginViewers(FALSE, path));

		if (param1)
		{
			OpString* str = reinterpret_cast<OpString*>(param1);
			OP_DELETE(str);
		}
	}
}

#endif // NS4P_COMPONENT_PLUGINS

PluginViewers::PluginViewers()
	: m_has_detected_plugins(FALSE)
#ifdef NS4P_COMPONENT_PLUGINS
	, m_detector(NULL)
#else // !NS4P_COMPONENT_PLUGINS
	, m_has_set_callback(FALSE)
#endif // !NS4P_COMPONENT_PLUGINS
{
}

PluginViewers::~PluginViewers()
{
	OP_ASSERT((this != g_plugin_viewers || !g_viewers || g_viewers->GetViewerCount()==0) && "For performance reasons, please destroy Viewers before PluginViewers");

	if (UINT32 count = m_manually_disabled_path_list.GetCount())
	{
		while(count-- > 0)
		{
			uni_char *tmp = m_manually_disabled_path_list.Get(count);
			m_manually_disabled_path_list.Remove(count);
			OP_DELETEA(tmp);
		}
	}

#ifdef NS4P_COMPONENT_PLUGINS
	OP_DELETE(m_detector);
#else // NS4P_COMPONENT_PLUGINS
	if (m_has_set_callback)
		g_main_message_handler->UnsetCallBacks(this);
#endif // NS4P_COMPONENT_PLUGINS
}

OP_STATUS PluginViewers::ConstructL()
{
#ifdef NS4P_COMPONENT_PLUGINS
	m_detector = OP_NEW(PluginDetector, (this));
	LEAVE_IF_NULL(m_detector);

	// Ignore any errors because failing to detect plug-ins isn't fatal and
	// Opera should work fine despite the lack of plug-ins.
	OpStatus::Ignore(DetectPluginViewers(UNI_L(""), true));
#else // !NS4P_COMPONENT_PLUGINS
	g_main_message_handler->SetCallBack(this, MSG_VIEWERS_DETECT_PLUGINS, 0);
	m_has_set_callback = TRUE;

	g_main_message_handler->PostMessage(MSG_VIEWERS_DETECT_PLUGINS, 0, 0);
#endif // !NS4P_COMPONENT_PLUGINS

	return OpStatus::OK;
}

OP_STATUS PluginViewers::DetectPluginViewers(const OpStringC& plugin_path, bool async)
{
	if (m_has_detected_plugins)
		return OpStatus::OK;

	m_has_detected_plugins = TRUE;

	ReadDisabledPluginsPref();

	OpString path;
	RETURN_IF_ERROR(path.Set(plugin_path));
	if (path.IsEmpty())
		RETURN_IF_LEAVE(g_op_system_info->GetPluginPathL(g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath), path));

#ifdef NS4P_COMPONENT_PLUGINS
	return async ? m_detector->StartDetection(path) : m_detector->DetectPlugins(path);
#else // !NS4P_COMPONENT_PLUGINS
	return g_op_system_info->DetectPlugins(path, this);
#endif // !NS4P_COMPONENT_PLUGINS
}

OP_STATUS PluginViewers::OnPrepareNewPlugin(const OpStringC& full_path,
											const OpStringC& plugin_name,
											const OpStringC& plugin_description,
											const OpStringC& plugin_version,
											OpComponentType component_type,
											BOOL enabled,
											void*& token)
{
	if (full_path.IsEmpty())
		return OpStatus::ERR;

	if (FindPluginViewerByPath(full_path) != NULL)
		return OpStatus::ERR;

#ifdef _HAS_PLUGIN_BAN_INTERFACE_
	BOOL IsPluginBanned(const uni_char* path);
	if (IsPluginBanned(full_path.CStr()))
		return OpStatus::ERR;
#endif // _HAS_PLUGIN_BAN_INTERFACE_

	if (g_pcapp->IsPluginToBeIgnored(full_path.CStr()))
		return OpStatus::ERR;

	OP_STATUS ret;
	PluginViewer* prepared_plugin = OP_NEW(PluginViewer, ());
	if (!prepared_plugin || (ret=prepared_plugin->Construct(full_path, plugin_name, plugin_description, plugin_version, component_type))!=OpStatus::OK)
	{
		OP_DELETE(prepared_plugin);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Disable plugins that are on the list of manually disabled ones
	UINT32 count = m_manually_disabled_path_list.GetCount();
	for (UINT32 i = 0; i < count; i++)
	{
		if (uni_str_eq(m_manually_disabled_path_list.Get(i), full_path.CStr()))
		{
			enabled = FALSE;
			break;
		}
	}
	prepared_plugin->SetEnabledPluginViewer(enabled);
	token = prepared_plugin;
	return m_prepared_plugin_list.Add(prepared_plugin);
}

OP_STATUS PluginViewers::OnAddContentType(void* token, const OpStringC& content_type, const OpStringC& content_type_description)
{
	PluginViewer* plugin = static_cast<PluginViewer*>(token);
	if (!plugin || m_prepared_plugin_list.Find(plugin)==-1 || content_type.IsEmpty())
		return OpStatus::ERR;

	PluginContentTypeDetails* content_details = plugin->GetContentTypeDetails(content_type);
	if (content_details)
		return OpStatus::OK; //Should this return ERR instead?

	content_details = OP_NEW(PluginContentTypeDetails, ());
	if (!content_details)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret;
	if ((ret=content_details->SetContentType(content_type, content_type_description)) != OpStatus::OK)
	{
		OP_DELETE(content_details);
		return OpStatus::ERR_NO_MEMORY;
	}

	return plugin->AddContentType(content_details);
}

OP_STATUS PluginViewers::OnAddExtensions(void* token, const OpStringC& content_type, const OpStringC& extensions, const OpStringC& extension_description)
{
	PluginViewer* plugin = static_cast<PluginViewer*>(token);
	if (!plugin || m_prepared_plugin_list.Find(plugin)==-1 || content_type.IsEmpty())
		return OpStatus::ERR;

	PluginContentTypeDetails* content_details = plugin->GetContentTypeDetails(content_type);
	if (!content_details)
		return OpStatus::ERR;

	return content_details->AddExtensions(extensions, UNI_L(","), extension_description, UNI_L(""));
}

OP_STATUS PluginViewers::OnCommitPreparedPlugin(void* token)
{
	PluginViewer* plugin = static_cast<PluginViewer*>(token);
	if (!plugin || m_prepared_plugin_list.Find(plugin)==-1)
		return OpStatus::ERR;

	m_prepared_plugin_list.RemoveByItem(plugin);

	OP_STATUS ret = AddPluginViewer(plugin);
	if (OpStatus::IsError(ret))
		OP_DELETE(plugin);
	else
	{
		Window* window = g_windowManager->FirstWindow();
		while (window)
		{
			WindowCommander* cmd = window->GetWindowCommander();
			for (UINT32 i = 0; i < plugin->m_supported_content_type_list.GetCount(); ++i)
				cmd->OnPluginDetected(plugin->m_supported_content_type_list.Get(i)->m_content_type);
			window = window->Suc();
		}
	}
	return ret;
}

OP_STATUS PluginViewers::OnCancelPreparedPlugin(void* token)
{
	m_prepared_plugin_list.Delete(static_cast<PluginViewer*>(token));
	return OpStatus::OK;
}

bool PluginViewers::IsDetecting() const
{
#ifdef NS4P_COMPONENT_PLUGINS
	return m_detector->IsDetecting();
#else // !NS4P_COMPONENT_PLUGINS
	return false;
#endif // !NS4P_COMPONENT_PLUGINS
}

#ifdef NS4P_COMPONENT_PLUGINS
OP_STATUS PluginViewers::AddDetectionDoneListener(OpDetectionDoneListener* listener)
{
	return m_detector->AddDetectionDoneListener(listener);
}

OP_STATUS PluginViewers::RemoveDetectionDoneListener(OpDetectionDoneListener* listener)
{
	return m_detector->RemoveDetectionDoneListener(listener);
}
#endif // NS4P_COMPONENT_PLUGINS

OP_STATUS PluginViewers::DeletePluginViewer(PluginViewer* plugin)
{
	if (!plugin)
		return OpStatus::ERR_NULL_POINTER;

	PluginViewer* plugin_viewer = FindPluginViewerByPath(plugin->GetPath());
	if (!plugin_viewer)
		return OpStatus::OK;

	OnPluginViewerDeleted();
	return m_plugin_list.Delete(plugin_viewer); //PluginViewer/Viewer connections will be disconnected in PluginViewer destructor
}

PluginViewer* PluginViewers::FindPluginViewerByPath(const OpStringC& path)
{
	OpStatus::Ignore(MakeSurePluginsAreDetected());

	if (path.IsEmpty())
		return NULL; //Nothing to search. We're done

	for (UINT i = 0; i < m_plugin_list.GetCount(); i++)
	{
		PluginViewer* plugin_viewer = m_plugin_list.Get(i);
		if (plugin_viewer->GetPath())
		{
			// FIXME: OOM
			BOOL match;
			if (OpStatus::IsSuccess(g_op_system_info->PathsEqual(plugin_viewer->GetPath(), path.CStr(), &match)) && match)
				return plugin_viewer; //We've found a match
		}
	}
	return NULL;
}

OP_STATUS PluginViewers::RefreshPluginViewers(BOOL delayed, const OpStringC& path)
{
#ifdef NS4P_COMPONENT_PLUGINS
	if (!delayed)
		m_detector->StopDetection();
	else if (m_detector->IsDetecting())
		return OpStatus::OK;
#else // !NS4P_COMPONENT_PLUGINS
	if (delayed)
	{
		if (!m_has_set_callback)
		{
			g_main_message_handler->SetCallBack(this, MSG_VIEWERS_DETECT_PLUGINS, 0);
			m_has_set_callback = TRUE;

			MH_PARAM_1 param1 = 0;
			OpString* tmp_string = NULL;
			if (path.HasContent())
			{
				OP_STATUS ret = OpStatus::ERR_NO_MEMORY;
				tmp_string = OP_NEW(OpString, ());
				if (!tmp_string || OpStatus::IsError(ret = tmp_string->Set(path)))
				{
					OP_DELETE(tmp_string);
					return ret;
				}
				param1 = reinterpret_cast<MH_PARAM_1>(tmp_string);
			}

			if (!g_main_message_handler->PostMessage(MSG_VIEWERS_DETECT_PLUGINS, param1, 0))
				OP_DELETE(tmp_string);
		}
		return OpStatus::OK;
	}
#endif // !NS4P_COMPONENT_PLUGINS

	DeleteAllPlugins();
	m_has_detected_plugins = FALSE;

	return DetectPluginViewers(path, !!delayed);
}

OP_STATUS PluginViewers::ReadDisabledPluginsPref()
{
	if (UINT32 count = m_manually_disabled_path_list.GetCount())
	{
		while(count-- > 0)
		{
			uni_char *tmp = m_manually_disabled_path_list.Get(count);
			m_manually_disabled_path_list.Remove(count);
			OP_DELETEA(tmp);
		}
	}

	// Get list of disabled plugin paths, separated by semicolon
	OpStringC disabled_paths = g_pcapp->GetStringPref(PrefsCollectionApp::DisabledPlugins);

	// Iterate over list of disabled plugins and put them in vector
	const uni_char *current_path = disabled_paths.IsEmpty() ? NULL : disabled_paths.CStr();
	while (current_path)
	{
		const uni_char *separator_pos = uni_strchr(current_path, CLASSPATHSEPCHAR);
		const uni_char *next_path = separator_pos ? separator_pos + 1 : NULL;

		// Add plugin path if not empty
		if (current_path && *current_path && (!separator_pos || separator_pos - current_path))
		{
			uni_char* path;
			if (separator_pos)
				RETURN_OOM_IF_NULL(path = UniSetNewStrN(current_path, separator_pos - current_path));
			else
				RETURN_OOM_IF_NULL(path = UniSetNewStr(current_path));
			RETURN_IF_ERROR(m_manually_disabled_path_list.Add(path));

			// Try to find and disable plugin as we have the path
			if (PluginViewer *plugin = FindPluginViewerByPath(path))
				plugin->SetEnabledPluginViewer(FALSE);
		}

		current_path = next_path;
	}

	return OpStatus::OK;
}

OP_STATUS PluginViewers::SaveDisabledPluginsPref()
{
	TempBuffer pref;
	PluginViewer *plug;
	UINT32 count = m_plugin_list.GetCount();

	for (UINT32 i = 0; i < count; i++)
	{
		plug = m_plugin_list.Get(i);

		/* Remove all paths from list that match current plugin's path.
		   Remaining ones will be appended to pref as we don't want to loose
		   disabled paths that are not accessible in current session. */
		if (UINT32 disabled_count_no = m_manually_disabled_path_list.GetCount())
			while (disabled_count_no-- > 0)
				if (uni_str_eq(plug->GetPath(), m_manually_disabled_path_list.Get(disabled_count_no)))
				{
					uni_char *tmp = m_manually_disabled_path_list.Get(disabled_count_no);
					m_manually_disabled_path_list.Remove(disabled_count_no);
					OP_DELETEA(tmp);
				}

		if (!plug->IsEnabled())
		{
			uni_char *path;
			RETURN_OOM_IF_NULL(path = UniSetNewStr(plug->GetPath()));
			RETURN_IF_ERROR(m_manually_disabled_path_list.Add(path));
		}
	}

	count = m_manually_disabled_path_list.GetCount();

	for (UINT32 i = 0; i < count; i++)
		RETURN_IF_ERROR(pref.AppendFormat(UNI_L("%s%c"), m_manually_disabled_path_list.Get(i), CLASSPATHSEPCHAR));

	// Save updated pref
	RETURN_IF_LEAVE(g_pcapp->WriteStringL(PrefsCollectionApp::DisabledPlugins, pref.GetStorage()));
	RETURN_IF_LEAVE(g_prefsManager->CommitL());

	return OpStatus::OK;
}

OP_STATUS PluginViewers::MakeSurePluginsAreDetected(const OpStringC& path)
{
	if (m_has_detected_plugins)
		return OpStatus::OK;

	return DetectPluginViewers(path);
}

OP_STATUS PluginViewers::AddPluginViewer(PluginViewer* plugin)
{
	if (!plugin)
		return OpStatus::ERR_NULL_POINTER;

	if (FindPluginViewerByPath(plugin->GetPath()) != NULL)
		return OpStatus::ERR; //Already exists!

	plugin->ConnectToViewers();
	OnPluginViewerAdded();
	return m_plugin_list.Add(plugin);
}

void PluginViewers::DeleteAllPlugins()
{
	if (g_viewers)
		g_viewers->RemovePluginViewerReferences();

	m_plugin_list.DeleteAll();
	m_has_detected_plugins = TRUE; //We have explicitly asked for no plugins
}

UINT PluginViewers::GetPluginViewerCount(BOOL enabled_only) const
{
	if (!enabled_only)
		return m_plugin_list.GetCount();

	UINT32 enabled_count = 0;
	UINT32 count = m_plugin_list.GetCount();
	for (UINT32 i = 0; i < count; i++)
	{
		if (m_plugin_list.Get(i)->IsEnabled())
			enabled_count++;
	}

	return enabled_count;
}

#endif // _PLUGIN_SUPPORT_
