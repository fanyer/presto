/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(EMBEDDED_ICC_SUPPORT) && defined(LCMS_3P_SUPPORT)

#include "modules/img/imagecolormanager.h"
#include "modules/img/src/imagemanagerimp.h"
#include "modules/liblcms/include/lcms2.h"

class LCMS_ICCProfile : public ICCProfile
{
public:
	LCMS_ICCProfile() : m_profile(0) {}
	virtual ~LCMS_ICCProfile();

	cmsHPROFILE m_profile;
};

class LCMS_ColorTransform : public ImageColorTransform
{
public:
	LCMS_ColorTransform() : m_transform(0) {}
	virtual ~LCMS_ColorTransform();

	void Apply(UINT32* dst, const UINT32* src, unsigned length);
	UINT32* Apply(const UINT32* src, unsigned length);

	cmsHTRANSFORM m_transform;
};

class LCMS_ColorManager : public ImageColorManager
{
public:
	LCMS_ColorManager() : m_srgb_profile(NULL) {}
	virtual ~LCMS_ColorManager();

	virtual OP_STATUS CreateProfile(ICCProfile** profile, const UINT8* data, unsigned datalen);

	virtual OP_STATUS CreateTransform(ImageColorTransform** transform,
									  ICCProfile* src, ICCProfile* dst = NULL);

private:
	LCMS_ICCProfile* GetDeviceProfile();

	LCMS_ICCProfile* m_srgb_profile;
};

ImageColorManager* ImageColorManager::Create()
{
	return OP_NEW(LCMS_ColorManager, ());
}

LCMS_ICCProfile::~LCMS_ICCProfile()
{
	if (m_profile == 0)
		return;

	cmsCloseProfile(m_profile);
}

LCMS_ColorTransform::~LCMS_ColorTransform()
{
	if (m_transform == 0)
		return;

	cmsDeleteTransform(m_transform);
}

void LCMS_ColorTransform::Apply(UINT32* dst, const UINT32* src, unsigned length)
{
	cmsDoTransform(m_transform, src, dst, length);

	if (dst != src)
	{
		// Transfer the alpha channel from source to destination. Usually
		// it ought to be 255, but lets avoid assuming that for now.
		while (length-- > 0)
		{
			UINT32 src_a = *src & (0xffu << 24);
			UINT32 dst_rgb = *dst & 0xffffff;
			*dst = dst_rgb | src_a;

			dst++, src++;
		}
	}
}

UINT32* LCMS_ColorTransform::Apply(const UINT32* src, unsigned length)
{
	ImageManagerImp* imgman_impl = static_cast<ImageManagerImp*>(imgManager);
	if (UINT32* tmp = imgman_impl->GetScratchBuffer(length))
	{
		Apply(tmp, src, length);
		return tmp;
	}

	return NULL;
}

LCMS_ColorManager::~LCMS_ColorManager()
{
	OP_DELETE(m_srgb_profile);
}

LCMS_ICCProfile* LCMS_ColorManager::GetDeviceProfile()
{
	if (!m_srgb_profile)
	{
		m_srgb_profile = OP_NEW(LCMS_ICCProfile, ());
		if (m_srgb_profile)
		{
			cmsHPROFILE profile = cmsCreate_sRGBProfile();
			if (!profile)
			{
				OP_DELETE(m_srgb_profile);
				m_srgb_profile = NULL;
			}
			else
			{
				m_srgb_profile->m_profile = profile;
			}
		}
	}

	return m_srgb_profile;
}

OP_STATUS LCMS_ColorManager::CreateProfile(ICCProfile** profile, const UINT8* data, unsigned datalen)
{
	*profile = NULL;

	LCMS_ICCProfile* p = OP_NEW(LCMS_ICCProfile, ());
	if (!p)
		return OpStatus::ERR_NO_MEMORY;

	p->m_profile = cmsOpenProfileFromMem(data, datalen);
	if (!p->m_profile)
	{
		OP_DELETE(p);
		return OpStatus::ERR_NO_MEMORY;
	}

	*profile = p;
	return OpStatus::OK;
}

OP_STATUS LCMS_ColorManager::CreateTransform(ImageColorTransform** transform, ICCProfile* src, ICCProfile* dst)
{
	LCMS_ColorTransform* xfrm = OP_NEW(LCMS_ColorTransform, ());
	if (!xfrm)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	do
	{
		LCMS_ICCProfile* lcms_dst = static_cast<LCMS_ICCProfile*>(dst);
		if (!lcms_dst)
			lcms_dst = GetDeviceProfile();

		if (!lcms_dst)
			break;

		cmsHPROFILE dst_profile = lcms_dst->m_profile;
		OP_ASSERT(dst_profile);

		xfrm->m_transform = cmsCreateTransform(static_cast<LCMS_ICCProfile*>(src)->m_profile, TYPE_BGRA_8,
											   dst_profile, TYPE_BGRA_8,
											   INTENT_PERCEPTUAL, 0);

		if (!xfrm->m_transform)
			break;

		*transform = xfrm;

		return OpStatus::OK;
	} while(0);

	OP_DELETE(xfrm);

	return OpStatus::ERR_NO_MEMORY;
}

#endif // EMBEDDED_ICC_SUPPORT && LCMS_3P_SUPPORT
