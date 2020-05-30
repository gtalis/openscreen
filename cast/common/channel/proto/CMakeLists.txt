PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER cast_channel.proto)
add_library(cast_common_channel_pb ${PROTO_SRC} ${PROTO_HEADER})