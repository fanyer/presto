/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * karie@opera.com
 *
 * (extracted from ComposeDesktopWindow)
 *
 */

#ifndef OP_COMPOSE_EDIT_H
#define OP_COMPOSE_EDIT_H

#include "modules/widgets/OpEdit.h"

class OpMultiline;

/***********************************************************************************
 **
 **   OpComposeEdit
 **
 ***********************************************************************************/

class OpComposeEdit : public OpEdit
{
 public:

  /*static OP_STATUS		Construct(ComposeEdit** obj, const uni_char* menu)
    {
    if (!(*obj = new ComposeEdit(menu)))
    return OpStatus::ERR_NO_MEMORY;

    return OpStatus::OK;
    }
  */

  OpComposeEdit(const uni_char* menu) : m_dropdown_menu(menu) {}
  OpComposeEdit() : m_dropdown_menu(NULL) {}
  virtual ~OpComposeEdit() {}

  static OP_STATUS Construct(OpComposeEdit** obj);
  static OP_STATUS Construct(OpComposeEdit** obj, const uni_char* menu);
  void SetDropdown(const uni_char* menu);

  virtual Type GetType() {return WIDGET_TYPE_COMPOSE_EDIT;}
  virtual BOOL OnInputAction(OpInputAction* action);
  virtual void OnDragDrop(OpDragObject* drag_object, const OpPoint& point);

 private:
  const uni_char* m_dropdown_menu;
};

#endif // OP_COMPOSE_EDIT_H
