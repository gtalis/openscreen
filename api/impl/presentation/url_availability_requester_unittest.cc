// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/presentation/url_availability_requester.h"

#include <memory>

#include "api/impl/quic/quic_client.h"
#include "api/impl/quic/testing/fake_quic_connection_factory.h"
#include "api/impl/testing/fake_clock.h"
#include "api/public/network_service_manager.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace presentation {

using ::testing::_;

class MockReceiverObserver final : public ReceiverObserver {
 public:
  ~MockReceiverObserver() override = default;

  MOCK_METHOD2(OnRequestFailed, void(const std::string&, const std::string&));
  MOCK_METHOD2(OnReceiverAvailable,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(OnReceiverUnavailable,
               void(const std::string&, const std::string&));
};

class MockMessageCallback final : public MessageDemuxer::MessageCallback {
 public:
  ~MockMessageCallback() override = default;

  MOCK_METHOD5(OnStreamMessage,
               ErrorOr<size_t>(uint64_t endpoint_id,
                               uint64_t connection_id,
                               msgs::Type message_type,
                               const uint8_t* buffer,
                               size_t buffer_size));
};

class NullObserver final : public ProtocolConnectionServiceObserver {
 public:
  ~NullObserver() override = default;
  void OnRunning() override {}
  void OnStopped() override {}
  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}
};

class UrlAvailabilityRequesterTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto fake_factory = std::make_unique<FakeQuicConnectionFactory>(
        controller_endpoint_, &receiver_demuxer_);
    fake_factory_ = fake_factory.get();
    auto quic_client = std::make_unique<QuicClient>(
        &controller_demuxer_, std::move(fake_factory), &null_observer_);
    quic_client->Start();
    NetworkServiceManager::Get()->Create(nullptr, nullptr,
                                         std::move(quic_client), nullptr);

    availability_watch_ = receiver_demuxer_.SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback_);
  }

  void TearDown() override {
    availability_watch_ = MessageDemuxer::MessageWatch();
    NetworkServiceManager::Get()->Dispose();
  }

 protected:
  void RunTasksUntilIdle() {
    do {
      NetworkServiceManager::Get()->GetProtocolConnectionClient()->RunTasks();
    } while (!fake_factory_->idle());
  }

  void SendAvailabilityResponse(
      const msgs::PresentationUrlAvailabilityRequest& request,
      std::vector<msgs::PresentationUrlAvailability>&& availabilities,
      FakeQuicStream* stream) {
    msgs::PresentationUrlAvailabilityResponse response;
    response.request_id = request.request_id;
    response.url_availabilities = std::move(availabilities);
    msgs::CborEncodeBuffer buffer;
    ssize_t encode_result =
        msgs::EncodePresentationUrlAvailabilityResponse(response, &buffer);
    ASSERT_GT(encode_result, 0);
    stream->ReceiveData(buffer.data(), buffer.size());
  }

  void SendAvailabilityEvent(
      uint64_t watch_id,
      std::vector<std::string>&& urls,
      std::vector<msgs::PresentationUrlAvailability>&& availabilities,
      FakeQuicStream* stream) {
    msgs::PresentationUrlAvailabilityEvent event;
    event.watch_id = watch_id;
    event.urls = std::move(urls);
    event.url_availabilities = std::move(availabilities);
    msgs::CborEncodeBuffer buffer;
    ssize_t encode_result =
        msgs::EncodePresentationUrlAvailabilityEvent(event, &buffer);
    ASSERT_GT(encode_result, 0);
    stream->ReceiveData(buffer.data(), buffer.size());
  }

  MockMessageCallback mock_callback_;
  MessageDemuxer::MessageWatch availability_watch_;
  const IPEndpoint controller_endpoint_{{192, 168, 1, 3}, 4321};
  const IPEndpoint receiver_endpoint_{{192, 168, 1, 17}, 1234};
  MessageDemuxer controller_demuxer_;
  MessageDemuxer receiver_demuxer_;
  FakeQuicConnectionFactory* fake_factory_;
  NullObserver null_observer_;
  platform::TimeDelta now_{platform::TimeDelta::FromSeconds(213489)};
  std::unique_ptr<FakeClock> fake_clock_owned_{
      std::make_unique<FakeClock>(now_)};
  FakeClock* fake_clock_{fake_clock_owned_.get()};
  UrlAvailabilityRequester listener_{std::move(fake_clock_owned_)};

  std::string url1_{"https://example.com/foo.html"};
  std::string url2_{"https://example.com/bar.html"};
  std::string service_id_{"asdf"};
  std::string friendly_name_{"turtle"};
  ServiceInfo info1_{service_id_, friendly_name_, 1, receiver_endpoint_};
};

TEST_F(UrlAvailabilityRequesterTest, AvailableObserverFirst) {
  MockReceiverObserver mock_observer;
  listener_.AddObserver({url1_}, &mock_observer);

  listener_.AddReceiver(info1_);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, AvailableReceiverFirst) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer;
  listener_.AddObserver({url1_}, &mock_observer);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, Unavailable) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer;
  listener_.AddObserver({url1_}, &mock_observer);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, AvailabilityIsCached) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();

  MockReceiverObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_));
  listener_.AddObserver({url1_}, &mock_observer2);
}

TEST_F(UrlAvailabilityRequesterTest, AvailabilityCacheIsTransient) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();

  listener_.RemoveObserverUrls({url1_}, &mock_observer1);
  MockReceiverObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  listener_.AddObserver({url1_}, &mock_observer2);
}

TEST_F(UrlAvailabilityRequesterTest, PartiallyCachedAnswer) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();

  MockReceiverObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_));
  listener_.AddObserver({url1_, url2_}, &mock_observer2);

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url2_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url2_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url2_, service_id_));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, MultipleOverlappingObservers) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();

  MockReceiverObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url1_, service_id_));
  listener_.AddObserver({url1_, url2_}, &mock_observer2);

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url2_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url2_, service_id_));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, RemoveObserverUrls) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  uint64_t url1_watch_id = request.watch_id;
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();

  MockReceiverObserver mock_observer2;
  listener_.AddObserver({url1_, url2_}, &mock_observer2);

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url2_}, request.urls);

  listener_.RemoveObserverUrls({url1_}, &mock_observer1);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer2, OnReceiverAvailable(_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url2_, service_id_));
  RunTasksUntilIdle();

  SendAvailabilityEvent(
      url1_watch_id, std::vector<std::string>{url1_},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, RemoveObserver) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  uint64_t url1_watch_id = request.watch_id;
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();

  MockReceiverObserver mock_observer2;
  listener_.AddObserver({url1_, url2_}, &mock_observer2);

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  uint64_t url2_watch_id = request.watch_id;
  EXPECT_EQ(std::vector<std::string>{url2_}, request.urls);

  listener_.RemoveObserver(&mock_observer1);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer2, OnReceiverAvailable(_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url2_, service_id_));
  RunTasksUntilIdle();

  SendAvailabilityEvent(
      url1_watch_id, std::vector<std::string>{url1_},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();

  listener_.RemoveObserver(&mock_observer2);

  SendAvailabilityEvent(
      url1_watch_id, std::vector<std::string>{url1_},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  SendAvailabilityEvent(
      url2_watch_id, std::vector<std::string>{url2_},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(_, service_id_)).Times(0);
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, EventUpdate) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_, url2_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ((std::vector<std::string>{url1_, url2_}), request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible,
                                                     msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url2_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(_, service_id_)).Times(0);
  RunTasksUntilIdle();

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _)).Times(0);
  SendAvailabilityEvent(
      request.watch_id, std::vector<std::string>{url2_},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url2_, service_id_));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, RefreshWatches) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(_, service_id_)).Times(0);
  RunTasksUntilIdle();

  fake_clock_->Advance(platform::TimeDelta::FromSeconds(60));

  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  listener_.RefreshWatches();
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityRequesterTest, ResponseAfterRemoveObserver) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  listener_.RemoveObserverUrls({url1_}, &mock_observer1);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();

  MockReceiverObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  listener_.AddObserver({url1_}, &mock_observer2);
}

TEST_F(UrlAvailabilityRequesterTest,
       EmptyCacheAfterRemoveObserverThenReceiver) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1_}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_));
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  RunTasksUntilIdle();

  listener_.RemoveObserverUrls({url1_}, &mock_observer1);
  listener_.RemoveReceiver(info1_);
  MockReceiverObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer2, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  listener_.AddObserver({url1_}, &mock_observer2);
}

TEST_F(UrlAvailabilityRequesterTest, RemoveObserverInSteps) {
  listener_.AddReceiver(info1_);

  MockReceiverObserver mock_observer1;
  listener_.AddObserver({url1_, url2_}, &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ((std::vector<std::string>{url1_, url2_}), request.urls);
  listener_.RemoveObserverUrls({url1_}, &mock_observer1);
  listener_.RemoveObserverUrls({url2_}, &mock_observer1);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible,
                                                     msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));
  SendAvailabilityEvent(
      request.watch_id, std::vector<std::string>{url1_, url2_},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible,
                                                     msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url1_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url1_, service_id_))
      .Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverAvailable(url2_, service_id_)).Times(0);
  EXPECT_CALL(mock_observer1, OnReceiverUnavailable(url2_, service_id_))
      .Times(0);
  // NOTE: This message was generated between the two RemoveObserverUrls calls
  // above.  So even though the request is internally cancelled almost
  // immediately, this still went out on the wire.
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _))
      .WillOnce(::testing::Invoke(
          [&request, &connection_id](
              uint64_t endpoint_id, uint64_t cid, msgs::Type message_type,
              const uint8_t* buffer, size_t buffer_size) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();
  EXPECT_EQ((std::vector<std::string>{url2_}), request.urls);

  fake_clock_->Advance(platform::TimeDelta::FromSeconds(60));

  listener_.RefreshWatches();
  EXPECT_CALL(mock_callback_, OnStreamMessage(_, _, _, _, _)).Times(0);
  RunTasksUntilIdle();
}

}  // namespace presentation
}  // namespace openscreen
