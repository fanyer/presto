/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPAUTOPTR_H
#define OPAUTOPTR_H

#if defined _WIN32_WCE && defined ARM
#  define THROWCLAUSE 
#else
#  define THROWCLAUSE throw()
#endif

/**
 * Utility class to make sure objects allocated on the heap are
 * released when they are no longer used.
 * 
 * This is an implementation of the Standard C++ Library auto_ptr.
 * based on "The Standard C++ Library, a tutorial and reference", by
 * Nicolai M. Josuttis, the GNU stdc++ library and the ISO C++
 * standard.
 *
 * Do not modify this class unless you checked the behaviour of
 * Standard C++ auto_ptr and know this implementation is wrong!
 *
 * @author Petter Reinholdtsen <pere@opera.com> */

template<class T> class OpAutoPtr {
 private:
  T* ap; /// Pointer to the owned object

  template<class Y> struct OpAutoPtrRef {
    Y* yp;
    OpAutoPtrRef(Y* rhs) : yp(rhs) {};
  };

 public:
  typedef T element_type;

  explicit OpAutoPtr(T* ptr = 0) THROWCLAUSE : ap(ptr) {};

  /**
   * Copy constructor for non-constant parameter, acquire the ownership
   * of the object currently owned by the argument OpAutoPtr.
   */
  OpAutoPtr(OpAutoPtr& rhs) THROWCLAUSE : ap(rhs.release()) {};

  // This failed to compile on EPOC (simulator) with Visual C++.
  // Disabled until it starts working. [pere 2001-05-04]
#ifdef HAVE_CXX_MEMBER_TEMPLATES
  /**
   * Copy constructor for classes Y convertible to element_type. (?)
   */
  template<class Y>
    OpAutoPtr(OpAutoPtr<Y>& rhs) THROWCLAUSE : ap(rhs.release()) {};
#endif // HAVE_CXX_MEMBER_TEMPLATES

  /**
   * Assignment with implisit conversion.
   */
  OpAutoPtr& operator=(OpAutoPtr& rhs) THROWCLAUSE
  {
    reset(rhs.release());
    return *this;
  };

  /**
   * Destructor.  Delete object owned by this OpAutoPtr.
   */
  ~OpAutoPtr() THROWCLAUSE
  {
    delete ap;
  };

  /**
   * Return pointer to the object currently owned by this OpAutoPtr.
   *
   * @return element_type pointer
   */
  T* get() const THROWCLAUSE
  {
    return ap;
  };

  T& operator*() const THROWCLAUSE
  {
    return *ap;
  };

  T* operator->() const THROWCLAUSE
  {
    return ap;
  };

  /**
   * Release ownership of the object pointed to by this OpAutoPtr.
   * @return pointer to the object.
   */
  T* release() THROWCLAUSE
  {
    T* tmp = ap;
    ap = 0;
    return tmp;
  };

  /**
   * Replace the object owned by this OpAutoPtr with the object pointed
   * to by ptr.  Delete the object currently owned by this OpAutoPtr
   * before taking the new ownership.
   *
   * @param ptr pointer to 'new' object.
   */
  void reset(T* ptr=0) THROWCLAUSE
  {
    if (ap != ptr)
    {
      delete ap;
      ap = ptr;
    }
  };

  /**
   * Special conversions with auxiliary type to enable copies and
   * assigments.
   */
  OpAutoPtr(OpAutoPtrRef<T> rhs) THROWCLAUSE : ap(rhs.yp) {};

  OpAutoPtr& operator=(OpAutoPtrRef<T> rhs) THROWCLAUSE
  {
    reset(rhs.yp);
    return *this;
  };

  template<class Y>
    operator OpAutoPtrRef<Y>() THROWCLAUSE
    {
      return OpAutoPtrRef<Y>(release());
    }

  template<class Y>
    operator OpAutoPtr<Y>() THROWCLAUSE
    {
      return OpAutoPtr<Y>(release());
    }
 };
  
#endif // OPAUTOPTR_H
