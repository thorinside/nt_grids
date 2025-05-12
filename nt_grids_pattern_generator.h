// Copyright (C) 2012 Emilie Gillet.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// Based on the original Grids by Emilie Gillet.

#ifndef NT_GRIDS_PATTERN_GENERATOR_H_
#define NT_GRIDS_PATTERN_GENERATOR_H_

#include <stdint.h>
#include <cstring> // For memset, C++ style

#include "nt_grids_utils.h"     // For DISALLOW_COPY_AND_ASSIGN, Random
#include "nt_grids_resources.h" // For kNumParts, kStepsPerPattern, data access

namespace nt_grids_port
{
  namespace grids
  {

    // Constants from original grids/pattern_generator.h
    // kNumParts and kStepsPerPattern are also in nt_grids_resources.h, ensure they are consistent.
    // For now, we trust nt_grids_resources.h definitions.
    // const uint8_t kNumParts = 3;
    // const uint8_t kStepsPerPattern = 32;
    const uint8_t kPulsesPerStep = 3; // 24 ppqn ; 8 steps per quarter note.
    const uint8_t kPulseDuration = 8; // 8 ticks of the main clock.

    struct DrumsSettings
    {
      uint8_t x;
      uint8_t y;
      uint8_t randomness; // Chaos amount for drum patterns
    };

    struct EuclideanSettings // Options specific to Euclidean mode
    {
      // Length for each part is managed by PatternGenerator::current_euclidean_length_ directly,
      // not stored in this settings struct, to simplify updates from parameters.
      uint8_t chaos_amount; // Chaos amount for Euclidean patterns
    };

    struct PatternGeneratorSettings
    {
      union Options
      {
        DrumsSettings drums;
        EuclideanSettings euclidean; // Use the new struct for Euclidean options
      } options;
      uint8_t density[kNumParts]; // kNumParts is from nt_grids_resources.h
    };

    enum OutputMode
    {
      OUTPUT_MODE_EUCLIDEAN,
      OUTPUT_MODE_DRUMS
    };

    enum ClockResolution
    {
      CLOCK_RESOLUTION_4_PPQN,
      CLOCK_RESOLUTION_8_PPQN,
      CLOCK_RESOLUTION_24_PPQN,
      CLOCK_RESOLUTION_LAST
    };

    // OutputBits might not be directly used in the Disting NT context
    // but kept for porting completeness of the Options struct.
    enum OutputBits
    {
      OUTPUT_BIT_TRIG_1 = 1,
      OUTPUT_BIT_TRIG_2 = 2,
      OUTPUT_BIT_TRIG_3 = 4,
      OUTPUT_BIT_ACCENT = 8,
      OUTPUT_BIT_CLOCK = 16, // May not be used directly by Disting NT plugin outputs
      OUTPUT_BIT_RESET = 32  // May not be used directly
    };

    struct Options
    {
      ClockResolution clock_resolution;
      OutputMode output_mode;
      bool output_clock;            // True if Grids itself should output a clock signal
      bool gate_mode;               // True for gates, false for triggers
      bool original_grids_clocking; // True to use original Grids sub-tick clocking logic

      // pack() and unpack() were for EEPROM storage on the original AVR hardware.
      // May not be strictly necessary for Disting NT presets but retained for porting completeness.
      uint8_t pack() const
      {
        uint8_t byte = clock_resolution;
        if (output_clock)
        {
          byte |= 0x20;
        }
        if (output_mode == OUTPUT_MODE_DRUMS)
        {
          byte |= 0x40;
        }
        if (!gate_mode)
        {
          byte |= 0x80;
        }
        return byte;
      }

      void unpack(uint8_t byte)
      {
        gate_mode = !(byte & 0x80);
        output_mode = (byte & 0x40) ? OUTPUT_MODE_DRUMS : OUTPUT_MODE_EUCLIDEAN;
        output_clock = byte & 0x20;
        clock_resolution = static_cast<ClockResolution>(byte & 0x7);
        if (clock_resolution >= CLOCK_RESOLUTION_LAST) // Safety check for unpacked value
        {
          clock_resolution = CLOCK_RESOLUTION_24_PPQN;
        }
      }
    };

    class PatternGenerator
    {
    public:
      PatternGenerator() {} // Non-static constructor
      ~PatternGenerator() {}

      static const uint8_t kOriginalGridsPulsesPerStep = 3; // For original Grids clocking mode (24PPQN / 8th note = 3)

      static void Init();      // Initializes with default settings
      static void Reset();     // Resets pattern to the beginning
      static void Retrigger(); // Re-evaluates and outputs the current step's triggers

      // Advances the pattern based on an external clock tick.
      // Behavior depends on `original_grids_clocking` option.
      static void TickClock(bool external_clock_tick);

      static uint8_t step() { return step_; } // Current step in the main 32-step sequence (0-31)

      // --- Option Accessors & Mutators --- Tied to Disting NT parameters
      static bool output_clock_active() { return options_.output_clock; }
      static bool gate_mode_active() { return options_.gate_mode; }
      static OutputMode current_output_mode() { return options_.output_mode; }
      static ClockResolution current_clock_resolution() { return options_.clock_resolution; }

      static void set_output_clock_active(bool active) { options_.output_clock = active; }
      static void set_output_mode(OutputMode mode)
      {
        options_.output_mode = mode;
      }
      static void set_clock_resolution(ClockResolution resolution)
      {
        if (resolution >= CLOCK_RESOLUTION_LAST)
        {
          resolution = CLOCK_RESOLUTION_24_PPQN;
        }
        options_.clock_resolution = resolution;
      }
      static void set_gate_mode(bool active) { options_.gate_mode = active; }
      static void set_original_grids_clocking(bool enabled);

      // Manages pulse durations for trigger outputs
      static void IncrementPulseCounter();

      static PatternGeneratorSettings settings_[2]; // Index 0 for Euclidean, 1 for Drums
      static Options options_;
      static bool chaos_globally_enabled_; // Master switch for chaos effects
      static void set_global_chaos(bool enabled) { chaos_globally_enabled_ = enabled; }

      // Provides the current trigger state for all parts (and accent).
      // Bit 0: Part 1 (BD/EUC1), Bit 1: Part 2 (SD/EUC2), Bit 2: Part 3 (HH/EUC3)
      // Bit 3: Accent (in Drum mode or chaotic Euclidean)
      static uint8_t get_trigger_state() { return state_; }

      // --- Status Info ---
      static bool on_first_beat() { return first_beat_; }
      static bool on_beat() { return beat_; }

      // --- Euclidean Parameter Setters ---
      static void SetLength(uint8_t channel, uint8_t length);
      static void SetFill(uint8_t channel, uint8_t fill_param_value); // fill_param_value is 0-255 from UI

      // --- Internal State Variables ---
      static uint8_t output_buffer_[kStepsPerPattern >> 3]; // Stores the full pattern bitmask (not directly used for real-time state_)
      static uint8_t pulse_counter_[kNumParts];             // Tracks individual part pulse counts (if ever needed)
      static uint8_t pulse_duration_[kNumParts];            // Individual pulse durations (if ever needed, currently global via state_)
      static uint8_t current_euclidean_length_[kNumParts];  // Active length for each Euclidean part
      static uint8_t fill_[kNumParts];                      // Calculated number of active steps for Euclidean parts, based on density
      static uint8_t step_counter_[kNumParts];              // Generic step counter per part, used for Euclidean perturbation in original code
      static uint8_t part_perturbation_[kNumParts];         // Randomness value applied per part in Drum mode
      static uint8_t euclidean_step_[kNumParts];            // Current step for each Euclidean generator (0 to length-1)

      static uint8_t state_; // Holds the current trigger/accent state for the current tick for all parts + accent.
      static uint8_t step_;  // Current step in the main 32-step sequence (0-31), synonymous with sequence_step_

      // Clock and timing related
      static uint16_t internal_clock_ticks_; // Counts sub-ticks for original Grids clocking mode.
      static uint8_t beat_counter_;          // Counts beats (e.g., quarter notes based on sequence_step_).
      static uint8_t sequence_step_;         // Current step in the sequence (0-31), drives pattern evaluation.
      static bool swing_applied_;            // Tracks if swing has been applied in the current sub-step (more relevant to original complex swing).
      static bool first_beat_;               // True if current step is the first beat of the pattern.
      static bool beat_;                     // True if current step is on a beat (typically quarter note).

    private:
      static void Evaluate();
      static void EvaluateEuclidean();
      static void EvaluateDrums();

      static uint8_t ReadDrumMap(
          uint8_t step,
          uint8_t instrument,
          uint8_t x,
          uint8_t y);

      // State variables
      static uint8_t pulse_;
      static uint16_t pulse_duration_counter_;

      DISALLOW_COPY_AND_ASSIGN(PatternGenerator); // From nt_grids_utils.h
    };

  } // namespace grids
} // namespace nt_grids_port

#endif // NT_GRIDS_PATTERN_GENERATOR_H_