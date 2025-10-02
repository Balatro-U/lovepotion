dkp_add_asset_target(${PROJECT_NAME}_cafe_content content)

dkp_track_assets(${PROJECT_NAME}_cafe_content
    FOLDER shaders
    FILES
        color.gsh
        texture.gsh
)

# Track pre-compiled shaders from CafeGLSL if available
file(GLOB COMPILED_SHADERS "${CMAKE_SOURCE_DIR}/platform/cafe/shaders/*.gsh")
if(COMPILED_SHADERS)
    list(LENGTH COMPILED_SHADERS SHADER_COUNT)
    foreach(shader ${COMPILED_SHADERS})
        get_filename_component(shader_name ${shader} NAME)
        message(STATUS "Adding pre-compiled shader: ${shader_name}")
    endforeach()
    dkp_track_assets(${PROJECT_NAME}_cafe_content
        FOLDER shaders
        FILES ${COMPILED_SHADERS}
    )
    message(STATUS "CafeGLSL: Added ${SHADER_COUNT} pre-compiled shaders")
else()
    message(STATUS "CafeGLSL: No pre-compiled shaders found in ${CMAKE_SOURCE_DIR}/platform/cafe/shaders/")
endif()

dkp_track_assets(${PROJECT_NAME}_cafe_content
    FOLDER nogame
    FILES
        cartridge.png
        nogame.png
)

# Game folder is bundled using manual fusing after build
# No need to track as asset anymore

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

# find source -type f | grep "\.cpp$" | clip
target_sources(${PROJECT_NAME} PRIVATE
source/boot.cpp
source/debug_log.cpp
source/DebugLogger.cpp
source/cemu_compat.cpp
source/common/screen.cpp
source/driver/audio/AudioBuffer.cpp
source/driver/audio/DigitalSound.cpp
source/driver/audio/SoundChannel.cpp
source/driver/display/Framebuffer.cpp
source/driver/display/GX2.cpp
source/driver/display/Uniform.cpp
source/driver/display/poly_capture_globals.cpp
source/driver/EventQueue.cpp
source/modules/audio/Source.cpp
source/modules/graphics/Graphics.cpp
source/modules/graphics/Shader.cpp
source/modules/graphics/ShaderStage.cpp
source/modules/graphics/Texture.cpp
source/modules/joystick/vpad/Joystick.cpp
source/modules/joystick/kpad/Joystick.cpp
source/modules/joystick/JoystickModule.cpp
source/modules/keyboard/Keyboard.cpp
source/modules/mouse/Mouse.cpp
source/modules/system/System.cpp
source/modules/timer/Timer.cpp
source/modules/video/Video.cpp
source/modules/window/Window.cpp
source/WiiUGraphicsOptimizer.cpp
)
