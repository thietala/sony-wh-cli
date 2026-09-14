cmake_minimum_required(VERSION 3.14)
project(sony-wh-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
find_package(Threads REQUIRED)

add_subdirectory(libs/sony-transport)
add_subdirectory(libs/sony-protocol)
add_subdirectory(libs/sony-core)

add_executable(sony-wh-cli main.cpp)
target_compile_features(sony-wh-cli PRIVATE cxx_std_20)
target_link_libraries(sony-wh-cli PRIVATE sony-core sony-protocol sony-transport Threads::Threads)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS sony-wh-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
