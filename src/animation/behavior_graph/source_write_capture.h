//
// Created by AmazingBuff on 2026/10/31.
//
#pragma once

namespace RE
{
    class Actor;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G3B-E2: install the two read-only setter observer detours (SetGraphVariableBool 63607,
    // SetGraphVariableInt 63609). Called once at kDataLoaded; a logged no-op unless
    // SourceWriteCapture is on with IndependentGraph=off and the other two source instruments
    // off. The pre-registered head bytes are verified before patching; a mismatch leaves the
    // process unpatched and logs the observed bytes.
    void install_source_write_capture();

    // Read-only observer, called once per post-UI frame: owns the F7 window state, drains the
    // write ring and samples the named phase variables for the transition anchors.
    void sample_source_write_capture(RE::Actor& actor);

    void arm_source_write_capture();
    void invalidate_source_write_capture();
}
PLUGIN_NAMESPACE_END
