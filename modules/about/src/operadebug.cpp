/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ABOUT_OPERA_DEBUG

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/scope/scope_connection_manager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/about/operadebug.h"


OP_STATUS OperaDebug::GenerateData()
{
	OpString line, template_string;
	OpString c_text, f_text;
	OpString connect_text, disconnect_text, ip_text, port_text, manual_debug_title;
	OpString warning_first, warning_second;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	OpString local_debuggers_title, no_local_debuggers_found;
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	int port;
	OpStringC ip;

#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_OPERA_DEBUG_TITLE, PrefsCollectionFiles::StyleDebugFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_OPERA_DEBUG_TITLE));
#endif

	// Fetch localized strings
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEBUG_CONNECTED, c_text));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEBUG_WARNING_1, warning_first));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEBUG_DISCONNECT, disconnect_text));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEBUG_IP_ADDRESS, ip_text));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEBUG_PORT_NUMBER, port_text));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEBUG_WARNING_2, warning_second));
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_FOUND_LOCAL_DEBUGGERS_TITLE, local_debuggers_title));
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MANUAL_ENTER_DEBUGGER_TITLE, manual_debug_title));

	// Escape localized strings used in JavaScript
	RETURN_IF_ERROR(JSSafeLocaleString(&f_text, Str::S_DEBUG_FAILED));
	RETURN_IF_ERROR(JSSafeLocaleString(&connect_text, Str::S_DEBUG_CONNECT));
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(JSSafeLocaleString(&no_local_debuggers_found, Str::S_NO_LOCAL_DEBUGGERS_FOUND));
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

	ip = g_pctools->GetStringPref(PrefsCollectionTools::ProxyHost);
	port = g_pctools->GetIntegerPref(PrefsCollectionTools::ProxyPort);

	RETURN_IF_ERROR(line.Append("<script type=\"text/javascript\">\n"
								"var previousDevices = [];\n"));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("var lastConnectedAddress=\"%s\";\n"), ip.CStr()));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("var lastConnectedPort=%d;\n"), port));
	RETURN_IF_ERROR(line.Append("var lastConnectElement;\n"
								"window.onload = function ()\n"
								"{\n"
								"   opera.setConnectStatusCallback(connectionCallback);\n"
								"   if (opera.isConnected())\n"
								"      setConnected();\n"
								"   else\n"
								"      setDisconnected();\n"));
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(line.Append("   var devicelist=opera.nearbyDragonflies;\n"
								"   opera.setDevicelistChangedCallback(discoverDragonflies, devicelist);\n"
								"   discoverDragonflies(devicelist); // make sure we update once, since the callback above only notifies changes\n"));
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(line.Append("}\n"
								"function setLastConnectionDetails(ipaddress, port, element)\n"
								"{\n"
								"   lastConnectedAddress = ipaddress;\n"
								"   lastConnectedPort = port;\n"
								"   lastConnectElement = element;\n"
								"}\n"
								"function setupAndConnectFromForm(triggerElement)\n"
								"{\n"
								"   var ip = document.getElementById('ip');\n"
								"   var port = document.getElementById('port');\n"
								"   doConnect(ip.value, port.value, document.getElementById('form_connect_button'));\n"
								"}\n"));
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(line.Append("function updateListviewNoneFound(current_items)\n"
								"{\n"
								"   var deviceUl = document.getElementById('detected_dragonflies');\n"
								"   if(!deviceUl)\n"
								"      return;\n"
								"   var retelem = document.getElementById('none_found');\n"
								"   if(current_items == 0 && !retelem)\n"
								"   {\n"
								"      li = deviceUl.appendChild(document.createElement('li'));\n"
								"      li.textContent = '"));
	RETURN_IF_ERROR(line.Append(no_local_debuggers_found.CStr()));
	RETURN_IF_ERROR(line.Append("';\n"
								"      li.offsetWidth; // trigger a reflow \n"
								"      li.className = 'show';\n"
								"      li.id = 'none_found';\n"
								"   }\n"
								"   else if(current_items != 0 && retelem)\n"
								"   {\n"
								"      deviceUl.removeChild(retelem); // remove the none_found item without transition\n"
								"   }\n"
								"}\n"
								"function eventFunctionRemoveElement(event)\n"
								"{\n"
								"   var deviceUl = document.getElementById('detected_dragonflies');\n"
								"   deviceUl.removeChild(event.target);\n"
								"// check if we don't have any devices visible and show the 'not found text'\n"
								"   updateListviewNoneFound(previousDevices.length);\n"
								"}\n"
								"// will copy the url entries from the device collection\n"
								"function copyDeviceList(obj)\n"
								"{\n"
								"   var newObj = new Array(obj.length);\n"
								"   for (var i=0; i<obj.length; i++)\n"
								"   {\n"
								"      newObj[i] = obj[i].url;\n"
								"   }\n"
								"   return newObj;\n"
								"}\n"
								"function removeDeviceElement(index)\n"
								"{\n"
								"   var deviceUl = document.getElementById('detected_dragonflies');\n"
								"   if(deviceUl)\n"
								"   {\n"
								"      var elemToRemove = deviceUl.childNodes[index];\n"
								"      if(previousDevices.length == 1)\n"
										  // remove the element without transition to avoid jumping from two to one list items
								"         deviceUl.removeChild(elemToRemove);\n"
								"      else\n"
								"      {\n"
								"         elemToRemove.addEventListener('transitionEnd', eventFunctionRemoveElement, false);\n"
								"         elemToRemove.className = 'hide';\n"
								"      }\n"
								"   }\n"
								"}\n"
								"function updateListElement(li, button, urlstring, iconstring)\n"
								"{\n"
								"   li.style.listStyleImage = \"url('\"+ iconstring + \"')\";\n"
								"   var targetip = urlstring.substring(urlstring.lastIndexOf('/')+1, urlstring.lastIndexOf(':'));\n"
								"   var targetport = urlstring.substring(urlstring.lastIndexOf(':')+1);\n"
								"   var iplabel = li.querySelector('.iplabel');\n"
								"   var portlabel = li.querySelector('.portlabel');\n"
								"   iplabel.textContent = targetip;\n"
								"   portlabel.textContent = targetport;\n"
								"   button.onclick = doConnect.bind(null, targetip, targetport, button);\n"
								"}\n"
								"function updateDeviceElement(index, devices)\n"
								"{\n"
								"   var deviceUl = document.getElementById('detected_dragonflies');\n"
								"   if(!deviceUl)\n"
								"      return;\n"
								"   var device=devices[index];\n"
								"   var elemToChange = deviceUl.childNodes[index];\n"
								"   var button = elemToChange.querySelector('.push_button');\n"
								"   // The expected form of .upnpDevicePayload is the \"http://[ip-address]:[port]\" portion of an URL\n"
								"   updateListElement(elemToChange, button, device.upnpDevicePayload, device.upnpDeviceIcon);\n"
								"}\n"
								"function showDeviceElement(index, devices)\n"
								"{\n"
								"   var deviceUl = document.getElementById('detected_dragonflies');\n"
								"   if(!deviceUl)\n"
								"      return;\n"
								"   var device=devices[index];\n"
								"   li = deviceUl.appendChild(document.createElement('li'));\n"
								"   li.offsetWidth;\n"
								"   li.className = 'show';\n"
								"   dl = li.appendChild(document.createElement('dl'));\n"
								"   iptext = dl.appendChild(document.createElement('dd'));\n"
								"   iptext.className = 'iplabel';\n"
								"   porttext = dl.appendChild(document.createElement('dd'));\n"
								"   porttext.className = 'portlabel';\n"
								"   statuslabel = li.appendChild(document.createElement('p'));\n"
								"   statuslabel.className = 'status_label';\n"
								"   button = li.appendChild(document.createElement('button'));\n"
								"   button.textContent = '"));
	RETURN_IF_ERROR(line.Append(connect_text.CStr()));
	RETURN_IF_ERROR(line.Append("';\n"
								"   button.className = 'push_button';\n"
								"   updateListElement(li, button, device.upnpDevicePayload, device.upnpDeviceIcon);\n"
								"}\n"
								"// set up list of nearby dragonflies\n"
								"function discoverDragonflies(devices)\n"
								"{\n"
								"   var biggest = previousDevices.length > devices.length ? previousDevices.length : devices.length;\n"
								"   for(var i=0; i<biggest; i++)\n"
								"   {\n"
								"      if((previousDevices[i] && devices[i]) && (previousDevices[i] != devices[i].url))\n"
								"      {\n"
								"         updateDeviceElement(i, devices);\n"
								"      }\n"
								"      else if(!devices[i])	// device was removed\n"
								"      {\n"
								"         removeDeviceElement(i);\n"
								"      }\n"
								"      else if(!previousDevices[i])	// device was added\n"
								"      {\n"
								"         showDeviceElement(i, devices);\n"
								"      }\n"
								"   }\n"
								"   previousDevices = copyDeviceList(devices);\n"
								"   updateListviewNoneFound(previousDevices.length);\n"
								"}\n"));
#endif // UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY
	RETURN_IF_ERROR(line.Append("function setConnectionFailed()\n"
								"{\n"
								"   var parentElement = lastConnectElement.parentNode;\n"
								"   var statuslabel = parentElement.querySelector('.status_label');\n"
								"   statuslabel.textContent = '"));
	RETURN_IF_ERROR(line.Append(f_text.CStr()));
	RETURN_IF_ERROR(line.Append("';\n"
								"}\n"
								"function setDisconnected()\n"
								"{\n"
								"   document.getElementById('setup_state').style.display='block';\n"
								"   document.getElementById('connected_state').style.display='none';\n"
								"}\n"
								"function setConnected()\n"
								"{\n"
								"   document.getElementById('setup_state').style.display='none';\n"
								"   document.getElementById('connected_state').style.display='block';\n"
								"   var connectedto = document.getElementById('connected_to');\n"
								"   connectedto.querySelector('.iplabel').textContent = lastConnectedAddress;\n"
								"   connectedto.querySelector('.portlabel').textContent = lastConnectedPort;\n"
								"}\n"
								"function connectionCallback(status)\n"
								"{\n"));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("   if (status == %u)\n"), OpScopeConnectionManager::CONNECTION_OK));
	RETURN_IF_ERROR(line.Append("      setConnected();\n"));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("   else if (status == %u)\n"), OpScopeConnectionManager::DISCONNECTION_OK));
	RETURN_IF_ERROR(line.Append("      setDisconnected();\n"));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L(
								"   else if (status == %u)\n"), OpScopeConnectionManager::CONNECTION_FAILED));
	RETURN_IF_ERROR(line.Append("   {\n"
								"      setDisconnected();\n"
								"      setConnectionFailed();\n"
								"   }\n"
								"}\n"
								"function doConnect(ip, port, triggerElement)\n"
								"{\n"
								"   setLastConnectionDetails(ip, port, triggerElement);\n"
								"   if (port > 0 && port < 65536)\n"
								"      opera.connect(ip, port);\n"
								"   else\n"
								"      setConnectionFailed();\n"
								"}\n"
								"function doDisconnect()\n"
								"{\n"
								"   if (opera.isConnected())\n"
								"      opera.disconnect();\n"
								"}\n"
								"</script>\n"
								));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	line.Empty();

	RETURN_IF_ERROR(OpenBody(Str::S_OPERA_DEBUG_TITLE));

	RETURN_IF_ERROR(template_string.Set(
								"    <div id=\"setup_state\">\n"
								"        <p>%s</p>\n"//warning_first
								"        <p><strong>%s</strong></p>\n"//warning_second
								));
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	RETURN_IF_ERROR(template_string.Append(
								"        <h2>%s</h2>\n"//local_debuggers_title
								"        <ul id=\"detected_dragonflies\"><li id=\"none_found\" class=\"show\">%s</li></ul>\n"));//no_local_debuggers_found
#endif // UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY
	RETURN_IF_ERROR(template_string.Append(
								"        <h2>%s</h2>\n"//manual_debug_title
								"        <div class=\"manual\">\n"
								"            <dl>\n"
								"                <dt>%s</dt>\n"//ip_text
								"                <dd>\n"
								"                    <input id=\"ip\" type=\"text\" value=\"%s\">\n"//ip
								"                </dd>\n"
								"            </dl>\n"
								"            <dl>\n"
								"                <dt>%s</dt>\n"//port_text
								"                <dd>\n"
								"                    <input id=\"port\" size=\"6\" type=\"text\" value=\"%d\">\n"//port
								"                </dd>\n"
								"            </dl>\n"
								"            <button id=\"form_connect_button\" onclick=\"setupAndConnectFromForm()\">%s</button>\n"//connect_text
								"            <p class=\"status_label\"></p>\n"
								"        </div>\n"
								"    </div>\n"
								"    <div id=\"connected_state\">\n"
								"        <p><strong>%s</strong></p>\n"//warning_second
								"        <ul id=\"connected_to\" class=\"connected\">\n"
								"            <li class=\"show\">\n"
								"                <dl>\n"
								"                   <dd>%s</dd>\n"//c_text
								"                   <dd class=\"iplabel\"></dd>\n"
								"                   <dd class=\"portlabel\"></dd>\n"
								"                   <dd><button onclick=\"doDisconnect()\">%s</button></dd>\n"//disconnect_text
								"                </dl>\n"
								"            </li>\n"
								"        </ul>\n"
								"    </div>\n"));

	OP_STATUS status = line.AppendFormat(template_string.CStr()
		,warning_first.CStr()
		,warning_second.CStr()
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
		,local_debuggers_title.CStr()
		,no_local_debuggers_found.CStr()
#endif // UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY
		,manual_debug_title.CStr()
		,ip_text.CStr()
		,ip.CStr()
		,port_text.CStr()
		,port
		,connect_text.CStr()
		,warning_second.CStr()
		,c_text.CStr()
		,disconnect_text.CStr()
		);
	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

	return CloseDocument();
}

OperaDebugProxy::OperaDebugProxy(ES_AsyncInterface *ai, ES_Runtime *runtime, DOM_Object *opera_object, ES_Object *callback)
	: m_ai(ai),
	  m_runtime(runtime),
	  m_callback(callback),
	  m_opera_object(opera_object)
{
}

OperaDebugProxy::~OperaDebugProxy()
{
	Out();
	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_CONNECTION_STATUS);
}

void OperaDebugProxy::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef SCOPE_CONNECTION_CONTROL
	OpScopeConnectionManager::ConnectionStatus status = static_cast<enum OpScopeConnectionManager::ConnectionStatus>(par2);
	ES_Value argv[1]; // ARRAY OK 2010-11-29 peter

	argv[0].value.number = status;
	argv[0].type = VALUE_NUMBER;

	m_ai->CallFunction(m_callback, NULL, 1, argv);

#endif // SCOPE_CONNECTION_CONTROL
}

#endif // ABOUT_OPERA_DEBUG
