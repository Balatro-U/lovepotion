#!/bin/bash

#installing deps
sudo chmod +x ./install_deps.sh
sudo bash ./install_deps.sh

BUILD_TYPE="Debug"
if [ ! -z "$1" ]; then
    BUILD_TYPE="$1"
fi

clear

echo "Building LOVE Potion for Wii U ($BUILD_TYPE build) without Docker..."

# Check for required tools
for tool in cmake ninja zip wget dos2unix; do
    if ! command -v $tool &> /dev/null; then
        echo "Error: $tool is not installed. Please install it first."
        read -p "Press enter to exit..."
        exit 1
    fi
done

# Create build/game.love from game folder
echo "Creating game.love from game folder..."
mkdir -p build
cd game
zip -r ../build/game.love .
cd ..
echo "game.love created."

# Compile shaders using CafeGLSL static compiler (must be installed and in PATH)
echo "Compiling shaders with CafeGLSL static compiler..."
CAFEGLSL_COMPILER="glslcompiler.elf"
SHADER_OUT="platform/cafe/content/shaders/game"
mkdir -p "$SHADER_OUT"

# Create default vertex shader
cat > /tmp/default_vs.glsl <<EOF
#version 330 core
layout(location = 0) in vec4 VertexPosition;
layout(location = 1) in vec4 VertexTexCoord;
layout(location = 2) in vec4 VertexColor;
uniform mat4 TransformProjectionMatrix;
varying vec4 VaryingTexCoord;
varying vec4 VaryingColor;
void main() {
    VaryingTexCoord = VertexTexCoord;
    VaryingColor = VertexColor;
    gl_Position = TransformProjectionMatrix * VertexPosition;
}
EOF

for shader in game/resources/shaders/*.fs; do
    name=$(basename "$shader" .fs)
    echo "Processing shader: $name"
    cat > "/tmp/${name}_fs.glsl" <<EOF
#version 330 core
uniform sampler2D MainTexture;
uniform float time;
uniform vec2 love_ScreenSize;
in vec4 VaryingTexCoord;
in vec4 VaryingColor;
out vec4 fragColor;
void main() {
    fragColor = texture(MainTexture, VaryingTexCoord.xy) * VaryingColor;
}
EOF
    "$CAFEGLSL_COMPILER" -vs /tmp/default_vs.glsl -ps "/tmp/${name}_fs.glsl" -o "$SHADER_OUT/${name}.gsh" && \
        echo "Successfully compiled ${name}.gsh" || echo "Failed to compile ${name}" 
done

echo "Shader compilation completed."

# Convert line endings for source files
find . -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h' | xargs dos2unix || true

# Configure CMake for Wii U build
echo "Configuring project with CMake..."
cd build
cmake .. -DNINTENDO_WIIU=ON -DUSE_CAFEGLSL=OFF -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake -G Ninja
if [ $? -ne 0 ]; then
    echo "CMake configure failed!"
    read -p "Press enter to exit..."
    exit 1
fi

# Build project
echo "Building project..."
ninja
if [ $? -ne 0 ]; then
    echo "Build failed!"
    read -p "Press enter to exit..."
    exit 1
fi


# Fuse game.love with wuhb if needed
if [ -f lovepotion.wuhb ] && [ -f game.love ]; then
    cat lovepotion.wuhb game.love > balatro.wuhb
    echo "=== GAME FUSED SUCCESSFULLY ==="
    ls -la balatro.wuhb
else
    echo "=== FUSING FAILED - Missing files ==="
    echo "lovepotion.wuhb exists: $(test -f lovepotion.wuhb && echo yes || echo no)"
    echo "game.love exists: $(test -f game.love && echo yes || echo no)"
fi

# Copy all build artifacts to build/ (like Dockerfile does to /output)
for f in balatro.wuhb lovepotion.elf lovepotion.rpx lovepotion.wuhb game.love; do
    if [ -f "$f" ]; then
        cp -v "$f" ../build/
    fi
done

# List all RPX/WUHB/ELF files in project (max 20)
echo "=== FIND ALL RPX/WUHB/ELF FILES IN PROJECT ==="
find .. -name "*.rpx" -o -name "*.wuhb" -o -name "*.elf" | head -20

# List all lovepotion executables (max 20)
echo "=== FIND ALL EXECUTABLE FILES ==="
find .. -name "*lovepotion*" -type f | head -20

# List build directory contents
echo "=== BUILD DIRECTORY CONTENTS ==="
ls -la ../build/

cd ..
# Run extract-build.sh if it exists
if [ -f ./extract-build.sh ]; then
    ./extract-build.sh
fi

read -p "Press enter to exit..."