cmake_minimum_required(VERSION 3.16)

project(ZUpdater VERSION 1.0.0 LANGUAGES CXX)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Qt setup
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network Concurrent)

# Source files
set(ZUPDATER_SOURCES
    src/ZUpdater.h
    src/ZUpdater.cpp
    src/ZDownloader.h
    src/ZDownloader.cpp
    src/ZDownloader.ui
)

# Create the static library
add_library(ZUpdater STATIC ${ZUPDATER_SOURCES})

# Set library properties
set_target_properties(ZUpdater PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "src/ZUpdater.h;src/ZDownloader.h"
)

# Link Qt libraries
target_link_libraries(ZUpdater 
    PUBLIC 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)

# Include directories
target_include_directories(ZUpdater
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/ZUpdater_autogen/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler definitions for building shared library
target_compile_definitions(ZUpdater PRIVATE ZUPDATER_LIBRARY)

# Only run install rules if this is the main project being built
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Install rules
    include(GNUInstallDirs)

    install(TARGETS ZUpdater
        EXPORT ZUpdaterTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ZUpdater
    )

    # Install export targets
    install(EXPORT ZUpdaterTargets
        FILE ZUpdaterTargets.cmake
        NAMESPACE ZUpdater::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZUpdater
    )

    # Create config file
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZUpdaterConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ZUpdaterConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZUpdater
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ZUpdaterConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ZUpdaterConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ZUpdaterConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZUpdater
    )
endif()

# Optional: Build example if requested
option(BUILD_ZUPDATER_EXAMPLES "Build ZUpdater examples" OFF)

if(BUILD_ZUPDATER_EXAMPLES)
    add_subdirectory(example)
endif()