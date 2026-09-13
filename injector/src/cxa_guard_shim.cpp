// Minimal C++ guard shim for mingw-w64 cross builds (no libstdc++ linked).
// Declaring the C++ functions with the literal `___cxa_guard_*` names gets
// mangled wrong, so the symbols are bound via asm labels instead.
static int ar_guard_acquire(unsigned char* guard) {
    if (guard && (*guard & 1)) return 0;   // already done (or in progress)
    if (guard) *guard = 1;                 // mark in progress
    return 1;                              // caller must initialize
}
static void ar_guard_release(unsigned char* guard) {
    if (guard) *guard = 3;                 // initialized
}
static void ar_guard_abort(unsigned char* guard) {
    if (guard) *guard = 0;
}

extern "C" int  cxa_guard_acquire_alias(unsigned char* g) __asm__("___cxa_guard_acquire");
extern "C" void cxa_guard_release_alias(unsigned char* g) __asm__("___cxa_guard_release");
extern "C" void cxa_guard_abort_alias(unsigned char* g)   __asm__("___cxa_guard_abort");

extern "C" int  cxa_guard_acquire_alias(unsigned char* g) { return ar_guard_acquire(g); }
extern "C" void cxa_guard_release_alias(unsigned char* g) { ar_guard_release(g); }
extern "C" void cxa_guard_abort_alias(unsigned char* g)   { ar_guard_abort(g); }
