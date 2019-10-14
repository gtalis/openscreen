// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SCOPED_WAKE_LOCK_MAC_H_
#define PLATFORM_IMPL_SCOPED_WAKE_LOCK_MAC_H_

#include <IOKit/pwr_mgt/IOPMLib.h>

#include <atomic>
#include <memory>

#include "platform/api/scoped_wake_lock.h"

namespace openscreen {
namespace platform {

class ScopedWakeLockMac : public ScopedWakeLock {
 public:
  ScopedWakeLockMac(TaskRunner* task_runner);
  ScopedWakeLockMac(const ScopedWakeLockMac& other) = delete;
  ScopedWakeLockMac(ScopedWakeLockMac&& other) = delete;
  ScopedWakeLockMac& operator=(const ScopedWakeLockMac& other) = delete;
  ScopedWakeLockMac& operator=(ScopedWakeLockMac&& other) = delete;
  ~ScopedWakeLockMac() override;

 private:
  struct LockState {
    int reference_count = 0;
    IOPMAssertionID assertion_id = kIOPMNullAssertionID;
  };

  void AcquireWakeLock();
  void ReleaseWakeLock();

  static LockState lock_state_;
  TaskRunner* const task_runner_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SCOPED_WAKE_LOCK_MAC_H_