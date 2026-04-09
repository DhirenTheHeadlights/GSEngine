# Descriptor Buffer Crash Investigation

## Hardware
- NVIDIA GeForce RTX 5090 (Blackwell)
- VK_EXT_descriptor_buffer enabled
- Descriptor buffer properties:
  - offset alignment: 64
  - storage buffer descriptor: 16 bytes
  - combined image sampler descriptor: 4 bytes
  - acceleration structure descriptor: 8 bytes

## Bugs Found and Fixed

### 1. `bindDescriptorBuffersEXT` offset invalidation

**Root cause:** `descriptor_heap::bind()` called `bindDescriptorBuffersEXT` every time any
descriptor set was bound. Per the Vulkan spec, calling `bindDescriptorBuffersEXT` invalidates
all previously set descriptor buffer offsets. This meant:

- `bind_descriptors(pipeline, set_0_region)` would call `bindDescriptorBuffersEXT` + `setDescriptorBufferOffsetsEXT(set=0)`
- `commit(writer, pipeline, 1)` would call `bindDescriptorBuffersEXT` again, **invalidating set 0's offset**
- The GPU would then read set 0 from an undefined offset

**Fix:**
- Call `bindDescriptorBuffersEXT` exactly **once** at the start of `render_graph::execute()`
- `descriptor_heap::bind()` now only calls `setDescriptorBufferOffsetsEXT`
- `descriptor_writer::commit()` only calls `setDescriptorBufferOffsetsEXT`

**Files changed:** `DescriptorHeap.cppm`, `RenderGraph.cppm`

### 2. UI renderer stale descriptor offset

**Root cause:** The UI renderer deduplicated descriptor commits by tracking bound texture/font IDs.
With descriptor buffers, each draw needs its own transient descriptor region. Skipping `begin()` +
`commit()` for a "same texture" batch would leave the GPU reading from the previous batch's
transient offset, which might have been overwritten or pointed to wrong data.

**Fix:** Always call `begin()` + write + `commit()` for every batch. Skip the draw entirely
if no valid descriptor can be written (texture/font handle is invalid).

**Files changed:** `UiRenderer.cppm`

## Remaining Crash

### Symptoms
- `ReadInvalid` at various garbage addresses (changes each run)
- `InstructionPointerUnknown` / `InstructionPointerFault`
- Crash detected at `waitForFences` or `waitIdle` (during BLAS building)
- Occurs ~5-15 seconds after startup, correlating with geometry loading
- Device fault vendor binary is present (100-300 KB)

### What was ruled out
- **Push descriptor data is valid** -- no null buffer addresses, no null image views
- **Descriptor layout sizes and binding offsets** match what the driver reports
- **Transient zone alignment** is correct (aligned to `descriptorBufferOffsetAlignment`)
- **Persistent descriptor writes** during init are correct
- **UI push descriptors** work correctly for thousands of frames before crash
- **Disabling individual renderers** (depth prepass, forward, RT shadow, light culling, cull compute, skin compute) does not prevent the crash when other renderers remain active
- **GPU-Assisted Validation** is incompatible with descriptor buffers (spec violation) and was removed

### Leading theories
1. **RTX 5090 driver bug with `VK_EXT_descriptor_buffer`** -- The extension is relatively new and
   Blackwell is a new architecture. The 4-byte combined image sampler descriptors suggest a compact
   descriptor format that may have edge cases.
2. **Interaction between acceleration structure commands and descriptor buffer usage** in the same
   command buffer -- TLAS rebuilds happen alongside descriptor buffer-based draws.
3. **Cross-pipeline-layout offset state** -- Multiple pipeline layouts with incompatible set layouts
   may cause driver-internal state corruption.

## Next Steps: GPU Frame Capture

Use NVIDIA Nsight Graphics or RenderDoc to capture the crashing frame. The device fault vendor
binary (logged as "Device fault vendor binary size: N bytes") can be decoded by NVIDIA tools.

### Nsight Graphics -- Crash Frame Capture

1. **Install** NVIDIA Nsight Graphics (free from https://developer.nvidia.com/nsight-graphics)

2. **Configure the capture:**
   - Open Nsight Graphics
   - File > New Project
   - Activity: "Frame Debugger"
   - Application Executable: point to your built .exe
   - Working Directory: your project root

3. **Enable crash capture:**
   - In Connection > Activity Settings:
     - Check **"Generate GPU Crash Dump on TDR/Device Lost"**
     - Check **"Collect GPU Crash Dump on Device Lost"**
     - Under Advanced, set "Crash Dump File" to a path you can find easily
   - This automatically captures GPU state when the device is lost

4. **Run and wait for crash:**
   - Click "Launch"
   - The app will run until it crashes
   - Nsight will save a GPU crash dump (`.nv-gpudmp` file)

5. **Analyze the crash dump:**
   - File > Open > select the `.nv-gpudmp` file
   - The "GPU Crash Dump Inspector" shows:
     - Which shader was executing when the fault occurred
     - The exact instruction that faulted
     - The descriptor state at the time of the fault
     - The buffer/image addresses being accessed
   - This will tell you definitively whether the issue is a bad descriptor,
     a freed buffer, or a driver bug

### RenderDoc -- Manual Frame Capture

RenderDoc does NOT support `VK_EXT_descriptor_buffer` (as of RenderDoc 1.35). It will likely fail
to capture or replay frames that use this extension. Use Nsight instead.

If you temporarily disable descriptor buffers and fall back to traditional descriptor sets,
RenderDoc can be used:

1. **Install** RenderDoc (https://renderdoc.org)
2. Launch your app through RenderDoc
3. Press F12 or PrintScreen to capture a frame
4. Inspect the API calls, resource state, and shader inputs

### Nsight Aftermath -- Lightweight Crash Dump (Alternative)

If full Nsight Graphics is too heavy, you can integrate NVIDIA Aftermath SDK directly into
the engine for automated crash dumps without an external tool:

1. Download the Aftermath SDK from NVIDIA
2. Call `GFSDK_Aftermath_EnableGpuCrashDumps()` at startup
3. On device lost, call `GFSDK_Aftermath_GetCrashDumpStatus()` and save the dump
4. Use Nsight Graphics to open the dump file offline

This is lighter weight and captures every crash automatically in normal runs.

### Interpreting the Device Fault Vendor Binary

The engine already captures the vendor binary size. To actually SAVE the binary for Nsight:

```cpp
// In your device lost handler, after querying fault info:
auto [result, counts] = device.getDeviceFaultInfoEXT();
if (counts.vendorBinarySize > 0) {
    std::vector<std::byte> vendor_binary(counts.vendorBinarySize);
    vk::DeviceFaultInfoEXT info;
    info.pVendorBinaryData = vendor_binary.data();
    device.getDeviceFaultInfoEXT(&info);
    // Write vendor_binary to a file
    std::ofstream out("gpu_crash.bin", std::ios::binary);
    out.write(reinterpret_cast<const char*>(vendor_binary.data()), vendor_binary.size());
}
```

Then open this `.bin` file in Nsight Graphics > GPU Crash Dump Inspector.
