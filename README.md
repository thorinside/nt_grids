# NT Grids for Disting NT

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
<!-- Optional: Add a badge for your latest release if you set one up -->
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/thorinside/nt_grids?label=latest%20release)](https://github.com/thorinside/nt_grids/releases/latest)

A port of Emilie Gillet's renowned Mutable Instruments Grids topographic drum sequencer for the Expert Sleepers Disting NT platform.

## History & Acknowledgements

This project brings the unique algorithmic drum pattern generation of the original Mutable Instruments Grids module to the Disting NT. Grids is described as a "topographic drum sequencer" - it generates a variety of drum patterns based on continuous interpolation through a "map" of patterns (Drum Mode) or using Euclidean algorithms (Euclidean Mode).

*   **Original Concept & Code:** Emilie Gillet (Mutable Instruments). The original Eurorack module source code can be found [here](https://github.com/pichenettes/eurorack/tree/master/grids).
*   **Disting NT Port:** Neal Sanche (GitHub: Thorinside)

The original Grids firmware is licensed under the GPLv3 license, however, as this port utilizes the Disting NT SDK which itself appears to use MIT-licensed components, this port is released under the **GNU General Public License v3.0 (GPLv3)**, consistent with the license used in the `nt_grids.cc` source file itself.

## Features

*   **Two Modes:** Switch between classic Drum map interpolation and Euclidean pattern generation.
*   **Three Trigger Outputs:** Generate patterns for three independent drum voices (e.g., Kick, Snare, Hat).
*   **Accent Output:** Provides an additional accent trigger output common to many Grids patterns.
*   **Clock Input:** External clock synchronization.
*   **Reset Input:** Resets the sequencer pattern to the beginning.
*   **Swing:** Apply swing timing to the generated patterns (On/Off).
*   **Chaos:** Introduce controlled randomness to patterns (On/Off, with Amount control).
*   **Custom Disting NT UI:** Optimized controls for hands-on tweaking using the Disting NT's pots, encoders, and buttons.

## Modes

### 1. Drum Mode

Generates patterns by interpolating through a 2D map of pre-analyzed drum patterns.

*   **Map X / Map Y:** Controls the position on the pattern map (0-255). Small changes typically result in related rhythmic variations.
*   **Density 1 / Density 2 / Density 3:** Controls the event density (fill) for each of the three main trigger outputs (0-255).
*   **Chaos Amount:** Controls the amount of randomness applied (when Chaos is enabled).

### 2. Euclidean Mode

Generates classic Euclidean rhythms for each of the three main trigger outputs independently.

*   **Length 1 / Length 2 / Length 3:** Sets the total number of steps in the sequence for each output (1-32).
*   **Fill 1 / Fill 2 / Fill 3:** Sets the number of triggers distributed as evenly as possible within the sequence length for each output (0-Length).
*   **Chaos Amount:** Controls the amount of random step-skipping/triggering (when Chaos is enabled).

## Inputs

Inputs are configured via the standard Disting NT parameter pages (`Routing` page).

*   **Clock In:**
    *   Parameter: `Clock In`
    *   Type: CV Input Bus (Select Bus 1-28)
    *   Function: Advances the internal sequencer based on a 24 PPQN (Pulses Per Quarter Note) clock signal. The internal step resolution is tied to this PPQN rate.
    *   Threshold: > ~0.5V
*   **Reset In:**
    *   Parameter: `Reset In`
    *   Type: CV Input Bus (Select Bus 1-28)
    *   Function: Resets the sequence to the first step on a rising edge.
    *   Threshold: > ~0.5V

## Outputs

Outputs are configured via the standard Disting NT parameter pages (`Routing` page).

*   **Trig 1 Out / Trig 2 Out / Trig 3 Out / Accent Out:**
    *   Parameter: `Trig X Out` / `Accent Out`
    *   Type: CV Output Bus (Select Bus 1-28)
    *   Function: Outputs a trigger/gate signal when an event occurs for that channel.
    *   Signal: Fixed duration (~5ms), +10V high, 0V low.
    *   Mode (`Trig X Out Mode` / `Accent Out Mode`):
        *   `0` (Off/Add): Adds the trigger voltage to any existing signal on the bus.
        *   `1` (On/Replace): Replaces any existing signal on the bus with the trigger voltage.

## Custom UI Mappings

The custom UI provides quick access to the most commonly used parameters for each mode.

*   **Mode Switch:** Short press the **Right Encoder Button** to toggle between **Drum Mode** and **Euclidean Mode**.

### Drum Mode UI

*   **Pot L:** Controls `Density 1`
*   **Pot C:** Controls `Density 2`
*   **Pot R (Turn Only):** Controls `Density 3` (with smooth takeover when releasing button)
*   **Pot R (Button Held + Turn):** Controls `Chaos Amount` (smooth takeover applies when releasing button)
*   **Encoder L:** Controls `Map X`
*   **Encoder R:** Controls `Map Y`
*   **Screen:**
    *   Row 1: `D1: [val]   D2: [val]   D3: [val]`
    *   Row 2: `   X: [val]   Y: [val]   Chaos: [val/Off]`

### Euclidean Mode UI

*   **Pot L:** Controls `Fill 1`
*   **Pot C:** Controls `Fill 2`
*   **Pot R:** Controls `Fill 3`
*   **Encoder L:** Controls `Chaos Amount` (steps of 5)
*   **Encoder R:** Controls `Length 1`
*   **Screen:**
    *   Row 1: `L1: [len]:[fill]   L2: [len]:[fill]   L3: [len]:[fill]` (centered)

### Common UI Elements

*   **Top Bar:** Displays algorithm title ("Grids", "by Emilie Gilet")
*   **Top Right:** Briefly displays "CLK" on incoming clock ticks and "RST" on reset events.

## Building from Source

### Prerequisites

*   **GNU Arm Embedded Toolchain:** Required for compiling for the Disting NT's ARM processor.
    *   macOS: `brew install --cask gcc-arm-embedded`
    *   Other OS: Download from [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)
*   **Faust Compiler:** While this specific plugin doesn't use Faust, the Disting NT SDK environment often relies on it being present.
    *   macOS: `brew install faust`
    *   Other OS: See [Faust Installation Instructions](https://faust.grame.fr/manual/installing/)
*   **Make:** Standard build utility.

### Build Process

1.  **Clone the repository:**
    ```bash
    git clone --recursive https://github.com/thorinside/nt_grids.git
    cd nt_grids
    ```
    *(Ensure `--recursive` is used to fetch submodules like `distingNT_API`)*

2.  **Compile:**
    ```bash
    make clean
    make nt_grids.o # Or potentially just 'make' depending on Makefile setup
    ```

3.  **Output:** The compiled plugin object file should be located at `plugins/nt_grids.o` (verify path based on your `Makefile`).

### Automated Builds

This repository includes a [GitHub Actions workflow](.github/workflows/release_nt_grids.yaml) that automatically builds the `nt_grids.o` file and packages it into a `nt_grids-plugin.zip` archive whenever a Git tag starting with `v` (e.g., `v1.0`) is pushed. The zip file is attached to the corresponding GitHub Release.

## License

This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE.md](LICENSE.md) file for details.

# Disclaimer

Documentation written by Gemini 2.5 LLM, and may not be factual.