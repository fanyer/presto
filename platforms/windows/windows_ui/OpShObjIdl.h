/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
/*	This is a copy of a part of ShObjIdl.h from the Windows SDK.
	It contains the interfaces needed for using Default applications
	in Vista and taskbar extensions in Windows 7 */
#ifndef WINDOWS_SHOBJIDL_INCLUDED
#define WINDOWS_SHOBJIDL_INCLUDED

#ifndef PROPERTYKEY_DEFINED
#define PROPERTYKEY_DEFINED
typedef struct _tagpropertykey
{
	GUID fmtid;
	DWORD pid;
} 	PROPERTYKEY;

#endif

#ifndef REFPROPERTYKEY
#ifdef __cplusplus
#define REFPROPERTYKEY const PROPERTYKEY &
#else // !__cplusplus
#define REFPROPERTYKEY const PROPERTYKEY * __MIDL_CONST
#endif // __cplusplus
#endif //REFPROPERTYKEY

#ifndef _REFPROPVARIANT_DEFINED
#define _REFPROPVARIANT_DEFINED
#ifdef __cplusplus
#define REFPROPVARIANT const PROPVARIANT &
#else
#define REFPROPVARIANT const PROPVARIANT * __MIDL_CONST
#endif
#endif

#ifndef DEFINE_PROPERTYKEY
#define DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) \
	const PROPERTYKEY name \
	= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 }, pid }
#endif // DEFINE_PROPERTYKEY

DEFINE_PROPERTYKEY(PKEY_Title, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 2);

typedef /* [v1_enum] */ 
enum tagASSOCIATIONLEVEL
    {	AL_MACHINE	= 0,
	AL_EFFECTIVE	= ( AL_MACHINE + 1 ) ,
	AL_USER	= ( AL_EFFECTIVE + 1 ) 
    } 	ASSOCIATIONLEVEL;

typedef /* [v1_enum] */ 
enum tagASSOCIATIONTYPE
    {	AT_FILEEXTENSION	= 0,
	AT_URLPROTOCOL	= ( AT_FILEEXTENSION + 1 ) ,
	AT_STARTMENUCLIENT	= ( AT_URLPROTOCOL + 1 ) ,
	AT_MIMETYPE	= ( AT_STARTMENUCLIENT + 1 ) 
    } 	ASSOCIATIONTYPE;

#ifndef __IApplicationAssociationRegistration_FWD_DEFINED__
#define __IApplicationAssociationRegistration_FWD_DEFINED__
typedef interface IApplicationAssociationRegistration IApplicationAssociationRegistration;
#endif 	/* __IApplicationAssociationRegistration_FWD_DEFINED__ */


#ifndef __IApplicationAssociationRegistrationUI_FWD_DEFINED__
#define __IApplicationAssociationRegistrationUI_FWD_DEFINED__
typedef interface IApplicationAssociationRegistrationUI IApplicationAssociationRegistrationUI;
#endif 	/* __IApplicationAssociationRegistrationUI_FWD_DEFINED__ */

#ifndef __ApplicationAssociationRegistration_FWD_DEFINED__
#define __ApplicationAssociationRegistration_FWD_DEFINED__

#ifdef __cplusplus
typedef class ApplicationAssociationRegistration ApplicationAssociationRegistration;
#else
typedef struct ApplicationAssociationRegistration ApplicationAssociationRegistration;
#endif /* __cplusplus */

#endif 	/* __ApplicationAssociationRegistration_FWD_DEFINED__ */


#ifndef __ApplicationAssociationRegistrationUI_FWD_DEFINED__
#define __ApplicationAssociationRegistrationUI_FWD_DEFINED__

#ifdef __cplusplus
typedef class ApplicationAssociationRegistrationUI ApplicationAssociationRegistrationUI;
#else
typedef struct ApplicationAssociationRegistrationUI ApplicationAssociationRegistrationUI;
#endif /* __cplusplus */

#endif 	/* __ApplicationAssociationRegistrationUI_FWD_DEFINED__ */


#if _MSC_VER < 1500
static const CLSID CLSID_ApplicationAssociationRegistration = {0x591209c7, 0x767b, 0x42b2, {0x9f, 0xba, 0x44, 0xee, 0x46, 0x15, 0xf2, 0xc7}};
#endif

#ifdef __cplusplus

class DECLSPEC_UUID("591209c7-767b-42b2-9fba-44ee4615f2c7")
ApplicationAssociationRegistration;
#endif

#if _MSC_VER < 1500
static const CLSID CLSID_ApplicationAssociationRegistrationUI = {0x1968106d, 0xf3b5, 0x44cf, {0x89, 0x0e, 0x11, 0x6f, 0xcb, 0x9e, 0xce, 0xf1}};
#endif

#ifdef __cplusplus

class DECLSPEC_UUID("1968106d-f3b5-44cf-890e-116fcb9ecef1")
ApplicationAssociationRegistrationUI;
#endif


#ifndef __IApplicationAssociationRegistration_INTERFACE_DEFINED__
#define __IApplicationAssociationRegistration_INTERFACE_DEFINED__

/* interface IApplicationAssociationRegistration */
/* [helpstring][unique][uuid][object] */ 


EXTERN_C const IID IID_IApplicationAssociationRegistration;

    MIDL_INTERFACE("4e530b0a-e611-4c77-a3ac-9031d022281b")
    IApplicationAssociationRegistration : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE QueryCurrentDefault( 
            /* [string][in] */ LPCWSTR pszQuery,
            /* [in] */ ASSOCIATIONTYPE atQueryType,
            /* [in] */ ASSOCIATIONLEVEL alQueryLevel,
            /* [string][out] */ LPWSTR *ppszAssociation) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE QueryAppIsDefault( 
            /* [string][in] */ LPCWSTR pszQuery,
            /* [in] */ ASSOCIATIONTYPE atQueryType,
            /* [in] */ ASSOCIATIONLEVEL alQueryLevel,
            /* [string][in] */ LPCWSTR pszAppRegistryName,
            /* [out] */ BOOL *pfDefault) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE QueryAppIsDefaultAll( 
            /* [in] */ ASSOCIATIONLEVEL alQueryLevel,
            /* [string][in] */ LPCWSTR pszAppRegistryName,
            /* [out] */ BOOL *pfDefault) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetAppAsDefault( 
            /* [string][in] */ LPCWSTR pszAppRegistryName,
            /* [string][in] */ LPCWSTR pszSet,
            /* [in] */ ASSOCIATIONTYPE atSetType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetAppAsDefaultAll( 
            /* [string][in] */ LPCWSTR pszAppRegistryName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ClearUserAssociations( void) = 0;
        
    };
    
#endif 	/* __IApplicationAssociationRegistration_INTERFACE_DEFINED__ */


#ifndef __IApplicationAssociationRegistrationUI_INTERFACE_DEFINED__
#define __IApplicationAssociationRegistrationUI_INTERFACE_DEFINED__

/* interface IApplicationAssociationRegistrationUI */
/* [helpstring][unique][uuid][object] */ 


EXTERN_C const IID IID_IApplicationAssociationRegistrationUI;

    MIDL_INTERFACE("1f76a169-f994-40ac-8fc8-0959e8874710")
    IApplicationAssociationRegistrationUI : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE LaunchAdvancedAssociationUI( 
            /* [string][in] */ LPCWSTR pszAppRegistryName) = 0;
        
    };
    
#endif 	/* __IApplicationAssociationRegistrationUI_INTERFACE_DEFINED__ */

#ifndef __ITaskbarList3_FWD_DEFINED__
#define __ITaskbarList3_FWD_DEFINED__
	typedef interface ITaskbarList3 ITaskbarList3;
#endif 	/* __ITaskbarList3_FWD_DEFINED__ */

#ifndef __ITaskbarList3_INTERFACE_DEFINED__
#define __ITaskbarList3_INTERFACE_DEFINED__

	/* interface ITaskbarList3 */
	/* [object][uuid] */ 

	typedef /* [v1_enum] */ 
		enum TBPFLAG
	{	TBPF_NOPROGRESS	= 0,
	TBPF_INDETERMINATE	= 0x1,
	TBPF_NORMAL	= 0x2,
	TBPF_ERROR	= 0x4,
	TBPF_PAUSED	= 0x8
	} 	TBPFLAG;

		typedef /* [v1_enum] */ 
		enum TBATFLAG
	{	TBATF_USEMDITHUMBNAIL	= 0x1,
	TBATF_USEMDILIVEPREVIEW	= 0x2
	} 	TBATFLAG;

#include <pshpack8.h>
	typedef struct tagTHUMBBUTTON
	{
		DWORD dwMask;
		UINT iId;
		UINT iBitmap;
		HICON hIcon;
		WCHAR szTip[ 260 ];
		DWORD dwFlags;
	} 	THUMBBUTTON;

	typedef struct tagTHUMBBUTTON *LPTHUMBBUTTON;

	EXTERN_C const IID IID_ITaskbarList3;

	MIDL_INTERFACE("ea1afb91-9e28-4b86-90e9-9e9f8a5eefaf")
ITaskbarList3 : public ITaskbarList2
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE SetProgressValue( 
			/* [in] */ HWND hwnd,
			/* [in] */ ULONGLONG ullCompleted,
			/* [in] */ ULONGLONG ullTotal) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetProgressState( 
			/* [in] */ HWND hwnd,
			/* [in] */ TBPFLAG tbpFlags) = 0;

		virtual HRESULT STDMETHODCALLTYPE RegisterTab( 
			/* [in] */ HWND hwndTab,
			/* [in] */ HWND hwndMDI) = 0;

		virtual HRESULT STDMETHODCALLTYPE UnregisterTab( 
			/* [in] */ HWND hwndTab) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetTabOrder( 
			/* [in] */ HWND hwndTab,
			/* [in] */ HWND hwndInsertBefore) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetTabActive( 
			/* [in] */ HWND hwndTab,
			/* [in] */ HWND hwndMDI,
			/* [in] */ TBATFLAG tbatFlags) = 0;

		virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons( 
			/* [in] */ HWND hwnd,
			/* [in] */ UINT cButtons,
			/* [size_is][in] */ LPTHUMBBUTTON pButton) = 0;

		virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons( 
			/* [in] */ HWND hwnd,
			/* [in] */ UINT cButtons,
			/* [size_is][in] */ LPTHUMBBUTTON pButton) = 0;

		virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList( 
			/* [in] */ HWND hwnd,
			/* [in] */ HIMAGELIST himl) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon( 
			/* [in] */ HWND hwnd,
			/* [in] */ HICON hIcon,
			/* [string][in] */ LPCWSTR pszDescription) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetThumbnailTooltip( 
			/* [in] */ HWND hwnd,
			/* [string][in] */ LPCWSTR pszTip) = 0;

		virtual HRESULT STDMETHODCALLTYPE SetThumbnailClip( 
			/* [in] */ HWND hwnd,
			/* [in] */ RECT *prcClip) = 0;

	};

#endif 	/* __ITaskbarList3_INTERFACE_DEFINED__ */

#ifndef __IObjectArray_FWD_DEFINED__
#define __IObjectArray_FWD_DEFINED__
	typedef interface IObjectArray IObjectArray;
#endif 	/* __IObjectArray_FWD_DEFINED__ */


#ifndef __IObjectArray_INTERFACE_DEFINED__
#define __IObjectArray_INTERFACE_DEFINED__

	/* interface IObjectArray */
	/* [unique][object][uuid][helpstring] */ 


	EXTERN_C const IID IID_IObjectArray;

	MIDL_INTERFACE("92CA9DCD-5622-4bba-A805-5E9F541BD8C9")
IObjectArray : public IUnknown
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE GetCount( 
			/* [out] */ UINT *pcObjects) = 0;

		virtual HRESULT STDMETHODCALLTYPE GetAt( 
			/* [in] */ UINT uiIndex,
			/* [in] */ REFIID riid,
			/* [iid_is][out] */ void **ppv) = 0;

	};

#endif 	/* __IObjectArray_INTERFACE_DEFINED__ */

#ifndef __IObjectCollection_FWD_DEFINED__
#define __IObjectCollection_FWD_DEFINED__
	typedef interface IObjectCollection IObjectCollection;

	MIDL_INTERFACE("5632b1a4-e38a-400a-928a-d4cd63230295")
IObjectCollection : public IObjectArray
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE AddObject( 
			/* [in] */ IUnknown *punk) = 0;

		virtual HRESULT STDMETHODCALLTYPE AddFromArray( 
			/* [in] */ IObjectArray *poaSource) = 0;

		virtual HRESULT STDMETHODCALLTYPE RemoveObjectAt( 
			/* [in] */ UINT uiIndex) = 0;

		virtual HRESULT STDMETHODCALLTYPE Clear( void) = 0;

	};
#endif 	/* __IObjectCollection_FWD_DEFINED__ */

#if _MSC_VER < 1600
static const CLSID CLSID_DestinationList = {0x77f10cf0, 0x3db5, 0x4966, {0xb5, 0x20, 0xb7, 0xc5, 0x4f, 0xd3, 0x5e, 0xd6}};
static const CLSID CLSID_ApplicationDestinations = {0x86c14003, 0x4d6b, 0x4ef3, {0xa7, 0xb4, 0x05, 0x06, 0x66, 0x3b, 0x2e, 0x68}};
static const CLSID CLSID_EnumerableObjectCollection = {0x2d3468c1, 0x36a7, 0x43b6, {0xac, 0x24, 0xd3, 0xf0, 0x2f, 0xd9, 0x60, 0x7a}};
#endif // _MSC_VER < 1600

#ifndef __ICustomDestinationList_FWD_DEFINED__
#define __ICustomDestinationList_FWD_DEFINED__
typedef interface ICustomDestinationList ICustomDestinationList;
#endif 	/* __ICustomDestinationList_FWD_DEFINED__ */


#ifndef __IApplicationDestinations_FWD_DEFINED__
#define __IApplicationDestinations_FWD_DEFINED__
typedef interface IApplicationDestinations IApplicationDestinations;
#endif 	/* __IApplicationDestinations_FWD_DEFINED__ */

#ifndef __DestinationList_FWD_DEFINED__
#define __DestinationList_FWD_DEFINED__

#ifdef __cplusplus
typedef class DestinationList DestinationList;
#else
typedef struct DestinationList DestinationList;
#endif /* __cplusplus */

#endif 	/* __DestinationList_FWD_DEFINED__ */


#ifndef __ApplicationDestinations_FWD_DEFINED__
#define __ApplicationDestinations_FWD_DEFINED__

#ifdef __cplusplus
typedef class ApplicationDestinations ApplicationDestinations;
#else
typedef struct ApplicationDestinations ApplicationDestinations;
#endif /* __cplusplus */

#endif 	/* __ApplicationDestinations_FWD_DEFINED__ */

#ifndef __ICustomDestinationList_INTERFACE_DEFINED__
#define __ICustomDestinationList_INTERFACE_DEFINED__

/* interface ICustomDestinationList */
/* [unique][object][uuid] */ 

typedef /* [v1_enum] */ 
enum tagKNOWNDESTCATEGORY
{	KDC_FREQUENT	= 1,
KDC_RECENT	= ( KDC_FREQUENT + 1 ) 
} 	KNOWNDESTCATEGORY;

EXTERN_C const IID IID_ICustomDestinationList;

MIDL_INTERFACE("6332debf-87b5-4670-90c0-5e57b408a49e")
ICustomDestinationList : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetAppID( 
		/* [string][in] */ LPCWSTR pszAppID) = 0;

	virtual HRESULT STDMETHODCALLTYPE BeginList( 
		/* [out] */ UINT *pcMaxSlots,
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void **ppv) = 0;

	virtual HRESULT STDMETHODCALLTYPE AppendCategory( 
		/* [string][in] */ LPCWSTR pszCategory,
		/* [in] */ IObjectArray *poa) = 0;

	virtual HRESULT STDMETHODCALLTYPE AppendKnownCategory( 
		/* [in] */ KNOWNDESTCATEGORY category) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddUserTasks( 
		/* [in] */ IObjectArray *poa) = 0;

	virtual HRESULT STDMETHODCALLTYPE CommitList( void) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetRemovedDestinations( 
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void **ppv) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteList( 
		/* [string][in] */ LPCWSTR pszAppID) = 0;

	virtual HRESULT STDMETHODCALLTYPE AbortList( void) = 0;

};
#endif // __ICustomDestinationList_INTERFACE_DEFINED__

#ifndef __IApplicationDestinations_INTERFACE_DEFINED__
#define __IApplicationDestinations_INTERFACE_DEFINED__
/* interface IApplicationDestinations */
/* [unique][object][uuid] */ 

EXTERN_C const IID IID_IApplicationDestinations;

MIDL_INTERFACE("12337d35-94c6-48a0-bce7-6a9c69d4d600")
IApplicationDestinations : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetAppID( 
		/* [in] */ LPCWSTR pszAppID) = 0;

	virtual HRESULT STDMETHODCALLTYPE RemoveDestination( 
		/* [in] */ IUnknown *punk) = 0;

	virtual HRESULT STDMETHODCALLTYPE RemoveAllDestinations( void) = 0;

};
#endif // __IApplicationDestinations_INTERFACE_DEFINED__

#ifndef __IPropertyStore_FWD_DEFINED__
#define __IPropertyStore_FWD_DEFINED__
typedef interface IPropertyStore IPropertyStore;
#endif 	/* __IPropertyStore_FWD_DEFINED__ */

#ifndef __IPropertyStore_INTERFACE_DEFINED__
#define __IPropertyStore_INTERFACE_DEFINED__

/* interface IPropertyStore */
/* [unique][object][helpstring][uuid] */ 


EXTERN_C const IID IID_IPropertyStore;

MIDL_INTERFACE("886d8eeb-8cf2-4446-8d02-cdba1dbdcf99")
IPropertyStore : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetCount( 
		/* [out] */ DWORD *cProps) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetAt( 
		/* [in] */ DWORD iProp,
		/* [out] */ PROPERTYKEY *pkey) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetValue( 
		/* [in] */ REFPROPERTYKEY key,
		/* [out] */ PROPVARIANT *pv) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetValue( 
		/* [in] */ REFPROPERTYKEY key,
		/* [in] */ REFPROPVARIANT propvar) = 0;

	virtual HRESULT STDMETHODCALLTYPE Commit( void) = 0;

};
#endif // __IPropertyStore_INTERFACE_DEFINED__

#ifndef __IFileDialog_INTERFACE_DEFINED__
#define __IFileDialog_INTERFACE_DEFINED__

/* interface IFileDialog */
/* [unique][object][uuid] */ 

/* [v1_enum] */ 
enum _FILEOPENDIALOGOPTIONS
{	FOS_OVERWRITEPROMPT	= 0x2,
FOS_STRICTFILETYPES	= 0x4,
FOS_NOCHANGEDIR	= 0x8,
FOS_PICKFOLDERS	= 0x20,
FOS_FORCEFILESYSTEM	= 0x40,
FOS_ALLNONSTORAGEITEMS	= 0x80,
FOS_NOVALIDATE	= 0x100,
FOS_ALLOWMULTISELECT	= 0x200,
FOS_PATHMUSTEXIST	= 0x800,
FOS_FILEMUSTEXIST	= 0x1000,
FOS_CREATEPROMPT	= 0x2000,
FOS_SHAREAWARE	= 0x4000,
FOS_NOREADONLYRETURN	= 0x8000,
FOS_NOTESTFILECREATE	= 0x10000,
FOS_HIDEMRUPLACES	= 0x20000,
FOS_HIDEPINNEDPLACES	= 0x40000,
FOS_NODEREFERENCELINKS	= 0x100000,
FOS_DONTADDTORECENT	= 0x2000000,
FOS_FORCESHOWHIDDEN	= 0x10000000,
FOS_DEFAULTNOMINIMODE	= 0x20000000,
FOS_FORCEPREVIEWPANEON	= 0x40000000
} ;
typedef DWORD FILEOPENDIALOGOPTIONS;

EXTERN_C const IID IID_IFileDialog;

typedef struct _COMDLG_FILTERSPEC
{
	LPCWSTR pszName;
	LPCWSTR pszSpec;
} 	COMDLG_FILTERSPEC;

typedef enum FDAP
{	
	FDAP_BOTTOM	= 0,
	FDAP_TOP	= 1
} 	FDAP;

MIDL_INTERFACE("42f85136-db7e-439c-85f1-e4075d135fc8")
IFileDialog : public IModalWindow
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetFileTypes( 
		/* [in] */ UINT cFileTypes,
		/* [size_is][in] */ const COMDLG_FILTERSPEC *rgFilterSpec) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFileTypeIndex( 
		/* [in] */ UINT iFileType) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFileTypeIndex( 
		/* [out] */ UINT *piFileType) = 0;

	virtual HRESULT STDMETHODCALLTYPE Advise( 
		/* [in] */ IFileDialogEvents *pfde,
		/* [out] */ DWORD *pdwCookie) = 0;

	virtual HRESULT STDMETHODCALLTYPE Unadvise( 
		/* [in] */ DWORD dwCookie) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetOptions( 
		/* [in] */ FILEOPENDIALOGOPTIONS fos) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetOptions( 
		/* [out] */ FILEOPENDIALOGOPTIONS *pfos) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetDefaultFolder( 
		/* [in] */ IShellItem *psi) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFolder( 
		/* [in] */ IShellItem *psi) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolder( 
		/* [out] */ IShellItem **ppsi) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCurrentSelection( 
		/* [out] */ IShellItem **ppsi) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFileName( 
		/* [string][in] */ LPCWSTR pszName) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFileName( 
		/* [string][out] */ LPWSTR *pszName) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetTitle( 
		/* [string][in] */ LPCWSTR pszTitle) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetOkButtonLabel( 
		/* [string][in] */ LPCWSTR pszText) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFileNameLabel( 
		/* [string][in] */ LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetResult( 
		/* [out] */ IShellItem **ppsi) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddPlace( 
		/* [in] */ IShellItem *psi,
		/* [in] */ FDAP fdap) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetDefaultExtension( 
		/* [string][in] */ LPCWSTR pszDefaultExtension) = 0;

	virtual HRESULT STDMETHODCALLTYPE Close( 
		/* [in] */ HRESULT hr) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetClientGuid( 
		/* [in] */ REFGUID guid) = 0;

	virtual HRESULT STDMETHODCALLTYPE ClearClientData( void) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFilter( 
		/* [in] */ IShellItemFilter *pFilter) = 0;

};


#endif // __IFileDialog_INTERFACE_DEFINED__

#ifndef __IFileOpenDialog_INTERFACE_DEFINED__
#define __IFileOpenDialog_INTERFACE_DEFINED__

EXTERN_C const IID IID_IFileOpenDialog;

MIDL_INTERFACE("d57c7288-d4ad-4768-be02-9d969532d960")
IFileOpenDialog : public IFileDialog
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetResults( 
		/* [out] */ __RPC__deref_out_opt IShellItemArray **ppenum) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetSelectedItems( 
		/* [out] */ __RPC__deref_out_opt IShellItemArray **ppsai) = 0;

};
#endif // __IFileOpenDialog_INTERFACE_DEFINED__

#ifndef __IFileSaveDialog_INTERFACE_DEFINED__
#define __IFileSaveDialog_INTERFACE_DEFINED__

EXTERN_C const IID IID_IFileSaveDialog;

MIDL_INTERFACE("84bccd23-5fde-4cdb-aea4-af64b83d78ab")
IFileSaveDialog : public IFileDialog
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetSaveAsItem( 
		/* [in] */ __RPC__in_opt IShellItem *psi) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetProperties( 
		/* [in] */ __RPC__in_opt IPropertyStore *pStore) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetCollectedProperties( 
		/* [in] */ __RPC__in_opt IPropertyDescriptionList *pList,
		/* [in] */ BOOL fAppendDefault) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetProperties( 
		/* [out] */ __RPC__deref_out_opt IPropertyStore **ppStore) = 0;

	virtual HRESULT STDMETHODCALLTYPE ApplyProperties( 
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [in] */ __RPC__in_opt IPropertyStore *pStore,
		/* [unique][in] */ __RPC__in_opt HWND hwnd,
		/* [unique][in] */ __RPC__in_opt IFileOperationProgressSink *pSink) = 0;

};
#endif // __IFileSaveDialog_INTERFACE_DEFINED__

#ifndef __IFileDialogEvents_INTERFACE_DEFINED__
#define __IFileDialogEvents_INTERFACE_DEFINED__

typedef /* [v1_enum] */ 
	enum FDE_OVERWRITE_RESPONSE
{	FDEOR_DEFAULT	= 0,
FDEOR_ACCEPT	= 1,
FDEOR_REFUSE	= 2
} 	FDE_OVERWRITE_RESPONSE;

typedef /* [v1_enum] */ 
	enum FDE_SHAREVIOLATION_RESPONSE
{	FDESVR_DEFAULT	= 0,
FDESVR_ACCEPT	= 1,
FDESVR_REFUSE	= 2
} 	FDE_SHAREVIOLATION_RESPONSE;

EXTERN_C const IID IID_IFileDialogEvents;

MIDL_INTERFACE("973510db-7d7f-452b-8975-74a85828d354")
IFileDialogEvents : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE OnFileOk( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnFolderChanging( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd,
		/* [in] */ __RPC__in_opt IShellItem *psiFolder) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnFolderChange( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnSelectionChange( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnShareViolation( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd,
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [out] */ __RPC__out FDE_SHAREVIOLATION_RESPONSE *pResponse) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnTypeChange( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnOverwrite( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd,
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [out] */ __RPC__out FDE_OVERWRITE_RESPONSE *pResponse) = 0;

};
#endif // __IFileDialogEvents_INTERFACE_DEFINED__

#endif // WINDOWS_SHOBJIDL_INCLUDED
