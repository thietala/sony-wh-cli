cmake_minimum_required(VERSION 3.14)
project(sony-core LANGUAGES CXX)

add_library(sony-core
    src/SonyDevice.cpp
    src/DeviceService.cpp
    src/IpcProtocol.cpp
    src/IpcServer.cpp
    src/IpcClient.cpp
    src/JsonProtocol.cpp
)

target_compile_features(sony-core PUBLIC cxx_std_20)
target_compile_options(sony-core PRIVATE ${SONY_WARNING_FLAGS})

# SonyDevice::sendRaw and `sony-wh-cli raw` bypass every capability check, so
# they only exist in Debug builds. Release, RelWithDebInfo, MinSizeRel and an
# unset build type all leave them compiled out. PUBLIC so every consumer of
# SonyDevice.h sees the same declaration; the generator expression keeps this
# correct for multi-config generators (Xcode, Visual Studio) too.
target_compile_definitions(sony-core PUBLIC $<$<CONFIG:Debug>:SONY_ENABLE_RAW>)

target_include_directories(sony-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(sony-core
    PUBLIC
        sony-protocol
        sony-transport
        Threads::Threads
)

target_include_directories(sony-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../third_party>)
