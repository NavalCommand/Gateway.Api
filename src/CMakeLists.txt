find_package(Protobuf CONFIG REQUIRED)
set(GATEWAY_API_PROTO_DIR
    "${PROJECT_SOURCE_DIR}/proto"
    CACHE INTERNAL "Canonical payload source" FORCE)
set(gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${gen}")
foreach(name session orders world incidents)
    set(path "navalcommand/gateway/v1/${name}")
    add_custom_command(
        OUTPUT "${gen}/${path}.pb.cc" "${gen}/${path}.pb.h"
        COMMAND protobuf::protoc --proto_path=${GATEWAY_API_PROTO_DIR} --cpp_out=${gen}
                ${GATEWAY_API_PROTO_DIR}/${path}.proto
        DEPENDS "${GATEWAY_API_PROTO_DIR}/${path}.proto" protobuf::protoc)
    list(APPEND sources "${gen}/${path}.pb.cc")
endforeach()
add_library(Gateway.Api STATIC ${sources} NavalCommand/Gateway/Api/Codec.cpp)
target_include_directories(Gateway.Api PUBLIC "${PROJECT_SOURCE_DIR}/public" "${gen}")
target_link_libraries(Gateway.Api PUBLIC protobuf::libprotobuf)
NavalCommandConfigureTarget(Gateway.Api)
