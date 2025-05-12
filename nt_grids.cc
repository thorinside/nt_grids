// //////////////////////////////////////////////////////////////
//
// nt_grids.cc: Ported by Neal Sanche
// based on the original Grids by Emilie Gillet.
// Copyright (C) 2012 Emilie Gillet.
// Copyright (C) 2024 Neal Sanchez
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

#include "distingnt/api.h"
#include <cstring>
#include <new>

#include "nt_grids_pattern_generator.h"
#include "nt_grids_resources.h"

// Forward declaration if needed, though PatternGenerator should be fully defined by its header
// namespace nt_grids_port { namespace grids { class PatternGenerator; } }

// Define a unique GUID for this algorithm
#define NT_GRIDS_GUID NT_MULTICHAR('N', 'T', 'G', 'R')

// No anonymous namespace, all file-scope symbols below will be static or const static.

// --- ParameterIndex Enum Definition (Moved before NtGridsAlgorithm) ---
enum ParameterIndex // File scope, effectively internal to this CU
{
  // Mode & Global
  kParamMode,
  kParamSwingEnable,
  kParamChaosEnable,
  kParamChaosAmount,

  // CV Inputs
  kParamClockInput,
  kParamResetInput,

  // Drum Mode Specific
  kParamDrumMapX,
  kParamDrumMapY,
  kParamDrumDensity1,
  kParamDrumDensity2,
  kParamDrumDensity3,

  // Euclidean Mode Specific
  kParamEuclideanLength1,
  kParamEuclideanLength2,
  kParamEuclideanLength3,
  kParamEuclideanFill1,
  kParamEuclideanFill2,
  kParamEuclideanFill3,

  // Outputs (Order matters: value, then mode, for each output)
  kParamOutputTrig1,
  kParamOutputTrig1Mode,
  kParamOutputTrig2,
  kParamOutputTrig2Mode,
  kParamOutputTrig3,
  kParamOutputTrig3Mode,
  kParamOutputAccent,
  kParamOutputAccentMode,

  kNumParameters // This should now correctly reflect the total number of parameters
};

// --- NtGridsAlgorithm Struct Definition ---
struct NtGridsAlgorithm : _NT_algorithm // Inherit from _NT_algorithm
{
  // REMOVED: const _NT_parameter *parameters; - Inherited
  // REMOVED: const _NT_parameterPages *parameterPages; - Inherited
  // REMOVED: int32_t v[64]; - Inherited as const int16_t* v;

  // Pointers to inputs/outputs (populated by host) - These are specific to our algorithm's direct needs
  const float *clock_in; // This seems unused if we read directly from busFrames
  const float *reset_in; // This also seems unused

  // Previous CV input states for edge detection (block-level)
  float prev_clock_cv_val;
  float prev_reset_cv_val;

  // State for fixed-duration triggers
  uint32_t trigger_on_samples_remaining[4]; // Index 0:Trig1, 1:Trig2, 2:Trig3, 3:Accent

  // For UI debugging
  bool debug_recent_clock_tick;
  bool debug_recent_reset;
  bool debug_param_changed_flags[kNumParameters]; // One flag per parameter

  // More detailed clock debugging
  float debug_current_clock_cv_val;
  float debug_prev_clock_cv_val_for_debug;

  // You can add any other state your algorithm needs here.
};

// --- Parameter Definitions (s_parameters array, etc.) ---
static const char *kEnumModeStrings[] = {"Euclidean", "Drums", NULL};
static const char *kEnumBooleanStrings[] = {"Off", "On", NULL};

static const _NT_parameter s_parameters[] = {
    {.name = "Mode", .min = 0, .max = 1, .def = 1, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kEnumModeStrings},
    {.name = "Swing", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kEnumBooleanStrings},
    {.name = "Chaos", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kEnumBooleanStrings},
    {.name = "Chaos Amount", .min = 0, .max = 255, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    NT_PARAMETER_CV_INPUT("Clock In", 0, 0)
        NT_PARAMETER_CV_INPUT("Reset In", 0, 0){.name = "Map X", .min = 0, .max = 255, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Map Y", .min = 0, .max = 255, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Density 1", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Density 2", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Density 3", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Length 1", .min = 1, .max = 32, .def = 16, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Length 2", .min = 1, .max = 32, .def = 16, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Length 3", .min = 1, .max = 32, .def = 16, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Fill 1", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Fill 2", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Fill 3", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Trig 1 Out", 0, 13)
        NT_PARAMETER_CV_OUTPUT_WITH_MODE("Trig 2 Out", 0, 14)
            NT_PARAMETER_CV_OUTPUT_WITH_MODE("Trig 3 Out", 0, 15)
                NT_PARAMETER_CV_OUTPUT_WITH_MODE("Accent Out", 0, 16)};

// --- Parameter Pages ---
static const uint8_t s_page_main[] = {
    kParamMode,
    kParamSwingEnable,
    kParamChaosEnable,
    kParamChaosAmount,
    // kParamClockInput, // Removed, moved to Routing page
    // kParamResetInput, // Removed, moved to Routing page
};
static const uint8_t s_page_drum[] = {
    kParamDrumMapX, kParamDrumMapY, kParamChaosAmount,
    kParamDrumDensity1, kParamDrumDensity2, kParamDrumDensity3};
static const uint8_t s_page_euclidean[] = {
    kParamEuclideanLength1, kParamEuclideanFill1,
    kParamEuclideanLength2, kParamEuclideanFill2,
    kParamEuclideanLength3, kParamEuclideanFill3};
static const uint8_t s_page_routing[] = {
    kParamClockInput,
    kParamResetInput,
    kParamOutputTrig1, kParamOutputTrig1Mode,
    kParamOutputTrig2, kParamOutputTrig2Mode,
    kParamOutputTrig3, kParamOutputTrig3Mode,
    kParamOutputAccent, kParamOutputAccentMode};

static const _NT_parameterPage s_pages[] = {
    {.name = "Main", .numParams = ARRAY_SIZE(s_page_main), .params = s_page_main},
    {.name = "Drum Params", .numParams = ARRAY_SIZE(s_page_drum), .params = s_page_drum},
    {.name = "Euclid Params", .numParams = ARRAY_SIZE(s_page_euclidean), .params = s_page_euclidean},
    {.name = "Routing", .numParams = ARRAY_SIZE(s_page_routing), .params = s_page_routing},
};

_NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(s_pages),
    .pages = s_pages,
};

// --- Helper: Update Grids PatternGenerator from parameters ---
// This function must be static as it's a file-scope helper
static void update_grids_from_params(const int16_t *param_values) // Changed to const int16_t*
{
  using namespace nt_grids_port::grids;

  OutputMode previous_mode = PatternGenerator::current_output_mode();
  OutputMode new_mode = (OutputMode)param_values[kParamMode]; // param_values are int16_t

  if (new_mode != previous_mode)
  {
    PatternGenerator::set_output_mode(new_mode);
  }
  OutputMode current_mode = PatternGenerator::current_output_mode();

  if (current_mode == OUTPUT_MODE_DRUMS)
  {
    // Values from param_values are int16_t, PatternGenerator expects uint8_t or similar
    PatternGenerator::settings_[current_mode].options.drums.x = (uint8_t)param_values[kParamDrumMapX];
    PatternGenerator::settings_[current_mode].options.drums.y = (uint8_t)param_values[kParamDrumMapY];
    PatternGenerator::settings_[current_mode].density[0] = (uint8_t)param_values[kParamDrumDensity1];
    PatternGenerator::settings_[current_mode].density[1] = (uint8_t)param_values[kParamDrumDensity2];
    PatternGenerator::settings_[current_mode].density[2] = (uint8_t)param_values[kParamDrumDensity3];
  }
  else // OUTPUT_MODE_EUCLIDEAN
  {
    for (int i = 0; i < nt_grids_port::kNumParts; ++i)
    {
      uint8_t length = (uint8_t)param_values[kParamEuclideanLength1 + i];
      PatternGenerator::SetLength(i, length);

      uint8_t fill_density_param_val = (uint8_t)param_values[kParamEuclideanFill1 + i];
      PatternGenerator::SetFill(i, fill_density_param_val);
    }
  }

  PatternGenerator::set_swing(param_values[kParamSwingEnable] != 0);
  PatternGenerator::set_global_chaos(param_values[kParamChaosEnable] != 0);

  if (PatternGenerator::chaos_globally_enabled_)
  {
    uint8_t chaos_val = (uint8_t)param_values[kParamChaosAmount];
    PatternGenerator::settings_[OUTPUT_MODE_DRUMS].options.drums.randomness = chaos_val;
    PatternGenerator::settings_[OUTPUT_MODE_EUCLIDEAN].options.euclidean.chaos_amount = chaos_val;
  }
  else
  {
    PatternGenerator::settings_[OUTPUT_MODE_DRUMS].options.drums.randomness = 0;
    PatternGenerator::settings_[OUTPUT_MODE_EUCLIDEAN].options.euclidean.chaos_amount = 0;
  }
}

// --- Instance Construction/Destruction Callbacks (Original Signatures) ---
static void nt_grids_calculate_static_requirements(_NT_staticRequirements &req) // Original Signature
{
  req.dram = 0;
}

static void nt_grids_initialise(_NT_staticMemoryPtrs &ptrs, const _NT_staticRequirements &req) // Original Signature
{
  nt_grids_port::Random::Init();
  nt_grids_port::grids::PatternGenerator::Init();
}

// Original Signature for calculateRequirements
static void nt_grids_calculate_requirements(_NT_algorithmRequirements &req, const int32_t *specifications)
{
  req.numParameters = kNumParameters; // MODIFIED: Was kParamCount, corrected to kNumParameters from enum
  req.sram = sizeof(NtGridsAlgorithm);
  req.dram = 0;
  req.dtc = 0;
  req.itc = 0;
}

// Original Signature for construct
static _NT_algorithm *nt_grids_construct(const _NT_algorithmMemoryPtrs &ptrs, const _NT_algorithmRequirements &req, const int32_t *specifications)
{
  NtGridsAlgorithm *alg = new (ptrs.sram) NtGridsAlgorithm();
  // Initialize inherited members:
  alg->parameters = s_parameters;
  alg->parameterPages = &parameterPages;

  // The host will initialize alg->v (the inherited const int16_t*).
  // If specifications are provided (e.g., from a preset), the host should use them
  // to populate alg->v. If specifications is NULL, host should use defaults.
  // For safety or if direct initialization of PatternGenerator from defaults is desired
  // before host fully initializes alg->v, one might call update_grids_from_params
  // with s_parameters[i].def values. However, typical flow is to rely on host
  // populating alg->v and then calling parameterChanged if needed, or just using alg->v.

  // Initialize algorithm-specific state:
  alg->prev_clock_cv_val = 0.0f;
  alg->prev_reset_cv_val = 0.0f;

  for (int i = 0; i < 4; ++i)
  {
    alg->trigger_on_samples_remaining[i] = 0;
  }

  alg->debug_recent_clock_tick = false;
  alg->debug_recent_reset = false;
  for (int i = 0; i < kNumParameters; ++i)
  {
    alg->debug_param_changed_flags[i] = false; // These flags still make sense for UI debug
  }

  alg->debug_current_clock_cv_val = -1.0f;
  alg->debug_prev_clock_cv_val_for_debug = -1.0f;

  // Update PatternGenerator based on initial parameters (now from inherited alg->v, set by host)
  // This assumes alg->v is populated by the host with either defaults or preset values by this point.
  update_grids_from_params(alg->v); // alg->v is now const int16_t* from _NT_algorithm
  nt_grids_port::grids::PatternGenerator::Reset();
  return reinterpret_cast<_NT_algorithm *>(alg); // This cast is fine as NtGridsAlgorithm IS-A _NT_algorithm
}

// Original Signature for parameterChanged
static void nt_grids_parameter_changed(_NT_algorithm *self_base, int p_idx)
{
  // self_base is already _NT_algorithm*. If NtGridsAlgorithm specific members need access, cast:
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base); // Changed to static_cast due to inheritance

  if (p_idx >= 0 && p_idx < kNumParameters) // kNumParameters should match the count for self_base->v
  {
    // No longer need to copy to a local self->v. self_base->v *is* the source of truth.
    // self->v (inherited) is self_base->v.
    self->debug_param_changed_flags[p_idx] = true; // Still useful for UI debug
  }
  update_grids_from_params(self_base->v); // Pass the host's parameter array directly
}

// Original Signature for step, but with new internal logic
static void nt_grids_step(_NT_algorithm *self_base, float *busFrames, int numFramesBy4)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base); // Use static_cast
  using nt_grids_port::grids::PatternGenerator;
  int num_frames_total = numFramesBy4 * 4; // Total samples in the block

  // CV Input Handling: Iterate through each sample in the block for edge detection
  const float cv_threshold = 0.5f;                         // Reverted to 0.5f to resolve compilation issue
  int clock_bus_idx_param_val = self->v[kParamClockInput]; // self->v is inherited const int16_t*
  int reset_bus_idx_param_val = self->v[kParamResetInput]; // self->v is inherited const int16_t*

  for (int s_cv = 0; s_cv < num_frames_total; ++s_cv)
  {
    // Clock Input Detection
    if (clock_bus_idx_param_val > 0) // Is a bus selected? (1-based)
    {
      int clock_bus_array_idx = clock_bus_idx_param_val - 1; // Convert to 0-based for array access
      if (clock_bus_array_idx < 28)                          // Check if bus index is valid (max 28 buses)
      {
        float current_sample_clock_cv = busFrames[clock_bus_array_idx * num_frames_total + s_cv];

        // Capture values for debugging for the current sample
        self->debug_current_clock_cv_val = current_sample_clock_cv;
        self->debug_prev_clock_cv_val_for_debug = self->prev_clock_cv_val; // This is the value from the *previous sample* processing

        if (current_sample_clock_cv > cv_threshold && self->prev_clock_cv_val <= cv_threshold)
        {
          PatternGenerator::TickClock(true);
          self->debug_recent_clock_tick = true; // Set debug flag
        }
        self->prev_clock_cv_val = current_sample_clock_cv;
      }
      else
      {
        self->prev_clock_cv_val = 0.0f; // Invalid bus, treat as no signal
      }
    }
    else
    {
      self->prev_clock_cv_val = 0.0f; // No bus selected, ensure previous state is low
    }

    // Reset Input Detection
    if (reset_bus_idx_param_val > 0) // Is a bus selected? (1-based)
    {
      int reset_bus_array_idx = reset_bus_idx_param_val - 1; // Convert to 0-based
      if (reset_bus_array_idx < 28)                          // Check if bus index is valid (max 28 buses)
      {
        float current_sample_reset_cv = busFrames[reset_bus_array_idx * num_frames_total + s_cv];
        if (current_sample_reset_cv > cv_threshold && self->prev_reset_cv_val <= cv_threshold)
        {
          PatternGenerator::Reset();
          for (int i = 0; i < 4; ++i)
            self->trigger_on_samples_remaining[i] = 0;
          self->debug_recent_reset = true; // Set debug flag
        }
        self->prev_reset_cv_val = current_sample_reset_cv;
      }
      else
      {
        self->prev_reset_cv_val = 0.0f; // Invalid bus, treat as no signal
      }
    }
    else
    {
      self->prev_reset_cv_val = 0.0f; // No bus selected, ensure previous state is low
    }
  } // End of CV input processing loop

  // Get current trigger state from PatternGenerator (AFTER all potential ticks/resets in this block)
  uint8_t current_pattern_state = PatternGenerator::state_;

  // --- Initiate trigger durations based on new events from PatternGenerator ---
  const float desired_trigger_duration_seconds = 0.005f; // Shortened to 5ms
  const uint32_t desired_trigger_samples = static_cast<uint32_t>(desired_trigger_duration_seconds * NT_globals.sampleRate);

  // Only re-evaluate starting triggers if a clock tick happened or if a reset just occurred (state might be initial).
  // More robustly, check current_pattern_state against ongoing triggers.
  // If a bit is set in current_pattern_state and the corresponding trigger is not already active, start it.

  // Trig 1 (output bit 0)
  if ((current_pattern_state & (1 << 0)) && self->trigger_on_samples_remaining[0] == 0)
  {
    self->trigger_on_samples_remaining[0] = desired_trigger_samples;
  }
  // Trig 2 (output bit 1)
  if ((current_pattern_state & (1 << 1)) && self->trigger_on_samples_remaining[1] == 0)
  {
    self->trigger_on_samples_remaining[1] = desired_trigger_samples;
  }
  // Trig 3 (output bit 2)
  if ((current_pattern_state & (1 << 2)) && self->trigger_on_samples_remaining[2] == 0)
  {
    self->trigger_on_samples_remaining[2] = desired_trigger_samples;
  }
  // Accent (output bit nt_grids_port::grids::OUTPUT_BIT_ACCENT, which is 1 << 7)
  // Store in trigger_on_samples_remaining[3]
  if ((current_pattern_state & nt_grids_port::grids::OUTPUT_BIT_ACCENT) && self->trigger_on_samples_remaining[3] == 0)
  {
    self->trigger_on_samples_remaining[3] = desired_trigger_samples;
  }

  // Main processing loop for each sample in the block
  // Writing outputs to busFrames based on parameter routing
  const float trigger_on_voltage = 1.0f;
  const float trigger_off_voltage = 0.0f;

  for (int s = 0; s < num_frames_total; ++s)
  {
    // Trig 1 Output
    int bus_idx_trig1 = self->v[kParamOutputTrig1] - 1; // self->v is inherited const int16_t*
    if (bus_idx_trig1 >= 0 && bus_idx_trig1 < 28)
    {
      bool replace_mode_trig1 = self->v[kParamOutputTrig1Mode]; // self->v is inherited const int16_t*
      float value_to_write_trig1;
      if (self->trigger_on_samples_remaining[0] > 0)
      {
        value_to_write_trig1 = trigger_on_voltage;
        self->trigger_on_samples_remaining[0]--;
      }
      else
      {
        value_to_write_trig1 = trigger_off_voltage;
      }

      if (replace_mode_trig1)
      {
        busFrames[bus_idx_trig1 * num_frames_total + s] = value_to_write_trig1;
      }
      else
      {
        busFrames[bus_idx_trig1 * num_frames_total + s] += value_to_write_trig1;
      }
    }

    // Trig 2 Output
    int bus_idx_trig2 = self->v[kParamOutputTrig2] - 1; // self->v is inherited const int16_t*
    if (bus_idx_trig2 >= 0 && bus_idx_trig2 < 28)
    {
      bool replace_mode_trig2 = self->v[kParamOutputTrig2Mode]; // self->v is inherited const int16_t*
      float value_to_write_trig2;
      if (self->trigger_on_samples_remaining[1] > 0)
      {
        value_to_write_trig2 = trigger_on_voltage;
        self->trigger_on_samples_remaining[1]--;
      }
      else
      {
        value_to_write_trig2 = trigger_off_voltage;
      }

      if (replace_mode_trig2)
      {
        busFrames[bus_idx_trig2 * num_frames_total + s] = value_to_write_trig2;
      }
      else
      {
        busFrames[bus_idx_trig2 * num_frames_total + s] += value_to_write_trig2;
      }
    }

    // Trig 3 Output
    int bus_idx_trig3 = self->v[kParamOutputTrig3] - 1; // self->v is inherited const int16_t*
    if (bus_idx_trig3 >= 0 && bus_idx_trig3 < 28)
    {
      bool replace_mode_trig3 = self->v[kParamOutputTrig3Mode]; // self->v is inherited const int16_t*
      float value_to_write_trig3;
      if (self->trigger_on_samples_remaining[2] > 0)
      {
        value_to_write_trig3 = trigger_on_voltage;
        self->trigger_on_samples_remaining[2]--;
      }
      else
      {
        value_to_write_trig3 = trigger_off_voltage;
      }

      if (replace_mode_trig3)
      {
        busFrames[bus_idx_trig3 * num_frames_total + s] = value_to_write_trig3;
      }
      else
      {
        busFrames[bus_idx_trig3 * num_frames_total + s] += value_to_write_trig3;
      }
    }

    // Accent Output
    int bus_idx_accent = self->v[kParamOutputAccent] - 1; // self->v is inherited const int16_t*
    if (bus_idx_accent >= 0 && bus_idx_accent < 28)
    {
      bool replace_mode_accent = self->v[kParamOutputAccentMode]; // self->v is inherited const int16_t*
      float value_to_write_accent;
      if (self->trigger_on_samples_remaining[3] > 0)
      {
        value_to_write_accent = trigger_on_voltage;
        self->trigger_on_samples_remaining[3]--;
      }
      else
      {
        value_to_write_accent = trigger_off_voltage;
      }

      if (replace_mode_accent)
      {
        busFrames[bus_idx_accent * num_frames_total + s] = value_to_write_accent;
      }
      else
      {
        busFrames[bus_idx_accent * num_frames_total + s] += value_to_write_accent;
      }
    }
  }
}

// --- Custom UI Callback Implementations (all static as per example) ---

static bool nt_grids_has_custom_ui(_NT_algorithm *self_base)
{
  return true;
}

static void nt_grids_setup_ui(_NT_algorithm *self_base, _NT_float3 &pots)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base); // Use static_cast
  bool is_drum_mode = (self->v[kParamMode] == 1);                      // self->v is inherited const int16_t*
  if (is_drum_mode)
  {
    pots[0] = (self->v[kParamDrumDensity1] / 255.0f); // self->v is inherited const int16_t*
    pots[1] = (self->v[kParamDrumDensity2] / 255.0f); // self->v is inherited const int16_t*
    pots[2] = (self->v[kParamDrumDensity3] / 255.0f); // self->v is inherited const int16_t*
  }
  else // Euclidean Mode
  {
    pots[0] = (self->v[kParamEuclideanFill1] / 255.0f); // self->v is inherited const int16_t*
    pots[1] = (self->v[kParamEuclideanFill2] / 255.0f); // self->v is inherited const int16_t*
    pots[2] = (self->v[kParamEuclideanFill3] / 255.0f); // self->v is inherited const int16_t*
  }
}

static void nt_grids_custom_ui(_NT_algorithm *self_base, const _NT_uiData &data)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base); // Use static_cast
  bool is_drum_mode = (self->v[kParamMode] == 1);                      // self->v is inherited const int16_t*
  uint32_t alg_idx = NT_algorithmIndex(self_base);
  uint32_t param_offset = NT_parameterOffset();

  if ((data.buttons & kNT_encoderButtonR) && !(data.lastButtons & kNT_encoderButtonR))
  {
    int32_t current_mode_val = self->v[kParamMode]; // self->v is inherited const int16_t*
    int32_t new_mode_val = 1 - current_mode_val;
    NT_setParameterFromUi(alg_idx, kParamMode + param_offset, new_mode_val);
    return;
  }

  const float MAX_PARAM_VAL_PERCENT = 255.0f;
  if (data.potChange & kNT_potL)
  {
    int32_t val = static_cast<int32_t>((data.pots[0] * MAX_PARAM_VAL_PERCENT) + 0.5f);
    NT_setParameterFromUi(alg_idx, (is_drum_mode ? kParamDrumDensity1 : kParamEuclideanFill1) + param_offset, val);
  }
  if (data.potChange & kNT_potC)
  {
    int32_t val = static_cast<int32_t>((data.pots[1] * MAX_PARAM_VAL_PERCENT) + 0.5f);
    NT_setParameterFromUi(alg_idx, (is_drum_mode ? kParamDrumDensity2 : kParamEuclideanFill2) + param_offset, val);
  }
  if (data.potChange & kNT_potR)
  {
    int32_t val = static_cast<int32_t>((data.pots[2] * MAX_PARAM_VAL_PERCENT) + 0.5f);
    NT_setParameterFromUi(alg_idx, (is_drum_mode ? kParamDrumDensity3 : kParamEuclideanFill3) + param_offset, val);
  }

  if (data.encoders[0] != 0) // Encoder L
  {
    int32_t current_val, new_val, min_val, max_val;
    ParameterIndex param_idx_to_change;
    if (is_drum_mode)
    {
      param_idx_to_change = kParamDrumMapX;
      current_val = self->v[param_idx_to_change]; // self->v is inherited const int16_t*
      min_val = s_parameters[param_idx_to_change].min;
      max_val = s_parameters[param_idx_to_change].max;
      new_val = current_val + data.encoders[0];
    }
    else
    {
      param_idx_to_change = kParamChaosAmount;
      current_val = self->v[param_idx_to_change]; // self->v is inherited const int16_t*
      min_val = s_parameters[param_idx_to_change].min;
      max_val = s_parameters[param_idx_to_change].max;
      new_val = current_val + data.encoders[0] * 5;
    }
    if (new_val < min_val)
      new_val = min_val;
    if (new_val > max_val)
      new_val = max_val;
    NT_setParameterFromUi(alg_idx, param_idx_to_change + param_offset, new_val);
  }

  if (data.encoders[1] != 0) // Encoder R
  {
    int32_t current_val, new_val, min_val, max_val;
    ParameterIndex param_idx_to_change;
    if (is_drum_mode)
    {
      param_idx_to_change = kParamDrumMapY;
      current_val = self->v[param_idx_to_change]; // self->v is inherited const int16_t*
      min_val = s_parameters[param_idx_to_change].min;
      max_val = s_parameters[param_idx_to_change].max;
      new_val = current_val + data.encoders[1];
    }
    else
    {
      param_idx_to_change = kParamEuclideanLength1;
      current_val = self->v[param_idx_to_change]; // self->v is inherited const int16_t*
      min_val = s_parameters[param_idx_to_change].min;
      max_val = s_parameters[param_idx_to_change].max;
      new_val = current_val + data.encoders[1];
    }
    if (new_val < min_val)
      new_val = min_val;
    if (new_val > max_val)
      new_val = max_val;
    NT_setParameterFromUi(alg_idx, param_idx_to_change + param_offset, new_val);
  }
}

static bool nt_grids_draw(_NT_algorithm *self_base)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base); // Use static_cast
  bool is_drum_mode = (self->v[kParamMode] == 1);                      // self->v is inherited const int16_t*
  // char buffer[32]; // Not needed if we are not drawing parameter values
  _NT_textSize textSize = kNT_textNormal;
  int current_y = 24;         // Adjusted from 2 to 2 + 22
  const int line_spacing = 9; // Reduced line spacing slightly
  char debug_buf[32];         // Buffer for string conversion

  // --- Debug Info Only ---
  NT_drawText(5, current_y, self->debug_recent_clock_tick ? "CLK!" : "Clk?", 15, kNT_textLeft, textSize);
  self->debug_recent_clock_tick = false; // Clear after drawing

  NT_drawText(45, current_y, self->debug_recent_reset ? "RST!" : "Rst?", 15, kNT_textLeft, textSize);
  self->debug_recent_reset = false; // Clear after drawing
  current_y += line_spacing;

  // Display Clock Input Bus
  NT_intToString(debug_buf, self->v[kParamClockInput]); // self->v is inherited const int16_t*
  NT_drawText(5, current_y, "ClkBus:", 15, kNT_textLeft, textSize);
  NT_drawText(55, current_y, debug_buf, 15, kNT_textLeft, textSize);
  current_y += line_spacing;

  // Display Current Clock CV
  NT_floatToString(debug_buf, self->debug_current_clock_cv_val, 2);
  NT_drawText(5, current_y, "CurCV:", 15, kNT_textLeft, textSize);
  NT_drawText(55, current_y, debug_buf, 15, kNT_textLeft, textSize);
  current_y += line_spacing;

  // Display Previous Clock CV
  NT_floatToString(debug_buf, self->debug_prev_clock_cv_val_for_debug, 2);
  NT_drawText(5, current_y, "PrvCV:", 15, kNT_textLeft, textSize);
  NT_drawText(55, current_y, debug_buf, 15, kNT_textLeft, textSize);
  current_y += line_spacing;

  NT_drawText(5, current_y, "MdCh:", 15, kNT_textLeft, textSize);
  NT_drawText(45, current_y, self->debug_param_changed_flags[kParamMode] ? "Y" : "N", 15, kNT_textLeft, textSize);
  self->debug_param_changed_flags[kParamMode] = false;

  NT_drawText(70, current_y, "ChsCh:", 15, kNT_textLeft, textSize);
  NT_drawText(120, current_y, self->debug_param_changed_flags[kParamChaosAmount] ? "Y" : "N", 15, kNT_textLeft, textSize);
  self->debug_param_changed_flags[kParamChaosAmount] = false;
  current_y += line_spacing;

  if (is_drum_mode)
  {
    NT_drawText(5, current_y, "XCh:", 15, kNT_textLeft, textSize);
    NT_drawText(45, current_y, self->debug_param_changed_flags[kParamDrumMapX] ? "Y" : "N", 15, kNT_textLeft, textSize);
    self->debug_param_changed_flags[kParamDrumMapX] = false;
    NT_drawText(70, current_y, "YCh:", 15, kNT_textLeft, textSize);
    NT_drawText(120, current_y, self->debug_param_changed_flags[kParamDrumMapY] ? "Y" : "N", 15, kNT_textLeft, textSize);
    self->debug_param_changed_flags[kParamDrumMapY] = false;
    current_y += line_spacing;
  }
  else // Euclidean Mode
  {
    NT_drawText(5, current_y, "L1Ch:", 15, kNT_textLeft, textSize);
    NT_drawText(45, current_y, self->debug_param_changed_flags[kParamEuclideanLength1] ? "Y" : "N", 15, kNT_textLeft, textSize);
    self->debug_param_changed_flags[kParamEuclideanLength1] = false;
    // Add other relevant Euclidean encoder targets if needed for debug
    // e.g., kParamEuclideanLength2, kParamEuclideanLength3 if mapped to other encoders
    current_y += line_spacing;
  }

  // Intentionally removed all other drawing of parameter values to focus on debug flags.

  return false; // Return false as we are not doing a full custom screen that would hide the top bar.
}

// --- Factory Definition (must be after all callback definitions it references) ---
static const _NT_factory s_nt_grids_factory = {
    .guid = NT_GRIDS_GUID,
    .name = "NT Grids",
    .description = "Grids Pattern Generator",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = nt_grids_calculate_static_requirements,
    .initialise = nt_grids_initialise,
    .calculateRequirements = nt_grids_calculate_requirements,
    .construct = nt_grids_construct,
    .parameterChanged = nt_grids_parameter_changed,
    .step = nt_grids_step,
    .draw = nt_grids_draw,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagUtility,
    .hasCustomUi = nt_grids_has_custom_ui,
    .customUi = nt_grids_custom_ui,
    .setupUi = nt_grids_setup_ui};

// --- Plugin Entry Point ---
extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t data)
{
  switch (selector)
  {
  case kNT_selector_version:
    return kNT_apiVersionCurrent;
  case kNT_selector_numFactories:
    return 1;
  case kNT_selector_factoryInfo:
    return (data == 0) ? reinterpret_cast<uintptr_t>(&s_nt_grids_factory) : 0;
  default:
    return 0;
  }
}

// --- Potentially add DISTING_NT_ALGORITHM_DEFINE if it's a common pattern ---
// However, gain.cpp uses pluginEntry directly.
// If DISTING_NT_ALGORITHM_DEFINE is a macro that generates pluginEntry,
// then this explicit definition would conflict or be redundant.
// For now, sticking to the explicit pluginEntry as per gain.cpp example.