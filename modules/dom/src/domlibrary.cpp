/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/dominternaltypes.h"

#ifdef DOM_LIBRARY_FUNCTIONS

#include "modules/dom/src/domlibrary.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domtraversal/nodeiterator.h"

const DOM_LibraryFunction::Binding DOM_Node_normalize_bindings[] =
{
	{ (void (*)()) DOM_Document::createTraversalObject, 0 },
	{ (void (*)()) DOM_NodeIterator::move, 1 },
	{ (void (*)()) DOM_Node::removeChild, INT_MAX },
	{ (void (*)()) DOM_CharacterData::appendData, INT_MAX },
	{ NULL, 0 }
};

const char *const DOM_Node_normalize_code[] =
{
	"var nodes=$0.apply(this.ownerDocument,[this,4,null,false]),node;while(no",
	"de=$1.apply(nodes)){if(node.nodeValue.length==0)$2.apply(node.parentNode",
	",[node]);else{var next,text=\"\";while((next=node.nextSibling)&&next.nodeT",
	"ype==3){text+=next.nodeValue;$2.apply(next.parentNode,[next]);}if(text.l",
	"ength!=0)$3.apply(node,[text]);}}",
	NULL
};

const DOM_LibraryFunction DOM_Node_normalize = { DOM_Node_normalize_bindings, DOM_Node_normalize_code };

#endif // DOM_LIBRARY_FUNCTIONS
