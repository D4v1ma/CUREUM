cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect on Apple platforms.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(CUREUM VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CUREUM_BUILD_TESTS "Build the CureumTests executable (unit tests, render and screenshot tools)" ON)
option(CUREUM_ENABLE_LTO "Enable link-time optimisation for the plugin" OFF)
set(CUREUM_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout. If empty, JUCE is downloaded.")
set(CUREUM_JUCE_TAG "9.0.6" CACHE STRING "JUCE git tag to download when CUREUM_JUCE_DIR is empty")

#----------------------------------------------------------------------------------------------
# JUCE
#----------------------------------------------------------------------------------------------
if(CUREUM_JUCE_DIR)
    add_subdirectory("${CUREUM_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${CUREUM_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

#----------------------------------------------------------------------------------------------
# Third-party code that ships with the repository
#----------------------------------------------------------------------------------------------
add_subdirectory(ThirdParty/zstd)

#----------------------------------------------------------------------------------------------
# Embedded resources (fonts, help pages)
#----------------------------------------------------------------------------------------------
file(GLOB CUREUM_HELP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Help/*.md")
juce_add_binary_data(CureumBinaryData
    NAMESPACE CureumBinary
    HEADER_NAME CureumBinaryData.h
    SOURCES
        Resources/Fonts/Barlow-Regular.ttf
        Resources/Fonts/Barlow-Medium.ttf
        Resources/Fonts/Barlow-SemiBold.ttf
        Resources/Fonts/Barlow-Bold.ttf
        ${CUREUM_HELP_FILES})
set_target_properties(CureumBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

#----------------------------------------------------------------------------------------------
# All CUREUM sources live in an INTERFACE library so that the plugin and the test tool
# compile exactly the same code.
#----------------------------------------------------------------------------------------------
file(GLOB_RECURSE CUREUM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

add_library(CureumCore INTERFACE)
target_sources(CureumCore INTERFACE ${CUREUM_SOURCES})
target_include_directories(CureumCore INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(CureumCore INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LV2=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_ARA=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    CUREUM_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(CureumCore INTERFACE
    CureumBinaryData
    cureum_zstd
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra)

#----------------------------------------------------------------------------------------------
# The plugin
#----------------------------------------------------------------------------------------------
set(CUREUM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CUREUM_FORMATS AU)
endif()

juce_add_plugin(Cureum
    PRODUCT_NAME "CUREUM"
    COMPANY_NAME "CUREUM Audio"
    COMPANY_WEBSITE "https://github.com/d4v1ma/cureum"
    BUNDLE_ID "com.cureumaudio.cureum"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Cure
    PLUGIN_CODE Crm1
    FORMATS ${CUREUM_FORMATS}
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_link_libraries(Cureum
    PRIVATE
        CureumCore
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(CUREUM_ENABLE_LTO)
    target_link_libraries(Cureum PUBLIC juce::juce_recommended_lto_flags)
endif()

#----------------------------------------------------------------------------------------------
# Tests / developer tools: unit tests, offline audio rendering, UI screenshots
#----------------------------------------------------------------------------------------------
if(CUREUM_BUILD_TESTS)
    juce_add_console_app(CureumTests PRODUCT_NAME "CureumTests")
    file(GLOB CUREUM_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp")
    target_sources(CureumTests PRIVATE ${CUREUM_TEST_SOURCES})
    target_compile_definitions(CureumTests PRIVATE
        CUREUM_TESTS=1
        CUREUM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(CureumTests
        PRIVATE
            CureumCore
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
