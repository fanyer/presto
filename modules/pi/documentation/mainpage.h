/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @mainpage Porting Interfaces module (pi)
@section Overview

This module defines a set of interfaces between core and platform code
in Opera. Many interfaces are mandatory, while a few are only required
if certain optional features are enabled. Some interfaces may have a
common implementation that a product may choose to use instead of
implementing their own version. GOGI is one such example, where the
various interfaces for displaying stuff are implemented in a
cross-platform way. See below.

@section all List of interfaces, listeners and utility classes

@subsection required_always Always required (must be implemented on every platform)
@li OpSocketAddress.h (Interfaces: OpSocketAddress)
@li OpSocket.h (Interfaces: OpSocket, Listeners: OpSocketListener)
@li OpSystemInfo.h (Interfaces: OpSystemInfo)
@li OpLocale.h (Interfaces: OpLocale)
@li OpLowLevelFile.h (Interfaces: OpLowLevelFile, Listeners: OpLowLevelFileListener)

@subsection required_if_directory_search_support Must be implemented if FEATURE_DIRECTORY_SEARCH is enabled
@li OpFolderLister.h (Interfaces: OpFolderLister)

@subsection required_unless_internal_host_resolver Must be implemented unless FEATURE_INTERNAL_HOST_RESOLVER is enabled
@li OpHostResolver.h (Interfaces: OpHostResolver, Listeners: OpHostResolverListener)

@subsection required_unless_gogi Must be implemented unless you run the GOGI co-platform
@li OpBitmap.h (Interfaces: OpBitmap)
@li OpFont.h (Interfaces: OpFont, OpFontManager)
@li OpPainter.h (Interfaces: OpPainter)
@li OpScreenInfo.h (Interfaces: OpScreenInfo)
@li OpUiInfo.h (Interfaces: OpUiInfo)
@li OpView.h (Interfaces: OpView, Listeners: OpPaintListener, OpMouseListener, OpDragListener)
@li OpWindow.h (Interfaces: OpWindow, Listeners: OpWindowListener)

@subsection misc Miscellaneous interfaces needed with certain functionality
@li OpAccessibilityExtension.h (Interfaces: OpAccessibilityExtension)
@li OpDragManager.h (Interfaces: OpDragManager) (must be implemented if FEATURE_DRAG is enabled)
@li OpTaskbar.h (Interfaces: OpTaskbar)
@li OpDLL.h (Interfaces: OpDLL) (must be implemented if FEATURE_DLL is enabled)
@li OpInputMethod.h (Listeners: OpInputMethodListener) (must be implemented if FEATURE_IME is enabled)
@li OpPluginDetectionListener.h (Listeners: OpPluginDetectionListener) (must be implemented if API_PI_PLUGIN_DETECT is imported)
@li OpPluginWindow.h (Interfaces: OpPluginWindow, Listeners: OpPluginWindowListener) (must be implemented if FEATURE_PLUGIN or FEATURE_JAVA_OPAPPLET are enabled)
@li OpForms.h (Interfaces: OpFormsObject, Listeners: OpFormsListener)
@li OpThreadTools.h (Interfaces: OpThreadTools)
@li OpTv.h (Interfaces: OpTv)
@li OpPrinterController.h (Interfaces: OpPrinterController) (must be implemented if FEATURE_PRINTING is enabled)
@li OpMediaPlayer.h (Interfaces: OpMediaManager, OpMediaPlayer, Listeners: OpMediaPlayerListener) (must be implemented if FEATURE_MEDIA is enabled)
@li OpClipboard.h (Interfaces: OpClipboard) (must be implemented if FEATURE_CLIPBOARD is enabled)
@li OpCertificate.h (Interfaces: OpCertificate) (may have to be implemented if FEATURE_EXTERNAL_SSL is enabled)
@li OpUdpSocket.h (Interfaces: OpUdpSocket, Listeners: OpUdpSocketListener) (must be implemented if API_PI_UDP_SOCKET is imported)
@li OpNetworkInterface.h (Interfaces: OpNetworkInterface, OpNetworkInterfaceManager, Listeners: OpNetworkInterfaceListener) (must be implemented if API_PI_NETWORK_INTERFACE_MANAGER is imported)
@li OpMemory.h (Interfaces: OpMemory) (various methods importable via various APIs)
@li OpGeolocation.h (Interfaces: OpGeolocation, Listeners: OpGeolocationListener) (support for navigator.geolocation)
@li OpTelephonyNetworkInfo.h (Interfaces: OpTelephonyNetworkInfo)
@li OpSubscriberInfo.h (Interfaces: OpSubscriberInfo)
@li OpCamera.h (Interfaces: OpCamera)
@li OpAddressBook.h (Interfaces: OpAddressBook)
@li OpAddressBookItem.h (Interfaces: OpAddressBookItem)
@li OpAccelerometer.h (Interfaces: OpAccelerometer)
@li OpCalendar.h (Interfaces: OpCalendar)
@li OpMessaging.h (Interfaces: OpMessaging)

*/
