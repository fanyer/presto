/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPVALUE_H
#define XPVALUE_H

#include "modules/xpath/src/xpdefs.h"

class XPath_Node;
class XPath_NodeSet;
class XPath_NodeList;
class XPath_Context;

class XPath_Value
{
private:
  unsigned refcount;

public:
  XPath_ValueType type;

  union
  {
    XPath_Node *node;
    XPath_NodeSet *nodeset;
    XPath_NodeList *nodelist;
    bool boolean;
    double number;
    uni_char *string;
    XPath_Value *nextfree;
  } data;

  static XPath_Value *NewL (XPath_Context *context);
  static void Free (XPath_Context *context, XPath_Value *value);

  XPath_Value ();
  ~XPath_Value ();

  void Reset (XPath_Context *context);
  void SetFree (XPath_Context *context, XPath_Value *nextfree);

  BOOL AsBoolean ();
  double AsNumberL ();
  const uni_char *AsStringL (TempBuffer &buffer);

  static double AsNumber (const uni_char *string);
  static double AsNumberL (const uni_char *string, unsigned string_length);
  static const uni_char *AsString (BOOL boolean);
  static const uni_char *AsStringL (double number, TempBuffer &buffer);

  XPath_Value *ConvertToBooleanL (XPath_Context *context);
  XPath_Value *ConvertToNumberL (XPath_Context *context);
  XPath_Value *ConvertToStringL (XPath_Context *context);

  BOOL IsValid () { return type == XP_VALUE_INVALID; }

  static XPath_Value *MakeL (XPath_Context *context, XPath_ValueType type);
  static XPath_Value *MakeBooleanL (XPath_Context *context, BOOL value);
  static XPath_Value *MakeNumberL (XPath_Context *context, double number);
  static XPath_Value *MakeStringL (XPath_Context *context, const uni_char *string = 0, unsigned length = ~0u);

  static XPath_Value *IncRef (XPath_Value *value);
  static void DecRef (XPath_Context *context, XPath_Value *value);

  static double Zero ();
};

class XPath_ValueAnchor
{
protected:
  XPath_Context *context;
  XPath_Value *value;

public:
  XPath_ValueAnchor (XPath_Context *context, XPath_Value *value);
  ~XPath_ValueAnchor ();
};

#define XP_ANCHOR_VALUE(context, name) XPath_ValueAnchor name##anchor(context, name);

#endif // XPVALUE_H
