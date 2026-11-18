# VOX RE:STORE — Version 2.0 source

A JUCE C++17 VST3/Standalone vocal-cleanup prototype.

## Version 2 improvements
- Separate band-limited detectors for metallic/harsh mid-high energy, sibilance, and top-end fizz.
- Dynamic attenuation is applied to the detected bands rather than using one broad high-frequency cut.
- Attack/release envelope tracking and parameter smoothing.
- Softer downward expansion for noise/gap control, designed to retain vocal tails and breaths.
- Low-cut, low-shelf body shaping, restrained saturation for warmth, conservative levelling, dry/wet mix, output trim and bypass.
- 10-second analysis with level and spectral-activity measurements to propose conservative starting settings.
- Three presets: Gentle Repair, Metallic Clean-up, Smooth Sibilance.

## Important limitations
This is a DSP prototype, not a neural restoration product. The controls use conventional filters, envelope detectors, dynamic attenuation, expansion and saturation. AUTO measures broad signal statistics; it does not understand lyrics, isolate speech from accompaniment, identify all artifact types, or reconstruct missing words. It cannot guarantee removal of metallic ringing or AI artifacts. Listen and adjust by ear; use modest settings.

The V2 code is a substantial prototype step, but it still needs compilation, audio QA, parameter automation testing, CPU/denormal testing, and validation in target DAWs before production use. In particular, spectral repair, high-quality denoising, transient/consonant preservation, calibrated loudness matching and a proper multiband compressor are future engineering tasks.

## Build
Requirements: CMake 3.22+, C++17 compiler, internet on first configure.

```bash
cmake -S . -B build
cmake --build build --config Release
```

The VST3 bundle is normally under `build/VOX_RE_STORE_artefacts/Release/VST3/`.

Install:
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Linux: `~/.vst3/`

This download is source code, not a compiled installer. JUCE 8.0.6 is fetched by CMake on first configure.
