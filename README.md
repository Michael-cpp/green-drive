# Green Drive

A circuit-level TS808-style overdrive plugin (VST3 and standalone), built with JUCE.

## How it works

The plugin models the circuit stage by stage, using its real component values:

- Input coupling and buffer
- Non-inverting clipping amplifier with antiparallel 1N4148 diodes in the feedback loop, solved sample by sample
- Post-clipping low-pass filter and active tone control
- Level control and output stage

The nonlinear stage runs at 4x oversampling. Op-amps are treated as ideal apart from an estimated output swing limit.

The pedal interface is rendered procedurally, with no image files.

## Controls

| Control | Purpose |
|---|---|
| Drive | Clipping amplifier gain |
| Tone | Active tone control |
| Level | Output level |
| Input Cal | Sets how hot the DAW signal looks to the circuit; 0 dB means 0 dBFS = 1 V peak |
| Footswitch | Bypass |

## Building

Requires CMake 3.22 or newer, Git and a C++17 compiler. JUCE is downloaded automatically during configuration.

```
cmake -S . -B build
cmake --build build --config Release --target GreenDrive_VST3 GreenDrive_Standalone
```

The plugin is written to `build/GreenDrive_artefacts/Release/VST3`.

To run the DSP checks:

```
cmake --build build --config Release --target ModelTest
build/Release/ModelTest
```
