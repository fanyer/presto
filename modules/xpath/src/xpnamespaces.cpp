/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpnamespaces.h"
#include "modules/xpath/src/xplexer.h"
#include "modules/xpath/src/xputils.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

XPath_Namespace::XPath_Namespace ()
  : prefix (0),
    uri (0)
{
}

XPath_Namespace::~XPath_Namespace ()
{
  OP_DELETEA (prefix);
  OP_DELETEA (uri);
}

XPath_Namespaces::XPath_Namespaces ()
  : hashed (this),
    refcount (1)
{
}

XPath_Namespaces::~XPath_Namespaces ()
{
  indexed.DeleteAll ();
  hashed.RemoveAll ();
}

void
XPath_Namespaces::SetL (const uni_char *prefix, const uni_char *uri)
{
  void *data;

  if (hashed.GetData (prefix, &data) == OpStatus::OK)
    {
      XPath_Namespace *ns = (XPath_Namespace *) data;
      LEAVE_IF_ERROR (UniSetStr (ns->uri, uri));
    }
  else
    {
      OpStackAutoPtr<XPath_Namespace> ns (OP_NEW_L (XPath_Namespace, ()));

      LEAVE_IF_ERROR (UniSetStr (ns->prefix, prefix));
      LEAVE_IF_ERROR (UniSetStr (ns->uri, uri));
      LEAVE_IF_ERROR (indexed.Add (ns.get ()));

      XPath_Namespace *ns_ptr = ns.release ();

      hashed.AddL (ns_ptr->prefix, ns_ptr);
    }
}

void
XPath_Namespaces::RemoveL (const uni_char *prefix)
{
  void *data;

  if (OpStatus::IsSuccess (hashed.Remove (prefix, &data)))
    {
      indexed.RemoveByItem ((XPath_Namespace *) data);
      XPath_Namespace *ns = static_cast<XPath_Namespace *> (data);
      OP_DELETE (ns);
    }
  else
    LEAVE (OpStatus::ERR);
}

void
XPath_Namespaces::SetURIL (unsigned index, const uni_char *uri)
{
  LEAVE_IF_ERROR (UniSetStr (indexed.Get (index)->uri, uri));
}

#if 0
void
XPath_Namespaces::CollectPrefixesFromElementL (XPath_Element *element)
{
  for (unsigned index = 0, count = XPath_Utils::GetNamespaceCount (element); index < count; ++index)
    {
      uni_char *prefix = 0, *uri = 0;

      XPath_Utils::GetNamespacePrefixAndURIL (element, index, prefix, uri);

      ANCHOR_ARRAY (uni_char, prefix);
      ANCHOR_ARRAY (uni_char, uri);

      SetL (prefix, uri);
    }
}
#endif // 0

void
XPath_Namespaces::CollectPrefixesFromExpressionL (const uni_char *expression)
{
  XPath_Lexer lexer (expression);
  XPath_Token token;
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  while (token != XP_TOKEN_END)
    {
      token = lexer.GetNextTokenL ();

      if (token == XP_TOKEN_NAMETEST || token == XP_TOKEN_VARIABLEREFERENCE || token == XP_TOKEN_FUNCTIONNAME)
        {
          XMLCompleteNameN completename (token.value, token.length);

          if (completename.GetPrefix ())
            {
              buffer.AppendL (completename.GetPrefix (), completename.GetPrefixLength ());
              SetL (buffer.GetStorage (), 0);
              buffer.Clear ();
            }
        }

      lexer.ConsumeToken ();
    }
}

unsigned
XPath_Namespaces::GetCount ()
{
  return indexed.GetCount ();
}

const uni_char *
XPath_Namespaces::GetPrefix (unsigned index)
{
  return indexed.Get (index)->prefix;
}

const uni_char *
XPath_Namespaces::GetURI (unsigned index)
{
  return indexed.Get (index)->uri;
}

const uni_char *
XPath_Namespaces::GetURI (const uni_char *prefix)
{
  void *data;

  if (uni_str_eq (prefix, "xml"))
    return UNI_L ("http://www.w3.org/XML/1998/namespace");
  else if (uni_str_eq (prefix, "xmlns"))
    return UNI_L ("http://www.w3.org/2000/xmlns/");
  else if (hashed.GetData (prefix, &data) == OpStatus::OK)
    return ((XPath_Namespace *) data)->uri;
  else
    return 0;
}

/* virtual */ UINT32
XPath_Namespaces::Hash (const void *key)
{
  return XPath_Utils::HashString ((const uni_char *) key);
}

/* virtual */ BOOL
XPath_Namespaces::KeysAreEqual (const void *key1, const void *key2)
{
  return uni_strcmp ((const uni_char *) key1, (const uni_char *) key2) == 0;
}

/* static */ XPath_Namespaces *
XPath_Namespaces::IncRef (XPath_Namespaces *namespaces)
{
  if (namespaces)
    ++namespaces->refcount;
  return namespaces;
}

/* static */ void
XPath_Namespaces::DecRef (XPath_Namespaces *namespaces)
{
  if (namespaces && --namespaces->refcount == 0)
    OP_DELETE (namespaces);
}

#endif // XPATH_SUPPORT
