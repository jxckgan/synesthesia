# synesthesia

Synesthesia is an application which provides a real-time visualisation of audio frequencies represented as colour. Features a rudimentary EQ, real-time frequency & wavelength information, and an audio input selection system.

### Building & Running

Synesthesia runs on Windows (DirectX 12), macOS (Metal), and Linux (Vulkan). To run this project, make sure `cmake` is installed, and just clone this repository w/ submodules and run `make run` (we build app dependencies locally):

> **Note**:
> For Windows clients, you must install VS Microsoft C++ Build Tools, [here is a guide](https://github.com/bycloudai/InstallVSBuildToolsWindows?tab=readme-ov-file) for installing and setting up your PATH.

```sh
# Clone the repository with submodules
git clone --recurse-submodules https://github.com/jxckgan/synesthesia
cd synesthesia

# Create (and enter) the build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .

# Run Synesthesia
./synesthesia
```

If you're downloading the prebuilt binaries for macOS, you'll need to right-click, press open, and then navigate to System Preferences -> Privacy & Security, and confirm you want to open it; this is because Synesthesia binaries aren't codesigned with an Apple Developer Account 
<details><summary><h3>Building an App Bundle on macOS</h3></summary>

In order to build a macOS Application Bundle, we use the following flags (`-DBUILD_MACOS_BUNDLE`) to enable our app-building option:

```sh
cmake .. -DBUILD_MACOS_BUNDLE=ON
cmake --build .
```

And your `.app` will be in the root of the build directory.

</details>
<details><summary><h3>Building an Executable on Windows</h3></summary>

To build a standalone/portable Windows executable, we use the following flags (`-DCMAKE_BUILD_TYPE=Release`) to build:

```sh
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Your executable will then be placed in the Release folder (placed at the root of your build directory).

</details>


### Todo

- [x] ~~Cross-platform support~~
- [x] ~~Better/granular EQ control~~
- [x] ~~Ability for visualisation of multiple frequency bands~~
- [ ] Vulkan port for Linux
- [ ] Generally more control over settings
- [ ] DAW-integration

### Credits & Footnotes

- Visible Light Spectrum to RGB: [Endolith](https://www.endolith.com/wordpress/2010/09/15/a-mapping-between-musical-notes-and-colors/)
- GUI built w/  [Dear ImGui](https://github.com/ocornut/imgui)
- Lightweight FFT possible w/ [KissFFT](https://github.com/mborgerding/kissfft)
- macOS Application Building: [Appify/OSX App in Plain C](https://github.com/jimon/osx_app_in_plain_c)

> **⚠️ Warning:**<br>
> This application may display rapidly changing colors when multiple frequencies are played. If you have photosensitive epilepsy, I strongly advise against using this application.

> **Note:**
> This application is artistic in nature, and doesn't aim to replicate Synesthesia (Chromesthesia) or to be scientifically accurate.