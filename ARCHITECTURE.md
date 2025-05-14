# nt_grids Module Architecture

This document provides an overview of the `nt_grids` module architecture, its components, and their interactions.

## 1. Overview

The `nt_grids` module is a Eurorack-style pattern generator implemented for the Expert Sleepers Disting NT platform. It is based on the core logic of Emilie Gillet's Grids module, providing drum and Euclidean pattern generation capabilities. The module is designed to integrate with the Disting NT's hardware UI (pots, buttons, display) and CV I/O.

## 2. Main Components

The module consists of the following primary C++ components:

### 2.1. `nt_grids` (`nt_grids.h`, `nt_grids.cc`)
- **Role**: Main algorithm and platform integration.
- **Responsibilities**:
    - Implements the `_NT_algorithm` interface for the Disting NT.
    - Handles plugin lifecycle (construct, step, draw, parameter changes).
    - Manages UI interactions via `nt_grids_custom_ui` and `nt_grids_setup_ui`.
    - Orchestrates `PatternGenerator`, `TakeoverPot`, and CV I/O.
    - Defines and manages module parameters.
- **Key Dependencies**: `distingnt/api.h`, `PatternGenerator`, `TakeoverPot`, `NtPlatformAdapter`, `nt_grids_parameter_defs.h`, `nt_grids_resources.h`.

### 2.2. `PatternGenerator` (`nt_grids_pattern_generator.h`, `nt_grids_pattern_generator.cc`)
- **Role**: Core pattern generation logic.
- **Responsibilities**:
    - Generates drum patterns based on X/Y map interpolation and density.
    - Generates Euclidean rhythms.
    - Manages internal state for pattern generation (currently via static members).
    - Handles clock ticks and resets.
- **Key Dependencies**: `nt_grids_resources.h` (for lookup tables), `nt_grids_utils.h` (for `Random` class used in chaos).

### 2.3. `TakeoverPot` (`nt_grids_takeover_pot.h`, `nt_grids_takeover_pot.cc`)
- **Role**: Manages UI potentiometers with takeover logic to prevent value jumps and handles an optional alternate parameter controlled by the pot's button (specifically for Pot R in Drum mode).
- **Responsibilities**:
    - Manages the state of a single physical potentiometer.
    - Implements "takeover" behavior to prevent value jumps when a physical pot's position doesn't match the current parameter value.
    - Translates physical pot movements into parameter changes.
- **Key Dependencies**: `nt_grids.h` (for `NtGridsAlgorithm`, `ParameterIndex`), `distingnt/api.h` (for UI data structures).

### 2.4. `nt_grids_resources` (`nt_grids_resources.h`, `nt_grids_resources.cc`)
- **Role**: Stores static data for pattern generation.
- **Responsibilities**:
    - Provides lookup tables (drum maps, Euclidean patterns).
- **Key Dependencies**: Standard integer types.

### 2.5. `nt_grids_utils` (`nt_grids_utils.h`, `nt_grids_utils.cc`)
- **Role**: Provides utility functions and classes.
- **Responsibilities**:
    - Offers basic math utilities (e.g., `U8Mix`).
    - Implements a `Random` class for pseudo-random number generation (used for chaos).
- **Key Dependencies**: `distingnt/api.h` (for `NT_getCpuCycleCount` used in `Random::Init`).

### 2.6. `nt_grids_parameter_defs.h`
- **Role**: Centralized parameter definitions.
- **Responsibilities**:
    - Defines the `ParameterIndex` enum used throughout the module.

### 2.7. Platform Abstraction
- **Files**: `nt_platform_adapter.h`, `disting_nt_platform_adapter.h/.cc`, `tests/MockNtPlatformAdapter.h`
- **Role**: Decouples the core algorithm from platform-specific details.
- **Responsibilities**:
    - `nt_platform_adapter.h`: Defines the abstract interface for platform calls (e.g., getting sample rate, setting parameters, drawing to screen).
    - `disting_nt_platform_adapter.h/.cc`: Concrete implementation for the Disting NT hardware.
    - `MockNtPlatformAdapter.h`: Mock implementation used for unit testing, allowing tests to run on a host machine without actual hardware.

## 3. Key Data Flows and Interactions

- **UI Input**: `_NT_uiData` (pots, buttons) from the host is processed by `nt_grids_custom_ui`. `TakeoverPot` instances convert physical inputs to logical parameter changes, invoking `NtPlatformAdapter::setParameterFromUi`.
- **Parameter Updates**: `nt_grids_parameter_changed` (called when a parameter is modified) invokes `update_grids_from_params`, which in turn updates settings within the `PatternGenerator` (currently static settings).
- **Clocking and Reset**: The `nt_grids_step` function processes incoming CV clock/reset signals (from `busFrames`) and calls `PatternGenerator::TickClock()` or `PatternGenerator::Reset()`.
- **Pattern Output**: `PatternGenerator` updates its internal state (`PatternGenerator::state_`), which is read by `nt_grids_step` to produce trigger outputs on the `busFrames`.
- **Display Rendering**: `nt_grids_draw` reads current parameter values and relevant state from `NtGridsAlgorithm` and (indirectly) `PatternGenerator` to draw the UI via the `NtPlatformAdapter`.

## 4. Detailed Component Analysis

This section provides a more detailed analysis of each component, including code smells, areas for improvement, and specific observations.

### 4.1. `nt_grids` (`nt_grids.h`, `nt_grids.cc`)

#### `nt_grids.h` (Algorithm Structure Definition)
- **Overall Structure**: Clear definition for `NtGridsAlgorithm` struct.
- **Platform Adapter**: `m_platform_adapter_impl` (concrete) and `m_platform_adapter` (interface pointer) allow for proper injection for testing. Initialization in `nt_grids_construct` needs to be confirmed as correct.
- **Constructor**: Comment notes reliance on placement new. An explicit default constructor initializing all members (especially `m_platform_adapter` to `nullptr` or `&m_platform_adapter_impl` based on build) would improve clarity and safety.
- **`update()` method**: A member function `void update(const _NT_uiData &data);` is declared. Its role and relationship with `nt_grids_custom_ui` needs clarification from the `.cc` file (it appears not to be used/defined yet, `nt_grids_custom_ui` is the C-style callback).
- **Global Parameter Access**: `extern const _NT_parameter s_parameters[];` allows `nt_grids_takeover_pot.cc` to access global parameter definitions. This is a standard C/C++ pattern but creates a global compile-time dependency.

#### `nt_grids.cc` (Algorithm Implementation)
- **Overall Structure**: Follows Disting NT plugin patterns (GUID, static helpers, lifecycle callbacks, UI callbacks, factory, pluginEntry). Test cases are included under `TESTING_BUILD`.
- **Lifecycle Callbacks & `update_grids_from_params`**:
    - Standard lifecycle functions (`construct`, `parameterChanged`, etc.) appear correctly implemented for the Disting NT API.
    - `nt_grids_construct` handles member initialization comprehensively, including setting up the platform adapter pointer.
    - `update_grids_from_params` is a critical static helper translating the host's parameter array to `PatternGenerator` settings. It exhibits **high coupling** with the static `PatternGenerator` and its internal data structures (e.g., `PatternGenerator::settings_`). This is a **major area for refactoring** when `PatternGenerator` becomes instance-based (Task 2). Ideally, `PatternGenerator` instances would expose methods to update their own configuration based on parameter values, rather than `nt_grids.cc` modifying `PatternGenerator` internals.
- **`nt_grids_step` (Main Processing Loop) & Helpers**:
    - Handles CV input (clock/reset) with edge detection and generates trigger outputs.
    - Trigger activation logic (`update_trigger_activation_state`) correctly uses `m_clock_event_this_step` to ensure triggers fire only once per relevant clock event.
    - `process_trigger_output` helper correctly handles output bus selection and replace/add modes.
    - **Code Smells/Improvements**:
        - **Magic Numbers**: The value `28` (max CV input/output bus index) appears multiple times. This should be a named constant (e.g., `kMaxCvBuses`).
        - Hardcoded values for `desired_trigger_duration_seconds` (0.001f), `trigger_on_voltage` (5.0f), `trigger_off_voltage` (0.0f) could be `const static float` variables for clarity and maintainability.
        - **`busFrames` Indexing**: Assumes a planar-per-bus layout for `busFrames` (e.g., `busFrames[bus_idx * num_frames_total + sample_idx]`). This needs to be an explicit understanding based on Disting NT API to ensure correctness. (Self-correction: this is typical for such APIs).
        - Direct access to `PatternGenerator::state_` will need to change if `PatternGenerator` becomes instance-based.
- **UI Callbacks (`nt_grids_setup_ui`, `nt_grids_custom_ui`) & Helpers**:
    - `nt_grids_setup_ui` correctly initializes physical pot display values and `TakeoverPot` states.
    - `nt_grids_custom_ui` orchestrates UI event handling in a logical order: mode toggles, then Length/Fill toggle, then pot processing, then encoders. Helper functions (`handle_..._button`, `determine_..._config`, `process_pots_ui`, `handle_encoder_input`) effectively manage complexity.
    - Logic for reconfiguring `TakeoverPot` instances after mode or Length/Fill toggles (in `handle_mode_toggle_button` and `handle_euclidean_length_fill_toggle_button`) is now robust due to recent fixes.
    - **Code Smells/Improvements**:
        - **Redundant `TakeoverPot::configure` calls**: `process_pots_ui` calls `m_pots[i].configure(...)` on every UI update, even if the mode/sub-mode (and thus pot configuration) hasn't changed. While safe (as `configure` is currently lightweight), it's potentially redundant. Could be optimized to only call `configure` when the underlying parameters a pot targets actually change.
        - Constants like `MAX_PARAM_VAL_PERCENT_255` (255.0f) and `MAX_PARAM_VAL_PERCENT_32` (32.0f) used in `nt_grids_custom_ui` and helpers could be file-scope `const static float` for better organization.
- **Drawing (`nt_grids_draw`) & Helpers**:
    - Well-modularized with helpers like `draw_drum_mode_ui` and `draw_euclidean_mode_ui`.
    - Uses the platform adapter for all drawing operations.
    - Hardcoded coordinates for UI elements are standard for fixed-display embedded UIs.
    - `draw_status_indicators` correctly clears `m_clock_event_this_step` and `m_reset_event_this_step` flags after displaying them.
    - **Code Smells/Improvements**:
        - **Duplicated Logic (Euclidean Hits Display)**: `draw_euclidean_mode_ui` recalculates the number of Euclidean hits by looking up `lut_res_euclidean`. This logic is likely duplicated from `PatternGenerator::ComputeEuclideanPattern`. This is a candidate for refactoring (related to Task 6) if `PatternGenerator` could provide this information directly (e.g., via a method `getEuclideanHitCount(track_idx)`).

### 4.2. `PatternGenerator` (`nt_grids_pattern_generator.h`, `nt_grids_pattern_generator.cc`)

- **Dominant Architectural Trait: All Static Members and Methods**.
    - The `PatternGenerator` class is effectively a global singleton, with all its state (settings, current step, output triggers, etc.) and logic implemented as static members and methods.
    - **Pros**: Simple to call from anywhere in `nt_grids.cc` (e.g., `PatternGenerator::TickClock()`).
    - **Cons**: Limits reusability, makes isolated unit testing harder (requires managing global state), and is generally an anti-pattern for larger, more complex systems. For the single-instance nature of a Disting NT algorithm, it's functional but not ideal for code organization or future flexibility.
    - **Task 2 ("Refactor PatternGenerator to Use Instance-Based State Management") directly addresses this.** When refactored, static members will become instance members, and static methods will become non-static member functions.
- **State Management**:
    - `settings_[2]` (for Drum/Euclidean modes), `options_` (global options like clocking mode), `chaos_globally_enabled_`, `state_` (current output triggers), `step_` (current sequence step), `current_euclidean_length_`, `fill_` (scaled Euclidean fill), and numerous other static variables manage all aspects of pattern configuration and playback state.
    - `Init()` method in `.cc` initializes all this static state to default values using `memset` and direct assignments.
- **Core Logic (`TickClock`, `Evaluate`, `EvaluateDrums`, `EvaluateEuclidean`, `ReadDrumMap`)**:
    - `TickClock()`: Advances the pattern. Supports two clocking modes: a direct external clock mode (default, likely preferred for Disting NT) and an "original Grids clocking" mode emulating internal sub-ticks. The logic for advancing the main sequence step and per-track Euclidean steps differs based on this mode. Calls `Evaluate()` when the main step advances.
    - `IncrementPulseCounter()`: Manages trigger pulse duration. Clears `state_` after `kPulseDuration` (currently 8) ticks unless in gate mode. The definition of a "tick" here (external vs. internal) depends on the clocking mode.
    - `ReadDrumMap()`: Implements bilinear interpolation on the 5x5 drum map lookup table to derive values for drum pattern generation. Ported directly from original Grids.
    - `EvaluateDrums()`: Generates drum patterns based on map values from `ReadDrumMap`, applies density thresholds, and incorporates randomness (`part_perturbation_`). Accent generation is simplified to trigger if any part has a high-level event.
    - `EvaluateEuclidean()`: Generates Euclidean patterns using a lookup table (`lut_res_euclidean`) based on track length and scaled fill/density (0-255 fill parameter is scaled to 0-31 for LUT). Includes a simplified chaos mechanism that can flip trigger states or add accents.
    - `Evaluate()`: Top-level evaluation function called by `TickClock` and `Reset`. It dispatches to `EvaluateDrums` or `EvaluateEuclidean` based on the current `options_.output_mode`.
- **Parameter Setters (`SetLength`, `SetFill`, `set_global_chaos`, `set_output_mode`, etc.)**:
    - These static methods are called from `nt_grids.cc` (usually via `update_grids_from_params`) to update the static state variables within `PatternGenerator`.
    - `SetFill` stores the raw 0-255 UI parameter value into `settings_[OUTPUT_MODE_EUCLIDEAN].density[channel]`, which is then scaled and used by `EvaluateEuclidean` for LUT lookup.
- **Code Smells/Improvements (beyond the static nature)**:
    - **Hardcoded Accent Threshold (192)** in `EvaluateDrums`: Could be a named constant.
    - **Euclidean Chaos Logic**: Noted in comments as simplified and potentially needing refinement.
    - **Centralized Euclidean Pattern Calculation (Task 6)**: The LUT lookup and pattern bit extraction logic within `EvaluateEuclidean` is a candidate for encapsulation into a reusable helper function (e.g., `uint32_t ComputeEuclideanPatternBits(uint8_t length, uint8_t fill_param_0_255)`). This same helper could then be used by `draw_euclidean_mode_ui` in `nt_grids.cc` to avoid duplicating the LUT access and bit counting for display purposes.
- **Coupling**: `nt_grids.cc` is tightly coupled to the static nature of `PatternGenerator`, directly reading static members like `PatternGenerator::state_` and calling static methods. The `update_grids_from_params` function in `nt_grids.cc` also directly manipulates `PatternGenerator::settings_` arrays.

### 4.3. `TakeoverPot` (`nt_grids_takeover_pot.h`, `nt_grids_takeover_pot.cc`)

- **Role**: Manages UI potentiometers with takeover logic to prevent value jumps and handles an optional alternate parameter controlled by the pot's button (specifically for Pot R in Drum mode).
- **`nt_grids_takeover_pot.h` (Class Definition)**:
    - **Interface**: Provides `init`, `configure`, `resetTakeoverForModeSwitch`, `resetTakeoverForNewPrimary`, `syncPhysicalValue`, and `update` methods for lifecycle and interaction.
    - **Testability**: Uses `#ifdef TESTING_BUILD` to make internal state variables (like `m_primary_param`, `m_takeover_active_primary`) public for unit testing, while keeping them private in production builds. This is a good approach.
    - **Dependencies**: Forward declares `NtGridsAlgorithm` and includes `nt_grids_parameter_defs.h`.
- **`nt_grids_takeover_pot.cc` (Implementation)**:
    - **Constructor & `init`**: Properly initialize all members to default/safe states. `init` correctly uses the platform adapter (via `m_algo`) to get the pot's button mask.
    - **`configure`**: Sets the pot's target parameters (primary, alternate), scales, and `m_has_alternate` flag. Includes a safeguard against zero scales.
    - **Takeover Reset/Sync**: `resetTakeoverForModeSwitch`, `resetTakeoverForNewPrimary`, and `syncPhysicalValue` correctly manage the activation and deactivation of takeover states for different scenarios (mode changes, primary parameter changes within a mode, initial UI setup).
    - **`setParameter` Helper**: Private method that encapsulates clamping the value (using parameter definitions obtained via the platform adapter) and calling `m_algo->m_platform_adapter->setParameterFromUi()`.
    - **`update()` Method (Core Logic)**:
        - **Alternate Control**: Correctly identifies when Pot R in Drum mode should control its alternate parameter (Chaos Amount) based on its button state (pressed vs. released, edge detection). Sets `m_is_controlling_alternate` flag appropriately.
        - **Takeover for Primary Parameter**: Implements takeover logic correctly when `m_is_controlling_alternate` is false. It checks if the physical pot value has crossed the `m_primary_takeover_value` before allowing the parameter to be set.
        - **Code Smells/Improvements**:
            - **Missing Takeover for Alternate Parameter**: When `m_is_controlling_alternate` is true (Drum mode, Pot R button held, controlling Chaos), the `update()` method directly calls `setParameter` for the alternate parameter without checking/applying takeover logic (i.e., no check against `m_alternate_takeover_value`). This could lead to value jumps for the Chaos parameter when its button is first pressed if the pot isn't already near the current Chaos value. This should be made consistent with primary parameter takeover.
            - **Debug `drawText` Call**: Contains a `drawText` call inside the primary parameter handling logic that outputs debug information. This should be wrapped in `#ifdef TESTING_BUILD` or removed for release builds.
    - **Coupling**: `TakeoverPot` instances hold a pointer to `NtGridsAlgorithm` (`m_algo`) to access the platform adapter and current parameter values (`m_algo->v[]`). This is a necessary coupling for its functionality.

### 4.4. `nt_grids_resources` (`nt_grids_resources.h`, `nt_grids_resources.cc`)

- **Role**: Defines and provides access to large, static data tables used by `PatternGenerator`.
- **`nt_grids_resources.h` (Declarations)**:
    - Declares constants for data sizes (`LUT_RES_EUCLIDEAN_SIZE`, `NODE_DATA_SIZE`).
    - Provides `extern const` declarations for `lut_res_euclidean` (Euclidean pattern lookup table) and `node_0` through `node_24` (raw drum map data arrays).
    - Defines `struct DrumMapAccess` which contains `static const uint8_t *const drum_map_ptr[5][5];` This provides a structured way to access the drum map grid.
- **`nt_grids_resources.cc` (Definitions)**:
    - Contains the actual large `const` array definitions for `lut_res_euclidean` and all `node_X` arrays.
    - Initializes `DrumMapAccess::drum_map_ptr` to point to the respective `node_X` arrays, forming the 5x5 grid used by `PatternGenerator::ReadDrumMap`.
- **Design**: This component effectively isolates large, static data, which is a good practice. The data itself is a direct port from the original Grids firmware.

### 4.5. `nt_grids_utils` (`nt_grids_utils.h`, `nt_grids_utils.cc`)

- **Role**: Provides miscellaneous utility functions and a pseudo-random number generator.
- **`nt_grids_utils.h` (Inline Functions & Class Definition)**:
    - Defines `DISALLOW_COPY_AND_ASSIGN` macro.
    - Provides small, inline utility functions ported from original Grids: `U8Mix`, `U8U8MulShift8`, `U8U8Mul`.
    - Defines a static `Random` class for pseudo-random number generation:
        - Implements a Galois LFSR.
        - `Init()` method handles seeding, using `NT_getCpuCycleCount()` for production and a fixed seed for `TESTING_BUILD` (ensuring test reproducibility).
        - `Seed(uint16_t)` allows explicit seeding.
        - `GetByte()`, `GetWord()` are the main methods to retrieve random numbers.
- **`nt_grids_utils.cc` (Static Member Definitions)**:
    - Provides the necessary definitions for the static members of the `Random` class (`rng_state_`, `s_seeded_`).
- **Design**: The utility functions are straightforward. The `Random` class being static provides a global RNG, which is sufficient for current needs (chaos in `PatternGenerator`) but might be a consideration if multiple independent random sources were required in the future.

### 4.6. Platform Adapter (`disting_nt_platform_adapter.h`, `disting_nt_platform_adapter.cc`, `nt_platform_adapter.h`)

- **Role**: Abstracts platform-specific calls (Disting NT API) from the core algorithm logic, facilitating testing and portability.
- **`nt_platform_adapter.h` (Interface)**:
    - Defines the `INtPlatformAdapter` pure virtual interface class.
    - Declares methods like `setParameterFromUi`, `getAlgorithmIndex`, `getParameterOffset`, `getPotButtonMask`, `getSampleRate`, `drawText`, `intToString`, `getParameterDefinition`.
    - This interface is used by `NtGridsAlgorithm` and `TakeoverPot`.
- **`disting_nt_platform_adapter.h` (Concrete Implementation for Disting NT)**:
    - Declares `DistingNtPlatformAdapter` which inherits from `INtPlatformAdapter`.
    - This class will implement the interface methods by calling the actual Disting NT API functions.
- **`disting_nt_platform_adapter.cc` (Implementation Details - *Assumed based on typical adapter pattern*)**:
    - Would contain the definitions of the `DistingNtPlatformAdapter` methods, wrapping calls like `NT_setParameterFromUi`, `NT_getAlgorithmIndex`, etc.
- **Design**: The adapter pattern is well-suited here. It decouples `NtGridsAlgorithm` from direct Disting NT API calls, allowing a `MockNtPlatformAdapter` (seen in tests) to be injected for unit testing. This is a strong point of the current architecture for testability.

## 5. Identified Areas for Refactoring (Consolidated & Prioritized based on Detailed Analysis)

Based on the detailed component analysis in Section 4, the following specific areas for refactoring have been identified. They are grouped by component and cross-referenced with existing Task Master tasks where applicable.

### 5.1. `PatternGenerator` (`nt_grids_pattern_generator.h`, `.cc`)

1.  **Instance-Based State Management (Task 2)**: Convert all static members (e.g., `settings_`, `options_`, `state_`, `step_`, `current_euclidean_length_`) and static methods (`TickClock`, `Evaluate`, `SetLength`, etc.) to non-static instance members and methods. This is the highest priority refactoring for this component to improve modularity, testability, and adherence to OOP principles.
    *   _Impacts_: `nt_grids.cc` will need to instantiate `PatternGenerator` and call its methods. `update_grids_from_params` in `nt_grids.cc` will change significantly.
2.  **Centralize Euclidean Pattern Calculation (Task 6)**: Create a reusable helper function (e.g., `uint32_t ComputeEuclideanPatternBits(uint8_t length, uint8_t fill_param_0_255)`) within `PatternGenerator` to encapsulate the LUT lookup and pattern bit extraction currently in `EvaluateEuclidean`. This function can then be used internally and potentially exposed for use by `nt_grids.cc` (e.g., for display logic).
    *   _Addresses_: Duplication of LUT logic between `EvaluateEuclidean` and `draw_euclidean_mode_ui`.
3.  **Refine Euclidean Chaos Logic**: The current chaos implementation in `EvaluateEuclidean` is noted as simplified. Review and potentially refine this for more nuanced behavior, possibly making its parameters (like flip probability) configurable.
    *   _Consideration_: May involve new parameters or more complex internal state.
4.  **Constants for Hardcoded Values**: Replace magic numbers like the accent threshold (192) in `EvaluateDrums` with named constants (e.g., `kDrumAccentThreshold`).

### 5.2. `nt_grids` (`nt_grids.h`, `.cc`)

1.  **Adapt to Instance-Based `PatternGenerator` (Related to Task 2)**: Modify `nt_grids.cc` to instantiate and use the refactored `PatternGenerator` object. This includes:
    *   Changing `update_grids_from_params` to call methods on the `PatternGenerator` instance to set its configuration, rather than directly modifying static members like `PatternGenerator::settings_`.
    *   Updating `nt_grids_step` to call methods on the `PatternGenerator` instance (e.g., `instance.TickClock()`, `instance.get_trigger_state()`).
2.  **Use Centralized Euclidean Logic for Display (Related to Task 6)**: Modify `draw_euclidean_mode_ui` to use the new centralized Euclidean pattern calculation function from `PatternGenerator` (from 5.1.2) to get pattern data or hit counts, instead of re-implementing LUT access.
3.  **Named Constants for Magic Numbers**: In `nt_grids_step`, replace the magic number `28` (max CV bus index) with a named constant (e.g., `kMaxCvBuses`). Define constants for `desired_trigger_duration_seconds`, `trigger_on_voltage`, `trigger_off_voltage`.
4.  **File-Scope Constants for UI Values**: Group UI-related magic numbers like `MAX_PARAM_VAL_PERCENT_255` (255.0f) into file-scope `const static float` variables for better organization and clarity.
5.  **Review `TakeoverPot::configure` Call Frequency**: Investigate if the `m_pots[i].configure(...)` call within `process_pots_ui` (called on every UI update) is always necessary or if it can be called only when the pot's target parameters actually change. (Minor optimization).
6.  **Remove or Define `NtGridsAlgorithm::update()`**: The `update(const _NT_uiData &data)` method is declared in `nt_grids.h` but not defined or used in `nt_grids.cc`. Decide if it's intended for future use, implement it, or remove the declaration.

### 5.3. `TakeoverPot` (`nt_grids_takeover_pot.h`, `.cc`)

1.  **Implement Takeover for Alternate Parameter**: In the `update()` method, when `m_is_controlling_alternate` is true (Drum mode, Pot R button held), add takeover logic for the alternate parameter. This should mirror the existing takeover logic for the primary parameter, checking against `m_alternate_takeover_value` before setting the parameter.
    *   _Addresses_: Potential value jumps for the Chaos parameter when Pot R button is first pressed.
2.  **Conditional Debug Output**: Ensure the `drawText` call used for debugging in `TakeoverPot::update()` is wrapped in `#ifdef TESTING_BUILD` or removed for release builds.

### 5.4. `nt_grids_utils` (`nt_grids_utils.h`, `.cc`)

1.  **Consider Instance-Based `Random` (Low Priority)**: The current static `Random` class is sufficient. If future needs require multiple independent pseudo-random sequences, refactoring `Random` to be instance-based could be considered. For now, this is a minor point.

## 6. Refactoring Tasks

This section will be populated with specific areas identified for improvement as the refactoring task (Task 1) progresses. Initial thoughts include:
- Transitioning `PatternGenerator` from static to instance-based state.
- Replacing magic numbers with named constants.
- Centralizing duplicated logic (e.g., Euclidean pulse calculations).
- Clarifying and potentially refactoring the `PatternGenerator` option/settings hierarchy.
- Improving the structure for mode-specific behaviors (Drum vs. Euclidean) within `NtGridsAlgorithm`, possibly using a Strategy pattern.
- Encapsulating direct access to `PatternGenerator`'s internal state.
- Reviewing parameter scaling and mapping for consistency.
- Further modularizing complex functions like `nt_grids_custom_ui`. 