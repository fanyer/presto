/* -*- Mode: c; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 *
 */

#ifndef QUICK_WIDGET_DECLS_H
#define QUICK_WIDGET_DECLS_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"

/** @brief This is a convenience file that includes forward declarations for all QuickWidgets
  * To get a definition, you still need to include a specific header file
  */

// Wrapped widgets - include corresponding OpWidget header to use
typedef QuickOpWidgetWrapper<class OpBookmarkView>				QuickBookmarkView;
typedef QuickOpWidgetWrapper<class OpFindTextBar>				QuickFindTextBar;
typedef QuickOpWidgetWrapper<class OpGroup>						QuickGroup;
typedef QuickOpWidgetWrapper<class OpHelpTooltip>				QuickHelpTooltip;
typedef QuickOpWidgetWrapper<class OpHistoryView>				QuickHistoryView;
typedef QuickOpWidgetWrapper<class OpLinksView>					QuickLinksView;
typedef QuickOpWidgetWrapper<class OpListBox>					QuickListBox;
typedef QuickOpWidgetWrapper<class OpPersonalbar>				QuickPersonalbar;
typedef QuickOpWidgetWrapper<class OpProgressBar>				QuickProgressBar;
typedef QuickOpWidgetWrapper<class OpProgressField>				QuickProgressField;
typedef QuickOpWidgetWrapper<class OpQuickFind>					QuickQuickFind;
typedef QuickOpWidgetWrapper<class OpScrollbar>					QuickScrollbar;
typedef QuickOpWidgetWrapper<class OpSearchDropDown>			QuickSearchDropDown;
typedef QuickOpWidgetWrapper<class OpSlider>					QuickSlider;
typedef QuickOpWidgetWrapper<class OpSplitter>					QuickSplitter;
typedef QuickOpWidgetWrapper<class OpStatusField>				QuickStatusField;
typedef QuickOpWidgetWrapper<class DesktopToggleButton>			QuickToggleButton;
typedef QuickOpWidgetWrapper<class OpToolbar>					QuickToolbar;
typedef QuickOpWidgetWrapper<class OpTrustAndSecurityButton>	QuickTrustAndSecurityButton;
typedef QuickOpWidgetWrapper<class OpWebImageWidget>			QuickWebImageWidget;
typedef QuickOpWidgetWrapper<class OpWindowMover>				QuickWindowMover;
typedef QuickOpWidgetWrapper<class OpZoomDropDown>				QuickZoomDropDown;
typedef QuickOpWidgetWrapper<class OpZoomMenuButton>			QuickZoomMenuButton;
typedef QuickOpWidgetWrapper<class OpZoomSlider>				QuickZoomSlider;
typedef QuickOpWidgetWrapper<class RichTextEditor>				QuickRichTextEditor;

// Other widgets - include corresponding QuickWidget header to use
class QuickAddressDropDown;
class QuickBrowserView;
class QuickButton;
class QuickButtonStrip;
class QuickCentered;
class QuickCheckBox;
class QuickComposite;
class QuickDesktopFileChooserEdit;
class QuickDialog;
class QuickDropDown;
class QuickDynamicGrid;
class QuickEdit;
class QuickExpand;
class QuickGrid;
class QuickGroupBox;
class QuickIcon;
class QuickImage;
class QuickLabel;
class QuickMultilineEdit;
class QuickMultilineLabel;
class QuickNumberEdit;
class QuickPagingLayout;
class QuickRadioButton;
class QuickRichTextLabel;
class QuickScrollContainer;
class QuickSearchEdit;
class QuickSelectable;
class QuickSeparator;
class QuickSkinElement;
class QuickStackLayout;
class QuickTabs;
class QuickTextWidget;
class QuickTreeView;
class QuickWidget;
class QuickWindow;
class QuickLayoutBase;
class QuickWrapLayout;

#endif // QUICK_WIDGET_DECLS_H
