file(GLOB sources
    alarm.cc
    alarm.h
    big_endian.cc
    big_endian.h
    crypto/certificate_utils.cc
    crypto/certificate_utils.h
    crypto/digest_sign.cc
    crypto/digest_sign.h
    crypto/openssl_util.cc
    crypto/openssl_util.h
    crypto/rsa_private_key.cc
    crypto/rsa_private_key.h
    crypto/secure_hash.cc
    crypto/secure_hash.h
    crypto/sha2.cc
    crypto/sha2.h
    hashing.h
    integer_division.h
    json/json_serialization.cc
    json/json_serialization.h
    json/json_value.cc
    json/json_value.h
    operation_loop.cc
    operation_loop.h
    osp_logging.h
    saturate_cast.h
    simple_fraction.cc
    simple_fraction.h
    std_util.h
    stringprintf.cc
    stringprintf.h
    trace_logging.h
    trace_logging/macro_support.h
    trace_logging/scoped_trace_operations.cc
    trace_logging/scoped_trace_operations.h
    url.cc
    url.h
    weak_ptr.h
    yet_another_bit_vector.cc
    yet_another_bit_vector.h
)

file (GLOB mozilla_srcs
    ${PROJECT_SOURCE_DIR}/third_party/mozilla/url_parse.cc
    ${PROJECT_SOURCE_DIR}/third_party/mozilla/url_parse.h
    ${PROJECT_SOURCE_DIR}/third_party/mozilla/url_parse_internal.cc
    ${PROJECT_SOURCE_DIR}/third_party/mozilla/url_parse_internal.h
)

add_library(cast_util ${sources} ${mozilla_srcs})
target_link_libraries(cast_util ${JSONCPP_LIBRARIES} ${OPENSSL_LIBRARIES})