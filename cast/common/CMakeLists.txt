add_subdirectory(certificate/proto)
add_subdirectory(channel/proto)
include_directories("${CMAKE_BINARY_DIR}")

file(GLOB certificate_sources
    certificate/cast_cert_validator.cc
    certificate/cast_cert_validator.h
    certificate/cast_cert_validator_internal.cc
    certificate/cast_cert_validator_internal.h
    certificate/cast_crl.cc
    certificate/cast_crl.h
    certificate/cast_trust_store.cc
    certificate/cast_trust_store.h
    certificate/types.cc
    certificate/types.h
)

file (GLOB channel_sources
    channel/cast_socket.cc
    channel/connection_namespace_handler.cc
    channel/connection_namespace_handler.h
    channel/message_framer.cc
    channel/message_framer.h
    channel/message_util.cc
    channel/message_util.h
    channel/namespace_router.cc
    channel/namespace_router.h
    channel/virtual_connection.h
    channel/virtual_connection_manager.cc
    channel/virtual_connection_manager.h
    channel/virtual_connection_router.cc
    channel/virtual_connection_router.h
    public/cast_socket.h
)

file (GLOB service_info_sources
    public/service_info.cc
    public/service_info.h
)

add_library(cast_common ${certificate_sources} ${channel_sources})
target_link_libraries(cast_common cast_common_certificate_pb cast_common_channel_pb)
