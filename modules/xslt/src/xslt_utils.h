/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_UTILS_H
#define XSLT_UTILS_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_types.h"

class XMLDocumentInformation;
class TempBuffer;
class URL;

class XSLT_Utils
{
public:
  static OP_STATUS GrowArray (void ***array, unsigned used, unsigned need, unsigned &total);
  static void GrowArrayL (void ***array, unsigned used, unsigned need, unsigned &total) { LEAVE_IF_ERROR (GrowArray (array, used, need, total)); }
  static BOOL ParseFloatL (float &fvalue, const uni_char *value, unsigned value_length);
  static OP_STATUS ConvertToString (TempBuffer &buffer, double value, unsigned precision);
};

#endif // XSLT_SUPPORT
#endif // XSLT_UTILS_H
