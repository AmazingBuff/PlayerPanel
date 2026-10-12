//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

// The shipped engine binary is Steam-wrapped, so its code section only exists as code inside a
// running process. Measured on both SkyrimSE.exe copies installed here: `.text` reads 8.000 bits/byte
// of entropy with no `CC` padding and no MSVC prologues, while `.rdata` strings are intact and the
// entry point sits inside a `.bind` stub section - i.e. the section is encrypted on disk and the stub
// decrypts it at load, which is also why the address library's offsets still resolve (they are RVAs,
// and the section layout is preserved). A static reader therefore gets noise, and no offline copy on
// this machine is analyzable.
//
// One dump of the live section removes that limit permanently: it is written relative to the module
// base, so the file's offset 0 is the text section's first byte and every RVA in this project's logs
// and crash logs indexes it directly. The sidecar records the anchors (resolved ids and vtables) that
// make the result verifiable and give the reader entry points; tools/re/engine_scan.py reads the pair.
class EngineImage
{
public:
    // Writes the module's text section next to the plugin log, once per process. Idempotent across
    // sessions (the engine image does not change), read-only, and it never fails the caller: every
    // failure is a log line.
    static void dump_once();
};

PLUGIN_NAMESPACE_END
