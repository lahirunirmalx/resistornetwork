# SPDX-License-Identifier: MIT
#
# Resistor Network Calculator
# Cross-platform build: Linux, Windows, macOS
#
# Minimum CMake 3.10 for broad distro compatibility
#

cmake_minimum_required(VERSION 3.10)

project(resistorcal
    VERSION 1.0.0
    DESCRIPTION "Find resistor combinations for target resistance"
    LANGUAGES C
)

# C99 - works everywhere
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# Platform Detection
# ============================================================================

if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    message(STATUS "Platform: Windows")
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    message(STATUS "Platform: macOS")
else()
    set(PLATFORM_LINUX TRUE)
    message(STATUS "Platform: Linux/Unix")
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
    
    # Windows 7+ compatibility (MSYS2/MinGW)
    if(WIN32)
        add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7
        add_definitions(-DWINVER=0x0601)
    endif()
elseif(MSVC)
    add_compile_options(/W3 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7
    add_definitions(-DWINVER=0x0601)
endif()

# ============================================================================
# Find GTK3
# ============================================================================

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# ============================================================================
# Installation Directories
# ============================================================================

include(GNUInstallDirs)

if(NOT DEFINED CMAKE_INSTALL_DATADIR)
    set(CMAKE_INSTALL_DATADIR "share")
endif()

set(RESISTORCAL_DATADIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/resistorcal")

# ============================================================================
# Source Files
# ============================================================================

set(SOURCES src/resistor.c)

# Windows: Add resource file for icon
if(PLATFORM_WINDOWS AND EXISTS "${CMAKE_SOURCE_DIR}/data/icons/resistorcal.ico")
    # Generate .rc file that references the .ico
    configure_file(
        "${CMAKE_SOURCE_DIR}/data/resistorcal.rc"
        "${CMAKE_BINARY_DIR}/resistorcal.rc"
        COPYONLY
    )
    # Copy ICO to build dir
    configure_file(
        "${CMAKE_SOURCE_DIR}/data/icons/resistorcal.ico"
        "${CMAKE_BINARY_DIR}/resistorcal.ico"
        COPYONLY
    )
    list(APPEND SOURCES "${CMAKE_BINARY_DIR}/resistorcal.rc")
endif()

# ============================================================================
# Build Executable
# ============================================================================

if(PLATFORM_MACOS)
    # macOS: Build as app bundle
    set(MACOSX_BUNDLE_BUNDLE_NAME "Resistor Calculator")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.resistorcal.app")
    set(MACOSX_BUNDLE_ICON_FILE "resistorcal.icns")
    
    add_executable(resistorcal MACOSX_BUNDLE ${SOURCES})
    
    # Copy icon to bundle if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/data/icons/resistorcal.icns")
        set_source_files_properties(
            "${CMAKE_SOURCE_DIR}/data/icons/resistorcal.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(resistorcal PRIVATE
            "${CMAKE_SOURCE_DIR}/data/icons/resistorcal.icns"
        )
    endif()
    
    # Copy UI file to bundle Resources
    add_custom_command(TARGET resistorcal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:resistorcal>/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/data/ui.glade"
            "$<TARGET_BUNDLE_CONTENT_DIR:resistorcal>/Resources/ui.glade"
        COMMENT "Copying ui.glade to app bundle"
    )
elseif(PLATFORM_WINDOWS)
    # Windows: GUI application (no console window)
    add_executable(resistorcal WIN32 ${SOURCES})
else()
    # Linux: Standard executable
    add_executable(resistorcal ${SOURCES})
endif()

# ============================================================================
# Compile Definitions
# ============================================================================

# Tell the code where to find ui.glade at runtime
if(PLATFORM_MACOS)
    # macOS: Look in bundle Resources first
    target_compile_definitions(resistorcal PRIVATE
        DATADIR="Resources"
        PLATFORM_MACOS=1
    )
elseif(PLATFORM_WINDOWS)
    target_compile_definitions(resistorcal PRIVATE
        DATADIR="."
        PLATFORM_WINDOWS=1
    )
else()
    target_compile_definitions(resistorcal PRIVATE
        DATADIR="${RESISTORCAL_DATADIR}"
        PLATFORM_LINUX=1
    )
endif()

# ============================================================================
# Link Libraries
# ============================================================================

target_include_directories(resistorcal PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(resistorcal PRIVATE ${GTK3_LIBRARY_DIRS})

if(PLATFORM_MACOS)
    # macOS: Link CoreFoundation for bundle path resolution
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    target_link_libraries(resistorcal PRIVATE
        ${GTK3_LIBRARIES}
        m
        ${COREFOUNDATION_LIBRARY}
    )
else()
    target_link_libraries(resistorcal PRIVATE ${GTK3_LIBRARIES} m)
endif()

# ============================================================================
# Installation (Linux)
# ============================================================================

if(PLATFORM_LINUX)
    # Binary
    install(TARGETS resistorcal
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # UI file
    install(FILES data/ui.glade
        DESTINATION ${CMAKE_INSTALL_DATADIR}/resistorcal
    )
    
    # Desktop file
    install(FILES data/resistorcal.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    
    # Icons (various sizes for different contexts)
    foreach(SIZE 16 24 32 48 64 128 256)
        if(EXISTS "${CMAKE_SOURCE_DIR}/data/icons/resistorcal-${SIZE}.png")
            install(FILES "data/icons/resistorcal-${SIZE}.png"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps
                RENAME resistorcal.png
            )
        endif()
    endforeach()
    
    # Scalable SVG icon
    install(FILES data/icons/resistorcal.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    )
endif()

# Installation (Windows)
if(PLATFORM_WINDOWS)
    install(TARGETS resistorcal RUNTIME DESTINATION .)
    install(FILES data/ui.glade DESTINATION .)
    
    # Bundle GTK3 DLLs (done separately or via NSIS installer)
endif()

# Installation (macOS)
if(PLATFORM_MACOS)
    install(TARGETS resistorcal BUNDLE DESTINATION .)
endif()

# ============================================================================
# CPack Configuration
# ============================================================================

set(CPACK_PACKAGE_NAME "resistorcal")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_CONTACT "maintainer@example.com")
set(CPACK_PACKAGE_VENDOR "ResistorCal")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(PLATFORM_LINUX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgtk-3-0")
    set(CPACK_RPM_PACKAGE_REQUIRES "gtk3")
    set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
elseif(PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Resistor Calculator")
    set(CPACK_NSIS_PACKAGE_NAME "ResistorCalculator")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/data/icons/resistorcal.ico")
elseif(PLATFORM_MACOS)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Resistor Calculator")
endif()

include(CPack)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "resistorcal ${PROJECT_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
if(PLATFORM_LINUX)
    message(STATUS "  Data directory: ${RESISTORCAL_DATADIR}")
endif()
message(STATUS "  GTK3 version:   ${GTK3_VERSION}")
message(STATUS "")
