# DX12 bindless buffer read views

Written 2026-10-24. Status: **approved by the owner 2026-10-24 and implemented.** The Vulkan backend
and `Commands.cppm` are deliberately left unchanged.

## The defect

When DX12 shaders read a bindless buffer through an SRV-typed resource (`StructuredBuffer<T>`,
`ByteAddressBuffer`, or the `StructuredBuffer<T_ubo_data>` used for uniform blocks), the reads came
back as zeros or garbage. Under SM 6.6 `ResourceDescriptorHeap`, the descriptor in that slot has to
match the type the shader casts it to. However, `create_buffer` wrote one descriptor per buffer:

- a **UAV** for every `writable` buffer, which covers almost every compute buffer;
- an SRV for read-only buffers.

The codegen still emitted `StructuredBuffer` / `ByteAddressBuffer` for `ssbo_readonly`,
`byte_address_buffer` and `ubo`. Reading an SRV through a UAV descriptor is undefined. On this
machine it read zeros.

Vulkan has a single storage-buffer descriptor kind, so it was never affected.

Seen first in HumanoidLocomotion. The PD humanoid's `loco_observe` startup parity failed on DX12
with a worst error of 2.5e2 and passed on Vulkan with 3.1e-5. To isolate the cause, the kernel's
three struct reads were flipped to `ssbo_readwrite` and nothing else was changed. With that change
DX12 parity passed at 3.1e-5.

## The fix

The heap gets a mirror range so that every bindless buffer has a read view that matches its type,
and DX12 shaders read through the mirror.

- `gpu::dx12_buffer_read_view_offset` (16384, `gse.gpu_backend:bindless`) is the size of the buffer
  pool. The resource heap now allocates the buffer range twice: textures, images, buffers, then
  buffer read views. Read view index = buffer slot index + 16384.
- `dx12::device::create_buffer` records a `buffer_view_format` for each buffer: its stride and
  whether it is byte-address. It writes the primary descriptor exactly as before, then writes an
  SRV of the same format into the mirror slot. Byte-address buffers get a raw SRV; all other
  buffers get a structured SRV.
- `dx12::device::write_storage_buffer` repoints an existing slot at a sub-range. It now also
  rewrites the mirror, using the format of the buffer that owns the address.
  - `PhysicsTransformRenderer` reads its alternate view as `StructuredBuffer<body_state>`, so this
    must be a structured SRV and not a raw one.
  - A buffer that `create_buffer` did not make has no recorded format. Passing one asserts instead
    of guessing.
- In `emit_slang_binding`, when `active_backend == dx12`, the Handle cast for `ssbo_readonly`,
  `byte_address_buffer` and `ubo` uses `({name}_idx + 16384)`. Read-write bindings still use the
  primary UAV slot. The CPU side still pushes the primary slot index, so no binding code changes.
  This follows the existing `active_backend` branch the codegen already makes for TLAS bindings.

## Why Vulkan is untouched

Vulkan reads were already correct, and Vulkan reference hashes are the gate for every learning
screen. On Vulkan the codegen string is the same text as before, byte for byte. No Vulkan heap,
descriptor or command code changed. Applying the mirror to both APIs would cost a second
descriptor write on Vulkan and move its shader hashes while fixing nothing there.

## Cost

- One extra SRV descriptor write per bindless buffer creation, and one per
  `write_storage_buffer`.
- 16384 more descriptors in the CPU and GPU resource heap.
- No per-dispatch cost.

## Gates

See the HumanoidLocomotion record `MUSCLE_PLAN.md`, "DX12 bindless read views as built".
