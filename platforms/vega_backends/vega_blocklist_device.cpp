/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKENDS_USE_BLOCKLIST

#include "modules/about/operagpu.h"
#include "modules/libvega/vega3ddevice.h"
#include "modules/util/opautoptr.h"
#include "platforms/vega_backends/vega_blocklist_device.h"
#include "platforms/vega_backends/vega_blocklist_file.h"

static
const OpStringC GetStatusString(VEGABlocklistDevice::BlocklistStatus status)
{
	switch (status)
	{
	case VEGABlocklistDevice::Supported:
		return UNI_L("Supported");
	case VEGABlocklistDevice::BlockedDriverVersion:
		return UNI_L("Blocked driver version");
	case VEGABlocklistDevice::BlockedDevice:
		return UNI_L("Blocked device");
	case VEGABlocklistDevice::Discouraged:
		return UNI_L("Discouraged");
	case VEGABlocklistDevice::ListError:
		return UNI_L("List error");
	}

	OP_ASSERT(!"unknown blocklist status encountered");
	return UNI_L("");
}

/**
   temporarily hijack g_vegaGlobals.vega3dDevice, create a
   new 3d device, make it generate some content, destroy and clean up
 */
// static
OP_STATUS VEGABlocklistDevice::CreateContent(unsigned int device_type_index, OperaGPU* page)
{
	OP_ASSERT(page);
	VEGA3dDevice* const gdev = g_vegaGlobals.vega3dDevice;

	// no need creating a new device of the same type
	if (gdev && gdev->getDeviceType() == device_type_index)
	{
		OpStatus::Ignore(((VEGABlocklistDevice*)gdev)->GenerateBackendInfo(page, FALSE));
		return OpStatus::OK;
	}

	// VEGAGlDevice is implemented in such a way that it requires
	// g_vegaGlobals.vega3dDevice to point to the device
	// in use.
	VEGA3dDevice*& dev = g_vegaGlobals.vega3dDevice;
	const OP_STATUS status = VEGA3dDeviceFactory::Create(device_type_index, &dev
#ifdef VEGA_3DDEVICE
	                                                     , 0
#endif // VEGA_3DDEVICE
		);
	if (OpStatus::IsSuccess(status))
	{
		((VEGABlocklistDevice*)dev)->GenerateBackendInfo(page, TRUE);
		dev->Destroy();
		OP_DELETE(dev);
	}
	dev = gdev; // restore global pointer
	return status;
}

// show the 'target, status' pair, appending 'reason' if status is not 'Supported'
static inline
OP_STATUS ShowStatus(OperaGPU* page, const OpStringC& target, VEGABlocklistDevice::BlocklistStatus status, const uni_char* reason)
{
	return page->ListEntry(target, GetStatusString(status), status == VEGABlocklistDevice::Supported ? 0 : reason);
}

OP_STATUS VEGABlocklistDevice::GenerateBackendInfo(OperaGPU* page, BOOL construct_device)
{
	const BlocklistType type = GetBlocklistType();
	VEGABlocklistFile blocklist;

	page->Heading(UNI_L("Blocklist status"), 3);

	RETURN_IF_ERROR(page->OpenDefinitionList());

	if (OpStatus::IsError(blocklist.Load(type)))
	{
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Blocklist status for 2D"), GetStatusString(ListError)));
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Blocklist status for 3D"), GetStatusString(ListError)));
		return page->CloseDefinitionList();
	}

	OpAutoPtr<VEGABlocklistDevice::DataProvider> provider(CreateDataProvider());
	RETURN_OOM_IF_NULL(provider.get());

	RETURN_IF_ERROR(page->ListEntry(UNI_L("Blocklist version"), blocklist.GetVersion()));

	VEGABlocklistFileEntry* e;
	RETURN_IF_ERROR(blocklist.FindMatchingEntry(provider.get(), e));
	VEGABlocklistDevice::BlocklistStatus status2d = Supported;
	if (e)
	{
		status2d = e->status2d;
		RETURN_IF_ERROR(ShowStatus(page, UNI_L("Blocklist status for 2D"), e->status2d, e->reason2d));
		RETURN_IF_ERROR(ShowStatus(page, UNI_L("Blocklist status for 3D"), e->status3d, e->reason3d));
	}
	else
	{
		// no matching entry means device is supported
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Blocklist status for 2D"), GetStatusString(Supported)));
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Blocklist status for 3D"), GetStatusString(Supported)));
	}

	bool device_initialized = false;
	if (status2d == Supported && construct_device)
	{
		device_initialized = !!OpStatus::IsSuccess(Construct3dDevice());
		if (!device_initialized)
		{
			RETURN_IF_ERROR(page->ListEntry(UNI_L("Creation status"), g_vega_backends_module.GetCreationStatus()));
		}
	}
	RETURN_IF_ERROR(page->CloseDefinitionList());

	// If device failed to initialize better off avoid any further calls into the device
	if (!construct_device || device_initialized)
	{
		page->Heading(UNI_L("Graphics hardware and driver details"), 3);

		RETURN_IF_ERROR(GenerateSpecificBackendInfo(page, &blocklist, provider.get()));

		// driver link
		VEGABlocklistDriverLinkEntry* l;
		RETURN_IF_ERROR(blocklist.FindMatchingDriverLink(provider.get(), l));
		if (l)
		{
			RETURN_IF_ERROR(page->Link(UNI_L("Download driver"), l->m_driver_link));
		}
	}

	return OpStatus::OK;
}

#endif // VEGA_BACKENDS_USE_BLOCKLIST
