
cmake_minimum_required(VERSION 3.22)

project(ChefsTouch VERSION 0.3.1 LANGUAGES C CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory(third_party/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 654bdbc7a9db0dae90849b44abe91f339e9f0768)
    FetchContent_MakeAvailable(JUCE)
endif()
set(plugin_formats VST3)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    option(CHEFS_TOUCH_MAC_VST3 "Build VST3 alongside the macOS Audio Unit" ON)
    if(NOT CHEFS_TOUCH_MAC_VST3)
        set(plugin_formats)
    endif()
    list(APPEND plugin_formats AU)
endif()

juce_add_plugin(ChefsTouch
    COMPANY_NAME "Chef Tai"
    BUNDLE_ID "com.cheftai.chefstouch"
    PLUGIN_MANUFACTURER_CODE ChTi
    PLUGIN_CODE CfTc
    FORMATS ${plugin_formats}
    PRODUCT_NAME "Chefs Touch"
    PLUGIN_NAME "Chef's Touch"
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ChefsTouch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/Processing.cpp
    Source/DSP/BeatAnalysis.cpp
)

target_compile_features(ChefsTouch PRIVATE cxx_std_20)

target_compile_definitions(ChefsTouch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ChefsTouch PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_binary_data(ChefsTouchArt SOURCES
    Assets/Images/ChefSauceSheet.png
)
target_link_libraries(ChefsTouch PRIVATE ChefsTouchArt)

option(CHEFS_TOUCH_BUILD_TESTS "Build the plug-in host smoke tests" OFF)
if(CHEFS_TOUCH_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(ChefsTouchValidation PRODUCT_NAME "Chefs Touch Validation")
    target_sources(ChefsTouchValidation PRIVATE Tests/PluginValidation.cpp Source/DSP/BeatAnalysis.cpp Source/DSP/Processing.cpp)
    target_compile_features(ChefsTouchValidation PRIVATE cxx_std_20)
    target_compile_definitions(ChefsTouchValidation PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ChefsTouchValidation PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
    )
    if(TARGET ChefsTouch_VST3)
        target_compile_definitions(ChefsTouchValidation PRIVATE JUCE_PLUGINHOST_VST3=1)
        add_dependencies(ChefsTouchValidation ChefsTouch_VST3)
        add_test(NAME ChefsTouchVST3 COMMAND ChefsTouchValidation
            "$<TARGET_GENEX_EVAL:ChefsTouch_VST3,$<TARGET_PROPERTY:ChefsTouch_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
    endif()
    if(TARGET ChefsTouch_AU)
        target_compile_definitions(ChefsTouchValidation PRIVATE JUCE_PLUGINHOST_AU=1)
        target_link_libraries(ChefsTouchValidation PRIVATE "-framework AudioUnit" "-framework CoreAudioKit")
        add_dependencies(ChefsTouchValidation ChefsTouch_AU)
    endif()
endif()
