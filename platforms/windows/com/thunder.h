

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0366 */
/* at Fri Feb 12 15:20:58 2010
 */
/* Compiler settings for thunder.idl:
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


#ifndef __thunder_h__
#define __thunder_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IAgent_FWD_DEFINED__
#define __IAgent_FWD_DEFINED__
typedef interface IAgent IAgent;
#endif 	/* __IAgent_FWD_DEFINED__ */


#ifndef __Agent_FWD_DEFINED__
#define __Agent_FWD_DEFINED__

#ifdef __cplusplus
typedef class Agent Agent;
#else
typedef struct Agent Agent;
#endif /* __cplusplus */

#endif 	/* __Agent_FWD_DEFINED__ */


#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 


#ifndef __THUNDERAGENTLib_LIBRARY_DEFINED__
#define __THUNDERAGENTLib_LIBRARY_DEFINED__

/* library THUNDERAGENTLib */
/* [custom][custom][helpstring][version][uuid] */ 



EXTERN_C const IID LIBID_THUNDERAGENTLib;

#ifndef __IAgent_INTERFACE_DEFINED__
#define __IAgent_INTERFACE_DEFINED__

/* interface IAgent */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IAgent;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1622F56A-0C55-464C-B472-377845DEF21D")
    IAgent : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetInfo( 
            /* [in] */ BSTR pInfoName,
            /* [retval][out] */ BSTR *ppResult) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AddTask( 
            /* [in] */ BSTR pURL,
            /* [defaultvalue][optional][in] */ BSTR pFileName = L"",
            /* [defaultvalue][optional][in] */ BSTR pPath = L"",
            /* [defaultvalue][optional][in] */ BSTR pComments = L"",
            /* [defaultvalue][optional][in] */ BSTR pReferURL = L"",
            /* [defaultvalue][optional][in] */ int nStartMode = -1,
            /* [defaultvalue][optional][in] */ int nOnlyFromOrigin = 0,
            /* [defaultvalue][optional][in] */ int nOriginThreadCount = -1) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE CommitTasks( 
            /* [retval][out] */ int *pResult) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE CancelTasks( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetTaskInfo( 
            /* [in] */ BSTR pURL,
            /* [in] */ BSTR pInfoName,
            /* [retval][out] */ BSTR *ppResult) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetInfoStruct( 
            /* [in] */ int pInfo) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetTaskInfoStruct( 
            /* [in] */ int pTaskInfo) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IAgentVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAgent * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAgent * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAgent * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IAgent * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IAgent * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IAgent * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IAgent * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetInfo )( 
            IAgent * This,
            /* [in] */ BSTR pInfoName,
            /* [retval][out] */ BSTR *ppResult);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AddTask )( 
            IAgent * This,
            /* [in] */ BSTR pURL,
            /* [defaultvalue][optional][in] */ BSTR pFileName,
            /* [defaultvalue][optional][in] */ BSTR pPath,
            /* [defaultvalue][optional][in] */ BSTR pComments,
            /* [defaultvalue][optional][in] */ BSTR pReferURL,
            /* [defaultvalue][optional][in] */ int nStartMode,
            /* [defaultvalue][optional][in] */ int nOnlyFromOrigin,
            /* [defaultvalue][optional][in] */ int nOriginThreadCount);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *CommitTasks )( 
            IAgent * This,
            /* [retval][out] */ int *pResult);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *CancelTasks )( 
            IAgent * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetTaskInfo )( 
            IAgent * This,
            /* [in] */ BSTR pURL,
            /* [in] */ BSTR pInfoName,
            /* [retval][out] */ BSTR *ppResult);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetInfoStruct )( 
            IAgent * This,
            /* [in] */ int pInfo);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetTaskInfoStruct )( 
            IAgent * This,
            /* [in] */ int pTaskInfo);
        
        END_INTERFACE
    } IAgentVtbl;

    interface IAgent
    {
        CONST_VTBL struct IAgentVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAgent_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IAgent_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IAgent_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IAgent_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IAgent_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IAgent_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IAgent_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IAgent_GetInfo(This,pInfoName,ppResult)	\
    (This)->lpVtbl -> GetInfo(This,pInfoName,ppResult)

#define IAgent_AddTask(This,pURL,pFileName,pPath,pComments,pReferURL,nStartMode,nOnlyFromOrigin,nOriginThreadCount)	\
    (This)->lpVtbl -> AddTask(This,pURL,pFileName,pPath,pComments,pReferURL,nStartMode,nOnlyFromOrigin,nOriginThreadCount)

#define IAgent_CommitTasks(This,pResult)	\
    (This)->lpVtbl -> CommitTasks(This,pResult)

#define IAgent_CancelTasks(This)	\
    (This)->lpVtbl -> CancelTasks(This)

#define IAgent_GetTaskInfo(This,pURL,pInfoName,ppResult)	\
    (This)->lpVtbl -> GetTaskInfo(This,pURL,pInfoName,ppResult)

#define IAgent_GetInfoStruct(This,pInfo)	\
    (This)->lpVtbl -> GetInfoStruct(This,pInfo)

#define IAgent_GetTaskInfoStruct(This,pTaskInfo)	\
    (This)->lpVtbl -> GetTaskInfoStruct(This,pTaskInfo)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_GetInfo_Proxy( 
    IAgent * This,
    /* [in] */ BSTR pInfoName,
    /* [retval][out] */ BSTR *ppResult);


void __RPC_STUB IAgent_GetInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_AddTask_Proxy( 
    IAgent * This,
    /* [in] */ BSTR pURL,
    /* [defaultvalue][optional][in] */ BSTR pFileName,
    /* [defaultvalue][optional][in] */ BSTR pPath,
    /* [defaultvalue][optional][in] */ BSTR pComments,
    /* [defaultvalue][optional][in] */ BSTR pReferURL,
    /* [defaultvalue][optional][in] */ int nStartMode,
    /* [defaultvalue][optional][in] */ int nOnlyFromOrigin,
    /* [defaultvalue][optional][in] */ int nOriginThreadCount);


void __RPC_STUB IAgent_AddTask_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_CommitTasks_Proxy( 
    IAgent * This,
    /* [retval][out] */ int *pResult);


void __RPC_STUB IAgent_CommitTasks_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_CancelTasks_Proxy( 
    IAgent * This);


void __RPC_STUB IAgent_CancelTasks_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_GetTaskInfo_Proxy( 
    IAgent * This,
    /* [in] */ BSTR pURL,
    /* [in] */ BSTR pInfoName,
    /* [retval][out] */ BSTR *ppResult);


void __RPC_STUB IAgent_GetTaskInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_GetInfoStruct_Proxy( 
    IAgent * This,
    /* [in] */ int pInfo);


void __RPC_STUB IAgent_GetInfoStruct_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IAgent_GetTaskInfoStruct_Proxy( 
    IAgent * This,
    /* [in] */ int pTaskInfo);


void __RPC_STUB IAgent_GetTaskInfoStruct_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IAgent_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_Agent;

#ifdef __cplusplus

class DECLSPEC_UUID("485463B7-8FB2-4B3B-B29B-8B919B0EACCE")
Agent;
#endif
#endif /* __THUNDERAGENTLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


