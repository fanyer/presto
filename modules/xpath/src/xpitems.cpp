/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpitems.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpnode.h"

template <class XPath_Item>
XPath_Items<XPath_Item>::XPath_Items ()
  : blocks (0),
    firstfree (0),
    blocksused (0),
    blockstotal (8)
{
#ifdef _DEBUG
  itemsfree = 0;
  itemstotal = 0;
#endif // _DEBUG
}

template <class XPath_Item>
XPath_Items<XPath_Item>::~XPath_Items ()
{
}

template <class XPath_Item>
XPath_Item *
XPath_Items<XPath_Item>::GetL ()
{
  if (!firstfree)
    {
#ifdef _DEBUG
      OP_ASSERT (itemsfree == 0);
#endif // _DEBUG

      if (!blocks || blocksused == blockstotal)
        {
          blockstotal += blockstotal;

          XPath_Item **newblocks = OP_NEWA_L (XPath_Item *, blockstotal);

          if (blocks)
            {
              op_memcpy (newblocks, blocks, sizeof *blocks * blocksused);
              op_memset (newblocks + blocksused, 0, sizeof *blocks * blocksused);
            }
          else
            op_memset (newblocks, 0, sizeof *blocks * blockstotal);

          OP_DELETEA (blocks);
          blocks = newblocks;
        }

      firstfree = blocks[blocksused++] = OP_NEWA_L (XPath_Item, blockstotal);

      for (unsigned index = 0; index < blockstotal - 1; ++index)
        firstfree[index].data.nextfree = &firstfree[index + 1];

      firstfree[blockstotal - 1].data.nextfree = 0;

#ifdef _DEBUG
      itemsfree += blockstotal;
      itemstotal += blockstotal;
#endif // _DEBUG
    }

  XPath_Item *item = firstfree;
  firstfree = firstfree->data.nextfree;

#ifdef _DEBUG
  --itemsfree;
#endif // _DEBUG

  return item;
}

template <class XPath_Item>
void
XPath_Items<XPath_Item>::Free (XPath_Context *context, XPath_Item *item)
{
  item->SetFree (context, firstfree);
  firstfree = item;

#ifdef _DEBUG
  ++itemsfree;
#endif // _DEBUG
}

template <class XPath_Item>
void
XPath_Items<XPath_Item>::Clean ()
{
  for (unsigned index = 0; index < blocksused; ++index)
    OP_DELETEA (blocks[index]);
  OP_DELETEA (blocks);

  blocks = 0;
  firstfree = 0;
  blocksused = 0;
  blockstotal = 8;

#ifdef _DEBUG
  itemsfree = 0;
  itemstotal = 0;
#endif // _DEBUG
}

#ifdef _DEBUG

template <class XPath_Item>
BOOL
XPath_Items<XPath_Item>::IsAllFree ()
{
  return itemsfree == itemstotal;
}

#endif // _DEBUG

template class XPath_Items<XPath_Value>;
template class XPath_Items<XPath_Node>;

#endif // XPATH_SUPPORT
