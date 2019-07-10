// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/select.h>

#include <algorithm>
#include <vector>

#include "platform/api/event_waiter.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {
namespace {

Error AddToVectorIfMissing(UdpSocketPosix* socket,
                           std::vector<UdpSocketPosix*>* watched_sockets) {
  for (const auto* s : *watched_sockets) {
    if (s->GetFd() == socket->GetFd())
      return Error::Code::kAlreadyListening;
  }
  watched_sockets->push_back(socket);
  return Error::None();
}

Error RemoveFromVectorIfPresent(UdpSocketPosix* socket,
                                std::vector<UdpSocketPosix*>* watched_sockets) {
  const auto it = std::find_if(
      watched_sockets->begin(), watched_sockets->end(),
      [socket](UdpSocketPosix* s) { return s->GetFd() == socket->GetFd(); });
  if (it == watched_sockets->end())
    return Error::Code::kNoItemFound;

  watched_sockets->erase(it);
  return Error::None();
}

}  // namespace

struct EventWaiterPrivate {
  std::vector<UdpSocketPosix*> read_sockets;
  std::vector<UdpSocketPosix*> write_sockets;
};

EventWaiterPtr CreateEventWaiter() {
  return new EventWaiterPrivate;
}

void DestroyEventWaiter(EventWaiterPtr waiter) {
  delete waiter;
}

Error WatchUdpSocketReadable(EventWaiterPtr waiter, UdpSocket* socket) {
  return AddToVectorIfMissing(static_cast<UdpSocketPosix*>(socket),
                              &waiter->read_sockets);
}

Error StopWatchingUdpSocketReadable(EventWaiterPtr waiter, UdpSocket* socket) {
  return RemoveFromVectorIfPresent(static_cast<UdpSocketPosix*>(socket),
                                   &waiter->read_sockets);
}

Error WatchUdpSocketWritable(EventWaiterPtr waiter, UdpSocket* socket) {
  return AddToVectorIfMissing(static_cast<UdpSocketPosix*>(socket),
                              &waiter->write_sockets);
}

Error StopWatchingUdpSocketWritable(EventWaiterPtr waiter, UdpSocket* socket) {
  return RemoveFromVectorIfPresent(static_cast<UdpSocketPosix*>(socket),
                                   &waiter->write_sockets);
}

Error WatchNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement network change watching.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

Error StopWatchingNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement stop network change watching.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

ErrorOr<Events> WaitForEvents(EventWaiterPtr waiter) {
  int max_fd = -1;
  fd_set readfds;
  fd_set writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  for (const auto* read_socket : waiter->read_sockets) {
    FD_SET(read_socket->GetFd(), &readfds);
    max_fd = std::max(max_fd, read_socket->GetFd());
  }
  for (const auto* write_socket : waiter->write_sockets) {
    FD_SET(write_socket->GetFd(), &writefds);
    max_fd = std::max(max_fd, write_socket->GetFd());
  }
  if (max_fd == -1)
    return Error::Code::kIOFailure;

  struct timeval tv = {};
  const int rv = select(max_fd + 1, &readfds, &writefds, nullptr, &tv);
  if (rv == -1 || rv == 0)
    return Error::Code::kIOFailure;

  Events events;
  for (auto* read_socket : waiter->read_sockets) {
    if (FD_ISSET(read_socket->GetFd(), &readfds))
      events.udp_readable_events.push_back({read_socket});
  }
  for (auto* write_socket : waiter->write_sockets) {
    if (FD_ISSET(write_socket->GetFd(), &writefds))
      events.udp_writable_events.push_back({write_socket});
  }

  return std::move(events);
}

}  // namespace platform
}  // namespace openscreen
