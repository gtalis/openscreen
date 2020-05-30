file(GLOB sources
    base/error.cc
    base/error.h
    base/interface_info.cc
    base/interface_info.h
    base/ip_address.cc
    base/ip_address.h
    base/location.cc
    base/location.h
    base/socket_state.h
    base/tls_connect_options.h
    base/tls_credentials.cc
    base/tls_credentials.h
    base/tls_listen_options.h
    base/trace_logging_activation.cc
    base/trace_logging_activation.h
    base/trace_logging_types.h
    base/trivial_clock_traits.cc
    base/trivial_clock_traits.h
    base/udp_packet.cc
    base/udp_packet.h
)

file (GLOB api_srcs
    api/export.h
    api/logging.h
    api/network_interface.h
    api/scoped_wake_lock.cc
    api/scoped_wake_lock.h
    api/serial_delete_ptr.h
    api/task_runner.h
    api/time.h
    api/tls_connection.cc
    api/tls_connection.h
    api/tls_connection_factory.cc
    api/tls_connection_factory.h
    api/trace_logging_platform.cc
    api/trace_logging_platform.h
    api/udp_socket.cc
    api/udp_socket.h
)

file (GLOB linux_impl_srcs
    impl/logging.h
    impl/network_interface.cc
    impl/network_interface.h
    impl/socket_handle.h
    impl/socket_handle_waiter.cc
    impl/socket_handle_waiter.h
    impl/stream_socket.h
    impl/task_runner.cc
    impl/task_runner.h
    impl/text_trace_logging_platform.cc
    impl/text_trace_logging_platform.h
    impl/time.cc
    impl/tls_write_buffer.cc
    impl/tls_write_buffer.h
    impl/network_interface_linux.cc
    impl/scoped_wake_lock_linux.cc
    impl/scoped_wake_lock_linux.h
    impl/logging_posix.cc
    impl/platform_client_posix.cc
    impl/platform_client_posix.h
    impl/scoped_pipe.h
    impl/socket_address_posix.cc
    impl/socket_address_posix.h
    impl/socket_handle_posix.cc
    impl/socket_handle_posix.h
    impl/socket_handle_waiter_posix.cc
    impl/socket_handle_waiter_posix.h
    impl/stream_socket_posix.cc
    impl/stream_socket_posix.h
    impl/timeval_posix.cc
    impl/timeval_posix.h
    impl/tls_connection_factory_posix.cc
    impl/tls_connection_factory_posix.h
    impl/tls_connection_posix.cc
    impl/tls_connection_posix.h
    impl/tls_data_router_posix.cc
    impl/tls_data_router_posix.h
    impl/udp_socket_posix.cc
    impl/udp_socket_posix.h
    impl/udp_socket_reader_posix.cc
    impl/udp_socket_reader_posix.h
)

add_library(cast_platform ${sources} ${api_srcs} ${linux_impl_srcs})

if (ENABLE_TRACE_LOGGING)
target_compile_definitions(cast_platform PRIVATE -DENABLE_TRACE_LOGGING)
endif()


# target_compile_options(cast_platform PUBLIC -DNDEBUG -DENABLE_TRACE_LOGGING  -Wall -Werror -fno-strict-aliasing
# -Wno-maybe-uninitialized -fvisibility=hidden -fPIC -m64 -O2 -g1 -fstack-protector -std=c++14 -Wno-noexcept-type -fno-exceptions # -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-rtti -Wno-implicit-fallthrough)