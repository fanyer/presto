/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpstringset.h"

XPath_StringSet::~XPath_StringSet ()
{
  Clear ();
}

BOOL
XPath_StringSet::AddL (const uni_char *string)
{
  uni_char *copy;

  if (string && *string)
    {
      unsigned count = uni_strlen (string) + 1;
      copy = OP_NEWA_L (uni_char, count);
      uni_strcpy (copy, string);
    }
  else
    copy = const_cast<uni_char *> (UNI_L (""));

  OP_STATUS status = OpGenericStringHashTable::Add (copy, copy);

  if (status == OpStatus::OK)
    return TRUE;
  else
    {
      if (*copy)
        OP_DELETEA (copy);

      if (status != OpStatus::ERR)
        LEAVE (status);

      return FALSE;
    }
}

BOOL
XPath_StringSet::Contains (const uni_char *string)
{
  return OpGenericStringHashTable::Contains (string ? string : UNI_L (""));
}

void
XPath_StringSet_DeleteString (const void *key, void *data)
{
  /* Empty strings are stored unallocated, so we shouldn't delete them. */
  uni_char *string = static_cast<uni_char *> (data);
  if (*string)
    OP_DELETEA (string);
}

void
XPath_StringSet::Clear ()
{
  OpGenericStringHashTable::ForEach (XPath_StringSet_DeleteString);
  OpGenericStringHashTable::RemoveAll ();
}

#endif // XPATH_SUPPORT
