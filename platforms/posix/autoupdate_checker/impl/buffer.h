// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA
#ifndef PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_BUFFER_H_
#define PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_BUFFER_H_
#include <stdint.h>

namespace opera_update_checker {
namespace ipc {

/** A simple wrapper for a heap-based array.
 * Automatically cleans up on destruction.
 */
template<typename T>
class Buffer {
 public:
  Buffer() : data_(NULLPTR), size_(0) {}

  ~Buffer() {
    Delete();
  }

  /** Attempts to allocate a buffor of @a size elements.
   * If the Buffer was already owning an array, it will be deleted before
   * allocating a new one.
   * @param size Number of elements (not necessarily size in bytes)
   * @retval true if the allocation was successfull,
   * @retval false on OOM.
   */
  bool Allocate(size_t size) {
    Delete();
    data_ = OAUC_NEWA(T, size);
    if (data_) {
      size_ = size;
      return true;
    } else {
      return false;
    }
  }

  /** @returns Pointer to the underlying array. Buffer retains ownership, caller
   * must not delete the received pointer. May be NULL.
   */
  T* Pointer() const { return data_; }

  /** @returns Size of the underlying array. 0 if empty.
   */
  size_t Size() const { return size_; }

  /** Drops ownership of the array.
   * Will NOT delete the underlying array anymore upon destruction. It's OK to
   * reuse the object by calling Allocate() again.
   * @returns Pointer to the released array. Caller is now responsible for
   * deleting it.
   */
  T* Release() {
    T* obj = data_;
    data_ = NULLPTR;
    size_ = 0;
    return obj;
  }

 private:
  void Delete() {
    OAUC_DELETEA(data_);
    size_ = 0;
  }

  T* data_;
  size_t size_;
};
}  // namespace ipc
}  // namespace opera_update_checker

#endif  // PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_BUFFER_H_
