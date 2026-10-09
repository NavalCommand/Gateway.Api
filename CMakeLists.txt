cmake_minimum_required(VERSION 4.2)
cmake_policy(SET CMP0168 NEW)
project(Gateway.Api VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(ncbuild
  GIT_REPOSITORY https://github.com/NavalCommand/Build.CMake.git
  GIT_TAG main)
FetchContent_MakeAvailable(ncbuild)
include("${ncbuild_SOURCE_DIR}/NavalCommand.Build.CMake.cmake")


NavalCommandConfigureModule()
