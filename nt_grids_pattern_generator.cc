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

#include "nt_grids_pattern_generator.h"
#include "nt_grids_utils.h"     // For Random, U8Mix etc.
#include "nt_grids_resources.h" // For lut_res_euclidean, DrumMapAccess

#include <algorithm> // For std::min, std::max if needed

namespace nt_grids_port
{
  namespace grids
  {

    // Static member variable definitions
    PatternGeneratorSettings PatternGenerator::settings_[2]; // Index 0 for Euclidean, 1 for Drums
    Options PatternGenerator::options_;
    bool PatternGenerator::chaos_globally_enabled_ = false;

    uint8_t PatternGenerator::pulse_ = 0;
    uint8_t PatternGenerator::step_ = 0;
    bool PatternGenerator::first_beat_ = false;
    bool PatternGenerator::beat_ = false;
    uint8_t PatternGenerator::state_ = 0; // Correct: single byte for current tick's output
    uint16_t PatternGenerator::pulse_duration_counter_ = 0;

    // This was a static member in the original .cc, let's add its definition here.
    uint8_t PatternGenerator::part_perturbation_[kNumParts]; // Already defined like this, ensure it's consistent with .h

    // Tap tempo related static members - will be initialized in Init()
    // uint16_t PatternGenerator::tempo_ = 0;
    // uint16_t PatternGenerator::tap_tempo_sum_ = 0;
    // uint8_t PatternGenerator::tap_tempo_count_ = 0;
    // uint16_t PatternGenerator::tap_tempo_timer_ = 0;
    // bool PatternGenerator::set_tempo_mode_ = false;

    // The following members were part of an earlier incorrect refactor attempt
    // and are no longer used or defined:
    // uint32_t PatternGenerator::s_current_state_for_euclidean_eval_ = 0;
    // bool PatternGenerator::Options::chaos_enabled = false;
    // uint32_t PatternGenerator::s_current_tick_output_state_ = 0;

    uint8_t PatternGenerator::step_counter_[kNumParts];
    uint8_t PatternGenerator::pulse_duration_[kNumParts];
    uint8_t PatternGenerator::current_euclidean_length_[kNumParts];
    uint8_t PatternGenerator::fill_[kNumParts];
    uint8_t PatternGenerator::euclidean_step_[kNumParts];

    uint8_t PatternGenerator::output_buffer_[kStepsPerPattern >> 3];
    uint8_t PatternGenerator::pulse_counter_[kNumParts];
    uint8_t PatternGenerator::beat_counter_ = 0;
    uint8_t PatternGenerator::sequence_step_ = 0;
    uint16_t PatternGenerator::internal_clock_ticks_ = 0;
    bool PatternGenerator::swing_applied_ = false;

    void PatternGenerator::Init()
    {
      std::memset(settings_, 0, sizeof(settings_));
      std::memset(&options_, 0, sizeof(options_));
      std::memset(output_buffer_, 0, sizeof(output_buffer_));
      std::memset(pulse_counter_, 0, sizeof(pulse_counter_));
      std::memset(pulse_duration_, 0, sizeof(pulse_duration_));
      std::memset(current_euclidean_length_, 0, sizeof(current_euclidean_length_));
      std::memset(fill_, 0, sizeof(fill_));
      std::memset(step_counter_, 0, sizeof(step_counter_));
      std::memset(part_perturbation_, 0, sizeof(part_perturbation_));
      std::memset(euclidean_step_, 0, sizeof(euclidean_step_));

      options_.output_mode = OUTPUT_MODE_DRUMS; // Corrected
      state_ = 0;
      beat_counter_ = 0;
      sequence_step_ = 0;
      internal_clock_ticks_ = 0;
      swing_applied_ = false;
      step_ = 0; // Ensure step_ (if different from sequence_step_) is also init
      pulse_ = 0;
      pulse_duration_counter_ = 0;
      first_beat_ = true; // Initialize these as well
      beat_ = true;

      options_.original_grids_clocking = false; // Default: external clock directly drives main steps
      chaos_globally_enabled_ = false;

      options_.clock_resolution = CLOCK_RESOLUTION_24_PPQN; // Default from original Grids firmware options
      options_.output_clock = false;
      options_.gate_mode = false;

      // Default settings for Drum mode
      settings_[OUTPUT_MODE_DRUMS].options.drums.x = 128;        // Center of map
      settings_[OUTPUT_MODE_DRUMS].options.drums.y = 128;        // Center of map
      settings_[OUTPUT_MODE_DRUMS].options.drums.randomness = 0; // No chaos initially
      settings_[OUTPUT_MODE_DRUMS].density[0] = 255;             // Full density BD
      settings_[OUTPUT_MODE_DRUMS].density[1] = 255;             // Full density SD
      settings_[OUTPUT_MODE_DRUMS].density[2] = 255;             // Full density HH

      // Default settings for Euclidean mode
      settings_[OUTPUT_MODE_EUCLIDEAN].options.euclidean.chaos_amount = 0; // No chaos initially
      for (int i = 0; i < kNumParts; ++i)
      {
        current_euclidean_length_[i] = 16;                 // Default length: 16 steps
        fill_[i] = 8;                                      // Default fill: 8 steps (50% for a 16-step length)
        settings_[OUTPUT_MODE_EUCLIDEAN].density[i] = 128; // Density 128/255 maps to ~8 steps for a 16-step length
      }
    }

    void PatternGenerator::Reset()
    {
      step_ = 0;
      pulse_ = 0;
      std::memset(euclidean_step_, 0, sizeof(euclidean_step_));
      std::memset(part_perturbation_, 0, sizeof(part_perturbation_));
      first_beat_ = true;
      beat_ = true;
      state_ = 0;
      pulse_duration_counter_ = 0;
      internal_clock_ticks_ = 0;
      Evaluate(); // Evaluate to set initial trigger states based on reset conditions
    }

    void PatternGenerator::Retrigger()
    {
      // Re-evaluate the current step without advancing time
      Evaluate();
    }

    // Placeholder for swing implementation. Original Grids swing was tied to its internal
    // 24PPQN clock and affected by the randomness parameter in drum mode.
    // A full implementation would require delaying specific pulses based on the clock mode.
    // Currently returns 0, effectively disabling swing from this module.
    // int8_t PatternGenerator::swing_amount()
    // {
    //   if (!options_.swing) // Removed: Swing functionality is being removed
    //     return 0;
    //   // Original Grids swing was subtle and tied to its 24PPQN clock structure.
    //   // This is a simplified placeholder. A proper swing implementation would
    //   // delay every second 16th note (or equivalent based on resolution).
    //   // For now, just return a small fixed offset if swing is on.
    //   // This would need to be applied to timing of specific pulses.
    //   // The Disting NT might have its own swing handling or expect this module to provide timed triggers.
    //   // Let's return 0 for now, as applying swing correctly requires deeper integration with the host clock.
    //   return 0;
    // }  // Removed: Swing functionality is being removed

    // `external_clock_tick = true` signals one tick from the host/external clock source.
    void PatternGenerator::TickClock(bool external_clock_tick)
    {
      if (!external_clock_tick) // Only process if there's an actual external clock tick
      {
        return;
      }

      bool main_step_advanced = false;

      if (options_.original_grids_clocking)
      {
        internal_clock_ticks_++;
        if (internal_clock_ticks_ >= kOriginalGridsPulsesPerStep)
        {
          internal_clock_ticks_ = 0;
          main_step_advanced = true;

          // Before advancing sequence_step_, check if it's an even step for Euclidean advancement
          if (!(sequence_step_ & 1)) // If current sequence_step_ (0-31) is even (original Grids Euclidean steps on 8ths)
          {
            for (uint8_t i = 0; i < kNumParts; ++i)
            {
              if (current_euclidean_length_[i] > 0)
              {
                euclidean_step_[i] = (euclidean_step_[i] + 1) % current_euclidean_length_[i];
              }
            }
          }
          // Now advance the main sequence step
          sequence_step_ = (sequence_step_ + 1) % kStepsPerPattern;
          step_ = sequence_step_;
        }
      }
      else // Direct clocking: each external tick advances main sequence and all Euclidean parts
      {
        main_step_advanced = true;
        sequence_step_ = (sequence_step_ + 1) % kStepsPerPattern;
        step_ = sequence_step_;

        for (uint8_t i = 0; i < kNumParts; ++i)
        {
          if (current_euclidean_length_[i] > 0)
          {
            euclidean_step_[i] = (euclidean_step_[i] + 1) % current_euclidean_length_[i];
          }
        }
      }

      if (main_step_advanced)
      {
        first_beat_ = (sequence_step_ == 0);
        uint8_t steps_per_beat = kStepsPerPattern / 4;
        if (steps_per_beat == 0)
          steps_per_beat = 1; // Avoid division by zero for short patterns
        beat_ = (sequence_step_ % steps_per_beat) == 0;

        Evaluate(); // Evaluate patterns based on the new step
      }

      IncrementPulseCounter(); // Handle pulse durations on every external tick, regardless of main step advancement
    }

    void PatternGenerator::IncrementPulseCounter()
    {
      ++pulse_duration_counter_;
      if (pulse_duration_counter_ >= kPulseDuration && !options_.gate_mode)
      {
        state_ = 0; // Triggers off after kPulseDuration if not in gate mode
      }
    }

    // The original tap tempo functionality and related button handling (StartSetTempo, etc.)
    // have been removed as this port relies on external clocking provided by the Disting NT host.
    // void PatternGenerator::ClockFallingEdge() { ... }
    // void PatternGenerator::RefreshTapTempo() { ... }
    // void PatternGenerator::StartSetTempo() { ... }
    // void PatternGenerator::StopSetTempo() { ... }
    // void PatternGenerator::HandleSetTempoButton() { ... }

    // Reads the drum map, interpolating between 4 points in the map.
    // x and y are 0-255 coordinates for map interpolation.
    uint8_t PatternGenerator::ReadDrumMap(
        uint8_t step,
        uint8_t instrument,
        uint8_t x, // 0-255, derived from UI Map X parameter
        uint8_t y  // 0-255, derived from UI Map Y parameter
    )
    {
      uint8_t i = x >> 6; // Determines a 2x2 cell in the 5x5 map based on X (quantized to 0-3 for cell index)
      uint8_t j = y >> 6; // Determines a 2x2 cell in the 5x5 map based on Y (quantized to 0-3 for cell index)

      // Ensure i and j are within bounds for a 5x5 map access (max index 3 for base of 2x2 cell)
      // DrumMapAccess::drum_map_ptr is 5x5; accessing [i+1] or [j+1] requires i,j <= 3.
      i = std::min(i, static_cast<uint8_t>(3));
      j = std::min(j, static_cast<uint8_t>(3));

      const uint8_t *a_map = DrumMapAccess::drum_map_ptr[j][i];         // Top-left node in the interpolation cell
      const uint8_t *b_map = DrumMapAccess::drum_map_ptr[j][i + 1];     // Top-right node
      const uint8_t *c_map = DrumMapAccess::drum_map_ptr[j + 1][i];     // Bottom-left node
      const uint8_t *d_map = DrumMapAccess::drum_map_ptr[j + 1][i + 1]; // Bottom-right node

      uint8_t offset = (instrument * kStepsPerPattern) + step; // kStepsPerPattern is typically 32

      uint8_t a = a_map[offset];
      uint8_t b = b_map[offset];
      uint8_t c = c_map[offset];
      uint8_t d = d_map[offset];

      // Interpolation weights are x % 64 and y % 64, scaled to 0-255 (by << 2)
      uint8_t x_weight = (x % 64) << 2;
      uint8_t y_weight = (y % 64) << 2;

      return nt_grids_port::U8Mix(nt_grids_port::U8Mix(a, b, x_weight), nt_grids_port::U8Mix(c, d, x_weight), y_weight);
    }

    void PatternGenerator::EvaluateDrums()
    {
      if (step_ == 0 && pulse_ == 0)
      { // Generate perturbation only once at the very start of the 32-step sequence (when pulse_ is also 0)
        uint8_t randomness = settings_[OUTPUT_MODE_DRUMS].options.drums.randomness;
        randomness >>= 2; // Scale randomness for perturbation amount

        for (uint8_t i = 0; i < kNumParts; ++i)
        {
          part_perturbation_[i] = nt_grids_port::U8U8MulShift8(nt_grids_port::Random::GetByte(), randomness);
        }
      }

      uint8_t current_step_in_pattern = step_;
      uint8_t new_state_for_tick = 0; // Accumulates trigger and accent bits for the current tick

      uint8_t x = settings_[OUTPUT_MODE_DRUMS].options.drums.x;
      uint8_t y = settings_[OUTPUT_MODE_DRUMS].options.drums.y;
      uint8_t density_thresholds[kNumParts];
      for (int i = 0; i < kNumParts; ++i)
        density_thresholds[i] = ~settings_[OUTPUT_MODE_DRUMS].density[i];

      uint8_t accent_bits_for_parts = 0; // Tracks which of the 3 parts have an accent

      for (uint8_t i = 0; i < kNumParts; ++i)
      {
        uint8_t level = ReadDrumMap(current_step_in_pattern, i, x, y);
        if (level < 255 - part_perturbation_[i])
        {
          level += part_perturbation_[i];
        }
        else
        {
          level = 255;
        }

        if (level > density_thresholds[i])
        {
          if (level > 192) // Threshold for accent
          {
            accent_bits_for_parts |= (1 << i); // Mark part 'i' (0,1,2) as having an accent
          }
          new_state_for_tick |= (1 << i); // Set trigger bit for part 'i' (maps to OUTPUT_BIT_TRIG_1/2/3)
        }
      }

      // Handle the ACCENT output bit (bit 3 / OUTPUT_BIT_ACCENT)
      // In this port, accent is triggered if any part has an accent.
      // Original Grids had more complex accent/common bit logic related to output_clock option.
      if (accent_bits_for_parts != 0)
      { // If any part has an accent
        new_state_for_tick |= OUTPUT_BIT_ACCENT;
      }

      // The original Grids' options_.output_clock logic for setting OUTPUT_BIT_RESET
      // and modifying accent_bits based on clock/bar information has been removed
      // to simplify for the plugin context. This port focuses on core triggers + one combined accent.
      state_ = new_state_for_tick; // Update the main trigger/accent state for the current tick
    }

    void PatternGenerator::EvaluateEuclidean()
    {
      state_ = 0; // Clear previous state
      for (uint8_t i = 0; i < kNumParts; ++i)
      {
        uint8_t length = current_euclidean_length_[i];
        // uint8_t fill = fill_[i]; // fill_[i] (calculated active steps) is not directly used by the LUT approach;
        // the LUT uses scaled density directly.

        if (length == 0)
          continue;

        uint8_t density_param_val = settings_[OUTPUT_MODE_EUCLIDEAN].density[i]; // 0-255 from UI
        uint8_t density_for_lut = density_param_val >> 3;                        // Scale to 0-31 for LUT index
        if (density_for_lut > 31)
          density_for_lut = 31; // Clamp to max LUT index for density

        uint16_t address = (length - 1) * 32 + density_for_lut;
        if (address >= nt_grids_port::LUT_RES_EUCLIDEAN_SIZE) // Safety check for LUT bounds
        {
          address = 0; // Should not happen with valid length/density
        }

        uint32_t pattern_bits = nt_grids_port::lut_res_euclidean[address];
        uint32_t current_step_in_part = euclidean_step_[i]; // Current step for this part (0 to length-1)

        if ((pattern_bits >> current_step_in_part) & 1)
        {
          state_ |= (1 << i); // Set trigger for part i
        }

        // Chaos perturbation for Euclidean mode - simplified from original Grids.
        // May need refinement for more nuanced behavior.
        if (chaos_globally_enabled_ && settings_[OUTPUT_MODE_EUCLIDEAN].options.euclidean.chaos_amount > 0)
        {
          if ((Random::GetWord() % 256) < settings_[OUTPUT_MODE_EUCLIDEAN].options.euclidean.chaos_amount)
          {
            // Randomly flip the state of this part for chaos effect
            if ((Random::GetWord() % 8) == 0) // Lower probability of flip for subtlety
            {
              state_ ^= (1 << i);
            }
            // Introduce random accents if chaos amount is high
            if (settings_[OUTPUT_MODE_EUCLIDEAN].options.euclidean.chaos_amount > 192 && (Random::GetWord() % 16) == 0)
            {
              state_ |= OUTPUT_BIT_ACCENT;
            }
          }
        }
      }
    }

    void PatternGenerator::Evaluate()
    {
      pulse_duration_counter_ = 0; // Reset pulse timer for new evaluation cycle

      // The general random bit (0x80) from original Grids' Evaluate() that was ORed into state_
      // is not explicitly added here. Accent generation is now handled within EvaluateDrums/Euclidean.
      // A generic random trigger output could be added here if desired as a separate feature.

      if (options_.output_mode == OUTPUT_MODE_DRUMS)
      {
        EvaluateDrums();
      }
      else
      {
        EvaluateEuclidean();
      }
    }

    void PatternGenerator::SetLength(uint8_t channel, uint8_t length)
    {
      if (channel >= kNumParts)
        return;
      // Clamp length to 1-32. Disting parameters might send 0, which is invalid for length.
      if (length == 0)
        length = 1;
      if (length > 32)
        length = 32;
      current_euclidean_length_[channel] = length;
    }

    void PatternGenerator::SetFill(uint8_t channel, uint8_t fill_param_value) // fill_param_value is 0-255 from UI
    {
      if (channel >= kNumParts)
        return;
      // Store the raw 0-255 parameter value in settings.density for Euclidean mode;
      // this value is then scaled for LUT lookup in EvaluateEuclidean.
      settings_[OUTPUT_MODE_EUCLIDEAN].density[channel] = fill_param_value;

      uint8_t current_length = current_euclidean_length_[channel];
      if (current_length == 0) // Should be caught by SetLength clamping, but defensive.
      {
        fill_[channel] = 0;
        return;
      }
      // Scale fill_param_value (0-255) to the number of active steps (0 to current_length).
      // This fill_[channel] is mostly for potential display or alternative logic, not directly for LUT.
      uint8_t active_steps = (static_cast<uint16_t>(fill_param_value) * current_length + 127) / 255; // Add 127 for rounding

      if (active_steps > current_length)
        active_steps = current_length; // Clamp active_steps to current_length
      if (fill_param_value > 0 && active_steps == 0 && current_length > 0)
        active_steps = 1; // Ensure at least 1 step if fill > 0
      if (fill_param_value == 255)
        active_steps = current_length; // Max fill means all steps active

      fill_[channel] = active_steps;
    }

    void PatternGenerator::set_original_grids_clocking(bool enabled)
    {
      options_.original_grids_clocking = enabled;
      internal_clock_ticks_ = 0; // Reset internal tick counter on mode change for a clean start
    }

  } // namespace grids
} // namespace nt_grids_port