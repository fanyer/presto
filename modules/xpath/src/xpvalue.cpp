/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpnodelist.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpcontext.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/stdlib/include/double_format.h"

/* static */ XPath_Value *
XPath_Value::NewL (XPath_Context *context)
{
  return context->global->values_cache.GetL ();
}

/* static */ void
XPath_Value::Free (XPath_Context *context, XPath_Value *value)
{
  context->global->values_cache.Free (context, value);
}

XPath_Value::XPath_Value ()
  : refcount (0),
    type (XP_VALUE_INVALID)
{
}

XPath_Value::~XPath_Value ()
{
  /* Need context to reset, so need manual reset before destruction. */
  OP_ASSERT (type == XP_VALUE_INVALID);
}

void
XPath_Value::Reset (XPath_Context *context)
{
  if (type == XP_VALUE_NODE && data.node)
    XPath_Node::DecRef (context, data.node);
  else if (type == XP_VALUE_NODESET)
    {
      data.nodeset->Clear (context);
      OP_DELETE (data.nodeset);
    }
  else if (type == XP_VALUE_NODELIST)
    {
      data.nodelist->Clear (context);
      OP_DELETE (data.nodelist);
    }
  else if (type == XP_VALUE_STRING)
    OP_DELETEA (data.string);

  type = XP_VALUE_INVALID;
}

void
XPath_Value::SetFree (XPath_Context *context, XPath_Value *nextfree)
{
  Reset (context);

  type = XP_VALUE_INVALID;
  data.nextfree = nextfree;
}

BOOL
XPath_Value::AsBoolean ()
{
  switch (type)
    {
    case XP_VALUE_NODE:
      return data.node != 0;

    case XP_VALUE_NODESET:
      return data.nodeset->GetCount () != 0;

    case XP_VALUE_BOOLEAN:
      return data.boolean;

    case XP_VALUE_NUMBER:
      return !(data.number == 0. || op_isnan (data.number));

    case XP_VALUE_STRING:
      return *data.string != 0;

    default:
      return FALSE;
    }
}

double
XPath_Value::AsNumberL ()
{
  switch (type)
    {
    case XP_VALUE_BOOLEAN:
      return data.boolean ? 1. : 0.;

    case XP_VALUE_NUMBER:
      return data.number;

    case XP_VALUE_STRING:
      return AsNumber (data.string);
    }

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  return AsNumber (AsStringL (buffer));
}

const uni_char *
XPath_Value::AsStringL (TempBuffer &buffer)
{
  switch (type)
    {
    case XP_VALUE_NUMBER:
      return AsStringL (data.number, buffer);

    case XP_VALUE_BOOLEAN:
      return AsString (data.boolean);

    case XP_VALUE_STRING:
      return data.string;

    default:
      if (type == XP_VALUE_NODE && data.node == 0 || type == XP_VALUE_NODESET && data.nodeset->GetCount () == 0)
        return UNI_L ("");
      else
        {
          XPath_Node *node;

          if (type == XP_VALUE_NODESET)
            node = data.nodeset->Get (0);
          else
            node = data.node;

          node->GetStringValueL (buffer);
          return buffer.GetStorage () ? buffer.GetStorage () : UNI_L ("");
        }
    }
}

/* static */ double
XPath_Value::AsNumber (const uni_char *string)
{
  if (string)
    {
      while (XMLUtils::IsSpace (*string))
        ++string;
      if (*string)
        {
          uni_char *endptr;
          double result = uni_strtod (string, &endptr);
          while (XMLUtils::IsSpace (*endptr))
            ++endptr;
          if (!*endptr)
            return result;
        }
    }
  return op_nan (0);
}

/* static */ double
XPath_Value::AsNumberL (const uni_char *string, unsigned string_length)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  buffer.AppendL (string, string_length);

  uni_char *endptr;
  return uni_strtod (buffer.GetStorage (), &endptr);
}

/* static */ const uni_char *
XPath_Value::AsString (BOOL boolean)
{
  return boolean ? UNI_L ("true") : UNI_L ("false");
}

/* static */ const uni_char *
XPath_Value::AsStringL (double number, TempBuffer &buffer)
{
  char *storage8;

  buffer.ExpandL (33);
  buffer.SetCachedLengthPolicy (TempBuffer::UNTRUSTED);
  storage8 = reinterpret_cast<char *> (buffer.GetStorage ());

  if (op_isnan (number))
    return UNI_L ("NaN");
  else if (number == 0)
    return UNI_L ("0");
  else
    {
      if (!OpDoubleFormat::ToString (storage8, number))
        LEAVE (OpStatus::ERR_NO_MEMORY);

      char *e = 0, *p = 0;
      for (unsigned index = 0; storage8[index]; ++index)
        if (storage8[index] == '.')
          p = &storage8[index];
        else if (storage8[index] == 'e' || storage8[index] == 'E')
          e = &storage8[index];

      if (e)
        {
          TempBuffer b; ANCHOR (TempBuffer, b);

          if (number < 0)
            b.AppendL ("-");

          int exp = op_atoi (e + 1), index;
          *e = 0;
          if (exp > 0)
            {
              if (storage8[0] != '0')
                b.AppendL (storage8[0]);
              if (p)
                {
                  b.AppendL (p + 1, MIN (e - (p + 1), exp));
                  if (exp < e - (p + 1))
                    {
                      b.AppendL (".");
                      b.AppendL (p + 1 + exp, e - (p + 1 + exp));
                    }
                  else if (e - (p + 1) < exp)
                    for (index = exp; index < e - (p + 1); ++index)
                      b.AppendL ("0");
                }
              else
                for (index = 0; index < exp; ++index)
                  b.AppendL ("0");
            }
          else if (exp < 0)
            {
              b.AppendL ("0.");
              for (index = 1; index < -exp; ++index)
                b.AppendL ("0");
              for (index = 0; storage8[index]; ++index)
                if (op_isdigit (storage8[index]) && storage8[index] != '0')
                  break;
              for (; storage8[index]; ++index)
                if (op_isdigit (storage8[index]))
                  b.AppendL (storage8[index]);
            }

          buffer.Clear ();
          buffer.AppendL (b.GetStorage ());
        }
      else
        make_doublebyte_in_place (buffer.GetStorage (), op_strlen (storage8));
    }

  return buffer.GetStorage ();
}

XPath_Value *
XPath_Value::ConvertToBooleanL (XPath_Context *context)
{
  return MakeBooleanL (context, AsBoolean ());
}

XPath_Value *
XPath_Value::ConvertToNumberL (XPath_Context *context)
{
  return MakeNumberL (context, AsNumberL ());
}

XPath_Value *
XPath_Value::ConvertToStringL (XPath_Context *context)
{
  if (type == XP_VALUE_STRING)
    return IncRef (this);
  else
    {
      TempBuffer buffer; ANCHOR (TempBuffer, buffer);
      return MakeStringL (context, AsStringL (buffer));
    }
}

/* static */ XPath_Value *
XPath_Value::MakeL (XPath_Context *context, XPath_ValueType type)
{
  XPath_Value *value = XPath_Value::NewL (context);
  value->type = type;
  value->refcount = 1;
  return value;
}

/* static */ XPath_Value *
XPath_Value::MakeBooleanL (XPath_Context *context, BOOL boolean)
{
  XPath_Value *value = XPath_Value::NewL (context);
  value->type = XP_VALUE_BOOLEAN;
  value->refcount = 1;
  value->data.boolean = !!boolean;
  return value;
}

/* static */ XPath_Value *
XPath_Value::MakeNumberL (XPath_Context *context, double number)
{
  XPath_Value *value = XPath_Value::NewL (context);
  value->type = XP_VALUE_NUMBER;
  value->refcount = 1;
  value->data.number = number;
  return value;
}

/* static */ XPath_Value *
XPath_Value::MakeStringL (XPath_Context *context, const uni_char *string, unsigned length)
{
  if (!string)
    string = UNI_L ("");

  if (length == ~0u)
    length = uni_strlen (string);

  XPath_Value *value = XPath_Value::NewL (context);
  value->refcount = 1;
  XP_ANCHOR_VALUE (context, value);
  value->data.string = 0;
  value->data.string = XPath_Utils::CopyStringL (string, length);
  value->type = XP_VALUE_STRING;

  return IncRef (value);
}

/* static */ XPath_Value *
XPath_Value::IncRef (XPath_Value *value)
{
  if (value)
    ++value->refcount;

  return value;
}

/* static */ void
XPath_Value::DecRef (XPath_Context *context, XPath_Value *value)
{
  if (value && !--value->refcount)
    Free (context, value);
}

/* static */ double
XPath_Value::Zero ()
{
  return 0.;
}

XPath_ValueAnchor::XPath_ValueAnchor (XPath_Context *context, XPath_Value *value)
  : context (context),
    value (value)
{
}

XPath_ValueAnchor::~XPath_ValueAnchor ()
{
  XPath_Value::DecRef (context, value);
}

#endif // XPATH_SUPPORT
