/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef COM_HELPERS_H
#define COM_HELPERS_H

//
// Releases a COM object and nullifies pointer.
//

template <typename InterfaceType>
inline void SafeRelease(InterfaceType** currentObject)
{
    if (*currentObject != NULL)
    {
        (*currentObject)->Release();
        *currentObject = NULL;
    }
}


///////////////////////////////////////////////////////////////
//
// Variants of the MS classes from ATL
//
// OpComPtr       CComPtr
// OpComBSTR      CComBSTR
// OpComVariant   CComVariant
//
// Short info for people w/out ATL knowledge:
// * OpComPtr provides the automatic reference counting for COM objects
//   removing the AddRef/Release methods from the operator->
// * OpComBSTR allows for automatic BSTR allocation/deallocation with
//   the OLE BSTR heap functions.
// * OpComVariant atomatically inits/clears VARIANTs. Also, supports
//   inplace VariantChangeType
// * When fresh/clean, objects of all three types can be safely used
//   as a last argument in all sorts of getters (that is, arguments
//   defined as IUnknown**, BSTR* and VARIANT*, respectively)
//

IUnknown* AtlComPtrAssign(IUnknown** pp, IUnknown* lp);
IUnknown* AtlComQIPtrAssign(IUnknown** pp, IUnknown* lp, REFIID riid);

template <class T>
class _NoAddRefReleaseOnOpComPtr : public T
{
private:
	STDMETHOD_(ULONG, AddRef)()=0;
	STDMETHOD_(ULONG, Release)()=0;
};

// OpComPtrBase provides the basis for all other smart pointers
// The other smartpointers add their own constructors and operators
template <class T>
class OpComPtrBase
{
protected:
	OpComPtrBase() throw()
	{
		p = NULL;
	}
	OpComPtrBase(__in int nNull) throw()
	{
		OP_ASSERT(nNull == 0);
		(void)nNull;
		p = NULL;
	}
	OpComPtrBase(__in_opt T* lp) throw()
	{
		p = lp;
		if (p != NULL)
			p->AddRef();
	}
public:
	typedef T _PtrClass;
	~OpComPtrBase() throw()
	{
		if (p)
			p->Release();
	}
	operator T*() const throw()
	{
		return p;
	}
	T& operator*() const
	{
		OP_ASSERT( p!= NULL);
		return *p;
	}
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the p member explicitly.
	T** operator&() throw()
	{
		OP_ASSERT(p == NULL);
		return &p;
	}
	_NoAddRefReleaseOnOpComPtr<T>* operator->() const throw()
	{
		OP_ASSERT(p!=NULL);
		return (_NoAddRefReleaseOnOpComPtr<T>*)p;
	}
	bool operator!() const throw()
	{
		return (p == NULL);
	}
	bool operator<(__in_opt T* pT) const throw()
	{
		return p < pT;
	}
	bool operator!=(__in_opt T* pT) const
	{
		return !operator==(pT);
	}
	bool operator==(__in_opt T* pT) const throw()
	{
		return p == pT;
	}

	// Release the interface and set to NULL
	void Release() throw()
	{
		T* pTemp = p;
		if (pTemp)
		{
			p = NULL;
			pTemp->Release();
		}
	}
	// Compare two objects for equivalence
	bool IsEqualObject(__in_opt IUnknown* pOther) throw()
	{
		if (p == NULL && pOther == NULL)
			return true;	// They are both NULL objects

		if (p == NULL || pOther == NULL)
			return false;	// One is NULL the other is not

		OpComPtr<IUnknown> punk1;
		OpComPtr<IUnknown> punk2;
		p->QueryInterface(__uuidof(IUnknown), (void**)&punk1);
		pOther->QueryInterface(__uuidof(IUnknown), (void**)&punk2);
		return punk1 == punk2;
	}
	// Attach to an existing interface (does not AddRef)
	void Attach(__in_opt T* p2) throw()
	{
		if (p)
			p->Release();
		p = p2;
	}
	// Detach the interface (does not Release)
	T* Detach() throw()
	{
		T* pt = p;
		p = NULL;
		return pt;
	}
	HRESULT CopyTo(__deref_out_opt T** ppT) throw()
	{
		OP_ASSERT(ppT != NULL);
		if (ppT == NULL)
			return E_POINTER;
		*ppT = p;
		if (p)
			p->AddRef();
		return S_OK;
	}
	HRESULT CoCreateInstance(__in REFCLSID rclsid, __in_opt LPUNKNOWN pUnkOuter = NULL, __in DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		OP_ASSERT(p == NULL);
		return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
	}
	HRESULT CoCreateInstance(__in LPCOLESTR szProgID, __in_opt LPUNKNOWN pUnkOuter = NULL, __in DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		CLSID clsid;
		HRESULT hr = CLSIDFromProgID(szProgID, &clsid);
		OP_ASSERT(p == NULL);
		if (SUCCEEDED(hr))
			hr = ::CoCreateInstance(clsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
		return hr;
	}
	template <class Q>
	HRESULT QueryInterface(__deref_out_opt Q** pp) const throw()
	{
		OP_ASSERT(pp != NULL);
		return p->QueryInterface(__uuidof(Q), (void**)pp);
	}
	T* p;
};

template <class T>
class OpComPtr : public OpComPtrBase<T>
{
public:
	OpComPtr() throw()
	{
	}
	OpComPtr(int nNull) throw() :
	OpComPtrBase<T>(nNull)
	{
	}
	OpComPtr(T* lp) throw() :
	OpComPtrBase<T>(lp)

	{
	}
	OpComPtr(__in const OpComPtr<T>& lp) throw() :
	OpComPtrBase<T>(lp.p)
	{
	}
	T* operator=(__in_opt T* lp) throw()
	{
		if(*this!=lp)
		{
			return static_cast<T*>(AtlComPtrAssign((IUnknown**)&p, lp));
		}
		return *this;
	}
	template <typename Q>
	T* operator=(__in const OpComPtr<Q>& lp) throw()
	{
		if( !IsEqualObject(lp) )
		{
			return static_cast<T*>(AtlComQIPtrAssign((IUnknown**)&p, lp, __uuidof(T)));
		}
		return *this;
	}
	T* operator=(__in const OpComPtr<T>& lp) throw()
	{
		if(*this!=lp)
		{
			return static_cast<T*>(AtlComPtrAssign((IUnknown**)&p, lp));
		}
		return *this;
	}
};

template <class T, const IID* piid = &__uuidof(T)>
class OpComQIPtr : 
	public OpComPtr<T>
{
public:
	OpComQIPtr() throw()
	{
	}
	OpComQIPtr(T* lp) throw() :
	OpComPtr<T>(lp)
	{
	}
	OpComQIPtr(const OpComQIPtr<T,piid>& lp) throw() :
	OpComPtr<T>(lp.p)
	{
	}
	OpComQIPtr(IUnknown* lp) throw()
	{
		if (lp != NULL)
			lp->QueryInterface(*piid, (void **)&p);
	}
	T* operator=(T* lp) throw()
	{
		if(*this!=lp)
		{
			return static_cast<T*>(AtlComPtrAssign((IUnknown**)&p, lp));
		}
		return *this;
	}
	T* operator=(const OpComQIPtr<T,piid>& lp) throw()
	{
		if(*this!=lp)
		{
			return static_cast<T*>(AtlComPtrAssign((IUnknown**)&p, lp.p));
		}
		return *this;
	}
	T* operator=(IUnknown* lp) throw()
	{
		if(*this!=lp)
		{
			return static_cast<T*>(AtlComQIPtrAssign((IUnknown**)&p, lp, *piid));
		}
		return *this;
	}
};

//Specialization to make it work
template<>
class OpComQIPtr<IUnknown, &IID_IUnknown> : 
	public OpComPtr<IUnknown>
{
public:
	OpComQIPtr() throw()
	{
	}
	OpComQIPtr(IUnknown* lp) throw()
	{
		//Actually do a QI to get identity
		if (lp != NULL)
			lp->QueryInterface(__uuidof(IUnknown), (void **)&p);
	}
	OpComQIPtr(_Inout_ const OpComQIPtr<IUnknown,&IID_IUnknown>& lp) throw() :
	OpComPtr<IUnknown>(lp.p)
	{
	}
	IUnknown* operator=(IUnknown* lp) throw()
	{
		if(*this!=lp)
		{
			//Actually do a QI to get identity
			return AtlComQIPtrAssign((IUnknown**)&p, lp, __uuidof(IUnknown));
		}
		return *this;
	}

	IUnknown* operator=(const OpComQIPtr<IUnknown,&IID_IUnknown>& lp) throw()
	{
		if(*this!=lp)
		{
			return AtlComPtrAssign((IUnknown**)&p, lp.p);
		}
		return *this;
	}
};



inline IUnknown* AtlComPtrAssign(__deref_out_opt IUnknown** pp, __in_opt IUnknown* lp)
{
	if (pp == NULL)
		return NULL;

	if (lp != NULL)
		lp->AddRef();
	if (*pp)
		(*pp)->Release();
	*pp = lp;
	return lp;
}

inline IUnknown* AtlComQIPtrAssign(__deref_out_opt IUnknown** pp, __in_opt IUnknown* lp, REFIID riid)
{
	if (pp == NULL)
		return NULL;

	IUnknown* pTemp = *pp;
	*pp = NULL;
	if (lp != NULL)
		lp->QueryInterface(riid, (void**)pp);
	if (pTemp)
		pTemp->Release();
	return *pp;
}

/////////////////////////////////////////////////////////////////////////////
// OpComBSTR

class OpComBSTR
{
public:
	BSTR m_str;

	OpComBSTR() throw()
	{
		m_str = NULL;
	}

#ifdef _OP_OpComBSTR_EXPLICIT_CONSTRUCTORS
	explicit OpComBSTR(_In_ int nSize)
#else
	OpComBSTR(_In_ int nSize)
#endif
	{
		if (nSize < 0)
		{
			nSize = 0;
		}
		
		if (nSize == 0)
		{
			m_str = NULL;
		}
		else
		{
			m_str = ::SysAllocStringLen(NULL, nSize);
		}
	}

	OpComBSTR(_In_ int nSize, _In_opt_count_(nSize) LPCOLESTR sz)
	{
		if (nSize < 0)
		{
			nSize = 0;
		}
		
		if (nSize == 0)
        {
			m_str = NULL;
        }
		else
		{
			m_str = ::SysAllocStringLen(sz, nSize);
		}
	}

	OpComBSTR(_In_opt_z_ LPCOLESTR pSrc)
	{
		if (pSrc == NULL)
        {
			m_str = NULL;
        }
		else
		{
			m_str = ::SysAllocString(pSrc);
		}
	}

	OpComBSTR(_In_ const OpComBSTR& src)
	{
		m_str = src.Copy();
	}	

	OpComBSTR(_In_ REFGUID guid)
	{
		OLECHAR szGUID[64];
		::StringFromGUID2(guid, szGUID, 64);
		m_str = ::SysAllocString(szGUID);
	}

	OpComBSTR& operator=(_In_ const OpComBSTR& src)
	{
		if (m_str != src.m_str)
		{
			::SysFreeString(m_str);
			m_str = src.Copy();
		}
		return *this;
	}

	OpComBSTR& operator=(_In_opt_z_ LPCOLESTR pSrc)
	{
		if (pSrc != m_str)
		{
			::SysFreeString(m_str);
			if (pSrc != NULL)
			{
				m_str = ::SysAllocString(pSrc);
			}
			else
            {
				m_str = NULL;
            }
		}
		return *this;
	}
	OpComBSTR(_Inout_ OpComBSTR&& src)
	{
		m_str = src.m_str;
		src.m_str = NULL;
	}
	
	OpComBSTR& operator=(_Inout_ OpComBSTR&& src)
	{
		if (m_str != src.m_str)
		{
			::SysFreeString(m_str);
			m_str = src.m_str;
			src.m_str = NULL;
		}
		return *this;
	}
	
	~OpComBSTR() throw();

	unsigned int Length() const throw()
	{
        return ::SysStringLen(m_str);
	}

	unsigned int ByteLength() const throw()
	{
        return ::SysStringByteLen(m_str);
	}

	operator BSTR() const throw()
	{
		return m_str;
	}


#ifndef OP_OpComBSTR_ADDRESS_OF_ASSERT
// Temp disable OpComBSTR::operator& Assert
#define OP_NO_OpComBSTR_ADDRESS_OF_ASSERT
#endif


	BSTR* operator&() throw()
	{
#ifndef OP_NO_OpComBSTR_ADDRESS_OF_ASSERT
		OP_ASSERT(!*this);
#endif
		return &m_str;
	}

	_Ret_opt_z_ BSTR Copy() const throw()
	{
		if (!*this)
		{
			return NULL;
		}
		else if (m_str != NULL)
		{
			return ::SysAllocStringByteLen((char*)m_str, ::SysStringByteLen(m_str));
		}
		else
		{
			return ::SysAllocStringByteLen(NULL, 0);
		}
	}

	_Check_return_ HRESULT CopyTo(_Deref_out_opt_z_ BSTR* pbstr) const throw()
	{
		OP_ASSERT(pbstr != NULL);
		if (pbstr == NULL)
        {
			return E_POINTER;
        }
		*pbstr = Copy();

		if ((*pbstr == NULL) && (m_str != NULL))
        {
			return E_OUTOFMEMORY;
        }
		return S_OK;
	}

	// copy BSTR to VARIANT
	_Check_return_ HRESULT CopyTo(_Out_ VARIANT *pvarDest) const throw()
	{
		OP_ASSERT(pvarDest != NULL);
		HRESULT hRes = E_POINTER;
		if (pvarDest != NULL)
		{
			pvarDest->vt = VT_BSTR;
			pvarDest->bstrVal = Copy();

			if (pvarDest->bstrVal == NULL && m_str != NULL)
            {
				hRes = E_OUTOFMEMORY;
            }
			else
            {
				hRes = S_OK;
            }
		}
		return hRes;
	}

	void Attach(_In_opt_z_ BSTR src) throw()
	{
		if (m_str != src)
		{
			::SysFreeString(m_str);
			m_str = src;
		}
	}

	_Ret_opt_z_ BSTR Detach() throw()
	{
		BSTR s = m_str;
		m_str = NULL;
		return s;
	}

	void Empty() throw()
	{
		::SysFreeString(m_str);
		m_str = NULL;
	}

	bool operator!() const throw()
	{
		return (m_str == NULL);
	}

	_Check_return_ HRESULT Append(_In_ const OpComBSTR& bstrSrc) throw()
	{
		return AppendBSTR(bstrSrc.m_str);
	}

	_Check_return_ HRESULT Append(_In_z_ LPCOLESTR lpsz) throw()
	{		
		return Append(lpsz, UINT(wcslen(lpsz)));
	}

	// a BSTR is just a LPCOLESTR so we need a special version to signify
	// that we are appending a BSTR
	_Check_return_ HRESULT AppendBSTR(_In_opt_z_ BSTR p) throw()
	{
        if (::SysStringLen(p) == 0)
        {
			return S_OK;
        }
		BSTR bstrNew = NULL;
		HRESULT hr;
		__analysis_assume(p);
		hr = VarBstrCat(m_str, p, &bstrNew);
		if (SUCCEEDED(hr))
		{
			::SysFreeString(m_str);
			m_str = bstrNew;
		}
		return hr;
	}

	_Check_return_ HRESULT Append(_In_opt_count_(nLen) LPCOLESTR lpsz, _In_ int nLen) throw()
	{
		if (lpsz == NULL || (m_str != NULL && nLen == 0))
		{
			return S_OK;
		}
		else if (nLen < 0)
		{
			return E_INVALIDARG;
		}
		
		const unsigned int n1 = Length();
		unsigned int n1Bytes = n1 * sizeof(OLECHAR);
		unsigned int nSize = n1 + nLen;
		
		BSTR b = ::SysAllocStringLen(NULL, nSize);
		if (b == NULL)
		{
			return E_OUTOFMEMORY;
		}
		
		if(::SysStringLen(m_str) > 0)
		{
			__analysis_assume(m_str); // ::SysStringLen(m_str) guarantees that m_str != NULL
			op_memcpy(b, m_str, n1Bytes);
		}
				
		op_memcpy(b+n1, lpsz, nLen*sizeof(OLECHAR));
		b[nSize] = '\0';
		SysFreeString(m_str);
		m_str = b;
		return S_OK;
	}

	_Check_return_ HRESULT Append(_In_ char ch) throw()
	{
		OLECHAR chO = ch;

		return( Append( &chO, 1 ) );
	}

	_Check_return_ HRESULT Append(_In_ wchar_t ch) throw()
	{
		return( Append( &ch, 1 ) );
	}

	_Check_return_ HRESULT AppendBytes(
		_In_opt_count_(nLen) const char* lpsz, 
		_In_ int nLen) throw()
	{
		if (lpsz == NULL || nLen == 0)
		{
			return S_OK;
		}
		else if (nLen < 0)
		{
			return E_INVALIDARG;
		}
				
		const unsigned int n1 = ByteLength();		
		unsigned int nSize = n1 + nLen;
						 
		BSTR b = ::SysAllocStringByteLen(NULL, nSize);
		if (b == NULL)
        {
			return E_OUTOFMEMORY;
        }
		
		op_memcpy(b, m_str, n1);
		op_memcpy(((char*)b) + n1, lpsz, nLen);
		
		*((OLECHAR*)(((char*)b) + nSize)) = '\0';
		SysFreeString(m_str);
		m_str = b;
		return S_OK;
	}

	_Check_return_ HRESULT AssignBSTR(_In_opt_z_ const BSTR bstrSrc) throw()
	{
		HRESULT hr = S_OK;
		if (m_str != bstrSrc)
		{
			::SysFreeString(m_str);
			if (bstrSrc != NULL)
			{
				m_str = ::SysAllocStringByteLen((char*)bstrSrc, ::SysStringByteLen(bstrSrc));
				if (!*this)
                {
					hr = E_OUTOFMEMORY;
                }
			}
			else
            {
				m_str = NULL;
            }
		}

		return hr;
	}

	_Check_return_ HRESULT ToLower() throw()
	{
		if (::SysStringLen(m_str) > 0)
		{
			// Convert in place
			CharLowerBuff(m_str, Length());
		}
		return S_OK;
	}
	_Check_return_ HRESULT ToUpper() throw()
	{
		if (::SysStringLen(m_str) > 0)
		{
			// Convert in place
			CharUpperBuff(m_str, Length());
		}
		return S_OK;
	}

	OpComBSTR& operator+=(_In_ const OpComBSTR& bstrSrc)
	{
		(void)AppendBSTR(bstrSrc.m_str);
		return *this;
	}

	OpComBSTR& operator+=(_In_z_ LPCOLESTR pszSrc)
	{
		(void)Append(pszSrc);
		return *this;
	}

	bool operator<(_In_ const OpComBSTR& bstrSrc) const throw()
	{
		return VarBstrCmp(m_str, bstrSrc.m_str, LOCALE_USER_DEFAULT, 0) == static_cast<HRESULT>(VARCMP_LT);
	}
	bool operator<(_In_z_ LPCOLESTR pszSrc) const
	{
		OpComBSTR bstr2(pszSrc);
		return operator<(bstr2);
	}
	bool operator<(_In_z_ LPOLESTR pszSrc) const
	{
		return operator<((LPCOLESTR)pszSrc);
	}

	bool operator>(_In_ const OpComBSTR& bstrSrc) const throw()
	{
		return VarBstrCmp(m_str, bstrSrc.m_str, LOCALE_USER_DEFAULT, 0) == static_cast<HRESULT>(VARCMP_GT);
	}
	bool operator>(_In_z_ LPCOLESTR pszSrc) const
	{
		OpComBSTR bstr2(pszSrc);
		return operator>(bstr2);
	}
	bool operator>(_In_z_ LPOLESTR pszSrc) const
	{
		return operator>((LPCOLESTR)pszSrc);
	}
	
	bool operator!=(_In_ const OpComBSTR& bstrSrc) const throw()
	{
		return !operator==(bstrSrc);
	}
	bool operator!=(_In_z_ LPCOLESTR pszSrc) const
	{
		return !operator==(pszSrc);
	}
	bool operator!=(_In_ int nNull) const throw()
	{
		return !operator==(nNull);
	}
	bool operator!=(_In_z_ LPOLESTR pszSrc) const
	{
		return operator!=((LPCOLESTR)pszSrc);
	}
	bool operator==(_In_ const OpComBSTR& bstrSrc) const throw()
	{
		return VarBstrCmp(m_str, bstrSrc.m_str, LOCALE_USER_DEFAULT, 0) == static_cast<HRESULT>(VARCMP_EQ);
	}
	bool operator==(LPCOLESTR pszSrc) const
	{
		OpComBSTR bstr2(pszSrc);
		return operator==(bstr2);
	}
	bool operator==(_In_z_ LPOLESTR pszSrc) const
	{
		return operator==((LPCOLESTR)pszSrc);
	}
	
	bool operator==(_In_ int nNull) const throw()
	{
		OP_ASSERT(nNull == 0);
		(void)nNull;
		return (!*this);
	}

	OpComBSTR(decltype(nullptr))
	{
		m_str = NULL;
	}
	bool operator==(decltype(nullptr)) const throw()
	{ 
		return *this == 0;
	}
	bool operator!=(decltype(nullptr)) const throw()
	{
		return *this != 0;
	}

	// each character in BSTR is copied to each element in SAFEARRAY
	HRESULT BSTRToArray(_Deref_out_ LPSAFEARRAY *ppArray) throw()
	{
		return VectorFromBstr(m_str, ppArray);
	}

	// first character of each element in SAFEARRAY is copied to BSTR
	_Check_return_ HRESULT ArrayToBSTR(_In_ const SAFEARRAY *pSrc) throw()
	{
		::SysFreeString(m_str);
		return BstrFromVector((LPSAFEARRAY)pSrc, &m_str);
	}
	static ULONG GetStreamSize(_In_opt_z_ BSTR bstr)
	{
		ULONG ulSize = sizeof(ULONG);
		if (bstr != NULL)
		{
			ulSize += SysStringByteLen(bstr) + sizeof(OLECHAR);			
		}		
		
		return ulSize;
	}
};

inline OpComBSTR::~OpComBSTR() throw()
	{
		::SysFreeString(m_str);
	}

inline void SysFreeStringHelper(_In_ OpComBSTR& bstr)
{
	bstr.Empty();
}

inline void SysFreeStringHelper(_In_opt_z_ BSTR bstr)
{
	::SysFreeString(bstr);
}

_Check_return_ inline HRESULT SysAllocStringHelper(
	_Out_ OpComBSTR& bstrDest,
	_In_opt_z_ BSTR bstrSrc)
{
	bstrDest=bstrSrc;
	return !bstrDest ? E_OUTOFMEMORY : S_OK;
}

_Check_return_ inline HRESULT SysAllocStringHelper(
	_Out_ BSTR& bstrDest,
	_In_opt_z_ BSTR bstrSrc)
{
	bstrDest=::SysAllocString(bstrSrc);

	return bstrDest==NULL ? E_OUTOFMEMORY : S_OK;
}
/////////////////////////////////////////////////////////////////////////////
// OpComVariant


#define OP_VARIANT_TRUE VARIANT_BOOL( -1 )
#define OP_VARIANT_FALSE VARIANT_BOOL( 0 )
#define OP_PREFAST_SUPPRESS(x) __pragma(warning(push)) __pragma(warning(disable: x))
#define OP_PREFAST_UNSUPPRESS() __pragma(warning(pop))

#ifdef _OP_DISABLE_DEPRECATED
#define OP_DEPRECATED(_Message)
#else
#define OP_DEPRECATED(_Message) __declspec( deprecated(_Message) )
#endif

template< typename T > 
class OpVarTypeInfo
{
//	static const VARTYPE VT;  // VARTYPE corresponding to type T
//	static T VARIANT::* const pmField;  // Pointer-to-member of corresponding field in VARIANT struct
};

template<>
class OpVarTypeInfo< char >
{
public:
#ifdef _CHAR_UNSIGNED
	ATLSTATIC_ASSERT(false, "OpVarTypeInfo< char > cannot be compiled with /J or _CHAR_UNSIGNED flag enabled");
#endif
	static const VARTYPE VT = VT_I1;
	static char VARIANT::* const pmField;
};

__declspec( selectany ) char VARIANT::* const OpVarTypeInfo< char >::pmField = &VARIANT::cVal;

template<>
class OpVarTypeInfo< unsigned char >
{
public:
	static const VARTYPE VT = VT_UI1;
	static unsigned char VARIANT::* const pmField;
};

__declspec( selectany ) unsigned char VARIANT::* const OpVarTypeInfo< unsigned char >::pmField = &VARIANT::bVal;

template<>
class OpVarTypeInfo< char* >
{
public:
#ifdef _CHAR_UNSIGNED
	ATLSTATIC_ASSERT(false, "OpVarTypeInfo< char* > cannot be compiled with /J or _CHAR_UNSIGNED flag enabled");
#endif
	static const VARTYPE VT = VT_I1|VT_BYREF;
	static char* VARIANT::* const pmField;
};

__declspec( selectany ) char* VARIANT::* const OpVarTypeInfo< char* >::pmField = &VARIANT::pcVal;

template<>
class OpVarTypeInfo< unsigned char* >
{
public:
	static const VARTYPE VT = VT_UI1|VT_BYREF;
	static unsigned char* VARIANT::* const pmField;
};

__declspec( selectany ) unsigned char* VARIANT::* const OpVarTypeInfo< unsigned char* >::pmField = &VARIANT::pbVal;

template<>
class OpVarTypeInfo< short >
{
public:
	static const VARTYPE VT = VT_I2;
	static short VARIANT::* const pmField;
};

__declspec( selectany ) short VARIANT::* const OpVarTypeInfo< short >::pmField = &VARIANT::iVal;

template<>
class OpVarTypeInfo< short* >
{
public:
	static const VARTYPE VT = VT_I2|VT_BYREF;
	static short* VARIANT::* const pmField;
};

__declspec( selectany ) short* VARIANT::* const OpVarTypeInfo< short* >::pmField = &VARIANT::piVal;

template<>
class OpVarTypeInfo< unsigned short >
{
public:
	static const VARTYPE VT = VT_UI2;
	static unsigned short VARIANT::* const pmField;
};

__declspec( selectany ) unsigned short VARIANT::* const OpVarTypeInfo< unsigned short >::pmField = &VARIANT::uiVal;

#ifdef _NATIVE_WCHAR_T_DEFINED  // Only treat unsigned short* as VT_UI2|VT_BYREF if BSTR isn't the same as unsigned short*
template<>
class OpVarTypeInfo< unsigned short* >
{
public:
	static const VARTYPE VT = VT_UI2|VT_BYREF;
	static unsigned short* VARIANT::* const pmField;
};

__declspec( selectany ) unsigned short* VARIANT::* const OpVarTypeInfo< unsigned short* >::pmField = &VARIANT::puiVal;
#endif  // _NATIVE_WCHAR_T_DEFINED

template<>
class OpVarTypeInfo< int >
{
public:
	static const VARTYPE VT = VT_I4;
	static int VARIANT::* const pmField;
};

__declspec( selectany ) int VARIANT::* const OpVarTypeInfo< int >::pmField = &VARIANT::intVal;

template<>
class OpVarTypeInfo< int* >
{
public:
	static const VARTYPE VT = VT_I4|VT_BYREF;
	static int* VARIANT::* const pmField;
};

__declspec( selectany ) int* VARIANT::* const OpVarTypeInfo< int* >::pmField = &VARIANT::pintVal;

template<>
class OpVarTypeInfo< unsigned int >
{
public:
	static const VARTYPE VT = VT_UI4;
	static unsigned int VARIANT::* const pmField;
};

__declspec( selectany ) unsigned int VARIANT::* const OpVarTypeInfo< unsigned int >::pmField = &VARIANT::uintVal;

template<>
class OpVarTypeInfo< unsigned int* >
{
public:
	static const VARTYPE VT = VT_UI4|VT_BYREF;
	static unsigned int* VARIANT::* const pmField;
};

__declspec( selectany ) unsigned int* VARIANT::* const OpVarTypeInfo< unsigned int* >::pmField = &VARIANT::puintVal;

template<>
class OpVarTypeInfo< long >
{
public:
	static const VARTYPE VT = VT_I4;
	static long VARIANT::* const pmField;
};

__declspec( selectany ) long VARIANT::* const OpVarTypeInfo< long >::pmField = &VARIANT::lVal;

template<>
class OpVarTypeInfo< long* >
{
public:
	static const VARTYPE VT = VT_I4|VT_BYREF;
	static long* VARIANT::* const pmField;
};

__declspec( selectany ) long* VARIANT::* const OpVarTypeInfo< long* >::pmField = &VARIANT::plVal;

template<>
class OpVarTypeInfo< unsigned long >
{
public:
	static const VARTYPE VT = VT_UI4;
	static unsigned long VARIANT::* const pmField;
};

__declspec( selectany ) unsigned long VARIANT::* const OpVarTypeInfo< unsigned long >::pmField = &VARIANT::ulVal;

template<>
class OpVarTypeInfo< unsigned long* >
{
public:
	static const VARTYPE VT = VT_UI4|VT_BYREF;
	static unsigned long* VARIANT::* const pmField;
};

__declspec( selectany ) unsigned long* VARIANT::* const OpVarTypeInfo< unsigned long* >::pmField = &VARIANT::pulVal;

template<>
class OpVarTypeInfo< __int64 >
{
public:
	static const VARTYPE VT = VT_I8;
	static __int64 VARIANT::* const pmField;
};

__declspec( selectany ) __int64 VARIANT::* const OpVarTypeInfo< __int64 >::pmField = &VARIANT::llVal;

template<>
class OpVarTypeInfo< __int64* >
{
public:
	static const VARTYPE VT = VT_I8|VT_BYREF;
	static __int64* VARIANT::* const pmField;
};

__declspec( selectany ) __int64* VARIANT::* const OpVarTypeInfo< __int64* >::pmField = &VARIANT::pllVal;

template<>
class OpVarTypeInfo< unsigned __int64 >
{
public:
	static const VARTYPE VT = VT_UI8;
	static unsigned __int64 VARIANT::* const pmField;
};

__declspec( selectany ) unsigned __int64 VARIANT::* const OpVarTypeInfo< unsigned __int64 >::pmField = &VARIANT::ullVal;

template<>
class OpVarTypeInfo< unsigned __int64* >
{
public:
	static const VARTYPE VT = VT_UI8|VT_BYREF;
	static unsigned __int64* VARIANT::* const pmField;
};

__declspec( selectany ) unsigned __int64* VARIANT::* const OpVarTypeInfo< unsigned __int64* >::pmField = &VARIANT::pullVal;

template<>
class OpVarTypeInfo< float >
{
public:
	static const VARTYPE VT = VT_R4;
	static float VARIANT::* const pmField;
};

__declspec( selectany ) float VARIANT::* const OpVarTypeInfo< float >::pmField = &VARIANT::fltVal;

template<>
class OpVarTypeInfo< float* >
{
public:
	static const VARTYPE VT = VT_R4|VT_BYREF;
	static float* VARIANT::* const pmField;
};

__declspec( selectany ) float* VARIANT::* const OpVarTypeInfo< float* >::pmField = &VARIANT::pfltVal;

template<>
class OpVarTypeInfo< double >
{
public:
	static const VARTYPE VT = VT_R8;
	static double VARIANT::* const pmField;
};

__declspec( selectany ) double VARIANT::* const OpVarTypeInfo< double >::pmField = &VARIANT::dblVal;

template<>
class OpVarTypeInfo< double* >
{
public:
	static const VARTYPE VT = VT_R8|VT_BYREF;
	static double* VARIANT::* const pmField;
};

__declspec( selectany ) double* VARIANT::* const OpVarTypeInfo< double* >::pmField = &VARIANT::pdblVal;

template<>

class OpVarTypeInfo< VARIANT* >
{
public:
	static const VARTYPE VT = VT_VARIANT|VT_BYREF;
};

template<>
class OpVarTypeInfo< BSTR >
{
public:
	static const VARTYPE VT = VT_BSTR;
	static BSTR VARIANT::* const pmField;
};

__declspec( selectany ) BSTR VARIANT::* const OpVarTypeInfo< BSTR >::pmField = &VARIANT::bstrVal;

template<>
class OpVarTypeInfo< BSTR* >
{
public:
	static const VARTYPE VT = VT_BSTR|VT_BYREF;
	static BSTR* VARIANT::* const pmField;
};

__declspec( selectany ) BSTR* VARIANT::* const OpVarTypeInfo< BSTR* >::pmField = &VARIANT::pbstrVal;

template<>
class OpVarTypeInfo< IUnknown* >
{
public:
	static const VARTYPE VT = VT_UNKNOWN;
	static IUnknown* VARIANT::* const pmField;
};

__declspec( selectany ) IUnknown* VARIANT::* const OpVarTypeInfo< IUnknown* >::pmField = &VARIANT::punkVal;

template<>
class OpVarTypeInfo< IUnknown** >
{
public:
	static const VARTYPE VT = VT_UNKNOWN|VT_BYREF;
	static IUnknown** VARIANT::* const pmField;
};

__declspec( selectany ) IUnknown** VARIANT::* const OpVarTypeInfo< IUnknown** >::pmField = &VARIANT::ppunkVal;

template<>
class OpVarTypeInfo< IDispatch* >
{
public:
	static const VARTYPE VT = VT_DISPATCH;
	static IDispatch* VARIANT::* const pmField;
};

__declspec( selectany ) IDispatch* VARIANT::* const OpVarTypeInfo< IDispatch* >::pmField = &VARIANT::pdispVal;

template<>
class OpVarTypeInfo< IDispatch** >
{
public:
	static const VARTYPE VT = VT_DISPATCH|VT_BYREF;
	static IDispatch** VARIANT::* const pmField;
};

__declspec( selectany ) IDispatch** VARIANT::* const OpVarTypeInfo< IDispatch** >::pmField = &VARIANT::ppdispVal;

template<>
class OpVarTypeInfo< CY >
{
public:
	static const VARTYPE VT = VT_CY;
	static CY VARIANT::* const pmField;
};

__declspec( selectany ) CY VARIANT::* const OpVarTypeInfo< CY >::pmField = &VARIANT::cyVal;

template<>
class OpVarTypeInfo< CY* >
{
public:
	static const VARTYPE VT = VT_CY|VT_BYREF;
	static CY* VARIANT::* const pmField;
};

__declspec( selectany ) CY* VARIANT::* const OpVarTypeInfo< CY* >::pmField = &VARIANT::pcyVal;

class OpComVariant : 
	public tagVARIANT
{
// Constructors
public:
	OpComVariant() throw()
	{
		::VariantInit(this);
	}
	~OpComVariant() throw()
	{
		HRESULT hr = Clear();
		OP_ASSERT(SUCCEEDED(hr));
		(hr);
	}
	OpComVariant(_In_ const VARIANT& varSrc) throw()
	{
		vt = VT_EMPTY;
		InternalCopy(&varSrc);
	}
	OpComVariant(_In_ const OpComVariant& varSrc) throw()
	{
		vt = VT_EMPTY;
		InternalCopy(&varSrc);
	}
	OpComVariant(_In_z_ LPCOLESTR lpszSrc) throw()
	{
		vt = VT_EMPTY;
		*this = lpszSrc;
	}
	OpComVariant(_In_ bool bSrc) throw()
	{
		vt = VT_BOOL;
		boolVal = bSrc ? OP_VARIANT_TRUE : OP_VARIANT_FALSE;
	}

	OpComVariant(_In_ int nSrc, _In_ VARTYPE vtSrc = VT_I4) throw()
	{
		OP_ASSERT(vtSrc == VT_I4 || vtSrc == VT_INT);
		if (vtSrc == VT_I4 || vtSrc == VT_INT)
		{
			vt = vtSrc;
			intVal = nSrc;
		}
		else
		{
			vt = VT_ERROR;
			scode = E_INVALIDARG;
		}
	}

	OpComVariant(_In_ BYTE nSrc) throw()
	{
		vt = VT_UI1;
		bVal = nSrc;
	}
	OpComVariant(_In_ short nSrc) throw()
	{
		vt = VT_I2;
		iVal = nSrc;
	}
	OpComVariant(_In_ long nSrc, _In_ VARTYPE vtSrc = VT_I4) throw()
	{
		OP_ASSERT(vtSrc == VT_I4 || vtSrc == VT_ERROR);
		if (vtSrc == VT_I4 || vtSrc == VT_ERROR)
		{
			vt = vtSrc;
			lVal = nSrc;
		}
		else
		{
			vt = VT_ERROR;
			scode = E_INVALIDARG;
		}
	}

	OpComVariant(_In_ float fltSrc) throw()
	{
		vt = VT_R4;
		fltVal = fltSrc;
	}
	OpComVariant(_In_ double dblSrc, _In_ VARTYPE vtSrc = VT_R8) throw()
	{
		OP_ASSERT(vtSrc == VT_R8 || vtSrc == VT_DATE);
		if (vtSrc == VT_R8 || vtSrc == VT_DATE)
		{
			vt = vtSrc;
			dblVal = dblSrc;
		}
		else
		{
			vt = VT_ERROR;
			scode = E_INVALIDARG;
		}
	}

#if (_WIN32_WINNT >= 0x0501) || defined(_OP_SUPPORT_VT_I8)
	OpComVariant(_In_ LONGLONG nSrc) throw()
	{
		vt = VT_I8;
		llVal = nSrc;
	}
	OpComVariant(_In_ ULONGLONG nSrc) throw()
	{
		vt = VT_UI8;
		ullVal = nSrc;
	}
#endif
	OpComVariant(_In_ CY cySrc) throw()
	{
		vt = VT_CY;
		cyVal.Hi = cySrc.Hi;
		cyVal.Lo = cySrc.Lo;
	}
	OpComVariant(_In_opt_ IDispatch* pSrc) throw()
	{
		vt = VT_DISPATCH;
		pdispVal = pSrc;
		// Need to AddRef as VariantClear will Release
		if (pdispVal != NULL)
			pdispVal->AddRef();
	}
	OpComVariant(_In_opt_ IUnknown* pSrc) throw()
	{
		vt = VT_UNKNOWN;
		punkVal = pSrc;
		// Need to AddRef as VariantClear will Release
		if (punkVal != NULL)
			punkVal->AddRef();
	}
	OpComVariant(_In_ char cSrc) throw()
	{
		vt = VT_I1;
		cVal = cSrc;
	}
	OpComVariant(_In_ unsigned short nSrc) throw()
	{
		vt = VT_UI2;
		uiVal = nSrc;
	}
	OpComVariant(_In_ unsigned long nSrc) throw()
	{
		vt = VT_UI4;
		ulVal = nSrc;
	}
	OpComVariant(_In_ unsigned int nSrc, _In_ VARTYPE vtSrc = VT_UI4) throw()
	{
		OP_ASSERT(vtSrc == VT_UI4 || vtSrc == VT_UINT);
		if (vtSrc == VT_UI4 || vtSrc == VT_UINT)
		{
			vt = vtSrc;
			uintVal= nSrc;
		}
		else
		{
			vt = VT_ERROR;
			scode = E_INVALIDARG;
		}		
	}
	OpComVariant(_In_ const OpComBSTR& bstrSrc) throw()
	{
		vt = VT_EMPTY;
		*this = bstrSrc;
	}
// Assignment Operators
public:
	OpComVariant& operator=(_In_ const OpComVariant& varSrc) throw()
	{
        if(this!=&varSrc)
        {
		    InternalCopy(&varSrc);
        }
		return *this;
	}
	OpComVariant& operator=(_In_ const VARIANT& varSrc) throw()
	{
        if(static_cast<VARIANT *>(this)!=&varSrc)
        {
		    InternalCopy(&varSrc);
        }
		return *this;
	}

	OpComVariant& operator=(_In_ const OpComBSTR& bstrSrc) throw()
	{
		Clear();

		vt = VT_BSTR;
		bstrVal = bstrSrc.Copy();

		if (bstrVal == NULL && bstrSrc.m_str != NULL)
		{
			vt = VT_ERROR;
			scode = E_OUTOFMEMORY;
		}

		return *this;
	}

	OpComVariant& operator=(_In_z_ LPCOLESTR lpszSrc) throw()
	{
		Clear();

		vt = VT_BSTR;
		bstrVal = ::SysAllocString(lpszSrc);

		if (bstrVal == NULL && lpszSrc != NULL)
		{
			vt = VT_ERROR;
			scode = E_OUTOFMEMORY;
		}
		return *this;
	}

	OpComVariant& operator=(_In_ bool bSrc) throw()
	{
		if (vt != VT_BOOL)
		{
			Clear();
			vt = VT_BOOL;
		}
		boolVal = bSrc ? OP_VARIANT_TRUE : OP_VARIANT_FALSE;
		return *this;
	}

	OpComVariant& operator=(_In_ int nSrc) throw()
	{
		if (vt != VT_I4)
		{
			Clear();
			vt = VT_I4;
		}
		intVal = nSrc;

		return *this;
	}

	OpComVariant& operator=(_In_ BYTE nSrc) throw()
	{
		if (vt != VT_UI1)
		{
			Clear();
			vt = VT_UI1;
		}
		bVal = nSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ short nSrc) throw()
	{
		if (vt != VT_I2)
		{
			Clear();
			vt = VT_I2;
		}
		iVal = nSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ long nSrc) throw()
	{
		if (vt != VT_I4)
		{
			Clear();
			vt = VT_I4;
		}
		lVal = nSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ float fltSrc) throw()
	{
		if (vt != VT_R4)
		{
			Clear();
			vt = VT_R4;
		}
		fltVal = fltSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ double dblSrc) throw()
	{
		if (vt != VT_R8)
		{
			Clear();
			vt = VT_R8;
		}
		dblVal = dblSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ CY cySrc) throw()
	{
		if (vt != VT_CY)
		{
			Clear();
			vt = VT_CY;
		}
		cyVal.Hi = cySrc.Hi;
		cyVal.Lo = cySrc.Lo;
		return *this;
	}

	OpComVariant& operator=(_Inout_opt_ IDispatch* pSrc) throw()
	{
		Clear();
		
		vt = VT_DISPATCH;
		pdispVal = pSrc;
		// Need to AddRef as VariantClear will Release
		if (pdispVal != NULL)
			pdispVal->AddRef();
		return *this;
	}

	OpComVariant& operator=(_Inout_opt_ IUnknown* pSrc) throw()
	{
		Clear();
		
		vt = VT_UNKNOWN;
		punkVal = pSrc;

		// Need to AddRef as VariantClear will Release
		if (punkVal != NULL)
			punkVal->AddRef();
		return *this;
	}

	OpComVariant& operator=(_In_ char cSrc) throw()
	{
		if (vt != VT_I1)
		{
			Clear();
			vt = VT_I1;
		}
		cVal = cSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ unsigned short nSrc) throw()
	{
		if (vt != VT_UI2)
		{
			Clear();
			vt = VT_UI2;
		}
		uiVal = nSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ unsigned long nSrc) throw()
	{
		if (vt != VT_UI4)
		{
			Clear();
			vt = VT_UI4;
		}
		ulVal = nSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ unsigned int nSrc) throw()
	{
		if (vt != VT_UI4)
		{
			Clear();
			vt = VT_UI4;
		}
		uintVal= nSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ BYTE* pbSrc) throw()
	{
		if (vt != (VT_UI1|VT_BYREF))
		{
			Clear();
			vt = VT_UI1|VT_BYREF;
		}
		pbVal = pbSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ short* pnSrc) throw()
	{
		if (vt != (VT_I2|VT_BYREF))
		{
			Clear();
			vt = VT_I2|VT_BYREF;
		}
		piVal = pnSrc;
		return *this;
	}

#ifdef _NATIVE_WCHAR_T_DEFINED
	OpComVariant& operator=(_In_ USHORT* pnSrc) throw()
	{
		if (vt != (VT_UI2|VT_BYREF))
		{
			Clear();
			vt = VT_UI2|VT_BYREF;
		}
		puiVal = pnSrc;
		return *this;
	}
#endif

	OpComVariant& operator=(_In_ int* pnSrc) throw()
	{
		if (vt != (VT_I4|VT_BYREF))
		{
			Clear();			
			vt = VT_I4|VT_BYREF;
		}
		pintVal = pnSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ UINT* pnSrc) throw()
	{
		if (vt != (VT_UI4|VT_BYREF))
		{
			Clear();
			vt = VT_UI4|VT_BYREF;
		}
		puintVal = pnSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ long* pnSrc) throw()
	{
		if (vt != (VT_I4|VT_BYREF))
		{
			Clear();
			vt = VT_I4|VT_BYREF;
		}
		plVal = pnSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ ULONG* pnSrc) throw()
	{
		if (vt != (VT_UI4|VT_BYREF))
		{
			Clear();
			vt = VT_UI4|VT_BYREF;
		}
		pulVal = pnSrc;
		return *this;
	}

#if (_WIN32_WINNT >= 0x0501) || defined(_OP_SUPPORT_VT_I8)
	OpComVariant& operator=(_In_ LONGLONG nSrc) throw()
	{
		if (vt != VT_I8)
		{
			Clear();
			vt = VT_I8;
		}
		llVal = nSrc;

		return *this;
	}

	OpComVariant& operator=(_In_ LONGLONG* pnSrc) throw()
	{
		if (vt != (VT_I8|VT_BYREF))
		{
			Clear();
			vt = VT_I8|VT_BYREF;
		}
		pllVal = pnSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ ULONGLONG nSrc) throw()
	{
		if (vt != VT_UI8)
		{
			Clear();
			vt = VT_UI8;
		}
		ullVal = nSrc;

		return *this;
	}

	OpComVariant& operator=(_In_ ULONGLONG* pnSrc) throw()
	{
		if (vt != (VT_UI8|VT_BYREF))
		{
			Clear();
			vt = VT_UI8|VT_BYREF;
		}
		pullVal = pnSrc;
		return *this;
	}
#endif

	OpComVariant& operator=(_In_ float* pfSrc) throw()
	{
		if (vt != (VT_R4|VT_BYREF))
		{
			Clear();
			vt = VT_R4|VT_BYREF;
		}
		pfltVal = pfSrc;
		return *this;
	}

	OpComVariant& operator=(_In_ double* pfSrc) throw()
	{
		if (vt != (VT_R8|VT_BYREF))
		{
			Clear();
			vt = VT_R8|VT_BYREF;
		}
		pdblVal = pfSrc;
		return *this;
	}

// Comparison Operators
public:
	bool operator==(_In_ const VARIANT& varSrc) const throw()
	{
		// For backwards compatibility
		if (vt == VT_NULL && varSrc.vt == VT_NULL)
		{
			return true;
		}
		// Variants not equal if types don't match
		if (vt != varSrc.vt)
		{
			return false;
		}
		return VarCmp((VARIANT*)this, (VARIANT*)&varSrc, LOCALE_USER_DEFAULT, 0) == static_cast<HRESULT>(VARCMP_EQ);
	}

	bool operator!=(_In_ const VARIANT& varSrc) const throw()
	{
		return !operator==(varSrc);
	}

	bool operator<(_In_ const VARIANT& varSrc) const throw()
	{
		if (vt == VT_NULL && varSrc.vt == VT_NULL)
			return false;
		return VarCmp((VARIANT*)this, (VARIANT*)&varSrc, LOCALE_USER_DEFAULT, 0)== static_cast<HRESULT>(VARCMP_LT);
	}

	bool operator>(_In_ const VARIANT& varSrc) const throw()
	{
		if (vt == VT_NULL && varSrc.vt == VT_NULL)
			return false;
		return VarCmp((VARIANT*)this, (VARIANT*)&varSrc, LOCALE_USER_DEFAULT, 0)== static_cast<HRESULT>(VARCMP_GT);
	}

private:
	inline HRESULT VarCmp(
		_In_ LPVARIANT pvarLeft, 
		_In_ LPVARIANT pvarRight, 
		_In_ LCID lcid, 
		_In_ ULONG dwFlags) const throw();

// Operations
public:
	HRESULT Clear()
	{ 
		return ::VariantClear(this); 
	}	
	HRESULT Copy(_In_ const VARIANT* pSrc)
	{ 
		return ::VariantCopy(this, const_cast<VARIANT*>(pSrc)); 
	}
	
OP_PREFAST_SUPPRESS(6387)
	// copy VARIANT to BSTR
	HRESULT CopyTo(_Deref_out_z_ BSTR *pstrDest) const
	{
		OP_ASSERT(pstrDest != NULL && vt == VT_BSTR);
		HRESULT hRes = E_POINTER;
		if (pstrDest != NULL && vt == VT_BSTR)
		{
			*pstrDest = ::SysAllocStringByteLen((char*)bstrVal, ::SysStringByteLen(bstrVal));
			if (*pstrDest == NULL)
				hRes = E_OUTOFMEMORY;
			else
				hRes = S_OK;
		}
		else if (vt != VT_BSTR)
			hRes = DISP_E_TYPEMISMATCH;
				
		return hRes;
	}
OP_PREFAST_UNSUPPRESS()
	
	HRESULT Attach(_In_ VARIANT* pSrc)
	{
		if(pSrc == NULL)
			return E_INVALIDARG;
			
		// Clear out the variant
		HRESULT hr = Clear();
		if (SUCCEEDED(hr))
		{
			// Copy the contents and give control to OpComVariant
			op_memcpy(this, pSrc, sizeof(VARIANT));
			pSrc->vt = VT_EMPTY;
			hr = S_OK;
		}
		return hr;
	}

	HRESULT Detach(_Out_ VARIANT* pDest)
	{
		OP_ASSERT(pDest != NULL);
		if(pDest == NULL)
			return E_POINTER;
			
		// Clear out the variant
		HRESULT hr = ::VariantClear(pDest);
		if (SUCCEEDED(hr))
		{
			// Copy the contents and remove control from OpComVariant
			op_memcpy(pDest, this, sizeof(VARIANT));
			vt = VT_EMPTY;
			hr = S_OK;
		}
		return hr;
	}

	HRESULT ChangeType(_In_ VARTYPE vtNew, _In_opt_ const VARIANT* pSrc = NULL)
	{
		VARIANT* pVar = const_cast<VARIANT*>(pSrc);
		// Convert in place if pSrc is NULL
		if (pVar == NULL)
			pVar = this;
		// Do nothing if doing in place convert and vts not different
		return ::VariantChangeType(this, pVar, 0, vtNew);
	}

	template< typename T >
	void SetByRef(_In_ T* pT) throw()
	{
		Clear();
		vt = OpVarTypeInfo< T* >::VT;
		byref = pT;
	}

	// Return the size in bytes of the current contents
	ULONG GetSize() const;
	HRESULT GetSizeMax(_Out_ ULARGE_INTEGER* pcbSize) const;

// Implementation
public:
	_Check_return_ HRESULT InternalClear() throw()
	{
		HRESULT hr = Clear();		
		OP_ASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			vt = VT_ERROR;
			scode = hr;
		}
		return hr;
	}

	void InternalCopy(_In_ const VARIANT* pSrc) throw()
	{
		HRESULT hr = Copy(pSrc);
		if (FAILED(hr))
		{
			vt = VT_ERROR;
			scode = hr;
		}
	}
};

inline HRESULT OpComVariant::GetSizeMax(_Out_ ULARGE_INTEGER* pcbSize) const
{
	OP_ASSERT(pcbSize != NULL);
	if (pcbSize == NULL)
	{
		return E_INVALIDARG;
	}
	
	HRESULT hr = S_OK;
	ULARGE_INTEGER nSize;
	nSize.QuadPart = sizeof(VARTYPE);	
	
	switch (vt)
	{
	case VT_UNKNOWN:
	case VT_DISPATCH:
		{	
			nSize.LowPart += sizeof(CLSID);
			
			if (punkVal != NULL)
			{
				OpComPtr<IPersistStream> spStream;
				
				hr = punkVal->QueryInterface(__uuidof(IPersistStream), (void**)&spStream);
				if (FAILED(hr))
				{
					hr = punkVal->QueryInterface(__uuidof(IPersistStreamInit), (void**)&spStream);
					if (FAILED(hr))
					{
						break;
					}
				}
				
				ULARGE_INTEGER nPersistSize;
				nPersistSize.QuadPart = 0;
				
				OP_ASSERT(spStream != NULL);
				hr = spStream->GetSizeMax(&nPersistSize);				
				if (SUCCEEDED(hr))
				{
					nSize.QuadPart += nPersistSize.QuadPart;
				}				
			}			
		}
		break;
	case VT_UI1:
	case VT_I1:
		nSize.LowPart += sizeof(BYTE);
		break;
	case VT_I2:
	case VT_UI2:
	case VT_BOOL:
		nSize.LowPart += sizeof(short);
		break;
	case VT_I4:
	case VT_UI4:
	case VT_R4:
	case VT_INT:
	case VT_UINT:
	case VT_ERROR:
		nSize.LowPart += sizeof(long);
		break;
	case VT_I8:
	case VT_UI8:
		nSize.LowPart += sizeof(LONGLONG);
		break;
	case VT_R8:
	case VT_CY:
	case VT_DATE:
		nSize.LowPart += sizeof(double);
		break;
	default:
		{
			VARTYPE vtTmp = vt;
			BSTR bstr = NULL;
			OpComVariant varBSTR;
			if (vtTmp != VT_BSTR)
			{
				hr = VariantChangeType(&varBSTR, const_cast<VARIANT*>((const VARIANT*)this), VARIANT_NOVALUEPROP, VT_BSTR);
				if (SUCCEEDED(hr))
				{
					bstr = varBSTR.bstrVal;
					vtTmp = VT_BSTR;
				}
			} 
			else
			{
				bstr = bstrVal;
			}

			if (vtTmp == VT_BSTR)
			{
				// Add the size of the length + string (in bytes) + NULL terminator.				
				nSize.QuadPart += OpComBSTR::GetStreamSize(bstr);
			}
		}		
	}
	
	if (SUCCEEDED(hr))
	{
		pcbSize->QuadPart = nSize.QuadPart;
	}
	
	return hr;
}

inline OP_DEPRECATED("GetSize has been replaced by GetSizeMax")
ULONG OpComVariant::GetSize() const
{
	ULARGE_INTEGER nSize;
	HRESULT hr = GetSizeMax(&nSize);
	
	if (SUCCEEDED(hr) && nSize.QuadPart <= ULONG_MAX)
	{
		return nSize.LowPart;	
	}
	
	return sizeof(VARTYPE);
}

/*
	Workaround for VarCmp function which does not compare VT_I1, VT_UI2, VT_UI4, VT_UI8 values
*/
inline HRESULT OpComVariant::VarCmp(
	_In_ LPVARIANT pvarLeft, 
	_In_ LPVARIANT pvarRight, 
	_In_ LCID lcid, 
	_In_ ULONG dwFlags) const throw()
{			
	switch(vt) 
	{
		case VT_I1:
			if (pvarLeft->cVal == pvarRight->cVal)
			{
				return VARCMP_EQ;
			}
			return pvarLeft->cVal > pvarRight->cVal ? VARCMP_GT : VARCMP_LT;			
		case VT_UI2:
			if (pvarLeft->uiVal == pvarRight->uiVal)
			{
				return VARCMP_EQ;
			}
			return pvarLeft->uiVal > pvarRight->uiVal ? VARCMP_GT : VARCMP_LT;

		case VT_UI4:
			if (pvarLeft->uintVal == pvarRight->uintVal) 
			{
				return VARCMP_EQ;
			}
			return pvarLeft->uintVal > pvarRight->uintVal ? VARCMP_GT : VARCMP_LT;				

		case VT_UI8:
			if (pvarLeft->ullVal == pvarRight->ullVal)
			{
				return VARCMP_EQ;
			}
			return pvarLeft->ullVal > pvarRight->ullVal ? VARCMP_GT : VARCMP_LT;

		default:
			return ::VarCmp(pvarLeft, pvarRight, lcid, dwFlags);
	}
}

// Helper macros for classes wanting to use OpComObject and OpComObjectEx
#define DECLARE_TYPE_MAP() inline HRESULT InternalQueryInterface(REFIID type, void** ptr);
#define BEGIN_TYPE_MAP() HRESULT InternalQueryInterface(REFIID type, void** ptr) { if (!ptr) return E_POINTER; *ptr = NULL; \
	if (type == __uuidof(IUnknown)) { *ptr = GetBaseObject(); } else
#define BEGIN_TYPE_MAP_DECL(_type) HRESULT _type::InternalQueryInterface(REFIID type, void** ptr) { if (!ptr) return E_POINTER; *ptr = NULL; \
	if (type == __uuidof(IUnknown)) { *ptr = GetBaseObject(); } else
#define TYPE_ENTRY(T) if (type == __uuidof(T)) { *ptr = (T*)this; } else
#define TYPE_ENTRY2(T, T2) if (type == __uuidof(T)::id()) { *ptr = (T*)(T2*)this; } else
//#define END_TYPE_MAP() {} if (*ptr) GetBaseObject()->AddRef(); if (!*ptr) _asm {int 3}; return *ptr != NULL ? S_OK : E_NOINTERFACE; }
#define END_TYPE_MAP() {} if (*ptr) GetBaseObject()->AddRef(); return *ptr != NULL ? S_OK : E_NOINTERFACE; }


#define SIMPLE_BASE_OBJECT() virtual IUnknown* GetBaseObject() const { return (IUnknown*)this; }
#define SIMPLE_BASE_OBJECT2(T2) virtual IUnknown* GetBaseObject() const { return (IUnknown*)(T2*)this; }

#define OLE(x) do { hr = (x); if (FAILED(hr)) return hr; } while(0)
#define OLE_(x) do { HRESULT hr = (x); if (FAILED(hr)) return hr; } while(0)

/** Implementation of IUnknown and facility for object creation for co-classes w/default ctor.
  * Classes compatible with this class should implement default constructor, InternalQueryInterface,
  * GetBaseObject and FinalCreate. The InternalQueryInterface and GetBaseObject can be implemented
  * with DECLARE_TYPE_MAP/BEGIN_TYPE_MAP_DECL/BEGIN_TYPE_MAP, TYPE_ENTRY/TYPE_ENTRY2, END_TYPE_MAP and
  * SIMPLE_BASE_OBJECT/SIMPLE_BASE_OBJECT2 macros.
  *
  * The clases should be created using properly typed Create static method, e.g.
  * 
  * class Implementation: public Interface { ... };
  * ...
  * OpComPtr<Interface> obj;
  * OLE( OpComObject< Implementation >::Create(&obj) );
  */
template <class Impl>
struct OpComObject: Impl
{
	volatile LONG counter;
	OpComObject() : counter(0) {} 

	STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&counter); }
	STDMETHODIMP_(ULONG) Release()
	{
		LONG ret = InterlockedDecrement(&counter);
		if (!ret) OP_DELETE(this);
		return ret;
	}
	STDMETHODIMP QueryInterface(REFIID type, void** ptr) { return Impl::InternalQueryInterface(type, ptr); }
	template <class T> STDMETHODIMP QueryInterface(T** ptr)	{ return QueryInterface(__uuidof(T), (void**)ptr); }

	template <class Q>
	static HRESULT Create(Q** out)
	{
		return Create(__uuidof(Q), (LPVOID*)out);
	}
	static HRESULT Create(REFIID type, void** ppout)
	{
		if (!ppout) return E_POINTER;
		*ppout = NULL;
		Impl* ptr = OP_NEW(OpComObject<Impl>, ());
		if (!ptr) return E_OUTOFMEMORY;
		OpComPtr<IUnknown> r = ptr->GetBaseObject();
		HRESULT hr = ptr->FinalCreate();
		if (FAILED(hr)) return hr;
		return r->QueryInterface(type, ppout);
	};
};

/** Implementation of IUnknown and facility for object creation for co-classes w/out default ctor.
  * Classes compatible with this class should implement a constructor taking single argument and
  * define a type alias Init for the type of that argument. Also, as with OpComObject, compatible
  * class should also implement InternalQueryInterface, GetBaseObject and FinalCreate.
  * The InternalQueryInterface and GetBaseObject can be implemented with DECLARE_TYPE_MAP/
  * BEGIN_TYPE_MAP_DECL/BEGIN_TYPE_MAP, TYPE_ENTRY/TYPE_ENTRY2, END_TYPE_MAP and
  * SIMPLE_BASE_OBJECT/SIMPLE_BASE_OBJECT2 macros.
  *
  * The clases should be created using properly typed Create static method, e.g.
  * 
  * class Implementation: public Interface
  * {
  * public:
  *   typedef X Init;
  *   Implementation(const X& x) ...
  *   ...
  * };
  * ...
  * OpComPtr<Interface> obj;
  * OLE( OpComObjectEx< Implementation >::Create( X(), &obj) );
  */

template <class Impl>
struct OpComObjectEx: Impl
{
	typedef typename Impl::Init Init;

	volatile LONG counter;
	OpComObjectEx(const Init& init) : Impl(init), counter(0) {} 

	STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&counter); }
	STDMETHODIMP_(ULONG) Release()
	{
		LONG ret = InterlockedDecrement(&counter);
		if (!ret) OP_DELETE(this);
		return ret;
	}
	STDMETHODIMP QueryInterface(REFIID type, void** ptr) { return Impl::InternalQueryInterface(type, ptr); }
	template <class T> STDMETHODIMP QueryInterface(T** ptr)	{ return QueryInterface(__uuidof(T), (void**)ptr); }

	template <class Q>
	static HRESULT Create(const Init& init, Q** out)
	{
		return Create(init, __uuidof(Q), (LPVOID*)out);
	}
	static HRESULT Create(const Init& init, REFIID type, void** ppout)
	{
		if (!ppout) return E_POINTER;
		*ppout = NULL;
		Impl* ptr = OP_NEW(OpComObjectEx<Impl>, (init));
		if (!ptr) return E_OUTOFMEMORY;
		OpComPtr<IUnknown> r = ptr->GetBaseObject();
		HRESULT hr = ptr->FinalCreate();
		if (FAILED(hr)) return hr;
		return r->QueryInterface(type, ppout);
	};
};

#endif // COM_HELPERS_H
