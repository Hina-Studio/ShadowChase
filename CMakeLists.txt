cmake_minimum_required(VERSION 3.20)
project(SlashCoVR VERSION 0.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SLASHCO_ENABLE_RAYLIB "Build with raylib graphics window" ON)
option(SLASHCO_ENABLE_ENET "Build with ENet networking" OFF)
option(SLASHCO_ENABLE_STEAM "Link Valve Steamworks SDK (requires STEAM_SDK_PATH)" OFF)
option(SLASHCO_ENABLE_PACKAGING "Generate install rules and CPack presets" ON)

add_executable(SlashCoVR
    src/main.cpp
    src/Game.cpp
    src/core/Renderer.cpp
    src/core/Input.cpp
    src/core/Network.cpp
    src/core/Logger.cpp
    src/core/Config.cpp
    src/core/EventManager.cpp
    src/core/AssetManager.cpp
    src/core/Timer.cpp
    src/core/Random.cpp
    src/core/Audio.cpp
    src/core/Connectivity.cpp
    src/core/Platform.cpp
    src/core/Loc.cpp
    src/core/Profile.cpp
    src/core/LanDiscovery.cpp
    src/core/Profiler.cpp
    src/game/StateMachine.cpp
    src/game/Health.cpp
    src/game/KillerAI.cpp
    src/game/NavGrid.cpp
    src/game/World.cpp
    src/game/Replay.cpp
)

target_include_directories(SlashCoVR PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(SlashCoVR PRIVATE SLASHCO_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(SlashCoVR PRIVATE /utf-8)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.ini ${CMAKE_CURRENT_BINARY_DIR}/config.ini COPYONLY)

if(WIN32)
    target_link_libraries(SlashCoVR PRIVATE ws2_32)
endif()

if(SLASHCO_ENABLE_RAYLIB)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
    target_link_libraries(SlashCoVR PRIVATE raylib)
    target_compile_definitions(SlashCoVR PRIVATE SLASHCO_RAYLIB=1)
endif()

if(SLASHCO_ENABLE_ENET)
    include(FetchContent)
    FetchContent_Declare(enet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG v1.3.18
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(enet)
    target_link_libraries(SlashCoVR PRIVATE enet)
    target_compile_definitions(SlashCoVR PRIVATE SLASHCO_ENET=1)
endif()

if(SLASHCO_ENABLE_STEAM)
    if(NOT DEFINED STEAM_SDK_PATH)
        message(FATAL_ERROR "SLASHCO_ENABLE_STEAM requires -DSTEAM_SDK_PATH=<path to steamworks sdk>")
    endif()
    target_include_directories(SlashCoVR PRIVATE ${STEAM_SDK_PATH}/public)
    target_compile_definitions(SlashCoVR PRIVATE SLASHCO_ENABLE_STEAM=1)
    if(WIN32)
        target_link_libraries(SlashCoVR PRIVATE ${STEAM_SDK_PATH}/redistributable_bin/win64/steam_api64.lib)
    elseif(APPLE)
        target_link_libraries(SlashCoVR PRIVATE ${STEAM_SDK_PATH}/redistributable_bin/osx/libsteam_api.dylib)
    else()
        target_link_libraries(SlashCoVR PRIVATE ${STEAM_SDK_PATH}/redistributable_bin/linux64/libsteam_api.so)
    endif()
endif()

if(SLASHCO_ENABLE_PACKAGING)
    install(TARGETS SlashCoVR RUNTIME DESTINATION .)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.ini DESTINATION .)

    set(CPACK_PACKAGE_NAME "SlashCoVR")
    set(CPACK_PACKAGE_VENDOR "ShadowChase")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Shadow Chase - PvE co-op horror prototype")
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Shadow Chase")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    include(CPack)
endif()
