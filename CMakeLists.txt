cmake_minimum_required(VERSION 3.20)
project(SlashCo VERSION 0.3.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SLASHCO_ENABLE_RAYLIB "Build the graphical client (raylib)" ON)
option(SLASHCO_ENABLE_ENET "Build networking (ENet)" ON)
option(SLASHCO_ENABLE_IMGUI "Build debug panel (Dear ImGui + rlImGui)" ON)
option(SLASHCO_ENABLE_PACKAGING "Generate install rules and CPack presets" ON)

set(SHARED_SOURCES
    src/shared/Sim.cpp
    src/shared/Protocol.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/Profiler.cpp
    src/core/Random.cpp
    src/core/Timer.cpp
)

add_library(slashco_shared STATIC ${SHARED_SOURCES})
target_include_directories(slashco_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(slashco_shared PUBLIC SLASHCO_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(slashco_shared PRIVATE /utf-8)
endif()

if(SLASHCO_ENABLE_ENET)
    include(FetchContent)
    FetchContent_Declare(enet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG v1.3.18
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(enet)
endif()

add_executable(SlashCoServer src/server/main_server.cpp)
target_link_libraries(SlashCoServer PRIVATE slashco_shared)
if(SLASHCO_ENABLE_ENET)
    target_link_libraries(SlashCoServer PRIVATE enet)
    target_include_directories(SlashCoServer PRIVATE ${enet_SOURCE_DIR}/include)
    target_compile_definitions(SlashCoServer PRIVATE SLASHCO_ENET=1)
endif()
if(WIN32)
    target_link_libraries(SlashCoServer PRIVATE ws2_32 winmm)
endif()
if(MSVC)
    target_compile_options(SlashCoServer PRIVATE /utf-8)
endif()

if(SLASHCO_ENABLE_RAYLIB)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)

    if(SLASHCO_ENABLE_IMGUI)
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG master
            GIT_SHALLOW TRUE
        )
        FetchContent_Declare(rlimgui
            GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
            GIT_TAG main
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(imgui rlimgui)

        add_library(slashco_imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${rlimgui_SOURCE_DIR}/rlImGui.cpp
        )
        target_include_directories(slashco_imgui PUBLIC ${imgui_SOURCE_DIR} ${rlimgui_SOURCE_DIR})
        target_link_libraries(slashco_imgui PUBLIC raylib)
        if(MSVC)
            target_compile_options(slashco_imgui PRIVATE /utf-8)
        endif()
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/client/main_client.cpp)
        add_executable(SlashCoClient
            src/client/main_client.cpp
            src/client/ClientApp.cpp
            src/client/DebugPanel.cpp
        )
        target_include_directories(SlashCoClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                                                         ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(SlashCoClient PRIVATE slashco_shared raylib)
        if(SLASHCO_ENABLE_IMGUI)
            target_link_libraries(SlashCoClient PRIVATE slashco_imgui)
            target_compile_definitions(SlashCoClient PRIVATE SLASHCO_IMGUI=1)
        endif()
        if(SLASHCO_ENABLE_ENET)
            target_link_libraries(SlashCoClient PRIVATE enet)
            target_include_directories(SlashCoClient PRIVATE ${enet_SOURCE_DIR}/include)
            target_compile_definitions(SlashCoClient PRIVATE SLASHCO_ENET=1)
        endif()
        if(WIN32)
            target_link_libraries(SlashCoClient PRIVATE ws2_32 winmm)
        endif()
        if(MSVC)
            target_compile_options(SlashCoClient PRIVATE /utf-8)
        endif()
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.ini ${CMAKE_CURRENT_BINARY_DIR}/config.ini COPYONLY)

if(SLASHCO_ENABLE_PACKAGING)
    install(TARGETS SlashCoServer RUNTIME DESTINATION .)
    if(TARGET SlashCoClient)
        install(TARGETS SlashCoClient RUNTIME DESTINATION .)
    endif()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.ini DESTINATION .)

    set(CPACK_PACKAGE_NAME "SlashCo")
    set(CPACK_PACKAGE_VENDOR "ShadowChase")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Shadow Chase - PvE co-op horror (dedicated server + client)")
    set(CPACK_GENERATOR "ZIP")
    include(CPack)
endif()
