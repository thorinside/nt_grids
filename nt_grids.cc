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
#include "nt_grids.h" // Includes NtGridsAlgorithm struct, TakeoverPot class, ParameterIndex enum

// Forward declaration if needed, though PatternGenerator should be fully defined by its header
// namespace nt_grids_port { namespace grids { class PatternGenerator; } }

// Define a unique GUID for this algorithm
#define NT_GRIDS_GUID NT_MULTICHAR('N', 'T', 'G', 'R')

// No anonymous namespace, all file-scope symbols below will be static or const static.

// --- ParameterDefinitions (s_parameters array, etc.) ---
static const char *kEnumModeStrings[] = {"Euclidean", "Drums", NULL};
static const char *kEnumBooleanStrings[] = {"Off", "On", NULL};

// Define s_parameters (matches extern declaration in nt_grids.h)
// clang-format off
const _NT_parameter s_parameters[] = {
    {.name = "Mode", .min = 0, .max = 1, .def = 1, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kEnumModeStrings},
    {.name = "Chaos Enable", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kEnumBooleanStrings},
    {.name = "Chaos Amount", .min = 0, .max = 255, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Drum Map X", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Drum Map Y", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Drum Density 1", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Drum Density 2", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Drum Density 3", .min = 0, .max = 255, .def = 128, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Len/Ctrl", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = (const char*[]){"Length", "Fill", "Shift", NULL}},
    {.name = "Euclid Length 1", .min = 1, .max = 16, .def = 8, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Fill 1", .min = 0, .max = 16, .def = 4, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Shift 1", .min = 0, .max = 15, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Length 2", .min = 1, .max = 16, .def = 8, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Fill 2", .min = 0, .max = 16, .def = 4, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Shift 2", .min = 0, .max = 15, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Length 3", .min = 1, .max = 16, .def = 8, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Fill 3", .min = 0, .max = 16, .def = 4, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Euclid Shift 3", .min = 0, .max = 15, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    NT_PARAMETER_AUDIO_INPUT("Clock In", 0, 0)
    NT_PARAMETER_AUDIO_INPUT("Reset In", 0, 0)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Trig 1 Out", 1, 0)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Trig 2 Out", 2, 0)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Trig 3 Out", 3, 0)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Accent Out", 4, 0)
};
// clang-format on
// Note: kNumParameters enum value must match the size implicitly

static const uint8_t NUM_TRIGGER_STEPS = 5;

// --- Parameter Pages ---
static const uint8_t s_page_main[] = {
    kParamMode,
    kParamChaosEnable,
    kParamChaosAmount,
};
static const uint8_t s_page_drum[] = {
    kParamDrumMapX, kParamDrumMapY, kParamChaosAmount,
    kParamDrumDensity1, kParamDrumDensity2, kParamDrumDensity3};
static const uint8_t s_page_euclidean[] = {
    kParamEuclideanControlsLength, // Added to page
    kParamEuclideanLength1, kParamEuclideanFill1, kParamEuclideanShift1,
    kParamEuclideanLength2, kParamEuclideanFill2, kParamEuclideanShift2,
    kParamEuclideanLength3, kParamEuclideanFill3, kParamEuclideanShift3};
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

const _NT_parameterPages parameterPages = {
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
      uint8_t length = (uint8_t)param_values[kParamEuclideanLength1 + i * 3]; // Corrected indexing for Length, Fill, Shift
      PatternGenerator::SetLength(i, length);

      uint8_t fill_density_param_val = (uint8_t)param_values[kParamEuclideanFill1 + i * 3]; // Corrected indexing
      PatternGenerator::SetFill(i, fill_density_param_val);

      // uint8_t shift_param_val = (uint8_t)param_values[kParamEuclideanShift1 + i * 3]; // Corrected indexing
      // PatternGenerator::SetShift(i, shift_param_val);
    }
  }

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

// --- NtGridsAlgorithm Constructor Definition ---
NtGridsAlgorithm::NtGridsAlgorithm()
    : m_platform_adapter(this),
      m_euclidean_mode_strategy(this)
{
  prev_clock_cv_val = 0.0f;
  prev_reset_cv_val = 0.0f;
  for (int i = 0; i < 4; ++i)
  {
    trigger_active_steps_remaining[i] = 0;
  }
  debug_recent_clock_tick = false;
  debug_recent_reset = false;
  memset(debug_param_changed_flags, 0, sizeof(debug_param_changed_flags));
  m_last_mode = -1; // Initialize to an invalid mode to ensure first mode set is detected
  m_current_mode_strategy = nullptr;

  // Initialize TakeoverPots
  for (int i = 0; i < 3; ++i)
  {
    m_pots[i].init(this, i, &m_platform_adapter);
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
  // Use placement new for NtGridsAlgorithm itself, which calls the constructor above.
  NtGridsAlgorithm *alg = new (ptrs.sram) NtGridsAlgorithm();

  // Initialize inherited members:
  alg->parameters = s_parameters;
  alg->parameterPages = &parameterPages;

  // m_platform_adapter is now a direct member, constructed by NtGridsAlgorithm's constructor.

  // Initialize m_last_mode (needs alg->v)
  alg->m_last_mode = alg->v[kParamMode];

  // Set the initial mode strategy (needs alg->v)
  if (alg->v[kParamMode] == 1) // 1 is typically Drum Mode based on kEnumModeStrings
  {
    alg->m_current_mode_strategy = &alg->m_drum_mode_strategy;
  }
  else
  {
    alg->m_current_mode_strategy = &alg->m_euclidean_mode_strategy;
  }
  if (alg->m_current_mode_strategy) // Check before dereferencing
  {
    alg->m_current_mode_strategy->onModeActivated(alg);
  }

  update_grids_from_params(alg->v);
  nt_grids_port::grids::PatternGenerator::Reset();
  return reinterpret_cast<_NT_algorithm *>(alg);
}

// Original Signature for parameterChanged
static void nt_grids_parameter_changed(_NT_algorithm *self_base, int p_idx)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base);
  // if (p_idx >= 0 && p_idx < kNumParameters)
  // {
  //   self->debug_param_changed_flags[p_idx] = true;
  // }

  if (p_idx == kParamMode)
  {
    int16_t new_mode_val = self->v[kParamMode];
    if (new_mode_val != self->m_last_mode)
    {
      // Deactivate old strategy
      if (self->m_current_mode_strategy)
      {
        self->m_current_mode_strategy->onModeDeactivated(self);
      }
      self->m_last_mode = new_mode_val;
      // Switch strategy pointer
      if (new_mode_val == 1)
      { // Drum Mode
        self->m_current_mode_strategy = &self->m_drum_mode_strategy;
      }
      else
      { // Euclidean Mode
        self->m_current_mode_strategy = &self->m_euclidean_mode_strategy;
      }
      // Activate new strategy
      if (self->m_current_mode_strategy)
      {
        self->m_current_mode_strategy->onModeActivated(self);
      }
    }
  }
  update_grids_from_params(self_base->v); // This is now active
}

// Original Signature for step, but with new internal logic
static void nt_grids_step(_NT_algorithm *self_base, float *busFrames, int numFramesBy4)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base); // Use static_cast
  using nt_grids_port::grids::PatternGenerator;
  int num_frames_total = numFramesBy4 * 4; // Total samples in the block

  const float cv_threshold = 0.5f;

  bool tick_this_step = false;

  for (int s_cv = 0; s_cv < num_frames_total; ++s_cv)
  {
    // Clock Input Detection
    if (self->v[kParamClockInput] > 0) // Use self->v[] to access parameter value
    {
      int clock_bus_array_idx = self->v[kParamClockInput] - 1;
      if (clock_bus_array_idx < 28) // Max 28 CV buses on Disting EX
      {
        float current_sample_clock_cv = busFrames[clock_bus_array_idx * num_frames_total + s_cv];
        if (current_sample_clock_cv > cv_threshold && self->prev_clock_cv_val <= cv_threshold)
        {
          PatternGenerator::TickClock(true);
          self->debug_recent_clock_tick = true; // Set global debug flag
          tick_this_step = true;                // Local flag for this step's logic
        }
        self->prev_clock_cv_val = current_sample_clock_cv;
      }
      else
      {
        self->prev_clock_cv_val = 0.0f; // Invalid bus selected
      }
    }
    else
    {
      self->prev_clock_cv_val = 0.0f; // Clock input is 'Off'
    }

    // Reset Input Detection
    if (self->v[kParamResetInput] > 0) // Use self->v[] to access parameter value
    {
      int reset_bus_array_idx = self->v[kParamResetInput] - 1;
      if (reset_bus_array_idx < 28) // Max 28 CV buses on Disting EX
      {
        float current_sample_reset_cv = busFrames[reset_bus_array_idx * num_frames_total + s_cv];
        if (current_sample_reset_cv > cv_threshold && self->prev_reset_cv_val <= cv_threshold)
        {
          PatternGenerator::Reset();
          for (int i = 0; i < 4; ++i)
          {
            self->trigger_active_steps_remaining[i] = 0;
          }
          self->debug_recent_reset = true;
        }
        self->prev_reset_cv_val = current_sample_reset_cv;
      }
      else
      {
        self->prev_reset_cv_val = 0.0f; // Invalid bus selected
      }
    }
    else
    {
      self->prev_reset_cv_val = 0.0f; // Reset input is 'Off'
    }
  } // End of CV input processing loop

  uint8_t current_pattern_state = PatternGenerator::state_;

  // Trigger Initiation: If a clock ticked in this step, set duration for active pattern bits
  if (tick_this_step)
  {
    if (current_pattern_state & (1 << 0))
      self->trigger_active_steps_remaining[0] = NUM_TRIGGER_STEPS;
    if (current_pattern_state & (1 << 1))
      self->trigger_active_steps_remaining[1] = NUM_TRIGGER_STEPS;
    if (current_pattern_state & (1 << 2))
      self->trigger_active_steps_remaining[2] = NUM_TRIGGER_STEPS;
    if (current_pattern_state & nt_grids_port::grids::OUTPUT_BIT_ACCENT)
      self->trigger_active_steps_remaining[3] = NUM_TRIGGER_STEPS;
  }

  // Main processing loop for each sample in the block
  const float trigger_on_voltage = 5.0f;
  const float trigger_off_voltage = 0.0f;

  for (int s = 0; s < num_frames_total; ++s)
  {
    for (int i = 0; i < 4; ++i) // Loop through 4 triggers (Trig1, Trig2, Trig3, Accent)
    {
      ParameterIndex bus_param_idx;
      ParameterIndex mode_param_idx;

      // Determine which parameters to use based on the trigger index 'i'
      switch (i)
      {
      case 0: // Trig1
        bus_param_idx = kParamOutputTrig1;
        mode_param_idx = kParamOutputTrig1Mode;
        break;
      case 1: // Trig2
        bus_param_idx = kParamOutputTrig2;
        mode_param_idx = kParamOutputTrig2Mode;
        break;
      case 2: // Trig3
        bus_param_idx = kParamOutputTrig3;
        mode_param_idx = kParamOutputTrig3Mode;
        break;
      case 3: // Accent
        bus_param_idx = kParamOutputAccent;
        mode_param_idx = kParamOutputAccentMode;
        break;
      default: // Should not happen
        continue;
      }

      int bus_idx = self->v[bus_param_idx] - 1; // v is inherited const int16_t*
      if (bus_idx >= 0 && bus_idx < 28)         // Max 28 buses
      {
        bool replace_mode = self->v[mode_param_idx];
        float value_to_write = (self->trigger_active_steps_remaining[i] > 0) ? trigger_on_voltage : trigger_off_voltage;

        if (replace_mode)
        {
          busFrames[bus_idx * num_frames_total + s] = value_to_write;
        }
        else
        {
          busFrames[bus_idx * num_frames_total + s] += value_to_write;
        }
      }
    }
  }

  // Countdown active trigger steps at the end of the block processing
  for (int i = 0; i < 4; ++i)
  {
    if (self->trigger_active_steps_remaining[i] > 0)
    {
      self->trigger_active_steps_remaining[i]--;
    }
  }

  // Flags are now cleared in draw()
  // self->debug_recent_clock_tick = false;
  // self->debug_recent_reset = false;
}

// --- Custom UI Callback Implementations (all static as per example) ---

static bool nt_grids_has_custom_ui(_NT_algorithm *self_base)
{
  return true;
}

static void nt_grids_setup_ui(_NT_algorithm *self_base, _NT_float3 &pots)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base);
  if (self->m_current_mode_strategy)
  {
    self->m_current_mode_strategy->setupTakeoverPots(self, pots); // Original call, now restored
    // For debugging: if strategy exists, try a safe platform adapter call
    // This is a bit artificial as setupUi's role is pots, but it tests the strategy pointer and adapter.
    // self->m_platform_adapter.drawText(1, 1, "S", 15, kNT_textLeft, kNT_textSmall); // Example debug draw
  }
  // else
  // {
  // If strategy is NULL, set pots to a different default to indicate this path was taken
  // pots[0] = 0.9f; // REMOVE
  // }

  // Fallback default pot values if the above logic doesn't cover all pots or if strategy is null.
  // Ensure all pots are initialized to prevent uninitialized reads by the firmware.
  // For any pots not explicitly set by the strategy, set them to a default (e.g., 0.0f).
  // This loop can be removed if the strategy guarantees to set all pots[0-2].
  // For now, keeping it as a safeguard is reasonable if strategies might not set all.
  // pots[0] = 0.0f; // REMOVE
  // pots[1] = 0.0f; // REMOVE
  // pots[2] = 0.0f; // REMOVE
}

//----------------------------------------------------------------------------------------------------
// Update custom UI controls -- THIS FUNCTION IS CURRENTLY BROKEN DUE TO EDIT ISSUES
// Corrected version using TakeoverPot from separate files
//----------------------------------------------------------------------------------------------------
static void nt_grids_custom_ui(_NT_algorithm *self_base, const _NT_uiData &data)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base);

  // --- Mode Toggle on Right Encoder Button Click (Press and Release) --- (NOW RE-ENABLING)
  if ((data.buttons & kNT_encoderButtonR) && !(data.lastButtons & kNT_encoderButtonR))
  {
    // The parameterChanged callback will handle strategy deactivation/activation and calling onModeActivated.
    // It will also call update_grids_from_params.
    // The UI will be redrawn due to parameter change, and setupUi will be called for the new mode.

    int32_t current_mode_val = self->v[kParamMode];
    int32_t new_mode_val = 1 - current_mode_val; // Toggle 0 (Euclidean) / 1 (Drums)

    // Set parameter via platform adapter
    uint32_t alg_idx = self->m_platform_adapter.getAlgorithmIndex(self_base); // Use self_base for platform adapter
    uint32_t param_offset = self->m_platform_adapter.getParameterOffset();
    self->m_platform_adapter.setParameterFromUi(alg_idx, kParamMode + param_offset, new_mode_val);
    // nt_grids_parameter_changed will be called by the system, which will switch strategies.
    return; // UI will be recalled due to parameter change, no further processing this call.
  }

  // Delegate other UI interactions to the current strategy
  if (self->m_current_mode_strategy)
  {
    // Pots L, C, R processing (NOW RE-ENABLING)
    self->m_current_mode_strategy->processPotsUI(self, data);

    // Encoder L and R processing (NOW RE-ENABLING)
    if (data.encoders[0] != 0)
    {
      self->m_current_mode_strategy->handleEncoderInput(self, data, 0); // Encoder L
    }
    if (data.encoders[1] != 0)
    {
      self->m_current_mode_strategy->handleEncoderInput(self, data, 1); // Encoder R
    }
  }
}

static bool nt_grids_draw(_NT_algorithm *self_base)
{
  NtGridsAlgorithm *self = static_cast<NtGridsAlgorithm *>(self_base);
  // char buffer[64]; // Buffer may not be needed if strategy call is commented

  // --- Title ---
  self->m_platform_adapter.drawText(128, 23, "Grids", 15, kNT_textCentre, kNT_textLarge);
  self->m_platform_adapter.drawText(128, 30, "by Emilie Gillet", 15, kNT_textCentre, kNT_textTiny);

  // --- Parameter Display --- Delegate to strategy (NOW RE-ENABLING)
  _NT_textSize textSize = kNT_textNormal;
  int current_y = 42;
  const int line_spacing = 11;
  char buffer[64]; // Buffer is needed by strategy draw functions

  if (self->m_current_mode_strategy)
  {
    self->m_current_mode_strategy->drawModeUI(self, current_y, textSize, line_spacing, buffer);
  }

  // --- Status Indicators --- (NOW RE-ENABLING)
  _NT_textSize statusTextSize = kNT_textNormal;
  if (self->debug_recent_clock_tick)
  {
    self->m_platform_adapter.drawText(250, 12, "CLK", 15, kNT_textRight, statusTextSize);
  }
  if (self->debug_recent_reset)
  {
    self->m_platform_adapter.drawText(220, 12, "RST", 15, kNT_textRight, statusTextSize);
  }
  self->debug_recent_clock_tick = false; // MOVED/UNCOMMENTED: Clear after drawing
  self->debug_recent_reset = false;      // MOVED/UNCOMMENTED: Clear after drawing

  return true;
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

// --- TakeoverPot Method Implementations (Placed After NtGridsAlgorithm and s_parameters) ---
// Moved to nt_grids_takeover_pot.cc