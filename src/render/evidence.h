#pragma once
// Forensic evidence I/O: the one-shot synchronous TGA dump of the studio
// target (F8 and the close-evidence contract). (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

PLUGIN_NAMESPACE_BEGIN
void dump_offscreen_to_log_dir();
PLUGIN_NAMESPACE_END
