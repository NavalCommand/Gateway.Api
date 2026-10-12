find_package(Protobuf CONFIG REQUIRED)
set(GATEWAY_API_PROTO_DIR "${PROJECT_SOURCE_DIR}/proto" CACHE INTERNAL "Canonical payload source" FORCE)
set(generatedDirectory "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generatedDirectory}")
set(protoRelativePath "navalcommand/gateway/v2/domain")
add_custom_command(
    OUTPUT "${generatedDirectory}/${protoRelativePath}.pb.cc" "${generatedDirectory}/${protoRelativePath}.pb.h"
    COMMAND protobuf::protoc --proto_path=${GATEWAY_API_PROTO_DIR}
        --cpp_out=${generatedDirectory} "${GATEWAY_API_PROTO_DIR}/${protoRelativePath}.proto"
    DEPENDS "${GATEWAY_API_PROTO_DIR}/${protoRelativePath}.proto" protobuf::protoc)
add_library(NavalCommand.Gateway.Api STATIC
    "${generatedDirectory}/${protoRelativePath}.pb.cc" NavalCommand/Gateway/Api/Codec.cpp)
target_include_directories(NavalCommand.Gateway.Api PUBLIC "${PROJECT_SOURCE_DIR}/public" "${generatedDirectory}")
target_link_libraries(NavalCommand.Gateway.Api PUBLIC
    NavalCommand.Units.Api NavalCommand.Observations.Api NavalCommand.Operations.Api
    NavalCommand.Environment.Api NavalCommand.Simulation.Api protobuf::libprotobuf)
NavalCommandConfigureTarget(NavalCommand.Gateway.Api)
