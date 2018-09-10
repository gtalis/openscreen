// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SOCKET_H_
#define PLATFORM_API_SOCKET_H_

#include "base/ip_address.h"

namespace openscreen {
namespace platform {

// Opaque type for the platform to implement.
struct UdpSocketPrivate;
using UdpSocketPtr = UdpSocketPrivate*;

UdpSocketPtr CreateUdpSocketIPv4();
UdpSocketPtr CreateUdpSocketIPv6();

// Closes the underlying platform socket and frees any allocated memory.
void DestroyUdpSocket(UdpSocketPtr socket);

bool BindUdpSocket(UdpSocketPtr socket,
                   const IPEndpoint& endpoint,
                   int32_t ifindex);
bool JoinUdpMulticastGroup(UdpSocketPtr socket,
                           const IPAddress& address,
                           int32_t ifindex);

int64_t ReceiveUdp(UdpSocketPtr socket,
                   void* data,
                   int64_t length,
                   IPEndpoint* src,
                   IPEndpoint* original_destination);
int64_t SendUdp(UdpSocketPtr socket,
                const void* data,
                int64_t length,
                const IPEndpoint& dest);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_H_
