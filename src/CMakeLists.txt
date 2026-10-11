find_package(Protobuf CONFIG REQUIRED)
set(GATEWAY_API_PROTO_DIR
        "${PROJECT_SOURCE_DIR}/proto"
        CACHE INTERNAL "Canonical payload source" FORCE)
set(generatedDirectory "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generatedDirectory}")
file(GLOB_RECURSE canonicalProtoInputs CONFIGURE_DEPENDS "${GATEWAY_API_PROTO_DIR}/*.proto")
foreach (protoName session observation orders world incidents)
    set(protoRelativePath "navalcommand/gateway/v1/${protoName}")
    add_custom_command(
            OUTPUT "${generatedDirectory}/${protoRelativePath}.pb.cc"
            "${generatedDirectory}/${protoRelativePath}.pb.h"
            COMMAND protobuf::protoc --proto_path=${GATEWAY_API_PROTO_DIR}
            --cpp_out=${generatedDirectory} ${GATEWAY_API_PROTO_DIR}/${protoRelativePath}.proto
            DEPENDS ${canonicalProtoInputs} protobuf::protoc)
    list(APPEND generatedSources "${generatedDirectory}/${protoRelativePath}.pb.cc")
endforeach ()
add_library(Gateway.Api STATIC ${generatedSources} NavalCommand/Gateway/Api/Codec.cpp)
target_include_directories(Gateway.Api PUBLIC "${PROJECT_SOURCE_DIR}/public"
        "${generatedDirectory}")
target_link_libraries(Gateway.Api PUBLIC Orders.Api World.Api protobuf::libprotobuf)
NavalCommandConfigureTarget(Gateway.Api)
