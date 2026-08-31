cmake_minimum_required(VERSION 3.20)
project(SoundMod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "This project targets Windows only (WebView2 + virtual-cable audio routing + Win32 input hooks).")
endif()

add_executable(SoundMod WIN32
    src/main.cpp
    src/AudioEngine.cpp
    src/InputHook.cpp
    src/BrowserPanel.cpp
    src/Config.cpp
)

target_include_directories(SoundMod PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2/build/native/include
    ${CMAKE_SOURCE_DIR}/packages/Microsoft.Windows.ImplementationLibrary/include
)

target_link_libraries(SoundMod PRIVATE
    ${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2/build/native/x64/WebView2LoaderStatic.lib
    shlwapi
)

target_compile_definitions(SoundMod PRIVATE UNICODE _UNICODE)

# Copy the WebView2Loader runtime dependency and default soundpack folder next to the exe.
add_custom_command(TARGET SoundMod POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SoundMod>/soundpacks/default
)
