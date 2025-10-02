#include "common/ppc_debugger.hpp"
#include <cstdio>
#include <cstring>
#include <whb/log.h>
#include <gx2/display.h>
#include <gx2/state.h>
#include <gx2/mem.h>
#include <gx2/context.h>

namespace love {
namespace ppc {

char PPCDebugger::debug_buffer[4096];

void PPCDebugger::writeToFile(const char* message) {
    FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (f) {
        fprintf(f, "[PPC_DEBUG] %s\n", message);
        fclose(f);
    }
    // Also to WHB console
    WHBLogPrintf("[PPC_DEBUG] %s", message);
}

void PPCDebugger::logGX2Operation(const char* operation, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char details[2048];
    vsnprintf(details, sizeof(details), format, args);
    va_end(args);
    
    snprintf(debug_buffer, sizeof(debug_buffer), 
        "GX2_OP[%s]: %s", operation, details);
    writeToFile(debug_buffer);
}

void PPCDebugger::logFramebufferState() {
    // Get current framebuffer info
    GX2ColorBuffer* colorBuffer = nullptr;
    GX2DepthBuffer* depthBuffer = nullptr;
    
    // Try to get current render targets
    // Note: This might need adjustment based on actual GX2 API
    snprintf(debug_buffer, sizeof(debug_buffer),
        "FRAMEBUFFER_STATE: ColorBuffer=%p, DepthBuffer=%p", 
        (void*)colorBuffer, (void*)depthBuffer);
    writeToFile(debug_buffer);
    
    // Log display state
    GX2TVScanMode tvScanMode = GX2GetSystemTVScanMode();
    GX2DrcRenderMode drcRenderMode = GX2GetSystemDRCMode();
    
    snprintf(debug_buffer, sizeof(debug_buffer),
        "DISPLAY_STATE: TV_ScanMode=%d, DRC_RenderMode=%d", 
        (int)tvScanMode, (int)drcRenderMode);
    writeToFile(debug_buffer);
}

void PPCDebugger::logVertexData(const void* data, size_t size) {
    if (!data) {
        writeToFile("VERTEX_DATA: NULL data pointer");
        return;
    }
    
    const float* vertices = (const float*)data;
    size_t floatCount = size / sizeof(float);
    
    snprintf(debug_buffer, sizeof(debug_buffer),
        "VERTEX_DATA: size=%zu bytes, floats=%zu", size, floatCount);
    writeToFile(debug_buffer);
    
    // Log first few vertices
    for (size_t i = 0; i < floatCount && i < 12; i += 2) {
        if (i + 1 < floatCount) {
            snprintf(debug_buffer, sizeof(debug_buffer),
                "  VERTEX[%zu]: x=%.2f, y=%.2f", i/2, vertices[i], vertices[i+1]);
            writeToFile(debug_buffer);
        }
    }
}

void PPCDebugger::logShaderState() {
    writeToFile("SHADER_STATE: Checking current shader program");
    
    // Log shader binding state
    snprintf(debug_buffer, sizeof(debug_buffer),
        "SHADER_STATE: Vertex/Fragment shaders active");
    writeToFile(debug_buffer);
}

void PPCDebugger::logPresentState() {
    writeToFile("PRESENT_STATE: About to present frame");
    
    // Check if we have valid render targets
    logFramebufferState();
    
    writeToFile("PRESENT_STATE: Calling GX2CopyColorBufferToScanBuffer");
}

void PPCDebugger::markCriticalPath(const char* function, const char* file, int line) {
    snprintf(debug_buffer, sizeof(debug_buffer),
        "CRITICAL_PATH: %s() at %s:%d", function, file, line);
    writeToFile(debug_buffer);
}

void PPCDebugger::logMemoryState() {
    // Get memory usage info
    size_t freeMemory = 0; // Would need actual Wii U memory API
    
    snprintf(debug_buffer, sizeof(debug_buffer),
        "MEMORY_STATE: Free=%zu bytes", freeMemory);
    writeToFile(debug_buffer);
}

void PPCDebugger::logDrawCallDetails(const char* type, float x, float y, float w, float h) {
    snprintf(debug_buffer, sizeof(debug_buffer),
        "DRAW_CALL[%s]: pos=(%.2f,%.2f) size=(%.2f,%.2f)", 
        type, x, y, w, h);
    writeToFile(debug_buffer);
    
    // Check if coordinates are within screen bounds
    bool inBounds = (x >= 0 && y >= 0 && x + w <= 854 && y + h <= 480);
    snprintf(debug_buffer, sizeof(debug_buffer),
        "DRAW_BOUNDS_CHECK: inBounds=%s (screen=854x480)", 
        inBounds ? "YES" : "NO");
    writeToFile(debug_buffer);
}

void PPCDebugger::logScreenState() {
    writeToFile("SCREEN_STATE: Analyzing current screen configuration");
    
    // Log TV output state
    GX2TVScanMode tvMode = GX2GetSystemTVScanMode();
    snprintf(debug_buffer, sizeof(debug_buffer),
        "SCREEN_STATE: TV_ScanMode=%d", (int)tvMode);
    writeToFile(debug_buffer);
    
    // Log DRC (GamePad) state
    GX2DrcRenderMode drcMode = GX2GetSystemDRCMode();
    snprintf(debug_buffer, sizeof(debug_buffer),
        "SCREEN_STATE: DRC_RenderMode=%d", (int)drcMode);
    writeToFile(debug_buffer);
}

void PPCDebugger::logViewportState() {
    writeToFile("VIEWPORT_STATE: Checking current viewport settings");
    
    // This would need actual viewport query from GX2
    snprintf(debug_buffer, sizeof(debug_buffer),
        "VIEWPORT_STATE: Need GX2 viewport query implementation");
    writeToFile(debug_buffer);
}

void PPCDebugger::logCoordinateTransform(float in_x, float in_y, float* out_x, float* out_y) {
    snprintf(debug_buffer, sizeof(debug_buffer),
        "COORD_TRANSFORM: input=(%.2f,%.2f) -> output=(%.2f,%.2f)", 
        in_x, in_y, out_x ? *out_x : -1.0f, out_y ? *out_y : -1.0f);
    writeToFile(debug_buffer);
}

void PPCDebugger::forceVisiblePixel(int x, int y, uint32_t color) {
    snprintf(debug_buffer, sizeof(debug_buffer),
        "FORCE_PIXEL: pos=(%d,%d) color=0x%08X", x, y, color);
    writeToFile(debug_buffer);
    
    // This would need direct framebuffer access
    writeToFile("FORCE_PIXEL: Direct framebuffer write would go here");
}

void PPCDebugger::drawDebugGrid() {
    writeToFile("DEBUG_GRID: Drawing visibility test grid");
    
    // Log that we're about to draw test patterns
    for (int y = 0; y < 480; y += 100) {
        for (int x = 0; x < 854; x += 100) {
            snprintf(debug_buffer, sizeof(debug_buffer),
                "DEBUG_GRID: Grid point at (%d,%d)", x, y);
            writeToFile(debug_buffer);
        }
    }
}

void PPCDebugger::testMinimalDraw() {
    writeToFile("MINIMAL_DRAW: Testing absolute minimal drawing operation");
    
    logScreenState();
    logFramebufferState();
    
    writeToFile("MINIMAL_DRAW: Ready for single pixel test");
}

} // namespace ppc
} // namespace love
