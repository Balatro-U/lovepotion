// Diagnostic globals used for capturing a sample polygon vertex and
// computing its transformed position in GX2::prepareDraw for logging.
// Intentionally unconditionally defined so the linker always sees them
// when this translation unit is part of the build. They are only
// referenced in Wii U specific code paths guarded by __WIIU__.
namespace love {
float g_lastPolyX = 0.0f;
float g_lastPolyY = 0.0f;
bool  g_haveLastPoly = false;
} // namespace love
