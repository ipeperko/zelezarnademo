cmake_minimum_required(VERSION 3.16)

project(zelezarna)

set(CMAKE_CXX_STANDARD 20)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

set (SOURCES
    main.cpp

    # deps
    deps/nlohmann/json.hpp
    deps/nlohmann/json_fwd.hpp

    # webserver
    webserver/HttpHandleRequest.cpp
    webserver/HttpHandleRequest.h
    webserver/HttpSession.h
    webserver/Webserver.cpp
    webserver/Webserver.h
    webserver/Webserver_common.h
    webserver/WebsocketSession.cpp
    webserver/WebsocketSession.h
    webserver/WebsocketDataBus.cpp
    webserver/WebsocketDataBus.h

    # application
    Application.cpp
    Application.h
    Daq.cpp
    Daq.h
    common.h
    KpiCalc.cpp
    KpiCalc.h
    Log.cpp
    Log.h
    Object.h
    TimeReference.cpp
    TimeReference.h
    )

find_package(dbm 0.1.11 REQUIRED)

find_package (Boost 1.71 REQUIRED COMPONENTS system program_options)

add_executable(zelezarna ${SOURCES})

target_link_libraries(zelezarna pthread
    dbm::dbm
    boost_system.a
    boost_program_options.a
    )

target_include_directories(zelezarna
    PRIVATE
    deps)

