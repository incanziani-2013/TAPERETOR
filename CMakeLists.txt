cmake_minimum_required(VERSION 3.22)
project(TRAPERETOR VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# VST2 is only built when the VST2 SDK is available (Steinberg no longer
# distributes it, so it cannot live in this repo). Pass -DVST2_SDK_PATH=<dir>
# or set the VST2_SDK_PATH environment variable; <dir> must contain
# pluginterfaces/vst2.x/aeffect.h.
set(VST2_SDK_PATH "$ENV{VST2_SDK_PATH}" CACHE PATH "Path to the VST2 SDK (leave empty to skip VST2)")
set(TRAPERETOR_FORMATS VST3 Standalone)
if(VST2_SDK_PATH)
  file(TO_CMAKE_PATH "${VST2_SDK_PATH}" VST2_SDK_PATH)
  if(NOT EXISTS "${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
    message(FATAL_ERROR "VST2_SDK_PATH does not contain pluginterfaces/vst2.x/aeffect.h: ${VST2_SDK_PATH}")
  endif()
  juce_set_vst2_sdk_path("${VST2_SDK_PATH}")
  list(APPEND TRAPERETOR_FORMATS VST)
  message(STATUS "Building VST2 with SDK at ${VST2_SDK_PATH}")
else()
  message(STATUS "VST2_SDK_PATH not set: skipping VST2")
endif()

juce_add_plugin(TRAPERETOR
  COMPANY_NAME "TRAPERETOR"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  PLUGIN_MANUFACTURER_CODE Trpr
  PLUGIN_CODE Trap
  FORMATS ${TRAPERETOR_FORMATS}
  PRODUCT_NAME "TRAPERETOR")

target_sources(TRAPERETOR PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(TRAPERETOR PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TRAPERETOR
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags)
