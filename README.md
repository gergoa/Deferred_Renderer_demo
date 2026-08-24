<!-- Improved compatibility of back to top link: See: https://github.com/othneildrew/Best-README-Template/pull/73 -->
<a id="readme-top"></a>

<!-- PROJECT SHIELDS -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/gergoa/halado_beadando">
    <img src="images/logo.png" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Advanced OpenGL Rendering Engine</h3>

  <p align="center">
    A 3D rendering engine built to implement advanced graphics programming techniques. 
    <br/>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li><a href="#key-features">Key Features</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About The Project

This project implements a modern rendering pipeline focusing on performance, dynamic lighting, and screen-space effects. 

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Built With

* [![C++][Cpp-shield]][Cpp-url]
* [![OpenGL][OpenGL-shield]][OpenGL-url]
* [![ImGui][ImGui-shield]][ImGui-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- FEATURES -->
## Key Features

### ⚙️ Core Architecture: Deferred Shading

<div align="center">
  <img src="scene_layers.gif" alt="Deferred Shading Debug Layers" width="100%">
  <br>
  <em>Debug uniform views — 1: Final Lit, 2: Diffuse/Albedo, 3: World Normals, 4: Depth, 5: SSAO, 6: Shadowmap</em>
</div>
<br>

The backbone of the engine is a robust Deferred Shading pipeline that decouples geometry rendering from lighting calculations for optimized performance.

* **Geometry Pass:** Renders scene data into a G-Buffer consisting of Diffuse (`GL_RGBA8`), Normal (`GL_RGB16_SNORM`), and Depth (`GL_DEPTH_COMPONENT24`) textures.
* **Light Pass:** Accumulates lighting contributions in the default framebuffer.
* **Optimized Reconstruction:** World-space positions of surface points are mathematically reconstructed entirely from the depth buffer and camera matrices, minimizing memory bandwidth.

### 💡 Advanced Illumination & Shadows

<div align="center">
  <table>
    <tr>
      <td align="center">
        <img src="stagger_shadowmap.gif" alt="Staggered Shadow Map Updates" width="100%">
        <br><em>Staggered Shadow Map Updates</em>
      </td>
      <td align="center">
        <img src="lighting_ui.gif" alt="Dynamic Lighting UI" width="100%">
        <br><em>Real-time Lighting UI</em>
      </td>
    </tr>
  </table>
</div>
<br>

* **Dynamic Shadow Mapping:** Supports both Directional lights and Omnidirectional point lights (using 6-pass cubemap rendering) via a 1024x1024 depth buffer.
* **Performance Optimization:** Includes a staggered frame update system for lights. The engine distributes rendering calls across frames (updating one cubemap face per frame).
* **Dynamic Lighting UI:** Features an integrated user interface allowing real-time modification of light properties (position, color, intensity, direction, shadow map size), adding/removing lights, and toggling shadow casting/receiving on a per-object basis.

### 🪞 Screen Space Effects

<div align="center">
  <table>
    <tr>
      <td align="center">
        <img src="duck_ssr.png" alt="Screen Space Reflections (SSR)" width="100%">
        <br><b>Duck SSR Showcase</b>
      </td>
      <td align="center">
        <img src="suzanne_ssao.png" alt="Screen Space Ambient Occlusion (SSAO)" width="100%">
        <br><b>Suzanne SSAO Debug Pass</b>
      </td>
    </tr>
  </table>
</div>
<br>

* **Screen Space Reflections (SSR):** Implements screen-space raymarching using depth and reflection textures. Utilizes multi-pixel stepping combined with binary search for precise intersection detection, allowing reflections to naturally pass behind foreground objects. Reflections are dynamically blurred based on distance and surface reflectivity.
* **Screen Space Ambient Occlusion (SSAO):** Approximates global illumination via a post-processing pass. Samples the depth buffer using a sparse kernel to calculate realistic self-shadowing and ambient light occlusion, completely independent of scene geometry complexity.

### 📐 Geometry & Advanced Rendering Techniques

<div align="center">
  <img src="images/tess_showcase.gif" alt="Distance-based Tessellation" width="100%">
  <br>
  <em>Distance-based Tessellation Demo</em>
</div>
<br>

* **Distance-Adaptive Mesh Tessellation:** Implements hardware tessellation using a 6-point triangular Bézier surface. Calculates tessellation levels dynamically per-edge based on camera distance, ensuring seamless transitions, continuous edges, and zero visual artifacts (no cracks or flickering) during camera movement.
* **Rasterized Portals:** Features a recursive portal system. Uses Framebuffer Objects (FBOs) and dynamically transformed cameras to render seamless views through portals, complete with optimized frustum culling.
* **Progressive Rendering & Anti-Aliasing:** Includes a "freeze time" mechanic where scene updates halt but the camera remains free. When the camera is static, the engine performs temporal accumulation with random subpixel jittering (1-pixel maximum offset) to generate  supersampled, anti-aliased frames (SSAA).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CREDITS -->
## Credits & Acknowledgments

* **Bird / Duck 3D Model ("Bird v1")**: Created by [printable_models](https://free3d.com/3d-model/bird-v1--282209.html) via Free3D (Personal Use License).
* **Weathered Textures**: Sourced from [Textures.com](https://www.textures.com/browse/regular-weathered/115004).
* **3D Model ("hsm0022-v2")**: Sourced via [Free3D](https://free3d.com/3d-model/hsm0022-v2--672066.html) (Personal Use License).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- LICENSE -->
## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Gergely Asztalos - gergoasztalos26@gmail.com

Project Link: [https://github.com/gergoa/halado_beadando](https://github.com/gergoa/halado_beadando)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
[contributors-shield]: https://img.shields.io/github/contributors/gergoa/halado_beadando.svg?style=for-the-badge
[contributors-url]: https://github.com/gergoa/halado_beadando/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/gergoa/halado_beadando.svg?style=for-the-badge
[forks-url]: https://github.com/gergoa/halado_beadando/network/members
[stars-shield]: https://img.shields.io/github/stars/gergoa/halado_beadando.svg?style=for-the-badge
[stars-url]: https://github.com/gergoa/halado_beadando/stargazers
[issues-shield]: https://img.shields.io/github/issues/gergoa/halado_beadando.svg?style=for-the-badge
[issues-url]: https://github.com/gergoa/halado_beadando/issues
[license-shield]: https://img.shields.io/github/license/gergoa/halado_beadando.svg?style=for-the-badge
[license-url]: https://github.com/gergoa/halado_beadando/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://www.linkedin.com/in/gergo-asztalos-9b8280427/

[Cpp-shield]: https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white
[Cpp-url]: https://cplusplus.com/
[OpenGL-shield]: https://img.shields.io/badge/OpenGL-FFFFFF?style=for-the-badge&logo=opengl
[OpenGL-url]: https://www.opengl.org/
[ImGui-shield]: https://img.shields.io/badge/ImGui-000000?style=for-the-badge&logo=cplusplus
[ImGui-url]: https://github.com/ocornut/imgui
