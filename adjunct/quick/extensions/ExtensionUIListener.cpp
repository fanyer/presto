/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Code in this file has been copied from adjunct/quick/managers/DesktopExtensionsManager.cpp
 * For history prior to 20th Dec 2010, see there...
 *
 */
#include "core/pch.h"

#ifdef EXTENSION_SUPPORT

#include "adjunct/quick/extensions/ExtensionUIListener.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"

void ExtensionUIListener::OnExtensionSpeedDialInfoUpdated(OpWindowCommander* commander)
{
	OpGadget* gadget = commander->GetGadget();
	if (!gadget)
		return;

	g_desktop_extensions_manager->ExtensionDataUpdated(gadget->GetIdentifier());
}

void ExtensionUIListener::OnExtensionUIItemAddRequest(OpWindowCommander* commander, ExtensionUIItem* item)
{
	OP_ASSERT(commander && item && item->callbacks);
	if (!item || !item->callbacks) return; // should not happen, but not much we could do here

	if (BlockedByPolicy(commander, item))
	{
		item->callbacks->ItemAdded(item->id, ItemAddedNotSupported);
		return;
	}

	OP_STATUS status = OpStatus::ERR;
	switch (item->parent_id)
	{
		case CONTEXT_TOOLBAR:
		{
			ExtensionButtonComposite *button = g_desktop_extensions_manager->GetButtonFromModel(item->id);
			if (button)
				status = UpdateExtensionButton(item);
			else
				status = CreateExtensionButton(item, commander->GetGadget());
			break;
		}

		case CONTEXT_NONE:
		case CONTEXT_PAGE:
		case CONTEXT_SELECTION:
		case CONTEXT_LINK:
		case CONTEXT_EDITABLE:
		case CONTEXT_IMAGE:
		case CONTEXT_VIDEO:
		case CONTEXT_AUDIO:
		case CONTEXT_LAST_BUILTIN:
		default:
			item->callbacks->ItemAdded(item->id, ItemAddedContextNotSupported);
			return;
	}

	if (OpStatus::IsSuccess(status))
		item->callbacks->ItemAdded(item->id, ItemAddedSuccess);
	else if (OpStatus::IsMemoryError(status))
		item->callbacks->ItemAdded(item->id, ItemAddedResourceExceeded);
	else
		item->callbacks->ItemAdded(item->id, ItemAddedFailed);
}

void ExtensionUIListener::OnExtensionUIItemRemoveRequest(OpWindowCommander* commander,
		ExtensionUIItem* item)
{
	OP_ASSERT(item);
	if (!item) return;

	OP_STATUS status = DeleteExtensionButton(item);

	// item->callbacks is null when opera is closing and core is cleaning up
	if (!item->callbacks) return; //not much we could do here

	if (OpStatus::IsSuccess(status))
		item->callbacks->ItemRemoved(item->id, ItemRemovedSuccess);
	else
		item->callbacks->ItemRemoved(item->id, ItemRemovedFailed);
}

OP_STATUS ExtensionUIListener::CreateExtensionButton(ExtensionUIItem* item,
		OpGadget *gadget)
{
	OP_ASSERT(!g_desktop_extensions_manager->GetButtonFromModel(item->id));
	RETURN_IF_ERROR(g_desktop_extensions_manager->CreateButtonInModel(item->id));
	ExtensionButtonComposite *button = g_desktop_extensions_manager->GetButtonFromModel(item->id);
	button->SetGadgetId(gadget->GetIdentifier());
	button->UpdateUIItem(item);
	RETURN_IF_ERROR(button->CreateOpExtensionButton(item->id));
	return OpStatus::OK;
}

OP_STATUS ExtensionUIListener::UpdateExtensionButton(ExtensionUIItem* item)
{
	ExtensionButtonComposite *button = g_desktop_extensions_manager->GetButtonFromModel(item->id);
	OP_ASSERT(button);
	RETURN_IF_ERROR(button->UpdateUIItem(item));
	return OpStatus::OK;
}

OP_STATUS ExtensionUIListener::DeleteExtensionButton(ExtensionUIItem* item)
{
	ExtensionButtonComposite *button = g_desktop_extensions_manager->GetButtonFromModel(item->id);
	OP_ASSERT(button);
	RETURN_IF_ERROR(button->RemoveOpExtensionButton(item->id));
	RETURN_IF_ERROR(g_desktop_extensions_manager->DeleteButtonInModel(item->id));
	return OpStatus::OK;	
}

BOOL ExtensionUIListener::BlockedByPolicy(OpWindowCommander* commander, ExtensionUIItem* item)
{
	OP_ASSERT(commander->GetGadget() && item);

	switch (item->parent_id)
	{
		case CONTEXT_TOOLBAR:
		{
			if (commander->GetGadget()->IsFeatureRequested("opera:speeddial"))
				return TRUE;
			break;
		}
		case CONTEXT_NONE:
		case CONTEXT_PAGE:
		case CONTEXT_SELECTION:
		case CONTEXT_LINK:
		case CONTEXT_EDITABLE:
		case CONTEXT_IMAGE:
		case CONTEXT_VIDEO:
		case CONTEXT_AUDIO:
		case CONTEXT_LAST_BUILTIN:
		default:
			return FALSE;
	}
	return FALSE;
}

#endif //EXTENSION_SUPPORT
