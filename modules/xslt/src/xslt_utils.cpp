/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_utils.h"
#include "modules/util/tempbuf.h"
#include "modules/stdlib/include/double_format.h"

/* static */ OP_STATUS
XSLT_Utils::GrowArray (void ***array, unsigned used, unsigned need, unsigned &total)
{
  OP_ASSERT(used <= total);
  OP_ASSERT(need >= used);

  if (need > total)
    {
      unsigned new_total = total == 0 ? 8 : total;
      while (need > new_total)
        new_total += new_total;
      void **new_array = OP_NEWA (void *, new_total);
      if (!new_array)
        return OpStatus::ERR_NO_MEMORY;

      op_memcpy (new_array, *array, used * sizeof new_array[0]);
      OP_DELETEA (*array);

      *array = new_array;
      total = new_total;
    }

  return OpStatus::OK;
}

/* static */ BOOL
XSLT_Utils::ParseFloatL (float &fvalue, const uni_char *value, unsigned value_length)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  buffer.AppendL (value, value_length);

  if (!buffer.GetStorage ())
    return FALSE;

  uni_char *endptr;

  fvalue = (float) uni_strtod (buffer.GetStorage (), &endptr);
  return TRUE;
}

/* static */ OP_STATUS
XSLT_Utils::ConvertToString (TempBuffer &buffer, double value, unsigned precision)
{
  char *string = OP_NEWA (char, 32);

  if (!string)
    return OpStatus::ERR_NO_MEMORY;

  char *result = OpDoubleFormat::ToFixed (string, value, (int) precision);

  OP_STATUS status = result ? buffer.Append (result) : OpStatus::ERR_NO_MEMORY;

  OP_DELETEA (string);

  return status;
}

#endif // XSLT_SUPPORT
