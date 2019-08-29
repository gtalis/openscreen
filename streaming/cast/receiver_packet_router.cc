// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver_packet_router.h"

#include <algorithm>

#include "platform/api/logging.h"
#include "streaming/cast/packet_util.h"
#include "streaming/cast/receiver.h"

namespace openscreen {
namespace cast_streaming {

ReceiverPacketRouter::ReceiverPacketRouter(Environment* environment)
    : environment_(environment) {
  OSP_DCHECK(environment_);
}

ReceiverPacketRouter::~ReceiverPacketRouter() {
  OSP_DCHECK(receivers_.empty());
}

void ReceiverPacketRouter::OnReceiverCreated(Ssrc ssrc, Receiver* receiver) {
  OSP_DCHECK(FindEntry(ssrc) == receivers_.end());
  receivers_.emplace_back(ssrc, receiver);

  // If there were no Receiver instances before, resume receiving packets for
  // dispatch. Reset/Clear the remote endpoint, in preparation for later setting
  // it to the source of the first packet received.
  if (receivers_.size() == 1) {
    environment_->set_remote_endpoint(IPEndpoint{});
    environment_->ResumeIncomingPackets(this);
  }
}

void ReceiverPacketRouter::OnReceiverDestroyed(Ssrc ssrc) {
  const auto it = FindEntry(ssrc);
  OSP_DCHECK(it != receivers_.end());
  receivers_.erase(it);

  // If there are no longer any Receivers, suspend receiving packets.
  if (receivers_.empty()) {
    environment_->SuspendIncomingPackets();
  }
}

void ReceiverPacketRouter::SendRtcpPacket(absl::Span<const uint8_t> packet) {
  OSP_DCHECK(InspectPacketForRouting(packet).first == ApparentPacketType::RTCP);

  // Do not proceed until the remote endpoint is known. See OnReceivedPacket().
  if (environment_->remote_endpoint().port == 0) {
    return;
  }

  environment_->SendPacket(packet);
}

void ReceiverPacketRouter::OnReceivedPacket(
    absl::Span<const uint8_t> packet,
    const IPEndpoint& source,
    platform::Clock::time_point arrival_time) {
  OSP_DCHECK_NE(source.port, uint16_t{0});

  // If the sender endpoint is known, ignore any packet that did not come from
  // that same endpoint.
  if (environment_->remote_endpoint().port != 0) {
    if (source != environment_->remote_endpoint()) {
      return;
    }
  }

  const std::pair<ApparentPacketType, Ssrc> seems_like =
      InspectPacketForRouting(packet);
  if (seems_like.first == ApparentPacketType::UNKNOWN) {
    return;
  }
  const auto it = FindEntry(seems_like.second);
  if (it != receivers_.end()) {
    // At this point, a valid packet has been matched with a receiver. Lock-in
    // the remote endpoint as the |source| of this |packet| so that only packets
    // from the same source are permitted from here onwards.
    if (environment_->remote_endpoint().port == 0) {
      environment_->set_remote_endpoint(source);
    }

    if (seems_like.first == ApparentPacketType::RTP) {
      it->second->OnReceivedRtpPacket(packet, arrival_time);
    } else if (seems_like.first == ApparentPacketType::RTCP) {
      it->second->OnReceivedRtcpPacket(packet, arrival_time);
    }
  }
}

ReceiverPacketRouter::ReceiverEntries::iterator ReceiverPacketRouter::FindEntry(
    Ssrc ssrc) {
  return std::find_if(receivers_.begin(), receivers_.end(),
                      [ssrc](const ReceiverEntries::value_type& entry) {
                        return entry.first == ssrc;
                      });
}

}  // namespace cast_streaming
}  // namespace openscreen