import { ring, ring_record, ring_try_write, RING_HEAD, RING_TAIL } from "./protocol.mjs";

const OP = [
  null, "request_adapter", "request_device", "get_queue", "create_surface", "configure_surface",
  "unconfigure_surface", "get_current_texture", "present", "create_buffer", "create_texture",
  "create_view", "create_sampler", "create_bind_group_layout", "create_bind_group",
  "create_pipeline_layout", "create_shader_module", "create_compute_pipeline",
  "create_render_pipeline", "create_command_encoder", "begin_render_pass", "begin_compute_pass",
  "finish", "submit", "on_submitted_work_done", "write_buffer", "write_texture", "map_read", "unmap",
  "push_error_scope", "pop_error_scope", "clear_buffer", "copy_buffer_to_buffer",
  "copy_texture_to_buffer", "copy_texture_to_texture", "push_debug_group", "pop_debug_group",
  "pass_end", "set_pipeline", "set_bind_group", "set_index_buffer", "set_viewport", "set_scissor",
  "draw", "draw_indexed", "draw_indexed_indirect", "dispatch", "dispatch_indirect", "release",
];

const EVENT_ADAPTER = 1, EVENT_DEVICE = 2, EVENT_MAPPED = 3, EVENT_WORK_DONE = 4,
      EVENT_SCOPE_POPPED = 5, EVENT_UNCAPTURED_ERROR = 6, EVENT_DEVICE_LOST = 7;

const FORMATS = [
  undefined, "r8unorm", "r8snorm", "r8uint", "r8sint", "r16unorm", "r16snorm", "r16uint", "r16sint",
  "r16float", "rg8unorm", "rg8snorm", "rg8uint", "rg8sint", "r32float", "r32uint", "r32sint",
  "rg16unorm", "rg16snorm", "rg16uint", "rg16sint", "rg16float", "rgba8unorm", "rgba8unorm-srgb",
  "rgba8snorm", "rgba8uint", "rgba8sint", "bgra8unorm", "bgra8unorm-srgb", "rgb10a2uint",
  "rgb10a2unorm", "rg11b10ufloat", "rgb9e5ufloat", "rg32float", "rg32uint", "rg32sint",
  "rgba16unorm", "rgba16snorm", "rgba16uint", "rgba16sint", "rgba16float", "rgba32float",
  "rgba32uint", "rgba32sint", "stencil8", "depth16unorm", "depth24plus", "depth24plus-stencil8",
  "depth32float", "depth32float-stencil8", "bc1-rgba-unorm", "bc1-rgba-unorm-srgb", "bc2-rgba-unorm",
  "bc2-rgba-unorm-srgb", "bc3-rgba-unorm", "bc3-rgba-unorm-srgb", "bc4-r-unorm", "bc4-r-snorm",
  "bc5-rg-unorm", "bc5-rg-snorm", "bc6h-rgb-ufloat", "bc6h-rgb-float", "bc7-rgba-unorm",
  "bc7-rgba-unorm-srgb",
];

const VERTEX_FORMATS = [
  undefined, "uint8", "uint8x2", "uint8x4", "sint8", "sint8x2", "sint8x4", "unorm8", "unorm8x2",
  "unorm8x4", "snorm8", "snorm8x2", "snorm8x4", "uint16", "uint16x2", "uint16x4", "sint16",
  "sint16x2", "sint16x4", "unorm16", "unorm16x2", "unorm16x4", "snorm16", "snorm16x2", "snorm16x4",
  "float16", "float16x2", "float16x4", "float32", "float32x2", "float32x3", "float32x4", "uint32",
  "uint32x2", "uint32x3", "uint32x4", "sint32", "sint32x2", "sint32x3", "sint32x4",
  "unorm10-10-10-2", "unorm8x4-bgra",
];

const FEATURES = [
  undefined, "core-features-and-limits", "depth-clip-control", "depth32float-stencil8",
  "texture-compression-bc", "texture-compression-bc-sliced-3d", "texture-compression-etc2",
  "texture-compression-astc", "texture-compression-astc-sliced-3d", "timestamp-query",
  "indirect-first-instance", "shader-f16", "rg11b10ufloat-renderable", "bgra8unorm-storage",
  "float32-filterable", "float32-blendable", "clip-distances", "dual-source-blending", "subgroups",
  "texture-formats-tier1", "texture-formats-tier2", "primitive-index", "texture-component-swizzle",
];

const LIMITS = [
  "maxTextureDimension1D", "maxTextureDimension2D", "maxTextureDimension3D", "maxTextureArrayLayers",
  "maxBindGroups", "maxBindGroupsPlusVertexBuffers", "maxBindingsPerBindGroup",
  "maxDynamicUniformBuffersPerPipelineLayout", "maxDynamicStorageBuffersPerPipelineLayout",
  "maxSampledTexturesPerShaderStage", "maxSamplersPerShaderStage", "maxStorageBuffersPerShaderStage",
  "maxStorageTexturesPerShaderStage", "maxUniformBuffersPerShaderStage", "maxUniformBufferBindingSize",
  "maxStorageBufferBindingSize", "minUniformBufferOffsetAlignment", "minStorageBufferOffsetAlignment",
  "maxVertexBuffers", "maxBufferSize", "maxVertexAttributes", "maxVertexBufferArrayStride",
  "maxInterStageShaderVariables", "maxColorAttachments", "maxColorAttachmentBytesPerSample",
  "maxComputeWorkgroupStorageSize", "maxComputeInvocationsPerWorkgroup", "maxComputeWorkgroupSizeX",
  "maxComputeWorkgroupSizeY", "maxComputeWorkgroupSizeZ", "maxComputeWorkgroupsPerDimension",
  "maxImmediateSize",
];

const TEXTURE_DIMENSION = [undefined, "1d", "2d", "3d"];
const VIEW_DIMENSION = [undefined, "1d", "2d", "2d-array", "cube", "cube-array", "3d"];
const ASPECT = [undefined, "all", "stencil-only", "depth-only"];
const ADDRESS = [undefined, "clamp-to-edge", "repeat", "mirror-repeat"];
const FILTER = [undefined, "nearest", "linear"];
const COMPARE = [undefined, "never", "less", "equal", "less-equal", "greater", "not-equal",
                 "greater-equal", "always"];
const BUFFER_BINDING = [null, "uniform", "uniform", "storage", "read-only-storage"];
const SAMPLER_BINDING = [null, "filtering", "filtering", "non-filtering", "comparison"];
const SAMPLE_TYPE = [null, "float", "float", "unfilterable-float", "depth", "sint", "uint"];
const STORAGE_ACCESS = [null, "write-only", "write-only", "read-only", "read-write"];
const STEP_MODE = [undefined, "vertex", "instance"];
const TOPOLOGY = [undefined, "point-list", "line-list", "line-strip", "triangle-list", "triangle-strip"];
const INDEX_FORMAT = [undefined, "uint16", "uint32"];
const FRONT_FACE = [undefined, "ccw", "cw"];
const CULL = [undefined, "none", "front", "back"];
const STENCIL_OP = [undefined, "keep", "zero", "replace", "invert", "increment-clamp",
                    "decrement-clamp", "increment-wrap", "decrement-wrap"];
const BLEND_OP = [undefined, "add", "subtract", "reverse-subtract", "min", "max"];
const BLEND_FACTOR = [undefined, "zero", "one", "src", "one-minus-src", "src-alpha",
                      "one-minus-src-alpha", "dst", "one-minus-dst", "dst-alpha", "one-minus-dst-alpha",
                      "src-alpha-saturated", "constant", "one-minus-constant", "src1", "one-minus-src1",
                      "src1-alpha", "one-minus-src1-alpha"];
const LOAD_OP = [undefined, "load", "clear"];
const STORE_OP = [undefined, "store", "discard"];
const ERROR_FILTER = [undefined, "validation", "out-of-memory", "internal"];

const U32_UNDEFINED = 0xffffffff;
const decoder = new TextDecoder ();
const f32 = new Float32Array (1), f32_bits = new Uint32Array (f32.buffer);
const f64 = new Float64Array (1), f64_bits = new Uint32Array (f64.buffer);

let commands, replies, mem, canvas;
const objects = new Map ();
const backlog = [];

function say (text) {
  postMessage ({ kind: "console", text: `gpu: ${text}` });
}

class reader {
  constructor (record) { this.words = record; this.at = 2; }
  u32 () { return this.words[this.at++]; }
  i32 () { return this.words[this.at++] | 0; }
  opt32 () { const v = this.u32 (); return v === U32_UNDEFINED ? undefined : v; }
  u64 () {
    const low = this.u32 (), high = this.u32 ();
    return low === U32_UNDEFINED && high === U32_UNDEFINED ? undefined : high * 0x100000000 + low;
  }
  f32 () { f32_bits[0] = this.u32 (); return f32[0]; }
  f64 () { f64_bits[0] = this.u32 (); f64_bits[1] = this.u32 (); return f64[0]; }
  bytes () {
    const size = this.u32 ();
    const out = new Uint8Array (this.words.buffer, this.words.byteOffset + this.at * 4, size);
    this.at += (size + 3) >> 2;
    return out;
  }
  str () { return decoder.decode (this.bytes ()); }
  label () { const text = this.str (); return text.length === 0 ? undefined : text; }
  id () { return this.u32 (); }
  obj () { const id = this.u32 (); return id === 0 ? undefined : objects.get (id); }
  list (read) { const out = []; for (let i = this.u32 (); i > 0; --i) out.push (read ()); return out; }
}

class writer {
  constructor (kind) { this.words = [kind, 0]; }
  u32 (v) { this.words.push (v >>> 0); return this; }
  u64 (v) {
    if (v === undefined) return this.u32 (U32_UNDEFINED).u32 (U32_UNDEFINED);
    return this.u32 (v % 0x100000000).u32 (Math.floor (v / 0x100000000));
  }
  str (text) {
    const bytes = new TextEncoder ().encode (text ?? "");
    this.u32 (bytes.length);
    const words = new Uint32Array ((bytes.length + 3) >> 2);
    new Uint8Array (words.buffer).set (bytes);
    for (const w of words) this.words.push (w);
    return this;
  }
  limits (source) {
    for (const name of LIMITS) this.u64 (source[name] ?? 0);
    return this;
  }
  send () {
    this.words[1] = this.words.length;
    backlog.push (Uint32Array.from (this.words));
    flush_backlog ();
  }
}

function flush_backlog () {
  while (backlog.length > 0 && ring_try_write (replies, backlog[0])) backlog.shift ();
}

function error_type (error) {
  if (error === null) return 1;
  if (error instanceof GPUValidationError) return 2;
  if (error instanceof GPUOutOfMemoryError) return 3;
  if (error instanceof GPUInternalError) return 4;
  return 5;
}

function constants (r) {
  const out = {};
  for (let i = r.u32 (); i > 0; --i) {
    const key = r.str ();
    out[key] = r.f64 ();
  }
  return out;
}

function stage (r) {
  const module = r.obj ();
  const entryPoint = r.label ();
  return { module, entryPoint, constants: constants (r) };
}

function texture_copy (r) {
  return { texture: r.obj (), mipLevel: r.u32 (), origin: { x: r.u32 (), y: r.u32 (), z: r.u32 () },
           aspect: ASPECT[r.u32 ()] };
}

function extent (r) {
  return { width: r.u32 (), height: r.u32 (), depthOrArrayLayers: r.u32 () };
}

function drop_undefined (value) {
  for (const key of Object.keys (value)) if (value[key] === undefined) delete value[key];
  return value;
}

const run = {
  request_adapter (r) {
    const future = r.u64 (), id = r.id (), power = r.u32 (), fallback = r.u32 () !== 0;
    navigator.gpu.requestAdapter ({
      powerPreference: power === 1 ? "low-power" : power === 2 ? "high-performance" : undefined,
      forceFallbackAdapter: fallback,
    }).then (adapter => {
      if (adapter === null) {
        new writer (EVENT_ADAPTER).u64 (future).u32 (3).u32 (0).str ("no WebGPU adapter").send ();
        return;
      }
      objects.set (id, adapter);
      const info = adapter.info ?? {};
      const features = [...adapter.features].map (f => FEATURES.indexOf (f)).filter (i => i > 0);
      const preferred = FORMATS.indexOf (navigator.gpu.getPreferredCanvasFormat ());
      say (`adapter ${info.vendor ?? "?"} ${info.architecture ?? "?"} ${info.description ?? ""}, `
           + `features ${[...adapter.features].join (" ")}`);
      const w = new writer (EVENT_ADAPTER).u64 (future).u32 (1).u32 (id).str ("")
        .str (info.vendor).str (info.architecture).str (info.device).str (info.description)
        .u32 (0).u32 (0).limits (adapter.limits).u32 (features.length);
      for (const f of features) w.u32 (f);
      w.u32 (preferred > 0 ? preferred : 0x1b).send ();
    }, error => new writer (EVENT_ADAPTER).u64 (future).u32 (4).u32 (0).str (error.message).send ());
  },

  request_device (r) {
    const future = r.u64 (), adapter = r.obj (), id = r.id (), label = r.label ();
    const requiredFeatures = r.list (() => FEATURES[r.u32 ()]).filter (f => f && adapter.features.has (f));
    const requiredLimits = {};
    if (r.u32 () !== 0) {
      for (const name of LIMITS) {
        const value = r.u64 ();
        if (value !== undefined && name in adapter.limits) requiredLimits[name] = value;
      }
    }
    adapter.requestDevice ({ label, requiredFeatures, requiredLimits }).then (device => {
      objects.set (id, device);
      device.onuncapturederror = (e) =>
        new writer (EVENT_UNCAPTURED_ERROR).u32 (id).u32 (error_type (e.error)).str (e.error.message).send ();
      device.lost.then (info =>
        new writer (EVENT_DEVICE_LOST).u32 (id).u32 (info.reason === "destroyed" ? 2 : 1).str (info.message).send ());
      new writer (EVENT_DEVICE).u64 (future).u32 (1).str ("").limits (device.limits).send ();
    }, error => new writer (EVENT_DEVICE).u64 (future).u32 (3).str (error.message).send ());
  },

  get_queue (r) { const device = r.obj (); objects.set (r.id (), device.queue); },

  create_surface (r) {
    objects.set (r.id (), { context: canvas.getContext ("webgpu"), configured: false });
  },

  configure_surface (r) {
    const surface = r.obj (), device = r.obj (), format = FORMATS[r.u32 ()], usage = r.u64 ();
    const width = r.u32 (), height = r.u32 (), alpha = r.u32 ();
    const viewFormats = r.list (() => FORMATS[r.u32 ()]);
    canvas.width = width;
    canvas.height = height;
    surface.context.configure ({ device, format, usage, viewFormats,
                                 alphaMode: alpha === 2 ? "premultiplied" : "opaque" });
    surface.configured = true;
  },

  unconfigure_surface (r) { r.obj ().context.unconfigure (); },

  get_current_texture (r) {
    const surface = r.obj ();
    objects.set (r.id (), surface.context.getCurrentTexture ());
    return "acquired";
  },

  present (r) { r.obj (); return "present"; },

  create_buffer (r) {
    const device = r.obj (), id = r.id ();
    objects.set (id, device.createBuffer ({ label: r.label (), usage: r.u64 (), size: r.u64 () }));
  },

  create_texture (r) {
    const device = r.obj (), id = r.id ();
    const label = r.label (), usage = r.u64 (), dimension = TEXTURE_DIMENSION[r.u32 ()], size = extent (r);
    const format = FORMATS[r.u32 ()], mipLevelCount = r.u32 (), sampleCount = r.u32 ();
    const viewFormats = r.list (() => FORMATS[r.u32 ()]);
    objects.set (id, device.createTexture (drop_undefined ({
      label, usage, dimension, size, format, mipLevelCount: mipLevelCount || undefined,
      sampleCount: sampleCount || undefined, viewFormats,
    })));
  },

  create_view (r) {
    const texture = r.obj (), id = r.id ();
    if (r.u32 () === 0) { objects.set (id, texture.createView ()); return; }
    const label = r.label (), format = FORMATS[r.u32 ()], dimension = VIEW_DIMENSION[r.u32 ()];
    const baseMipLevel = r.u32 (), mipLevelCount = r.opt32 ();
    const baseArrayLayer = r.u32 (), arrayLayerCount = r.opt32 ();
    const aspect = ASPECT[r.u32 ()], usage = r.u64 ();
    objects.set (id, texture.createView (drop_undefined ({
      label, format, dimension, baseMipLevel, mipLevelCount, baseArrayLayer, arrayLayerCount, aspect,
      usage: usage || undefined,
    })));
  },

  create_sampler (r) {
    const device = r.obj (), id = r.id ();
    const label = r.label ();
    const addressModeU = ADDRESS[r.u32 ()], addressModeV = ADDRESS[r.u32 ()], addressModeW = ADDRESS[r.u32 ()];
    const magFilter = FILTER[r.u32 ()], minFilter = FILTER[r.u32 ()], mipmapFilter = FILTER[r.u32 ()];
    const lodMinClamp = r.f32 (), lodMaxClamp = r.f32 (), compare = COMPARE[r.u32 ()];
    const maxAnisotropy = r.u32 ();
    objects.set (id, device.createSampler (drop_undefined ({
      label, addressModeU, addressModeV, addressModeW, magFilter, minFilter, mipmapFilter,
      lodMinClamp, lodMaxClamp, compare, maxAnisotropy: maxAnisotropy || undefined,
    })));
  },

  create_bind_group_layout (r) {
    const device = r.obj (), id = r.id (), label = r.label ();
    const entries = r.list (() => {
      const entry = { binding: r.u32 (), visibility: r.u64 () };
      r.u32 ();
      const buffer_type = BUFFER_BINDING[r.u32 ()], dynamic = r.u32 () !== 0, min_size = r.u64 ();
      const sampler_type = SAMPLER_BINDING[r.u32 ()];
      const sample_type = SAMPLE_TYPE[r.u32 ()], texture_dimension = VIEW_DIMENSION[r.u32 ()] ?? "2d";
      const multisampled = r.u32 () !== 0;
      const access = STORAGE_ACCESS[r.u32 ()], storage_format = FORMATS[r.u32 ()];
      const storage_dimension = VIEW_DIMENSION[r.u32 ()] ?? "2d";
      if (buffer_type) entry.buffer = { type: buffer_type, hasDynamicOffset: dynamic, minBindingSize: min_size };
      if (sampler_type) entry.sampler = { type: sampler_type };
      if (sample_type) entry.texture = { sampleType: sample_type, viewDimension: texture_dimension, multisampled };
      if (access) entry.storageTexture = { access, format: storage_format, viewDimension: storage_dimension };
      return entry;
    });
    objects.set (id, device.createBindGroupLayout ({ label, entries }));
  },

  create_bind_group (r) {
    const device = r.obj (), id = r.id (), label = r.label (), layout = r.obj ();
    const entries = r.list (() => {
      const binding = r.u32 (), buffer = r.obj (), offset = r.u64 (), size = r.u64 ();
      const sampler = r.obj (), view = r.obj ();
      const resource = buffer ? drop_undefined ({ buffer, offset, size }) : sampler ?? view;
      return { binding, resource };
    });
    objects.set (id, device.createBindGroup ({ label, layout, entries }));
  },

  create_pipeline_layout (r) {
    const device = r.obj (), id = r.id (), label = r.label ();
    const bindGroupLayouts = r.list (() => r.obj ());
    objects.set (id, device.createPipelineLayout ({ label, bindGroupLayouts }));
  },

  create_shader_module (r) {
    const device = r.obj (), id = r.id (), label = r.label ();
    objects.set (id, device.createShaderModule ({ label, code: r.str () }));
  },

  create_compute_pipeline (r) {
    const device = r.obj (), id = r.id (), label = r.label (), layout = r.obj () ?? "auto";
    objects.set (id, device.createComputePipeline ({ label, layout, compute: stage (r) }));
  },

  create_render_pipeline (r) {
    const device = r.obj (), id = r.id (), label = r.label (), layout = r.obj () ?? "auto";
    const vertex = stage (r);
    vertex.buffers = r.list (() => ({
      stepMode: STEP_MODE[r.u32 ()], arrayStride: r.u64 (),
      attributes: r.list (() => ({ format: VERTEX_FORMATS[r.u32 ()], offset: r.u64 (), shaderLocation: r.u32 () })),
    }));
    const primitive = drop_undefined ({
      topology: TOPOLOGY[r.u32 ()], stripIndexFormat: INDEX_FORMAT[r.u32 ()],
      frontFace: FRONT_FACE[r.u32 ()], cullMode: CULL[r.u32 ()],
    });
    if (r.u32 () !== 0) primitive.unclippedDepth = true;
    let depthStencil;
    if (r.u32 () !== 0) {
      const format = FORMATS[r.u32 ()], write = r.u32 (), depthCompare = COMPARE[r.u32 ()];
      const face = () => drop_undefined ({ compare: COMPARE[r.u32 ()], failOp: STENCIL_OP[r.u32 ()],
                                           depthFailOp: STENCIL_OP[r.u32 ()], passOp: STENCIL_OP[r.u32 ()] });
      const stencilFront = face (), stencilBack = face ();
      depthStencil = drop_undefined ({
        format, depthWriteEnabled: write === 2 ? undefined : write === 1, depthCompare, stencilFront, stencilBack,
        stencilReadMask: r.u32 (), stencilWriteMask: r.u32 (), depthBias: r.i32 (),
        depthBiasSlopeScale: r.f32 (), depthBiasClamp: r.f32 (),
      });
    }
    const count = r.u32 (), mask = r.u32 (), alphaToCoverageEnabled = r.u32 () !== 0;
    const multisample = { count: count || 1, mask, alphaToCoverageEnabled };
    let fragment;
    if (r.u32 () !== 0) {
      fragment = stage (r);
      fragment.targets = r.list (() => {
        const target = { format: FORMATS[r.u32 ()], writeMask: r.u64 () };
        if (r.u32 () !== 0) {
          const component = () => drop_undefined ({ operation: BLEND_OP[r.u32 ()],
                                                    srcFactor: BLEND_FACTOR[r.u32 ()],
                                                    dstFactor: BLEND_FACTOR[r.u32 ()] });
          target.blend = { color: component (), alpha: component () };
        }
        return target;
      });
    }
    objects.set (id, device.createRenderPipeline (drop_undefined ({
      label, layout, vertex, primitive, depthStencil, multisample, fragment,
    })));
  },

  create_command_encoder (r) {
    const device = r.obj (), id = r.id ();
    objects.set (id, device.createCommandEncoder ({ label: r.label () }));
  },

  begin_render_pass (r) {
    const encoder = r.obj (), id = r.id (), label = r.label ();
    const colorAttachments = r.list (() => {
      const view = r.obj (), depthSlice = r.opt32 (), resolveTarget = r.obj ();
      const loadOp = LOAD_OP[r.u32 ()], storeOp = STORE_OP[r.u32 ()];
      const clearValue = { r: r.f64 (), g: r.f64 (), b: r.f64 (), a: r.f64 () };
      return view ? drop_undefined ({ view, depthSlice, resolveTarget, loadOp, storeOp, clearValue }) : null;
    });
    let depthStencilAttachment;
    if (r.u32 () !== 0) {
      const view = r.obj (), depthLoadOp = LOAD_OP[r.u32 ()], depthStoreOp = STORE_OP[r.u32 ()];
      const clear = r.f32 (), depthReadOnly = r.u32 () !== 0;
      const stencilLoadOp = LOAD_OP[r.u32 ()], stencilStoreOp = STORE_OP[r.u32 ()];
      const stencilClearValue = r.u32 (), stencilReadOnly = r.u32 () !== 0;
      depthStencilAttachment = drop_undefined ({
        view, depthLoadOp, depthStoreOp, depthClearValue: Number.isNaN (clear) ? undefined : clear,
        depthReadOnly, stencilLoadOp, stencilStoreOp,
        stencilClearValue: stencilLoadOp ? stencilClearValue : undefined, stencilReadOnly,
      });
    }
    objects.set (id, encoder.beginRenderPass (drop_undefined ({ label, colorAttachments, depthStencilAttachment })));
  },

  begin_compute_pass (r) {
    const encoder = r.obj (), id = r.id ();
    objects.set (id, encoder.beginComputePass ({ label: r.label () }));
  },

  finish (r) {
    const encoder = r.obj (), id = r.id ();
    objects.set (id, encoder.finish ({ label: r.label () }));
  },

  submit (r) {
    const queue = r.obj ();
    queue.submit (r.list (() => r.obj ()));
  },

  on_submitted_work_done (r) {
    const queue = r.obj (), future = r.u64 ();
    queue.onSubmittedWorkDone ().then (
      () => new writer (EVENT_WORK_DONE).u64 (future).u32 (1).str ("").send (),
      error => new writer (EVENT_WORK_DONE).u64 (future).u32 (3).str (error.message).send ());
  },

  write_buffer (r) {
    const queue = r.obj (), buffer = r.obj (), offset = r.u64 ();
    queue.writeBuffer (buffer, offset, r.bytes ());
  },

  write_texture (r) {
    const queue = r.obj (), destination = texture_copy (r);
    const layout = drop_undefined ({ offset: r.u64 (), bytesPerRow: r.opt32 (), rowsPerImage: r.opt32 () });
    const size = extent (r);
    queue.writeTexture (destination, r.bytes (), layout, size);
  },

  map_read (r) {
    const buffer = r.obj (), future = r.u64 (), offset = r.u64 (), size = r.u64 (), staging = r.u32 ();
    buffer.mapAsync (GPUMapMode.READ, offset, size).then (() => {
      new Uint8Array (mem.buffer, staging, size).set (new Uint8Array (buffer.getMappedRange (offset, size)));
      new writer (EVENT_MAPPED).u64 (future).u32 (1).str ("").send ();
    }, error => new writer (EVENT_MAPPED).u64 (future).u32 (3).str (error.message).send ());
  },

  unmap (r) { r.obj ().unmap (); },

  push_error_scope (r) {
    const device = r.obj ();
    device.pushErrorScope (ERROR_FILTER[r.u32 ()]);
  },

  pop_error_scope (r) {
    const device = r.obj (), future = r.u64 ();
    device.popErrorScope ().then (
      error => new writer (EVENT_SCOPE_POPPED).u64 (future).u32 (1).u32 (error_type (error))
        .str (error?.message ?? "").send (),
      error => new writer (EVENT_SCOPE_POPPED).u64 (future).u32 (3).u32 (5).str (error.message).send ());
  },

  clear_buffer (r) {
    const encoder = r.obj (), buffer = r.obj ();
    encoder.clearBuffer (buffer, r.u64 (), r.u64 ());
  },

  copy_buffer_to_buffer (r) {
    const encoder = r.obj (), source = r.obj (), source_offset = r.u64 ();
    const destination = r.obj (), destination_offset = r.u64 ();
    encoder.copyBufferToBuffer (source, source_offset, destination, destination_offset, r.u64 ());
  },

  copy_texture_to_buffer (r) {
    const encoder = r.obj (), source = texture_copy (r);
    const destination = drop_undefined ({ buffer: r.obj (), offset: r.u64 (), bytesPerRow: r.opt32 (),
                                          rowsPerImage: r.opt32 () });
    encoder.copyTextureToBuffer (source, destination, extent (r));
  },

  copy_texture_to_texture (r) {
    const encoder = r.obj (), source = texture_copy (r), destination = texture_copy (r);
    encoder.copyTextureToTexture (source, destination, extent (r));
  },

  push_debug_group (r) { const target = r.obj (); target.pushDebugGroup (r.str ()); },
  pop_debug_group (r) { r.obj ().popDebugGroup (); },
  pass_end (r) { r.obj ().end (); },

  set_pipeline (r) { const pass = r.obj (); pass.setPipeline (r.obj ()); },

  set_bind_group (r) {
    const pass = r.obj (), index = r.u32 (), group = r.obj () ?? null;
    pass.setBindGroup (index, group, r.list (() => r.u32 ()));
  },

  set_index_buffer (r) {
    const pass = r.obj (), buffer = r.obj (), format = INDEX_FORMAT[r.u32 ()];
    pass.setIndexBuffer (buffer, format, r.u64 (), r.u64 ());
  },

  set_viewport (r) {
    const pass = r.obj ();
    pass.setViewport (r.f32 (), r.f32 (), r.f32 (), r.f32 (), r.f32 (), r.f32 ());
  },

  set_scissor (r) { const pass = r.obj (); pass.setScissorRect (r.u32 (), r.u32 (), r.u32 (), r.u32 ()); },
  draw (r) { const pass = r.obj (); pass.draw (r.u32 (), r.u32 (), r.u32 (), r.u32 ()); },

  draw_indexed (r) {
    const pass = r.obj ();
    pass.drawIndexed (r.u32 (), r.u32 (), r.u32 (), r.i32 (), r.u32 ());
  },

  draw_indexed_indirect (r) { const pass = r.obj (), buffer = r.obj (); pass.drawIndexedIndirect (buffer, r.u64 ()); },
  dispatch (r) { const pass = r.obj (); pass.dispatchWorkgroups (r.u32 (), r.u32 (), r.u32 ()); },
  dispatch_indirect (r) { const pass = r.obj (), buffer = r.obj (); pass.dispatchWorkgroupsIndirect (buffer, r.u64 ()); },
  release (r) { objects.delete (r.id ()); },
};

function next_frame () {
  return new Promise (resolve =>
    typeof requestAnimationFrame === "function" ? requestAnimationFrame (resolve) : setTimeout (resolve, 0));
}

const FRAME_HOLD_MS = 1000;

async function pump () {
  const header = commands.header;
  let tail = Atomics.load (header, RING_TAIL);
  let holding = false, warned = false, presented = false;
  for (;;) {
    const head = Atomics.load (header, RING_HEAD);
    if (tail === head) {
      flush_backlog ();
      if (holding && Atomics.wait (header, RING_HEAD, head, FRAME_HOLD_MS) !== "timed-out") continue;
      if (holding && !warned) {
        warned = true;
        say (`no command for ${FRAME_HOLD_MS} ms while holding a canvas texture; yielding, so this frame's texture may expire`);
      }
      holding = false;
      const waited = Atomics.waitAsync (header, RING_HEAD, head, 100);
      if (waited.async) await waited.value;
      continue;
    }
    const record = ring_record (commands, tail);
    tail = (tail + record[1]) & commands.mask;
    Atomics.store (header, RING_TAIL, tail);
    Atomics.notify (header, RING_TAIL);
    const name = OP[record[0]];
    let outcome;
    try {
      outcome = run[name] (new reader (record));
    } catch (error) {
      say (`${name ?? `op ${record[0]}`} threw: ${error.message}`);
    }
    if (outcome === "acquired") holding = true;
    if (outcome === "present") {
      if (!presented) postMessage ({ kind: "presenting" });
      presented = true;
      holding = false;
      flush_backlog ();
      await next_frame ();
    }
  }
}

onmessage = (e) => {
  ({ canvas, mem } = e.data);
  commands = ring (e.data.gpu);
  replies = ring (e.data.events);
  if (!navigator.gpu) say ("navigator.gpu is unavailable in this worker; WebGPU calls will fail");
  setInterval (flush_backlog, 4);
  pump ().catch (error => say (`pump stopped: ${error.message}\n${error.stack}`));
};
