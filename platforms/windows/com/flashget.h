

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0366 */
/* at Wed Feb 24 18:44:12 2010
 */
/* Compiler settings for flashget.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __flashget_h__
#define __flashget_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IFlashGetNetscape_FWD_DEFINED__
#define __IFlashGetNetscape_FWD_DEFINED__

#ifdef __cplusplus
typedef class IFlashGetNetscape IFlashGetNetscape;
#else
typedef struct IFlashGetNetscape IFlashGetNetscape;
#endif /* __cplusplus */

#endif 	/* __IFlashGetNetscape_FWD_DEFINED__ */


#ifndef __IIFlashGetNetscape_FWD_DEFINED__
#define __IIFlashGetNetscape_FWD_DEFINED__
typedef interface IIFlashGetNetscape IIFlashGetNetscape;
#endif 	/* __IIFlashGetNetscape_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 

#ifndef __IIFlashGetNetscape_INTERFACE_DEFINED__
#define __IIFlashGetNetscape_INTERFACE_DEFINED__

/* interface IIFlashGetNetscape */
/* [object][oleautomation][nonextensible][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IIFlashGetNetscape;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6DD9E779-2707-4BF0-8269-E4C6BD8B39B7")
    IIFlashGetNetscape : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddUrl( 
            /* [in] */ BSTR url,
            /* [in] */ BSTR text,
            /* [in] */ BSTR ref) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddAll( 
            /* [in] */ VARIANT *pVariant,
            /* [in] */ BSTR bRef,
            /* [in] */ BSTR modulename,
            /* [in] */ BSTR cookies,
            /* [in] */ BSTR flag) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddUrlEx( 
            /* [in] */ BSTR url,
            /* [in] */ BSTR text,
            /* [in] */ BSTR ref,
            /* [in] */ BSTR modulename,
            /* [in] */ BSTR cookies,
            /* [in] */ BSTR flag) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE FlvDetector( 
            BSTR url,
            /* [in] */ BSTR modulename,
            /* [in] */ BSTR flag) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddAllEx( 
            VARIANT *pVariant,
            BSTR ref,
            BSTR modulename,
            /* [in] */ BSTR cookies,
            long flag) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IIFlashGetNetscapeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IIFlashGetNetscape * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IIFlashGetNetscape * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IIFlashGetNetscape * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IIFlashGetNetscape * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IIFlashGetNetscape * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IIFlashGetNetscape * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IIFlashGetNetscape * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddUrl )( 
            IIFlashGetNetscape * This,
            /* [in] */ BSTR url,
            /* [in] */ BSTR text,
            /* [in] */ BSTR ref);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddAll )( 
            IIFlashGetNetscape * This,
            /* [in] */ VARIANT *pVariant,
            /* [in] */ BSTR bRef,
            /* [in] */ BSTR modulename,
            /* [in] */ BSTR cookies,
            /* [in] */ BSTR flag);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddUrlEx )( 
            IIFlashGetNetscape * This,
            /* [in] */ BSTR url,
            /* [in] */ BSTR text,
            /* [in] */ BSTR ref,
            /* [in] */ BSTR modulename,
            /* [in] */ BSTR cookies,
            /* [in] */ BSTR flag);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *FlvDetector )( 
            IIFlashGetNetscape * This,
            BSTR url,
            /* [in] */ BSTR modulename,
            /* [in] */ BSTR flag);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddAllEx )( 
            IIFlashGetNetscape * This,
            VARIANT *pVariant,
            BSTR ref,
            BSTR modulename,
            /* [in] */ BSTR cookies,
            long flag);
        
        END_INTERFACE
    } IIFlashGetNetscapeVtbl;

    interface IIFlashGetNetscape
    {
        CONST_VTBL struct IIFlashGetNetscapeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IIFlashGetNetscape_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IIFlashGetNetscape_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IIFlashGetNetscape_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IIFlashGetNetscape_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IIFlashGetNetscape_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IIFlashGetNetscape_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IIFlashGetNetscape_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IIFlashGetNetscape_AddUrl(This,url,text,ref)	\
    (This)->lpVtbl -> AddUrl(This,url,text,ref)

#define IIFlashGetNetscape_AddAll(This,pVariant,bRef,modulename,cookies,flag)	\
    (This)->lpVtbl -> AddAll(This,pVariant,bRef,modulename,cookies,flag)

#define IIFlashGetNetscape_AddUrlEx(This,url,text,ref,modulename,cookies,flag)	\
    (This)->lpVtbl -> AddUrlEx(This,url,text,ref,modulename,cookies,flag)

#define IIFlashGetNetscape_FlvDetector(This,url,modulename,flag)	\
    (This)->lpVtbl -> FlvDetector(This,url,modulename,flag)

#define IIFlashGetNetscape_AddAllEx(This,pVariant,ref,modulename,cookies,flag)	\
    (This)->lpVtbl -> AddAllEx(This,pVariant,ref,modulename,cookies,flag)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IIFlashGetNetscape_AddUrl_Proxy( 
    IIFlashGetNetscape * This,
    /* [in] */ BSTR url,
    /* [in] */ BSTR text,
    /* [in] */ BSTR ref);


void __RPC_STUB IIFlashGetNetscape_AddUrl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IIFlashGetNetscape_AddAll_Proxy( 
    IIFlashGetNetscape * This,
    /* [in] */ VARIANT *pVariant,
    /* [in] */ BSTR bRef,
    /* [in] */ BSTR modulename,
    /* [in] */ BSTR cookies,
    /* [in] */ BSTR flag);


void __RPC_STUB IIFlashGetNetscape_AddAll_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IIFlashGetNetscape_AddUrlEx_Proxy( 
    IIFlashGetNetscape * This,
    /* [in] */ BSTR url,
    /* [in] */ BSTR text,
    /* [in] */ BSTR ref,
    /* [in] */ BSTR modulename,
    /* [in] */ BSTR cookies,
    /* [in] */ BSTR flag);


void __RPC_STUB IIFlashGetNetscape_AddUrlEx_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IIFlashGetNetscape_FlvDetector_Proxy( 
    IIFlashGetNetscape * This,
    BSTR url,
    /* [in] */ BSTR modulename,
    /* [in] */ BSTR flag);


void __RPC_STUB IIFlashGetNetscape_FlvDetector_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IIFlashGetNetscape_AddAllEx_Proxy( 
    IIFlashGetNetscape * This,
    VARIANT *pVariant,
    BSTR ref,
    BSTR modulename,
    /* [in] */ BSTR cookies,
    long flag);


void __RPC_STUB IIFlashGetNetscape_AddAllEx_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IIFlashGetNetscape_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

unsigned long             __RPC_USER  VARIANT_UserSize(     unsigned long *, unsigned long            , VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserMarshal(  unsigned long *, unsigned char *, VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserUnmarshal(unsigned long *, unsigned char *, VARIANT * ); 
void                      __RPC_USER  VARIANT_UserFree(     unsigned long *, VARIANT * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


