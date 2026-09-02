# Shuttle Engine

![Shuttle Engine Preview](logo/photo_2026-08-26_15-17-46.jpg)

Shuttle Engine is an experimental Vulkan 1.3+ renderer and scene editor featuring a GPU-driven pipeline, built from scratch in C++.

The project combines low-level Vulkan rendering, a robust scene and environment asset loading system, and an editor interface for real-time inspection and tweaking.

[▶️ Watch the Demo on YouTube](https://www.youtube.com/watch?v=CKNqVKySFt0)

---

## Overview

The primary goal of the project is to explore modern approaches to building a high-performance real-time renderer:

- **Vulkan 1.3+** (Dynamic Rendering, Bindless Descriptors, Buffer Device Address);
- **GPU-driven rendering** (Indirect indexed drawing, Compute passes);
- **HDR & IBL** (Image-Based Lighting);
- **Interactive debugging** (MRT Visual Debugger);
- **Native UI integration** (Win32 API / SDL2 with custom window decorations).

---

## Features

### Rendering
- Vulkan-based rendering backend utilizing Dynamic Rendering.
- Dynamic creation and recreation of the swapchain.
- Indexed indirect drawing for efficient multi-mesh scene rendering.
- GPU resource management with deferred destruction (`ResourceBin`).

### GPU-driven Pipeline
The scene preparation pipeline offloads maximum workload to the GPU:
1. World transform updates.
2. Mesh instance counting.
3. Prefix sum (scan).
4. Instance remap buffer generation.
5. Indirect draw commands preparation.

### Assets & Editor UI
The editor supports importing and visualizing 3D scenes (FBX/glTF) and environments (HDR). The interface includes custom window decorations, a tab system, camera and rendering settings, and flexible debug modes.

---

## Debug Rendering

The renderer supports advanced visualization modes for deep inspection of the graphics pipeline:

- **Geometry & Space:** Albedo, Normal, Tangent, Bitangent, UV, World Position/Normal.
- **PBR:** Metallic, Roughness, AO, Emissive.
- **Depth & IDs:** Linear/View Depth, Mesh/Material/Instance ID.

**Viewport Layout Modes:**
Depending on the task, the debug viewport can operate in one of four layout modes:
1. **Single** — output of a single selected channel.
2. **Split Vertical / Horizontal** — screen split into 2 buffers.
3. **Quad Layout** — simultaneous output of all 4 debug attachments in a 2x2 grid.

---

## Architecture

The project is structured into logical layers:
- **Application:** Lifecycle management, event handling, and low-level resource orchestration (Swapchain, Allocator).
- **MainWindow:** Editor UI logic, file dialogs, and viewport interaction.
- **Render Passes:** Modular pass-based architecture (`WorldTransformUpdatePass`, `MeshInstancesCountPass`, `PrefixSumPass`, `InstanceRemapPass`, `MainRenderPass`, `UiPass`).

---

## Build

The project uses CMake:

```bash
git clone <repository-url>
cd ShuttleEngine
cmake -S . -B build
cmake --build build --config Release
```

---

## Project Status

### Implemented
- Complete Vulkan stack (Initialization, Synchronization, Indirect Rendering).
- Scene loading and importing (FBX/glTF) and HDR environments.
- Custom editor UI with window decorations support.
- Advanced debugging system (MRT, Quad Layout).

### Roadmap
- **Modularity:** Splitting into `Shuttle Engine Runtime` and `Shuttle Editor`.
- **UI:** Migration to `RmlUi` for production UI (keeping Dear ImGui strictly for debug panels).
- **Inspector:** Development of a full-featured asset and scene hierarchy inspector.
- **Debugging:** Implementation of independent 4-pass rendering for each quadrant (enabling real-time comparison of IBL/Tone Mapping settings).

---

## License

The source code of this project is licensed under the MIT License. See the [`LICENSE`](LICENSE) file for details.

## Author

**Shagu**
