

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0366 */
/* at Fri Feb 12 16:48:59 2010
 */
/* Compiler settings for qqdownaload.idl:
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

#ifndef __qqdownload_h__
#define __qqdownload_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __QQRightClick_FWD_DEFINED__
#define __QQRightClick_FWD_DEFINED__

#ifdef __cplusplus
typedef class QQRightClick QQRightClick;
#else
typedef struct QQRightClick QQRightClick;
#endif /* __cplusplus */

#endif 	/* __QQRightClick_FWD_DEFINED__ */


#ifndef __IQQRightClick_FWD_DEFINED__
#define __IQQRightClick_FWD_DEFINED__
typedef interface IQQRightClick IQQRightClick;
#endif 	/* __IQQRightClick_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 

#ifndef __IQQRightClick_INTERFACE_DEFINED__
#define __IQQRightClick_INTERFACE_DEFINED__

/* interface IQQRightClick */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IQQRightClick;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DCE3B5F0-6682-4C85-AE1F-272B0762E7FD")
    IQQRightClick : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SendUrl( 
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ BSTR lpStrCookie) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddTask( 
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SendMultiTask( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddCmnInfo( 
            /* [in] */ BSTR lpStrCookie) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetVersion( 
            /* [retval][out] */ BSTR *bstrVersion) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SendUrl2( 
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ BSTR lpStrCookie,
            /* [in] */ unsigned int unP2PRate,
            /* [in] */ unsigned int unCID) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddTask2( 
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ unsigned int unP2PRate) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SendUrl3( 
            BSTR lpStrUrl,
            BSTR lpStrRef,
            BSTR lpStrRemark,
            BSTR lpStrCookie,
            unsigned int unP2PRate,
            unsigned int unCID,
            BSTR lpFileSize,
            BSTR lpFileHash) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddTask5( 
            BSTR lpStrUrl,
            BSTR lpUnkown1,
            BSTR lpUnkown2,
            BSTR lpStrRemark,
            BSTR lpStrRef,
            int iUnknown1,
            int iUnknown2,
            int iUnknown3,
            BSTR lpStrCookie,
            BSTR lpUnkown3,
            BSTR lpUnkown4,
            int iUnknown4,
            BSTR lpUnkownn5,
            int iUnknown5) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE CommitTasks2( 
            int iUnknown1) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SendUrl4( 
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ BSTR lpStrCookie,
            /* [in] */ unsigned int unP2PRate,
            /* [in] */ unsigned int unCID,
            /* [in] */ BSTR lpFileSize,
            /* [in] */ BSTR lpFileHash,
            /* [in] */ BSTR lpStrFileName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddTask3( 
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ unsigned int unP2PRate,
            /* [in] */ BSTR lpStrFileName) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IQQRightClickVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IQQRightClick * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IQQRightClick * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IQQRightClick * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IQQRightClick * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IQQRightClick * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IQQRightClick * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IQQRightClick * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SendUrl )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ BSTR lpStrCookie);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddTask )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SendMultiTask )( 
            IQQRightClick * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddCmnInfo )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrCookie);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetVersion )( 
            IQQRightClick * This,
            /* [retval][out] */ BSTR *bstrVersion);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SendUrl2 )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ BSTR lpStrCookie,
            /* [in] */ unsigned int unP2PRate,
            /* [in] */ unsigned int unCID);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddTask2 )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ unsigned int unP2PRate);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SendUrl3 )( 
            IQQRightClick * This,
            BSTR lpStrUrl,
            BSTR lpStrRef,
            BSTR lpStrRemark,
            BSTR lpStrCookie,
            unsigned int unP2PRate,
            unsigned int unCID,
            BSTR lpFileSize,
            BSTR lpFileHash);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddTask5 )( 
            IQQRightClick * This,
            BSTR lpStrUrl,
            BSTR lpUnkown1,
            BSTR lpUnkown2,
            BSTR lpStrRemark,
            BSTR lpStrRef,
            int iUnknown1,
            int iUnknown2,
            int iUnknown3,
            BSTR lpStrCookie,
            BSTR lpUnkown3,
            BSTR lpUnkown4,
            int iUnknown4,
            BSTR lpUnkownn5,
            int iUnknown5);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *CommitTasks2 )( 
            IQQRightClick * This,
            int iUnknown1);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SendUrl4 )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ BSTR lpStrCookie,
            /* [in] */ unsigned int unP2PRate,
            /* [in] */ unsigned int unCID,
            /* [in] */ BSTR lpFileSize,
            /* [in] */ BSTR lpFileHash,
            /* [in] */ BSTR lpStrFileName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddTask3 )( 
            IQQRightClick * This,
            /* [in] */ BSTR lpStrUrl,
            /* [in] */ BSTR lpStrRef,
            /* [in] */ BSTR lpStrRemark,
            /* [in] */ unsigned int unP2PRate,
            /* [in] */ BSTR lpStrFileName);
        
        END_INTERFACE
    } IQQRightClickVtbl;

    interface IQQRightClick
    {
        CONST_VTBL struct IQQRightClickVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IQQRightClick_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IQQRightClick_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IQQRightClick_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IQQRightClick_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IQQRightClick_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IQQRightClick_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IQQRightClick_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IQQRightClick_SendUrl(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie)	\
    (This)->lpVtbl -> SendUrl(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie)

#define IQQRightClick_AddTask(This,lpStrUrl,lpStrRef,lpStrRemark)	\
    (This)->lpVtbl -> AddTask(This,lpStrUrl,lpStrRef,lpStrRemark)

#define IQQRightClick_SendMultiTask(This)	\
    (This)->lpVtbl -> SendMultiTask(This)

#define IQQRightClick_AddCmnInfo(This,lpStrCookie)	\
    (This)->lpVtbl -> AddCmnInfo(This,lpStrCookie)

#define IQQRightClick_GetVersion(This,bstrVersion)	\
    (This)->lpVtbl -> GetVersion(This,bstrVersion)

#define IQQRightClick_SendUrl2(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie,unP2PRate,unCID)	\
    (This)->lpVtbl -> SendUrl2(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie,unP2PRate,unCID)

#define IQQRightClick_AddTask2(This,lpStrUrl,lpStrRef,lpStrRemark,unP2PRate)	\
    (This)->lpVtbl -> AddTask2(This,lpStrUrl,lpStrRef,lpStrRemark,unP2PRate)

#define IQQRightClick_SendUrl3(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie,unP2PRate,unCID,lpFileSize,lpFileHash)	\
    (This)->lpVtbl -> SendUrl3(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie,unP2PRate,unCID,lpFileSize,lpFileHash)

#define IQQRightClick_AddTask5(This,lpStrUrl,lpUnkown1,lpUnkown2,lpStrRemark,lpStrRef,iUnknown1,iUnknown2,iUnknown3,lpStrCookie,lpUnkown3,lpUnkown4,iUnknown4,lpUnkownn5,iUnknown5)	\
    (This)->lpVtbl -> AddTask5(This,lpStrUrl,lpUnkown1,lpUnkown2,lpStrRemark,lpStrRef,iUnknown1,iUnknown2,iUnknown3,lpStrCookie,lpUnkown3,lpUnkown4,iUnknown4,lpUnkownn5,iUnknown5)

#define IQQRightClick_CommitTasks2(This,iUnknown1)	\
    (This)->lpVtbl -> CommitTasks2(This,iUnknown1)

#define IQQRightClick_SendUrl4(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie,unP2PRate,unCID,lpFileSize,lpFileHash,lpStrFileName)	\
    (This)->lpVtbl -> SendUrl4(This,lpStrUrl,lpStrRef,lpStrRemark,lpStrCookie,unP2PRate,unCID,lpFileSize,lpFileHash,lpStrFileName)

#define IQQRightClick_AddTask3(This,lpStrUrl,lpStrRef,lpStrRemark,unP2PRate,lpStrFileName)	\
    (This)->lpVtbl -> AddTask3(This,lpStrUrl,lpStrRef,lpStrRemark,unP2PRate,lpStrFileName)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_SendUrl_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrUrl,
    /* [in] */ BSTR lpStrRef,
    /* [in] */ BSTR lpStrRemark,
    /* [in] */ BSTR lpStrCookie);


void __RPC_STUB IQQRightClick_SendUrl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_AddTask_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrUrl,
    /* [in] */ BSTR lpStrRef,
    /* [in] */ BSTR lpStrRemark);


void __RPC_STUB IQQRightClick_AddTask_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_SendMultiTask_Proxy( 
    IQQRightClick * This);


void __RPC_STUB IQQRightClick_SendMultiTask_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_AddCmnInfo_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrCookie);


void __RPC_STUB IQQRightClick_AddCmnInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_GetVersion_Proxy( 
    IQQRightClick * This,
    /* [retval][out] */ BSTR *bstrVersion);


void __RPC_STUB IQQRightClick_GetVersion_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_SendUrl2_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrUrl,
    /* [in] */ BSTR lpStrRef,
    /* [in] */ BSTR lpStrRemark,
    /* [in] */ BSTR lpStrCookie,
    /* [in] */ unsigned int unP2PRate,
    /* [in] */ unsigned int unCID);


void __RPC_STUB IQQRightClick_SendUrl2_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_AddTask2_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrUrl,
    /* [in] */ BSTR lpStrRef,
    /* [in] */ BSTR lpStrRemark,
    /* [in] */ unsigned int unP2PRate);


void __RPC_STUB IQQRightClick_AddTask2_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_SendUrl3_Proxy( 
    IQQRightClick * This,
    BSTR lpStrUrl,
    BSTR lpStrRef,
    BSTR lpStrRemark,
    BSTR lpStrCookie,
    unsigned int unP2PRate,
    unsigned int unCID,
    BSTR lpFileSize,
    BSTR lpFileHash);


void __RPC_STUB IQQRightClick_SendUrl3_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_AddTask5_Proxy( 
    IQQRightClick * This,
    BSTR lpStrUrl,
    BSTR lpUnkown1,
    BSTR lpUnkown2,
    BSTR lpStrRemark,
    BSTR lpStrRef,
    int iUnknown1,
    int iUnknown2,
    int iUnknown3,
    BSTR lpStrCookie,
    BSTR lpUnkown3,
    BSTR lpUnkown4,
    int iUnknown4,
    BSTR lpUnkownn5,
    int iUnknown5);


void __RPC_STUB IQQRightClick_AddTask5_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_CommitTasks2_Proxy( 
    IQQRightClick * This,
    int iUnknown1);


void __RPC_STUB IQQRightClick_CommitTasks2_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_SendUrl4_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrUrl,
    /* [in] */ BSTR lpStrRef,
    /* [in] */ BSTR lpStrRemark,
    /* [in] */ BSTR lpStrCookie,
    /* [in] */ unsigned int unP2PRate,
    /* [in] */ unsigned int unCID,
    /* [in] */ BSTR lpFileSize,
    /* [in] */ BSTR lpFileHash,
    /* [in] */ BSTR lpStrFileName);


void __RPC_STUB IQQRightClick_SendUrl4_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IQQRightClick_AddTask3_Proxy( 
    IQQRightClick * This,
    /* [in] */ BSTR lpStrUrl,
    /* [in] */ BSTR lpStrRef,
    /* [in] */ BSTR lpStrRemark,
    /* [in] */ unsigned int unP2PRate,
    /* [in] */ BSTR lpStrFileName);


void __RPC_STUB IQQRightClick_AddTask3_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IQQRightClick_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


