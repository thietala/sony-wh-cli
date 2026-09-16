cmake_minimum_required(VERSION 3.14)
project(sony-wh-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Applied to this project's own targets only (never to fetched dependencies
# like Catch2), so a warning in our code fails the build instead of quietly
# shipping — see the MSVC C4101 that slipped through CI before this existed.
# -Wmissing-field-initializers is excluded: this codebase leans on C++20
# designated initializers to skip fields that already have a sane default
# (std::optional members, etc), which is exactly what that warning flags.
if(MSVC)
    set(SONY_WARNING_FLAGS /W4 /WX)
else()
    set(SONY_WARNING_FLAGS -Wall -Wextra -Wno-missing-field-initializers -Werror)
endif()

include(CTest)
find_package(Threads REQUIRED)

add_subdirectory(libs/sony-transport)
add_subdirectory(libs/sony-protocol)
add_subdirectory(libs/sony-core)

add_executable(sony-wh-cli main.cpp)
target_compile_features(sony-wh-cli PRIVATE cxx_std_20)
target_compile_options(sony-wh-cli PRIVATE ${SONY_WARNING_FLAGS})
target_link_libraries(sony-wh-cli PRIVATE sony-core sony-protocol sony-transport Threads::Threads)

# Background daemon that owns the Bluetooth connection and serves it over the
# local IPC socket; sony-wh-cli is a thin client to it by default. IPC itself
# is Unix-only (see IpcServer::start()), so this still builds on Windows for
# CI parity but exits with a clear error there instead of listening.
add_executable(sonyd sonyd.cpp)
target_compile_features(sonyd PRIVATE cxx_std_20)
target_compile_options(sonyd PRIVATE ${SONY_WARNING_FLAGS})
target_link_libraries(sonyd PRIVATE sony-core sony-protocol sony-transport Threads::Threads)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS sony-wh-cli sonyd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# systemd --user unit for running sonyd as a per-user background service.
# Only wired up when installing into a prefix systemd's user unit search
# path actually covers (/usr, /usr/local); at other prefixes the file is
# still installed but the user must symlink it into ~/.config/systemd/user.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES packaging/systemd/sonyd.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user)
endif()
