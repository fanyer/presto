/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPCAMERA_DIRECTSHOW_H
#define WINDOWSOPCAMERA_DIRECTSHOW_H

#include "adjunct/desktop_util/thread/DesktopMutex.h"

namespace DirectShow
{
	extern const GUID MEDIASUBTYPE_4CC_I420;
	namespace mtype
	{
		//AM_MEDIA_TYPE helpers, see http://msdn.microsoft.com/en-us/library/windows/desktop/dd375432(v=VS.85).aspx
		static inline void FreeMediaType(AM_MEDIA_TYPE& mt)
		{
			if (mt.cbFormat != 0)
			{
				CoTaskMemFree((PVOID)mt.pbFormat);
				mt.cbFormat = 0;
				mt.pbFormat = NULL;
			}

			if (mt.pUnk != NULL)
			{
				// pUnk should not be used.
				mt.pUnk->Release();
				mt.pUnk = NULL;
			}
		}

		static inline void DeleteMediaType(AM_MEDIA_TYPE *pmt)
		{
			if (pmt != NULL)
			{
				FreeMediaType(*pmt); 
				CoTaskMemFree(pmt);
			}
		}

		static inline HRESULT CopyMediaType(AM_MEDIA_TYPE *pmtTarget, const AM_MEDIA_TYPE *pmtSource)
		{
			if (pmtTarget == pmtSource)
				return S_OK;


			*pmtTarget = *pmtSource;

			if (pmtSource->cbFormat != 0)
			{
				pmtTarget->pbFormat = (PBYTE)CoTaskMemAlloc(pmtSource->cbFormat);
				if (pmtTarget->pbFormat == NULL)
				{
					pmtTarget->cbFormat = 0;
					return E_OUTOFMEMORY;
				}
				else
				{
					op_memcpy(pmtTarget->pbFormat, pmtSource->pbFormat, pmtTarget->cbFormat);
				}
			}

			if (pmtTarget->pUnk)
				pmtTarget->pUnk->AddRef();

			return S_OK;
		}

		static inline AM_MEDIA_TYPE * CreateMediaType(AM_MEDIA_TYPE const *pSrc)
		{
			// Allocate a block of memory for the media type
			AM_MEDIA_TYPE *pMediaType =
				(AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));

			if (!pMediaType)
				return NULL;

			// Copy the variable length format block
			if (pSrc)
			{
				HRESULT hr = CopyMediaType(pMediaType, pSrc);
				if (FAILED(hr))
				{
					CoTaskMemFree((PVOID)pMediaType);
					return NULL;
				}
			}

			return pMediaType;
		}

	};

	namespace coll
	{
		// IEnumXXXX collection implementation helper
		// Classes wanting to use it must be compatible with
		// OpComObjectEx
		template < class Impl >
		class OpEnumXXXX: public Impl
		{
		public:
			typedef typename Impl::Item Item;
			typedef typename Impl::BaseInterface BaseInterface;
			typedef typename Impl::Init Init;

			ULONG position;

			OpEnumXXXX(const typename Impl::Init& init): Impl(init), position(0) {}

			STDMETHODIMP Next(ULONG cItems, Item **ppItems, ULONG *pcFetched)
			{
				if (!cItems)
				{
					if (pcFetched)
						*pcFetched = 0;
					return S_OK;
				}

				// pcFetched is optional only when one item is taken.
				// buffer if always required
				if (!ppItems || (cItems > 1 && !pcFetched))
					return E_INVALIDARG;

				ULONG i = 0, pos = position, last = position + cItems;
				HRESULT hr = S_OK;

				ULONG size = CollectionSize();
				if (last > size)
				{
					last = size;
					hr = S_FALSE; // S_FALSE: Insufficient array size.
				}

				while (pos < last)
				{
					HRESULT _hr = CopyItem(pos, ppItems + i++);
					if (FAILED(hr))
					{
						ULONG j = 0;
						while (j < i)
							DestroyItem(ppItems + j++);

						return _hr;
					}
					++pos;
				}

				if (pcFetched) *pcFetched = i;
				position = pos;

				return hr;
			}

			STDMETHODIMP Skip(ULONG cItems)
			{
				HRESULT hr = S_OK;
				ULONG left = CollectionSize() - position;
				if (cItems > left)
				{
					cItems = left;
					hr = S_FALSE;
				}

				position += cItems;

				return hr;
			}

			STDMETHODIMP Reset()
			{
				position = 0;
				return S_OK;
			}

			STDMETHODIMP Clone(BaseInterface **ppEnum)
			{
				OpComPtr<BaseInterface> ref;
				OLE_(OpComObjectEx< OpEnumXXXX<Impl> >::Create(GetInit(), &ref));
				ref->Skip(position);
				return ref->QueryInterface(ppEnum);
			}
		};

	};
	// IEnumMediaTypes and IEnumPins collections
	namespace coll
	{
		struct FrameSize
		{
			// not keeping the bitrate and the average time per frame can
			// result in BSOD 1E (KMODE_EXCEPTION_NOT_HANDLED), where driver
			// raises the c0000094 (Integer divide-by-zero) exception
			LONG width, height, bitrate;
			REFERENCE_TIME frame_time;

			FrameSize(): width(0), height(0), bitrate(0), frame_time(0) {}
			inline bool operator == (const FrameSize& right) const
			{
				return
					width == right.width &&
					height == right.height &&
					bitrate == right.bitrate &&
					frame_time == right.frame_time;
			}

			template <typename T> inline void CopyFromFormat(const T* format)
			{
				width = format->bmiHeader.biWidth;
				height = format->bmiHeader.biHeight;
				bitrate = format->dwBitRate;
				frame_time = format->AvgTimePerFrame;
			}

			inline void FillMediaType(AM_MEDIA_TYPE& stencil, VIDEOINFOHEADER& header, ULONG samplesize) const
			{
				stencil.bFixedSizeSamples = TRUE;
				stencil.lSampleSize = samplesize;

				header.dwBitRate = bitrate;
				header.AvgTimePerFrame = frame_time;
				header.bmiHeader.biWidth = width;
				header.bmiHeader.biHeight = height;
				header.bmiHeader.biSizeImage = samplesize;
			}
		};
		typedef OpAutoVector<FrameSize> FrameSizes;

		struct MediaFormat
		{
			REFIID subtype;
			WORD bitcount;
			DWORD compression;
			MediaFormat(REFIID subtype, WORD bitcount, DWORD compression = 0)
				: subtype(subtype)
				, bitcount(bitcount)
				, compression(compression)
			{
			}

			inline void FillMediaType(AM_MEDIA_TYPE& stencil, VIDEOINFOHEADER& header) const
			{
				stencil.majortype = MEDIATYPE_Video;
				stencil.subtype = subtype;
				stencil.formattype = FORMAT_VideoInfo;

				header.bmiHeader.biPlanes = 1;
				header.bmiHeader.biBitCount = bitcount;
				header.bmiHeader.biCompression = compression;
			}
		};

		class OpInputMediaTypes: public IEnumMediaTypes
		{
		public:
			typedef AM_MEDIA_TYPE Item;
			typedef IEnumMediaTypes BaseInterface;
			typedef const FrameSizes& Init;
			const static MediaFormat formats[];
			const static size_t format_count;

			Init sizes;

#ifdef WINDOWS_CAMERA_GRAPH_TEST
			const MediaFormat* m_prefs_forced_format;
#endif

			OpInputMediaTypes(const Init& init)
				: sizes(init)
#ifdef WINDOWS_CAMERA_GRAPH_TEST
				, m_prefs_forced_format(NULL)
#endif
			{
			}

			ULONG CollectionSize() const;
			HRESULT CopyItem(ULONG pos, AM_MEDIA_TYPE** ppOut);

			void DestroyItem(AM_MEDIA_TYPE** ppOut)
			{
				mtype::DeleteMediaType(*ppOut);
				*ppOut = NULL;
			}

			const Init& GetInit() const { return sizes; }

			// IUnknown
			SIMPLE_BASE_OBJECT();
			BEGIN_TYPE_MAP()
				TYPE_ENTRY(IEnumMediaTypes)
			END_TYPE_MAP();

			HRESULT FinalCreate();
		};
		typedef OpEnumXXXX<OpInputMediaTypes> OpInputMediaTypesEnum;

		class OpSinglePin : public IEnumPins
		{
			OpComPtr<IPin> m_pin;
		public:
			typedef IPin Item;
			typedef IEnumPins BaseInterface;
			typedef IPin* Init;

			OpSinglePin(IPin* pin): m_pin(pin) {}

			ULONG CollectionSize() const { return 1; }

			HRESULT CopyItem(ULONG, IPin** ppOut)
			{
				return m_pin.CopyTo(ppOut);
			}

			void DestroyItem(IPin** ppOut)
			{
				if (ppOut)
				{
					(*ppOut)->Release();
					*ppOut = NULL;
				}
			}

			IPin* GetInit() const { return m_pin; }

			// IUnknown
			SIMPLE_BASE_OBJECT();
			BEGIN_TYPE_MAP()
				TYPE_ENTRY(IEnumPins)
			END_TYPE_MAP();

			HRESULT FinalCreate() { return S_OK; }
		};
		typedef OpEnumXXXX<OpSinglePin> OpSinglePinEnum;
	}

	namespace trans
	{
		struct FormVtbl
		{
			bool (*Supported)(REFIID mediasubtype);
			HRESULT (*Transform)(const unsigned char* src, long src_bytes, OpBitmap *frame, long width, long height);
		};

		// helper class to implement line-based decoders
		template <typename Impl>
		class OpTransform
		{
		public:
			static long BitmapStride(long raw_bytes_per_line) { return ((raw_bytes_per_line + 3) >> 2) << 2; }

			static HRESULT Transform(const unsigned char* src_data, long /* src_bytes */, OpBitmap *frame, long width, long height)
			{
				//This function will be called only, if OpBitmap supports pointers.
				//If current implementation of OpBitmap does not support pointers,
				//the OpDirectShowCamera::SetMediaType will start asserting and
				//OpDirectShowCamera::Receive will not call any transforms...

				unsigned char * dst_data = reinterpret_cast<unsigned char*>(frame->GetPointer());
				unsigned long dst_stride = width * 4; // ABGR32

				long src_stride = Impl::Stride(width);

				for (long y = 0; y < height; ++y)
				{
					//RGBs are bottom-ups
					unsigned char * dst = dst_data + y * dst_stride;
					const unsigned char * src = src_data + Impl::LineOffset(height, y, src_stride);

					Impl::TransformLine(width, dst, src);
				}

				frame->ReleasePointer();

				return S_OK;
			}
		};

		extern const FormVtbl* forms;
		extern const size_t form_count;

		// helper function to transform DirectShow media samples
		static inline HRESULT Form(const FormVtbl& transform, IMediaSample* pSample, OpBitmap *frame, long width, long height)
		{
			unsigned char * src_data;
			OLE_(pSample->GetPointer(&src_data));
			return transform.Transform(src_data, pSample->GetActualDataLength(), frame, width, height);
		}
	}

	// IPin (IMemInputPin) and IBaseFilter implementation
	namespace impl
	{
		class OpDirectShowCameraPin;
		class OpDirectShowCamera;

		/*
		  This filter works on two buffers. One, named 'frame',
		  is accessible from the directshow filter thread; the other,
		  'backbuffer', is accessible only from the Core thread.

		  Since the Core takes the bitmap out of our control, the only
		  place we can actually switch buffers is inside GetCurrentPreviewFrame,
		  when we know Core makes nothing with the bitmap. So, the Receive
		  copies the media sample onto the same bitmap until the Core asks
		  for a new frame. Then the bitmaps are switched and Receive starts working
		  on the other bitmap from now on. OTOH, if Core asks for a frame more than
		  once between two Receives, it will get the same bitmap twice.

		  The other place the backbuffer is accessed is the SetMediaType: first when the graph
		  is started and then possibly when camera changes size of the media type. Unfortunately
		  there is possibly no camera that would play such a prank on us, so this is untestable...
		*/

		class OpDirectShowCamera
			: public ICamera
			, public IBaseFilter
		{
			friend class OpDirectShowCameraPin;

			enum BACKBUFFER_STATUS
			{
				BACKBUFFER_FRESH,    //between ctor and the first GetCurrentPreviewFrame
				BACKBUFFER_ACCESSED, //between GetCurrentPreviewFrame and SetMediaType, the buffer is outside and should not be touched
				BACKBUFFER_STALE     //between SetMediaType and GetCurrentPreviewFrame, when next time in GCPF, delete the backbuffer and create a new one
			};
			// both ACCESSED and STALE mean not FRESH, see also the comments
			// inside OpDirectShowCamera::GetCurrentPreviewFrame

			OpComPtr<IPin>            in_pin;
			OpComPtr<IReferenceClock> clock;
			IFilterGraph             *graph; // we *do not* keep the reference to the graph, or we get cyclic references...
			                                 // the baseclasses from DS examples do not keep it...
			                                 //
			                                 // more properly, we keep only *weak reference* (not full WE thou, when referenced object goes,
			                                 // pointer is invalid and we crash, but the referenced object should die after us anyway)

			OpString                  name;
			coll::FrameSizes          sizes;
			OpCamera::Resolution      frame_res;

			AM_MEDIA_TYPE            *media_type;
			bool                      is_playing;
			WindowsOpCamera          *parent;
			const trans::FormVtbl    *curr_xform;
			DesktopMutex              camera_mutex;
			OpBitmap                 *frame;
			OpBitmap                 *backbuffer;
			bool                      frame_dirty;
			BACKBUFFER_STATUS         backbuffer_status;

			inline HRESULT   EnsureInputPinPresent();
			inline HRESULT   ReadSupportedSizes(IPin* pin);
			inline HRESULT   SetMediaType(const AM_MEDIA_TYPE* media_type, bool notify = true);

			// callbacks from the input pin

			inline bool      IsStarted() const { return is_playing; }
			inline HRESULT   Connect(IPin* pin, const AM_MEDIA_TYPE* media_type);
			inline HRESULT   Disconnect();
			inline HRESULT   Receive(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps);
			inline HRESULT   QueryMediaType(AM_MEDIA_TYPE* media_type);

		public:
			OpDirectShowCamera()
				: media_type(NULL)
				, is_playing(false)
				, parent(NULL)
				, curr_xform(NULL)
				, graph(NULL)
				, frame(NULL)
				, backbuffer(NULL)
				, frame_dirty(false)
				, backbuffer_status(BACKBUFFER_FRESH)
			{
			}

			~OpDirectShowCamera();

			// IUnknown
			SIMPLE_BASE_OBJECT2(ICamera);
			BEGIN_TYPE_MAP()
				TYPE_ENTRY(ICamera)
				TYPE_ENTRY(IBaseFilter)
				TYPE_ENTRY(IMediaFilter)
				TYPE_ENTRY(IPersist)
			END_TYPE_MAP();

			inline HRESULT FinalCreate(); //due to CAM2HR, moved to .cpp

			// IDirectShowCamera
			STDMETHODIMP CreateGraph(WindowsOpCamera* parent, DWORD device, IMediaControl** ppmc);
			STDMETHODIMP GetCurrentPreviewFrame(OpBitmap** frame);
			STDMETHODIMP GetPreviewDimensions(OpCamera::Resolution* dimensions);

			// IPersist
			STDMETHODIMP GetClassID(CLSID *pClassID)
			{
				if (!pClassID) return E_POINTER;

				// {40FDE60D-4819-47AD-A65D-6880B589E214}
				static const GUID classid = 
				{ 0x40fde60d, 0x4819, 0x47ad, { 0xa6, 0x5d, 0x68, 0x80, 0xb5, 0x89, 0xe2, 0x14 } };

				*pClassID = classid;
				return S_OK;
			}

			// IMediaFilter
			STDMETHODIMP Stop();
			STDMETHODIMP Pause();
			STDMETHODIMP Run(REFERENCE_TIME tStart);
			STDMETHODIMP GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *State);
			STDMETHODIMP SetSyncSource(IReferenceClock *pClock) { clock = pClock; return S_OK; }
			STDMETHODIMP GetSyncSource(IReferenceClock **pClock) { return clock.CopyTo(pClock); }

			// IBaseFilter
			STDMETHODIMP EnumPins(IEnumPins **ppEnum);
			STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin);
			STDMETHODIMP QueryFilterInfo(FILTER_INFO *pInfo);
			STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);
			STDMETHODIMP QueryVendorInfo(LPWSTR *pVendorInfo);

			// helpers
			AM_MEDIA_TYPE * GetMediaType() { return media_type; }
#ifdef WINDOWS_CAMERA_GRAPH_TEST
			// Returns the SupportedCameraFormat currentely set in the prefs, if actually set and recognized, or GUID_NULL otherwise
			inline static GUID OverridenCameraFormat();
#endif
		};

		class OpDirectShowCameraPin
			: public IPin
			, public IMemInputPin
		{
			OpDirectShowCamera* m_parent;
			OpComPtr<IPin> m_connection;
			OpComPtr<IMemAllocator> m_allocator;
			bool m_flushing;

			inline bool IsConnected() const
			{
				return m_connection != NULL;
			}
			inline bool IsStarted() const { return m_parent->IsStarted(); }
			inline bool IsFlushing() const { return m_flushing; }
			inline HRESULT GetSampleProps(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps);
		public:
			typedef OpDirectShowCamera* Init;

			OpDirectShowCameraPin(OpDirectShowCamera* parent): m_parent(parent), m_flushing(false) {}

			// IUnknown
			SIMPLE_BASE_OBJECT2(IPin);
			BEGIN_TYPE_MAP()
				TYPE_ENTRY(IPin)
				TYPE_ENTRY(IMemInputPin)
			END_TYPE_MAP();

			HRESULT FinalCreate() { return S_OK; }

			// IPin
			STDMETHODIMP Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt);
			STDMETHODIMP ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);
			STDMETHODIMP Disconnect();
			STDMETHODIMP ConnectedTo(IPin **pPin);
			STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt);
			STDMETHODIMP QueryPinInfo(PIN_INFO *pInfo);
			STDMETHODIMP QueryDirection(PIN_DIRECTION *pPinDir);
			STDMETHODIMP QueryId(LPWSTR *Id);
			STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt);
			STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);
			STDMETHODIMP QueryInternalConnections(IPin **apPin, ULONG *nPin);
			STDMETHODIMP EndOfStream();
			STDMETHODIMP BeginFlush();
			STDMETHODIMP EndFlush();
			STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

			// IMemInputPin
			STDMETHODIMP GetAllocator(IMemAllocator **ppAllocator);
			STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);
			STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);
			STDMETHODIMP Receive(IMediaSample *pSample);
			STDMETHODIMP ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed);
			STDMETHODIMP ReceiveCanBlock();
		};
	}

#if defined(_DEBUG) || defined(WINDOWS_CAMERA_GRAPH_TEST)
#define DEBUG_DSHOW_GRAPH_DUMP
#endif

#ifdef DEBUG_DSHOW_GRAPH_DUMP
	namespace debug
	{
		static inline HRESULT DumpGraph(IGraphBuilder* gb);
	};
#endif
};

#endif //WINDOWSOPCAMERA_DIRECTSHOW_H