/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef GADGETS_CAPABILITIES_H
#define GADGETS_CAPABILITIES_H

#define GADGETS_CAP_OPSTRING  //  We now use OpString in a number of places where we used to use uni_char arrays.

#define GADGETS_CAP_SPLIT_OPENURL  //  We split OpenURL into two functions:  one taking a string and one taking a URL.

#define GADGETS_CAP_HAS_CREATE_CLASS_WITH_PATH  //  Indicates that the OpGadgetManager har a CreateClassWithPath-method.

#define GADGETS_CAP_WIDGET_PROTOCOL  //  Widget protocol

#define GADGETS_CAP_WIDGET_PROTOCOL_EXT  //  Widget protocol extension

#define GADGETS_CAP_MODE  //  Modes for Media Queries Extensions.

#define GADGETS_CAP_GADGETS_ENTERS_SEC_ELEMENT // Security manager will not enter/leave the security xml elm

#define GADGETS_CAP_EXTENDED_GET_URL_API // OpGadget::GetGadgetUrl may return URL with or without index file

#define GADGETS_CAP_UNLOAD_CLOSE // Gadget window close operation that triggers unload event

#define GADGETS_CAP_OPERA_WIDGETS // API extension for opera:widgets

#define GADGETS_CAP_RESOURCE_CHECK // API to check if a URI resource is in a gadget file.

#define GADGETS_CAP_MULTIPLE_ICONS // Gadgets can have multiple icons

#define GADGETS_CAP_HAS_RELOAD // OpGadget class has reload function

#define GADGET_CAP_HAS_UPGRADE // OpGadgetManager has UpgradeGadget function

#define GADGETS_CAP_DECLARES_WINDOW // The gadget module forward-declares Window when it needs to

#define GADGETS_CAP_HAS_HASINTRANETACCESS // OpGadget class has HasIntranetAccess() function

#define GADGETS_CAP_HAS_ISROOTSERVICE // OpGadget class has IsRootService() function

#define GADGETS_CAP_BADGE //Widget-to-host communication with badge string is supported

#define GADGETS_CAP_HAS_GETAPPLICATIONPATH // OpPersistantStorageListener has GetApplicationPath() function

#define GADGETS_CAP_HAS_UPDATE // OpGadget has Update function used by widgets to initiate auto update functions

#define GADGETS_CAP_SIGNED // OpGadget has isSigned property used to know if a widget is signed or not

#define GADGETS_CAP_HAS_SETGLOBALDATA // OpGadget/OpGadgetManager has SetGlobalData and GetGlobalData functions

#define GADGETS_CAP_HAS_HASJSPLUGINSACCESS // OpGadget class has HasJSPluginsAccess() function

#define GADGETS_CAP_HAS_ALLOWEDWIDGETEXTENSION // OpGadgetManager has AddAllowedWidgetExtension() and IsAllowedExtension() functions

#define GADGETS_CAP_W3C_EXTENSION // The gadget module has the new functions needed to support the W3C widget spec.

#define GADGETS_CAP_DOMWIDGETMANAGER_INTERFACES // The gadgets module has functions needed to implement the widgetManager interface in DOM

#define GADGETS_CAP_HAS_OPENGADGET	// GadgetManager has the new OpenGadget function (Moving core code out the the platform layer)

#define GADGETS_CAP_HAS_SETOPGADET // GadgetWindow has SetOpGadget

#define GADGETS_CAP_HAS_SHAREDFOLDER // OpGadget has Get/SetSharedFolder

#define GADGETS_CAP_HAS_SETGADGETNAME // OpGadget has SetGadgetName

#define GADGETS_CAP_HAS_FEATURES // OpGadget has GetFeature/GetFeatures and FeatureCount

#define GADGETS_CAP_FIND_GADGET_FLAG // FindGadget() uses GADGETFindState flag instead of BOOL

#define GADGETS_CAP_NUMGADGETCLASSES ///< OpGadgetManager::NumGadgetClasses() and OpGadgetManager::GetGadgetClass() available. (2009-07)

#define GADGETS_CAP_HAS_FIND_BY_SERVICENAME ///< GADGETFindType has GADGET_FIND_BY_SERVICE_NAME

#endif // !GADGETS_CAPABILITIES_H
