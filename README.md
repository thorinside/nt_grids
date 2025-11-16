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

*   **Length 1 / Length 2 / Length 3:** Sets the total number of steps in the sequence for each output (1-16).
*   **Fill 1 / Fill 2 / Fill 3:** Sets the number of triggers distributed as evenly as possible within the sequence length for each output (0-Length).
*   **Chaos Amount:** Controls the amount of random step-skipping/triggering (when Chaos is enabled).

## Inputs

Inputs are configured via the standard Disting NT parameter pages (`Routing` page).

*   **Clock Input:**
    *   Parameter: `Clock Input`
    *   Type: CV Input Bus (Select Off, Input 1-12, Output 1-8)
    *   Default: Input 1
    *   Function: Advances the internal sequencer based on a 24 PPQN (Pulses Per Quarter Note) clock signal. The internal step resolution is tied to this PPQN rate.
    *   Threshold: > ~0.5V
*   **Reset Input:**
    *   Parameter: `Reset Input`
    *   Type: CV Input Bus (Select Off, Input 1-12, Output 1-8)
    *   Default: Input 2
    *   Function: Resets the sequence to the first step on a rising edge.
    *   Threshold: > ~0.5V

## Outputs

Outputs are configured via the standard Disting NT parameter pages (`Routing` page).

*   **Trig 1 Output / Trig 2 Output / Trig 3 Output / Accent Output:**
    *   Parameter: `Trig X Output` / `Accent Output`
    *   Type: CV Output Bus (Select Off, Input 1-12, Output 1-8)
    *   Defaults: Trig 1=Output 3, Trig 2=Output 4, Trig 3=Output 5, Accent=Output 6
    *   Function: Outputs a trigger/gate signal when an event occurs for that channel.
    *   Signal: Fixed duration (~5ms), +5V high, 0V low.
    *   Mode (`Trig X Output mode` / `Accent Output mode`):
        *   `0` (Add): Adds the trigger voltage to any existing signal on the bus.
        *   `1` (Replace): Replaces any existing signal on the bus with the trigger voltage.

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

*   **Pots (L, C, R):** Control `Length 1-3` or `Fill 1-3` for their respective channels (Pot L for Ch1, Pot C for Ch2, Pot R for Ch3).
*   **Pot R Button Press:** Toggles whether the pots control Lengths or Fills.
*   **Encoder L:** No function.
*   **Encoder R:** Controls `Chaos Amount`.
*   **Screen:**
    *   Row 1: `L1:[len]:[fill]   L2:[len]:[fill]   L3:[len]:[fill]` (centered). The parameter type currently being controlled by the pots (Length or Fill) is highlighted.
    *   Row 2: `Chaos: [val/Off]` (Displayed position matches Drum Mode).

### Common UI Elements

*   **Top Center:** Displays algorithm title ("Grids", "by Emilie Gillet")
*   **Top Right:** Displays plugin version from git tags (e.g., "v1.1.0")

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
    make
    ```

3.  **Output:** The compiled plugin object file should be located at `plugins/nt_grids.o` (verify path based on your `Makefile`).

### Automated Builds

This repository includes a [GitHub Actions workflow](.github/workflows/release_nt_grids.yaml) that automatically builds the `nt_grids.o` file and packages it into a `nt_grids-plugin.zip` archive whenever a Git tag starting with `v` (e.g., `v1.0`) is pushed. The zip file is attached to the corresponding GitHub Release.

## License

This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE.md](LICENSE.md) file for details.

# Disclaimer

Documentation written by Gemini 2.5 LLM, and may not be factual.