cmake_minimum_required(VERSION 3.16)
project(PriceAlert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)
find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_executable(PriceAlert WIN32
    ../core_api/Types.h
    ../core_api/AuthService.h
    ../core_api/PortfolioService.h

    ../mock/MockAuthService.h
    ../mock/MockAuthService.cpp
    ../mock/MockPortfolioService.h
    ../mock/MockPortfolioService.cpp

    ui/main.cpp
    ui/Format.h
    ui/AlertBridge.h
    ui/AlertBridge.cpp
    ui/AuthMessages.h
    ui/AuthMessages.cpp
    ui/AuthWindow.h
    ui/AuthWindow.cpp
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/AssetTableModel.h
    ui/AssetTableModel.cpp
    ui/PriceChart.h
    ui/PriceChart.cpp
    ui/TrayNotifier.h
    ui/TrayNotifier.cpp
    ui/resources.qrc
)

target_include_directories(PriceAlert PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(PriceAlert PRIVATE Qt6::Widgets Qt6::Charts Threads::Threads)
