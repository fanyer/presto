#ifndef MAC_WIDGETPAINTER_H
#define MAC_WIDGETPAINTER_H

#include "modules/skin/IndpWidgetPainter.h"
#include "modules/pi/OpBitmap.h"
class MacOpPainter;

class MacWidgetPainter : public IndpWidgetPainter
{
public:
	MacWidgetPainter();
	~MacWidgetPainter();

	virtual void InitPaint(VisualDevice* vd, OpWidget* widget);

	virtual OpWidgetInfo* GetInfo(OpWidget* widget = NULL);

	virtual BOOL CanHandleCSS() { return FALSE; }

	virtual BOOL DrawScrollbar(const OpRect &drawrect);
	virtual BOOL DrawResizeCorner(const OpRect &drawrect, BOOL active);
	virtual BOOL DrawEdit(const OpRect &drawrect);
	virtual BOOL DrawMultiEdit(const OpRect &drawrect);

	virtual BOOL DrawDropdown(const OpRect &drawrect);
	virtual BOOL DrawDropdownButton(const OpRect &drawrect, BOOL is_down) { return TRUE; }

	virtual BOOL DrawProgressbar(const OpRect &drawrect, double percent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin, const char *full_skin);

	virtual BOOL DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track);

private:
	virtual void DrawFocusRect(const OpRect &drawrect);
	void DrawRoundedFocusRect(const OpRect &drawrect, BOOL drawFocusRect);

	CGrafPtr m_offscreen;
	MacOpPainter* m_painter;
};

/**
 * Simple wrapper around OpBitmap. 
 * This class uses reference counting for managing object lifetime.
 */
class MacWidgetBitmap {
	long c_ref;
	OpBitmap* bitmap_;
	~MacWidgetBitmap() {
		OP_DELETE(bitmap_);
	}
	bool is_locked;
public:
	explicit MacWidgetBitmap(OpBitmap* p)
	: c_ref(1)
	, bitmap_(p)
	, is_locked(false)
	{ }
	long AddRef() { return ++c_ref; }
	long DecRef() {
		const long c = --c_ref;
		if (0 == c)
			OP_DELETE(this);
		return c;
	}
	///< Lock the bitmap for preventing race condition (though, locking itself isn't atomic).
	bool Lock() { if (is_locked) return false; is_locked = true; return true; }
	///< Unlock when you are done.
	bool Unlock() { if (!is_locked) return false; is_locked = false; return true; }
	///< Checks whether the bitmap is locked.
	bool IsLocked() const { return is_locked; }
	///< Retrieves OpBitmap.
	OpBitmap* GetOpBitmap() { return bitmap_; }
	///< Retrieves OpBitmap.
	const OpBitmap* GetOpBitmap() const { return bitmap_; }
};

class MacWidgetBitmap;

/**
 * OpHashTable cludge!
 * This class is a simple string hash table implementation with
 * overridden elemet deletion functions.
 * Because we store a malloc'ed char* (as key) and a reference
 * counted type (MacWidgetBitmap), we need special delete functions.
 */
class MacAutoString8HashTable : public OpGenericString8HashTable
{
public:
	explicit MacAutoString8HashTable(BOOL case_sensitive = FALSE) : OpGenericString8HashTable(case_sensitive) {}
	virtual void Delete(void* p) { static_cast<MacWidgetBitmap*>(p)->DecRef(); }
	virtual ~MacAutoString8HashTable() { this->UnrefAndDeleteAll(); }
	// special delete function which decreases ref cout of the "value"
	// and frees the "key".
	static void DeleteFunc(const void* key, void *data, OpHashTable* table);		
	void UnrefAndDeleteAll();		
	void UnrefAll();
};	

struct MacWidgetBitmapTraits {
	struct CreateParam {
		UINT32 w;
		UINT32 h;
		CreateParam(UINT32 w_, UINT32 h_)
		: w(w_), h(h_)
		{ }
	};
	typedef OpBitmap value_type;
	typedef OpBitmap* pointer;

	static bool MacWidgetBitmapIsEqual(const CreateParam& p1, const CreateParam& p2);
	static char* CreateParamHashKey(const CreateParam& cp);
	static OP_STATUS CreateResource(pointer* p, const CreateParam& cp);
	static void ReleaseHashKey(char* p) { free(p); }
	static bool Matches(const MacWidgetBitmap* wbmp, const CreateParam& cp);
};

/**
 * Class factory for making bitmaps at given size, or
 * returning one bitmap from cache that is either
 * equal to or larger than the required bitmap size.
 */
template<typename T, typename TTraits>
class MacCachedObjectFactory {	
public:
	enum CreateResult {
		CreateResultNew,
		CreateResultExact,
		CreateResultSuitable,
		CreateResultFailed
	};
	/**
	 * Initializes the (singleton) object.
	 */
	static OP_STATUS Init();
	/** Destroys the (singleton) object. */
	static void Destroy();
	/**
	 * Creates new bitmap or retrieves a bitmap suitable for requested size.
	 * @param [in]w Requested width.
	 * @param [in]h Requested height.
	 * @return Pointer to MacWidgetBitmap. This function increases reference count by 1.
	 *
	 * This function first searches for an exact match. If a bitmap with requested size is found,
	 * function checks whether the bitmap is locked. If found bitmap is locked, function continues
	 * search until it finds a larger and unlocked bitmap. If a bitmap is found, function returns
	 * that bitmap. Otherwise, function attempts to create a new bitmap.
	 *
	 * @remarks Returned bitmap must be locked for preventing nested calls to
	 *          mutate the same object.
	 */
	static T* CreateObject(const typename TTraits::CreateParam& cp, CreateResult* cr = NULL) { return instance_->CreateObjectImpl(cp, cr); }
private:

	static MacCachedObjectFactory* instance_;
	T* CreateObjectImpl(const typename TTraits::CreateParam& cp, CreateResult* cr);
	MacAutoString8HashTable map_;
	static const uint16_t kCacheSize; // fine tune this in cpp, if you need to
};

template<typename T, typename TTraits>
void MacCachedObjectFactory<T, TTraits>::Destroy() {
	instance_->map_.DeleteAll();
	OP_DELETE(instance_);
	instance_ = NULL;
}

template<typename T, typename TTraits>
OP_STATUS MacCachedObjectFactory<T, TTraits>::Init() {
	if (instance_)
		return OpStatus::OK;
	instance_ = OP_NEW(MacCachedObjectFactory, ());
	RETURN_OOM_IF_NULL(instance_);
	return OpStatus::OK;
}

/**
 * @brief Creates a bitmap or fetches one from cache.
 *
 * This function fetches a bitmap from the cache or creates a new one.
 * Function first checks whether there is an identical bitmap in the hash table.
 * Otherwise, it checks whether there is a bitmap that is larger than requested
 * bitmap size. If one is found, function returns the result. If not,
 * function tries to create a new bitmap and insert into hash table.
 *
 * @remarks Caller must call DecRef on the returned pointer.
 */
template<typename T, typename TTraits>
T* MacCachedObjectFactory<T, TTraits>::CreateObjectImpl(const typename TTraits::CreateParam& cp, CreateResult* cr)
{
	T* res;
	res = NULL;
	char* tmp = TTraits::CreateParamHashKey(cp);
	if (cr)
		*cr = CreateResultFailed;
	if (!tmp)
		return NULL;

	// check whether we already have a bitmap at this size.
	if (OpStatus::IsError(map_.GetData(tmp, (void**)&res)) || !res || res->IsLocked())
	{
		// there is no suitable bitmap in the cache, create a new bitmap.
		typename TTraits::pointer resource = NULL;
		if (OpStatus::IsError(TTraits::CreateResource(&resource, cp)))
		{
			TTraits::ReleaseHashKey(tmp);
			return NULL;
		}
		// create the wrapper - ref count is 1
		res = OP_NEW(T, (resource));
		if (!res)
		{
			OP_DELETE(resource);
			TTraits::ReleaseHashKey(tmp);
			return NULL;
		}
		// if we are to exceed cache size, delete the first item.
		// ideally, we should delete the first of least referenced items...
		if (map_.GetCount() == MacCachedObjectFactory::kCacheSize)
		{
			T* tmpbitmap;
			OpHashIterator *it = map_.GetIterator();
			if (OpStatus::IsError(it->First()))
			{
				res->DecRef();
				TTraits::ReleaseHashKey(tmp);
				return NULL;
			}
			do {
				tmpbitmap = reinterpret_cast<T*>(const_cast<void*>(it->GetData()));
				if (tmpbitmap->IsLocked()) {
					tmpbitmap = NULL;
					continue;
				}
				else
					break;
			}
			while (OpStatus::IsSuccess(it->Next()));
			// all bitmaps are locked, we can't do anything.
			if (tmpbitmap == NULL) {
				OP_DELETE(it);
				res->DecRef();
				TTraits::ReleaseHashKey(tmp);
				return NULL;
			}

			char* key_to_delete = reinterpret_cast<char*>(const_cast<void*>(it->GetKey()));
			OP_DELETE(it);
			// remove the key-value pair from the hash table
			// this does not delete/free key or the value.
			map_.Remove(key_to_delete, (void**)&tmpbitmap);
			tmpbitmap->DecRef();
			TTraits::ReleaseHashKey(key_to_delete);
		}
		// add newly created bitmap into hash table
		if (OpStatus::IsError(map_.Add(tmp, res)))
		{
			TTraits::ReleaseHashKey(tmp);
			res->DecRef();
			return NULL;
		}
		if (cr)
			*cr = CreateResultNew;
	}
	else
	{
		TTraits::ReleaseHashKey(tmp);
		if (cr)
			*cr = CreateResultExact;
	}

	// add reference.
	if (res)
		res->AddRef();

	return res;
}

#endif // MAC_WIDGET_PAINTER_H
