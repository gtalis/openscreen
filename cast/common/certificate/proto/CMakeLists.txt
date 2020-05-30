PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER revocation.proto)
add_library(cast_common_certificate_pb ${PROTO_SRC} ${PROTO_HEADER})