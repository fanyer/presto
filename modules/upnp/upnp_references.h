/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
*/

#ifndef UPNP_REFERENCES_H_
#define UPNP_REFERENCES_H_

////// This file is not really realted to UPnP, so it can be used even if UPnP is disabled ////
////// It should probably move to util...

/****
How to protect a pointer (let's called it ptr) from becoming a dangling pointer?
Let's say that it is of class class_ptr, and it is used by class_user (it can be used by more than one object / class,
but for simplicity we'll look to one pointer with one reference).
We'll also describe a real case.
There are several small steps to do, but at the end the dangling pointer will disappear, and it will just be a matter of checking for NULL
The suggested steps are:
* 1) rename ptr as ptr_ref, changing the class from class_ptr to ReferenceUserSingle<class_ptr> (or ReferenceUserList<class_ptr>)
* 2) create a getter function to retrieve the previous pointer
* 3) call the appropriate constructor
* 4) change all the read operations to use the new getter function
* 5) change all the assignment operations (to a valid, not NULL, pointer) with AddReference()
* 6) assignment to NULL should become a call to RemoveReference() or CleanReference()
* 7) you can keep delete operations unchanged (apart from the call to the getter method) or do a RemoveReference before the delete
* 8) class_ptr must have a ReferenceObjectSingle<class_ptr> or ReferenceObjectList<class_ptr> object
* 9) class_ptr must have a getter for the reference object


In UPnPXH_DescriptionGeneric:
* 1) (changed) UPnPService *cur_service   ===>   ReferenceUserSingle<UPnPService> cur_service_ref;
* 2) (added) UPnPService *GetCurrentService() { return cur_service_ref.GetPointer() }
* 3) (in the constructor) cur_service_ref(FALSE, FALSE)
* 4) (substition in read operations) cur_service ==> GetCurrentService()
* 5) (substition in write operations) OpStatus::Ignore(cur_service_ref.AddReference(service)); // It cannot really fail
* 6) (changed) cur_service = NULL ==> cur_service_ref.CleanReference() 
* 7) (unchanged: we keep the delete)
In UPnPService:
* 8) (added) ReferenceObjectSingle<UPnPService> ref_obj_xml;
* 9) (added) ReferenceObject<UPnPService> *GetReferenceObjectPtrXML() { return &ref_obj_xml; }
*****/


template <class T> class ReferenceUser;

/// Class used to prevent to use an object after it has been deleted.
/// The class that contains the reference has to implement ReferenceUserList
/// (or it has to have one or more object implementing it),
/// and the object itself must contain a ReferenceObject for every list
template <class T>
class ReferenceObject
{
protected:
	/// pointer to the object guarded
	T *ptr;
	/// TRUE if the user is allowed to delete the object
	BOOL enable_delete;
public:
	/// Save the list
	ReferenceObject(T *object_ptr);
	/// Destructor
	virtual ~ReferenceObject() {}
	
	/// Set the User; On a single reference implementation, it is illegal to call this function when ref_user is not NULL
	virtual OP_STATUS AddReferenceUser(ReferenceUser<T> *user) = 0;
	/// Remove the reference to the user supplied; return BOOL if found
	virtual BOOL RemoveReferenceUser(ReferenceUser<T> *user) = 0;
	/// Return TRUE if the ReferenceUser has been found
	virtual BOOL FindReferenceUser(ReferenceUser<T> *user) = 0;

	/// Get the pointer
	T *GetPointer() { return ptr; }
	/// Return TRUE if the list is allowed to delete the object
	BOOL IsDeleteEnabled() { return enable_delete; }
	/// Set it the delete is enabled
	void EnableDelete(BOOL b) { enable_delete=b; }
	/// Set the pointer
	void SetPointer(T *pointer) { OP_ASSERT(!ptr); ptr=pointer; }
};

/// Object that can keep only one reference
template <class T>
class ReferenceObjectSingle: public ReferenceObject<T>
{
private:
	/// User
	ReferenceUser<T> *ref_user;

public:
	ReferenceObjectSingle(T *object_ptr): ReferenceObject<T>(object_ptr), ref_user(NULL) {}
	virtual ~ReferenceObjectSingle();
	
	/// Set the User; it is illegal to call this function when ref_user is not NULL
	virtual OP_STATUS AddReferenceUser(ReferenceUser<T> *user);
	/// Remove the reference to the user supplied; return BOOL if found
	virtual BOOL RemoveReferenceUser(ReferenceUser<T> *user) { OP_ASSERT(!ref_user || ref_user==user); BOOL b=ref_user==user; if(ref_user==user) ref_user=NULL; return b; }
	/// Return TRUE if the ReferenceUser has been found
	virtual BOOL FindReferenceUser(ReferenceUser<T> *user) { return ref_user==user; }
};

/// Object that can keep a list of references
template <class T>
class ReferenceObjectList: public ReferenceObject<T>
{
private:
	/// Users of this object
	OpVector<ReferenceUser<T> > ref_user_list;
	
#ifdef _DEBUG
	/// Check that there will not be multiple delete
	void CheckDeleteBalance();
#endif

public:
	ReferenceObjectList(T *object_ptr): ReferenceObject<T>(object_ptr) {}
	virtual ~ReferenceObjectList();
	
	/// Set the User; On a single reference implementation, it is illegal to call this function when ref_user is not NULL
	virtual OP_STATUS AddReferenceUser(ReferenceUser<T> *user);
	/// Remove the reference to the user supplied; return BOOL if found
	virtual BOOL RemoveReferenceUser(ReferenceUser<T> *user);
	/// Return TRUE if the ReferenceUser has been found
	virtual BOOL FindReferenceUser(ReferenceUser<T> *user);
};

/// Interface used to prevent to use an object after it has been deleted.
template <class T>
class ReferenceUser
{
private:
	/// TRUE to delete the pointers
	BOOL del_ptr;
	/// TRUE to delete the referecies
	BOOL del_ref;
	
protected:
	/// TRUE if at least one pointer has ever been set (this can be useful on some cases), even if later it has been removed
	BOOL pointer_ever_set;
	
	ReferenceUser(BOOL delete_references, BOOL delete_pointers): del_ptr(delete_pointers), del_ref(delete_references), pointer_ever_set(FALSE) { }
	
	/// Delete a reference object in the right way; this function should be called in the destructor, for every reference kept
	void DestroyReference(ReferenceObject<T> *ref);
		
public:
	/// Destructor. Base classes shoud honour del_prt and del_ref
	virtual ~ReferenceUser() { }
	
	/// Remove the reference (without deleting it); return TRUE if found
	virtual BOOL RemoveReference(ReferenceObject<T> *ref) = 0;
	/// Add a reference
	virtual OP_STATUS AddReference(ReferenceObject<T> *ref) = 0;
	/// return the index of the refernce (0 if there is only one possible pointer) if the pointer has been found, -1 if not
	virtual INT32 FindReference(ReferenceObject<T> *ref) = 0;
	
	/// TRUE if the pointer will be deleted
	BOOL IsPointerToDelete() { return del_ptr; }
	/// TRUE if the reference will be deleted
	BOOL IsReferenceToDelete() { return del_ref; }
	/// TRUE if at least one pointer has ever been set (this can be useful on some cases), even if later it has been removed
	BOOL HasPointerEverBeenSet() { return this->pointer_ever_set; }
};

/// Class that keeps a reference to a single pointer
template <class T>
class ReferenceUserSingle: public ReferenceUser<T>
{
private:
	ReferenceObject<T> *ref_obj;
	
public:
	ReferenceUserSingle(BOOL delete_references, BOOL delete_pointers): ReferenceUser<T>(delete_references, delete_pointers) { ref_obj=NULL; }
	
	virtual ~ReferenceUserSingle() { this->DestroyReference(ref_obj); }
	
	/// Remove the reference (without deleting it); return TRUE if found
	virtual BOOL RemoveReference(ReferenceObject<T> *ref);
	/// Remove the reference currently in use
	BOOL CleanReference() { return RemoveReference(ref_obj); }
	
	/// Add a reference
	virtual OP_STATUS AddReference(ReferenceObject<T> *ref);
	/// Return the pointer
	T* GetPointer() { return ref_obj?ref_obj->GetPointer():NULL; } 
	/// return the index of the refernce (0 if there is only one possible pointer) if the pointer has been found, -1 if not
	virtual INT32 FindReference(ReferenceObject<T> *ref) { return (ref==ref_obj)?0:-1; }
};

/// Class used to prevent to use an object after it has been deleted.
/// The class that contains the reference has to implement ReferenceUserList
/// (or it has to have one or more object implementing it),
/// and the object itself must contain a ReferenceObject for every list
template <class T>
class ReferenceUserList: public ReferenceUser<T>
{
private:
	OpVector<ReferenceObject<T> > list;

public:
	ReferenceUserList(BOOL delete_references, BOOL delete_pointers): ReferenceUser<T>(delete_references, delete_pointers) { }
	
	/// Remove the reference (without deleting it); return TRUE if found
	BOOL RemoveReference(ReferenceObject<T> *ref);
	/// Remove the reference (without deleting it); return TRUE if found
	ReferenceObject<T> *RemoveReferenceByIndex(UINT32 index);
	/// return the index of the refernce (0 if there is only one possible pointer) if the pointer has been found, -1 if not
	virtual INT32 FindReference(ReferenceObject<T> *ref);
	/// Add a reference to the list
	OP_STATUS AddReference(ReferenceObject<T> *ref);
	/// Return the pointer at the given index
	T* GetPointer(UINT32 index);
	/// Swap two positions
	OP_STATUS Swap(UINT32 idx1, UINT32 idx2);
	/// Return the reference object from the pointer
	ReferenceObject<T>* GetReferenceByPointer(T* ptr);
	
	/// Return the number of referencies stored
	UINT32 GetCount() const { return list.GetCount(); }
	
	/// Remove (and optionally delete) all the objects
	BOOL RemoveAll(BOOL delete_references, BOOL delete_pointers);
	
	virtual ~ReferenceUserList();
};

template <class T>
ReferenceObject<T>::ReferenceObject(T *object_ptr)
	: ptr(object_ptr), enable_delete(TRUE)
{
	//OP_ASSERT(ptr);
}

template <class T>
ReferenceObjectSingle<T>::~ReferenceObjectSingle()
{
	if(ref_user)
	{
		OP_ASSERT(ref_user->FindReference(this)>=0);
		
	#ifdef _DEBUG
		ReferenceUser<T> *old_ref=ref_user;
		
		OP_ASSERT(ref_user->RemoveReference(this));
		OP_ASSERT(old_ref->FindReference(this)<0);
	#else
		ref_user->RemoveReference(this);
	#endif
	
		ref_user=NULL;
	}
}

#ifdef _DEBUG
template <class T>
void ReferenceObjectList<T>::CheckDeleteBalance()
{
	int num_del_ptr=0;
	int num_del_ref=0;
	
	for(unsigned int i=ref_user_list.GetCount(); i-- > 0; )
	{
		ReferenceUser<T> *user=ref_user_list.Get(i);
		
		if(user && user->IsPointerToDelete())
			num_del_ptr++;
			
		if(user && user->IsReferenceToDelete())
			num_del_ref++;
	}
	
	OP_ASSERT(num_del_ptr<=1);
	OP_ASSERT(num_del_ref<=1);
}
#endif


template <class T>
ReferenceObjectList<T>::~ReferenceObjectList()
{
#ifdef _DEBUG
	CheckDeleteBalance();
#endif

	for(unsigned int i=ref_user_list.GetCount(); i-- > 0; )
	{
		ReferenceUser<T> *user=ref_user_list.Get(i);
		
		OP_ASSERT(user);
		
		if(user)
		{
			#ifdef _DEBUG
				OP_ASSERT(user->RemoveReference(this));		// Also call RemoveReferenceUser() in this object
			#else
				user->RemoveReference(this);				// Also call RemoveReferenceUser() in this object
			#endif
		}
		
		OP_ASSERT(ref_user_list.GetCount()==i);
	}
	
	OP_ASSERT(ref_user_list.GetCount()==0);
}

template <class T>
OP_STATUS ReferenceObjectList<T>::AddReferenceUser(ReferenceUser<T> *user)
{
	OP_ASSERT(user);
	
	if(!user)
		return OpStatus::OK;
		
	BOOL b=FindReferenceUser(user);
	
	OP_ASSERT(!b);
	
	if(!b)
	{
		OP_STATUS ops=ref_user_list.Add(user);
		
	#ifdef _DEBUG
		CheckDeleteBalance();
	#endif

		return ops;
	}
		
	return OpStatus::OK;
}

template <class T>
BOOL ReferenceObjectList<T>::RemoveReferenceUser(ReferenceUser<T> *user)
{
	INT32 index=ref_user_list.Find(user);
	
	if(index<0)
		return FALSE;
		
	ref_user_list.Remove(index);
	
	return TRUE;
}

template <class T>
BOOL ReferenceObjectList<T>::FindReferenceUser(ReferenceUser<T> *user)
{
	return ref_user_list.Find(user)>=0;
}
	
template <class T>
void ReferenceUser<T>::DestroyReference(ReferenceObject<T> *ref)
{
	if(ref && RemoveReference(ref) && ref->IsDeleteEnabled())
	{
		T* ptr=ref->GetPointer();
		
		// The reference is supposed to be usually inside the pointer, so earlier delete the reference then the pointer...
		if(del_ref)
			OP_DELETE(ref);
		if(del_ptr)
			OP_DELETE(ptr);
	}
}

template <class T>
OP_STATUS ReferenceObjectSingle<T>::AddReferenceUser(ReferenceUser<T> *user)
{
	OP_ASSERT((!ref_user || !user) && "Multiple referencies");
	
#ifdef _DEBUG
	if(ref_user)
		OP_ASSERT(ref_user->FindReference(this)<0 && "Pending reference...");
#endif
	
	ref_user=user;
	
#ifdef _DEBUG
	if(ref_user)
		OP_ASSERT(ref_user->FindReference(this)>=0 && "Multiple referencies");
#endif

	return OpStatus::OK;
}

template <class T>
BOOL ReferenceUserSingle<T>::RemoveReference(ReferenceObject<T> *ref)
{
	OP_ASSERT(ref_obj==ref);
	
	if(!ref)
		return FALSE;
	
	if(ref_obj==ref)
	{
		ref_obj=NULL;
		ref->RemoveReferenceUser(this);
		
		return TRUE;
	}
	
	return FALSE;
}

template <class T>
OP_STATUS ReferenceUserSingle<T>::AddReference(ReferenceObject<T> *ref)
{
	OP_ASSERT(ref_obj==NULL || ref_obj==ref);
	
	OP_ASSERT(ref);
	
	if(!ref)
		return OpStatus::ERR_NULL_POINTER;
		
	if(ref_obj==ref)
	{
		OP_ASSERT(ref->FindReferenceUser(this));
	
		return OpStatus::OK;
	}
	
	ref_obj=ref;
	
	OP_STATUS ops=ref->AddReferenceUser(this);
	
	OP_ASSERT(OpStatus::IsSuccess(ops));
	
	if(OpStatus::IsError(ops))
		ref_obj=NULL;  // Rollback
	else
		this->pointer_ever_set=TRUE;
	
	return ops;
}

template <class T>
BOOL ReferenceUserList<T>::RemoveReference(ReferenceObject<T> *ref)
{
	if(!ref)
		return FALSE;
		
	#ifdef _DEBUG
		if(!ref->FindReferenceUser(this))
			OP_ASSERT(list.Find(ref)<0);
		else
			OP_ASSERT(list.Find(ref)>=0 && FindReference(ref)>=0);
	#endif
		
	RETURN_VALUE_IF_ERROR(list.RemoveByItem(ref), FALSE);
	
	ref->RemoveReferenceUser(this);

	return TRUE;
}

template <class T>
ReferenceObject<T> *ReferenceUserList<T>::RemoveReferenceByIndex(UINT32 index)
{
	ReferenceObject<T> *t=list.Remove(index);
	
	OP_ASSERT(t);
	
	return t;
}

template <class T>
BOOL ReferenceUserList<T>::RemoveAll(BOOL delete_references, BOOL delete_pointers)
{
	BOOL result=TRUE;
	
	for(UINT32 i=GetCount(); i>0; i--)
	{
		ReferenceObject<T> *ref=list.Get(i-1);
		
		if(!RemoveReference(ref))
			result=FALSE;
		else
		{
			T* ptr=ref->GetPointer();
			
			if(delete_references)
				OP_DELETE(ref);
				
			if(delete_pointers)
				OP_DELETE(ptr);
		}
	}
	
	return result;
}

template <class T>
INT32 ReferenceUserList<T>::FindReference(ReferenceObject<T> *ref)
{
	return list.Find(ref);
}

template <class T>
OP_STATUS ReferenceUserList<T>::AddReference(ReferenceObject<T> *ref)
{
	OP_ASSERT(ref);
	
	if(!ref)
		return OpStatus::ERR_NULL_POINTER;
	
	OP_ASSERT(!ref->FindReferenceUser(this));
	
	ref->RemoveReferenceUser(this);
	
	if(FindReference(ref)<0)
	{
		RETURN_IF_ERROR(list.Add(ref));
	}
	else
	{
		OP_ASSERT(FALSE && "multiple insert...");
	}
	
	OP_STATUS ops=ref->AddReferenceUser(this);
	
	OP_ASSERT(OpStatus::IsSuccess(ops));
	
	if(OpStatus::IsError(ops))
		list.RemoveByItem(ref); // Rollback
	else
		this->pointer_ever_set=TRUE;
	
	return ops;
}

template <class T>
T* ReferenceUserList<T>::GetPointer(UINT32 index)
{
	ReferenceObject<T> *ref=list.Get(index);
	
	if(!ref)
		return NULL;
		
	return ref->GetPointer();
}

template <class T>
OP_STATUS ReferenceUserList<T>::Swap(UINT32 idx1, UINT32 idx2)
{
	ReferenceObject<T> *ref1=list.Get(idx1);
	ReferenceObject<T> *ref2=list.Get(idx2);
	
	OP_ASSERT(ref1);
	OP_ASSERT(ref2);
	
	RETURN_IF_ERROR(list.Replace(idx1, ref2));
	RETURN_IF_ERROR(list.Replace(idx2, ref1));
	
	return OpStatus::OK;
}

template <class T>
ReferenceObject<T> *ReferenceUserList<T>::GetReferenceByPointer(T* ptr)
{
	for(int i=list.GetCount(); i>0; i--)
	{
		ReferenceObject<T> *ref=list.Get(i-1);
		
		OP_ASSERT(ref);
		
		if(ref && ref->GetPointer()==ptr)
			return ref;
	}
	
	return NULL;
}

template <class T>
ReferenceUserList<T>::~ReferenceUserList()
{
	for(int i=list.GetCount(); i-- > 0;)
	{
		ReferenceObject<T> *ref=list.Get(i);
		
		OP_ASSERT(ref);
		
		this->DestroyReference(ref);
	}
}

#endif // UPNP_REFERENCES_H_
