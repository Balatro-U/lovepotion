@echo off

set BUILD_TYPE=Debug
if not "%1"=="" set BUILD_TYPE=%1

cls

echo Building LOVE Potion for Wii U using Docker (%BUILD_TYPE% build)...

echo Checking Docker...
docker info >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Docker is not running. Start Docker Desktop and try again.
    exit /b 1
)

REM Build Docker image
if exist "build" rmdir /s /q build
echo Building Docker image...
docker build --build-arg BUILD_TYPE=%BUILD_TYPE% -f Dockerfile.wiiu -t lovepotion-wiiu .

if %ERRORLEVEL% neq 0 (
    echo Docker build failed!
    exit /b 1
)



REM Create build directory if it doesn't exist
if not exist "build" mkdir build

REM Run container and copy files
echo Extracting built files...
echo Running: docker run --rm -v "%CD%\build:/host/build" lovepotion-wiiu sh -c "cp -r /output/* /host/build/ 2>/dev/null || echo 'No output files to copy'"
docker run --rm -v "%CD%\build:/host/build" lovepotion-wiiu sh -c "cp -r /output/* /host/build/ 2>/dev/null || echo 'No output files to copy'"

echo Checking build directory...
dir /b build

cd
.\extract-build.bat