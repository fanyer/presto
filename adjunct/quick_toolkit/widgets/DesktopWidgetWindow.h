/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author karie, adamm
 */

#ifndef DESKTOP_WIDGET_WINDOW_H
#define DESKTOP_WIDGET_WINDOW_H

#include "modules/widgets/WidgetWindow.h"

/** @brief 
/*/
class DesktopWidgetWindow : public WidgetWindow
{

public:
  DesktopWidgetWindow() : m_id(GetUniqueID()) {}

  OP_STATUS Init(OpWindow::Style style, 
				 const OpStringC8& name,
				 OpWindow* parent_window = NULL, 
				 OpView* parent_view = NULL, 
				 UINT32 effects = 0, 
				 void* native_handle = NULL);

  virtual const char*         GetWindowName() { return m_window_name.CStr(); }

  virtual const uni_char* GetTitle() { 
    if (GetWindow()) 
      return GetWindow()->GetTitle(); 
    else return UNI_L(""); 
  }

  OpWidget* GetWidgetByName(const OpStringC8& name);
  OpWidget* GetWidgetByText(const OpStringC8& text);

  // Is an OpTypedObject cause WidgetWindow subclasses OpInputContext
  virtual Type				GetType() { return WIDGET_TYPE_WIDGETWINDOW; }
  virtual INT32				GetID()   { return m_id; }
  
  // == OpScopeDesktopWindowManager =======
  virtual OP_STATUS ListWidgets(OpVector<OpWidget> &widgets);
  OP_STATUS ListWidgets(OpWidget* widget, OpVector<OpWidget> &widgets);


 private:
  INT32      m_id;
  OpString8  m_window_name;

};

#endif // DESKTOP_WIDGET_WINDOW_H
