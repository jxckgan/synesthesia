# synesthesia

Synesthesia is an application which provides a real-time visualisation of audio frequencies represented as colour. Features a rudimentary EQ, real-time frequency & wavelength information, and an audio input selection system.

### Building & Running

As of now, this project uses Metal for rendering and subsequently only runs on macOS systems. To run this project, make sure `cmake` is installed, and just clone this repository w/ submodules and run `make run` (we build required dependencies locally):

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

To build a macOS application, we can use [Appify](/meta/appify.sh) to build a basic app bundle (app will be placed in the root of the repository):

```sh
# Configure with macOS app bundle support
cmake -DBUILD_MACOS_APP=ON ..

# Build the project and the app bundle target
cmake --build .
cmake --build . --target app
```

### Todo

- [ ] Cross-platform support
- [x] ~~Better/granular EQ control~~
- [x] ~~Ability for visualisation of multiple frequency bands~~
- [ ] Generally more control over settings
- [ ] DAW-integration

### Credits & Footnotes

- Visible Light Spectrum to RGB: [Endolith](https://www.endolith.com/wordpress/2010/09/15/a-mapping-between-musical-notes-and-colors/)
- GUI built w/  [Dear ImGui](https://github.com/ocornut/imgui)
- Lightweight FFT possible w/ [KissFFT](https://github.com/mborgerding/kissfft)
- macOS Application Building: [Appify/OSX App in Plain C](https://github.com/jimon/osx_app_in_plain_c)

> **Note**:
> This application is artistic in nature, and doesn't aim to replicate Synesthesia (Chromesthesia) or to be scientifically accurate.