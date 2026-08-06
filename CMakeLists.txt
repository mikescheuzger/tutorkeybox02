cmake_minimum_required(VERSION 3.22)
project(TutorKeyBox02 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_GUI "Build JUCE Desktop GUI Application" ON)

# ──────────────────────────────────────────────────────────────────────────────
# Fetch JUCE 8.0.4 via FetchContent
# ──────────────────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

# ──────────────────────────────────────────────────────────────────────────────
# Shared Source Files
# ──────────────────────────────────────────────────────────────────────────────
set(SHARED_SOURCES
    Source/Core/PresetManager.cpp
    Source/Core/MacroEngine.cpp
    Source/Audio/AudioEngine.cpp
    Source/Audio/MasterChain.cpp
    Source/Synth/LayeredSynth.cpp
    Source/Synth/CustomSamplerSound.cpp
    Source/Synth/CustomSamplerVoice.cpp
    Source/Synth/SampleContainerReader.cpp
    Source/Synth/SamplePackager.cpp
    Source/Synth/SfzReader.cpp
    Source/Synth/FXChannel.cpp
    Source/Network/NetworkServer.cpp
    Source/Network/DeployClient.cpp
    Source/Network/DeployServer.cpp
)

set(GUI_SOURCES
    Source/Core/Main.cpp
    Source/GUI/MainComponent.cpp
    Source/GUI/DeployControlBar.cpp
    Source/GUI/AudioSettingsWindow.cpp
    Source/GUI/LayerCardComponent.cpp
    Source/GUI/FXCardComponent.cpp
    Source/GUI/MasterChannelStrip.cpp
    Source/GUI/MacroPanelComponent.cpp
    Source/GUI/MidiConsoleWindow.cpp
    Source/GUI/InstrumentEditorWindow.cpp
    Source/GUI/TelemetryHeaderView.cpp
)

# ──────────────────────────────────────────────────────────────────────────────
# 1. Desktop GUI Application Target: TutorKeyBox
# ──────────────────────────────────────────────────────────────────────────────
if (BUILD_GUI)
    juce_add_gui_app(TutorKeyBox
        PRODUCT_NAME "TutorKeyBox 02"
        VERSION 2.0.0
    )

    target_sources(TutorKeyBox PRIVATE
        ${SHARED_SOURCES}
        ${GUI_SOURCES}
    )

    target_compile_definitions(TutorKeyBox PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_FLAC=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STANDALONE_APPLICATION=1
    )

    target_link_libraries(TutorKeyBox PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    )
endif()

# ──────────────────────────────────────────────────────────────────────────────
# 2. Raspberry Pi Headless Daemon Target: TutorKeyBoxCore
# ──────────────────────────────────────────────────────────────────────────────
juce_add_console_app(TutorKeyBoxCore
    PRODUCT_NAME "TutorKeyBoxCore"
    VERSION 2.0.0
)

target_sources(TutorKeyBoxCore PRIVATE
    Source/Core/HeadlessMain.cpp
    ${SHARED_SOURCES}
)

target_compile_definitions(TutorKeyBoxCore PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    HEADLESS_CORE=1
)

target_link_libraries(TutorKeyBoxCore PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_dsp
    juce::juce_events
)

if (UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(TutorKeyBoxCore PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(TutorKeyBoxCore PRIVATE ${GTK3_LIBRARIES})
endif()

# ──────────────────────────────────────────────────────────────────────────────
# 3. CLI Sample Packager Tool Target: TKBPackager
# ──────────────────────────────────────────────────────────────────────────────
juce_add_console_app(TKBPackager
    PRODUCT_NAME "TKBPackager"
    VERSION 2.0.0
)

target_sources(TKBPackager PRIVATE
    Source/Tools/PackagerMain.cpp
    Source/Synth/SamplePackager.cpp
    Source/Synth/SfzReader.cpp
    Source/Synth/CustomSamplerSound.cpp
    Source/Synth/CustomSamplerVoice.cpp
    Source/Synth/LayeredSynth.cpp
)

target_compile_definitions(TKBPackager PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
)

target_link_libraries(TKBPackager PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_dsp
)
