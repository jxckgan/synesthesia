# synesthesia

Synesthesia is an application which provides a real-time visualisation of audio frequencies represented as colour. Features a rudimentary EQ, real-time frequency & wavelength information, and an audio input selection system.

### Building & Running

As of now, this project uses Metal for rendering and subsequently only runs on macOS systems. ***(Working on an OpenGL implementation for Windows/Linux systems).*** To build/run this project, make sure you've installed `GLFW` and `portaudio` using [homebrew](https://brew.sh) and have Xcode installed. To run this project, just clone this repository w/ submodules and run `make run`:

```sh
git clone --recurse-submodules https://github.com/jxckgan/synesthesia \
make run
```

### Todo

- [ ] Cross-platform support
- [ ] Better/granular EQ control
- [ ] Ability for visualisation of multiple frequency bands
- [ ] Generally more control over settings
- [ ] DAW-integration

### Credits & Footnotes

- Visible Light Spectrum to RGB: [Endolith](https://www.endolith.com/wordpress/2010/09/15/a-mapping-between-musical-notes-and-colors/)
- GUI built w/  [Dear ImGui](https://github.com/ocornut/imgui)
- Lightweight FFT possible w/ [KissFFT](https://github.com/mborgerding/kissfft)

> **Note**:
> This application is artistic in nature, and doesn't aim to replicate either Synesthesia (Chromesthesia) or to be scientifically accurate.