# PathTracer_GaussianSplatting

A Vulkan-based **Path Tracing** engine designed to generate high-fidelity synthetic datasets for **3D Gaussian Splatting (3DGS)**. This project introduces a novel **Toroidal Acquisition** pipeline that decouples geometric and photometric data collection, effectively bypassing the limitations of traditional Structure-from-Motion (SfM) for scene initialization.

---

## Key Features

* **Vulkan Path Tracing Engine**: A physically accurate renderer utilizing the `VK_KHR_ray_tracing` extension and RT Core hardware to produce noise-free synthetic imagery.
* **Toroidal Sensor**: A custom sampling geometry that exploits **Variable Gaussian Curvature** to capture dense, ground-truth point clouds.
* **Decoupled Acquisition**: Separate acquisition of geometry (via toroidal rays) and photometry (via centerline camera trajectories), eliminating scale ambiguity and feature-matching failures.
* **Advanced Surface Sampling**: Support for Stochastic, Low-Discrepancy (Halton, LHS), and **Adaptive Importance Sampling** (Hit-based and Color-based) strategies.
* **3DGS Integration**: Directly generates `.ply` point clouds and `transforms.json` metadata for seamless training without requiring **Adaptive Density Control (ADC)**.

---

## Engine Architecture

The engine is built on modern GPU principles to ensure high-performance data generation:

* **PBR Workflows**: Full support for **Metallic-Roughness** (glTF 2.0 compliant) and Specular-Glossiness models.
* **Multiple Importance Sampling (MIS)**: Combines BSDF and Light sampling via the Power Heuristic to minimize variance:
  $$w_i(x) = \frac{(n_i p_i(x))^\beta}{\sum_k (n_k p_k(x))^\beta}$$
* **Temporal Accumulation**: Progressive rendering approach that reaches mathematical convergence for ground-truth quality.
* **Low-Discrepancy Sampling**: Utilizes Blue Noise and R2 sequences to decorrelate error over time.
* **Hardware Acceleration**: Specialized usage of **RT Cores** for BVH traversal and intersection testing.

---

## The Toroidal Pipeline

The engine utilizes a toroidal sensor positioned in the center of the simulated world:



1.  **Geometric Acquisition**: Rays are cast from the torus surface into the environment to generate a dense point cloud. The toroidal topology ensures a "watertight" volume and avoids topological singularities common in spherical sensors.
2.  **Photometric Acquisition**: A virtual camera traverses the torus's centerline, capturing high-resolution reference images with perfect extrinsic and intrinsic metadata.
3.  **Data Formatting**: Outputs are standardized in `transforms.json` format, allowing the 3DGS model to skip the SfM step entirely.

---

## Experimental Results

The pipeline has been validated on complex environments, demonstrating that starting from a dense, ground-truth point cloud allows for superior reconstruction quality and faster convergence.

### Performance Benchmarks (at 30k iterations)

| Scene | SSIM | PSNR | LPIPS |
| :--- | :--- | :--- | :--- |
| **Stanford Bunny** | 0.9870  | 41.55  | 0.0557  |
| **Happy Buddha** | 0.9873  | 42.14  | 0.0600  |
| **Teapot & Dragon** | 0.9863  | 40.92  | 0.0611  |
| **Spartan** | 0.9762  | 40.89  | 0.0640  |
| **Bistro Interior** | 0.9250  | 34.84  | 0.2007  |

---

## Comparison with Standard Pipelines

| Feature | Standard (SfM/Colmap) | This Pipeline (Toroidal) |
| :--- | :--- | :--- |
| **Initialization** | Sparse Point Cloud  | Dense Ground-Truth Point Cloud  |
| **Textureless Surfaces** | Often fails (holes)  | Robust reconstruction  |
| **Camera Poses** | Estimated (potential drift)  | Exact (Ground Truth)  |
| **Convergence** | Relies on ADC  | ADC can be bypassed  |

---

## Requirements

This project is written in C++ and uses Vulkan for rendering, along with several external libraries.

### Core Requirements
- C++17 compatible compiler (GCC / Clang)
- Vulkan SDK (tested with latest LunarG SDK)
- Make

### Dependencies
The following libraries are required:

- GLFW (window and input handling)
- GLM (math library)
- Assimp (model loading)
- stb (image loading)
- ImGui (UI)
- Vulkan Memory Allocator (VMA)

Make sure all dependencies are installed and accessible by the compiler.  
You may need to adjust include/library paths in the `Makefile` depending on your system.

### Setup Script

A helper script is provided to install the required system dependencies on Ubuntu/Debian systems.

#### Usage

Make the script executable:

```bash
chmod +x setup.sh
```

Run the script:

```bash
./setup.sh
```
The script installs all required libraries except for the Vulkan SDK, which must be installed manually.
Download it from: https://vulkan.lunarg.com/
After installing the Vulkan SDK, make sure to source the environment variables:
You may still need to adjust library/include paths in the Makefile
The script is intended for Ubuntu/Debian-based distributions

---

## How to run
The project uses a Makefile for compilation.
From the root directory, run:
```bash
make run
```
This will build the executable and run it
To clean the project:
```bash
make clean
```
To change the loaded scene, modify the path inside the main_scene.json file

