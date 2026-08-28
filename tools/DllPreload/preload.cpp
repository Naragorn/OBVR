// Starts a program with one DLL preloaded into it by full path.
//
// Exists for one job: making apitrace's d3dretrace replay a trace against
// DXVK's d3d9.dll on Windows. The retracer resolves "d3d9.dll" through the
// hardened search path, so a copy beside its executable is ignored - but
// the Windows loader identifies already-loaded modules by base name, so a
// DLL injected by full path before the first request wins every later
// LoadLib