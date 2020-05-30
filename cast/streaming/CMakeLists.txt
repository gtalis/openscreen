file (GLOB common_sources
    clock_drift_smoother.cc
    clock_drift_smoother.h
    constants.h
    encoded_frame.cc
    encoded_frame.h
    environment.cc
    environment.h
    expanded_value_base.h
    frame_crypto.cc
    frame_crypto.h
    frame_id.cc
    frame_id.h
    ntp_time.cc
    ntp_time.h
    packet_util.cc
    packet_util.h
    rtcp_common.cc
    rtcp_common.h
    rtcp_session.cc
    rtcp_session.h
    rtp_defines.cc
    rtp_defines.h
    rtp_time.cc
    rtp_time.h
    session_config.cc
    session_config.h
    ssrc.cc
    ssrc.h
)


file (GLOB receiver_sources
    answer_messages.cc
    answer_messages.h
    compound_rtcp_builder.cc
    compound_rtcp_builder.h
    frame_collector.cc
    frame_collector.h
    offer_messages.cc
    offer_messages.h
    packet_receive_stats_tracker.cc
    packet_receive_stats_tracker.h
    receiver.cc
    receiver.h
    receiver_packet_router.cc
    receiver_packet_router.h
    receiver_session.cc
    receiver_session.h
    rtp_packet_parser.cc
    rtp_packet_parser.h
    sender_report_parser.cc
    sender_report_parser.h
)

file (GLOB sender_sources
    bandwidth_estimator.cc
    bandwidth_estimator.h
    compound_rtcp_parser.cc
    compound_rtcp_parser.h
    rtp_packetizer.cc
    rtp_packetizer.h
    sender.cc
    sender.h
    sender_packet_router.cc
    sender_packet_router.h
    sender_report_builder.cc
    sender_report_builder.h
)

add_library(cast_streaming ${common_sources} ${receiver_sources} ${sender_sources})