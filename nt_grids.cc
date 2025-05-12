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

struct NtGridsAlgorithm
{
  // Standard Disting NT algorithm members (from example)
  const _NT_parameter *parameters;
  const _NT_parameterPages *parameterPages;

  // Parameter values (local cache)
  // This array must be MAX_PARAMS long, as that's what the host expects to RW.
  // MAX_PARAMS is usually 64, check api.h
  int32_t v[64];

  // Pointers to inputs/outputs (populated by host)
  const float *clock_in;
  const float *reset_in;
  float *cv_outs[4]; // Changed to 4 for Trig1, Trig2, Trig3, Accent

  // Previous CV input states for edge detection (block-level)
  float prev_clock_cv_val;
  float prev_reset_cv_val;

  // You can add any other state your algorithm needs here.
  // For example, if PatternGenerator was a class instance:
  // nt_grids_port::grids::PatternGenerator pattern_generator_instance_;
};

// --- Parameter Definitions ---
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
static void update_grids_from_params(const int32_t *param_values)
{
  using namespace nt_grids_port::grids; // Keep this using directive local to function if preferred

  OutputMode previous_mode = PatternGenerator::current_output_mode();
  OutputMode new_mode = (OutputMode)param_values[kParamMode];

  if (new_mode != previous_mode)
  {
    PatternGenerator::set_output_mode(new_mode);
  }
  OutputMode current_mode = PatternGenerator::current_output_mode();

  if (current_mode == OUTPUT_MODE_DRUMS)
  {
    PatternGenerator::settings_[current_mode].options.drums.x = param_values[kParamDrumMapX];
    PatternGenerator::settings_[current_mode].options.drums.y = param_values[kParamDrumMapY];
    // Density parameters are 0-255. Assign directly.
    PatternGenerator::settings_[current_mode].density[0] = param_values[kParamDrumDensity1];
    PatternGenerator::settings_[current_mode].density[1] = param_values[kParamDrumDensity2];
    PatternGenerator::settings_[current_mode].density[2] = param_values[kParamDrumDensity3];
  }
  else // OUTPUT_MODE_EUCLIDEAN
  {
    for (int i = 0; i < nt_grids_port::kNumParts; ++i)
    {
      // Length parameter is 1-32. Assign directly.
      uint8_t length = param_values[kParamEuclideanLength1 + i];
      PatternGenerator::SetLength(i, length);

      // Fill parameter is 0-255 (density). Assign directly.
      uint8_t fill_density_param_val = param_values[kParamEuclideanFill1 + i];
      PatternGenerator::SetFill(i, fill_density_param_val);
    }
  }

  PatternGenerator::set_swing(param_values[kParamSwingEnable] != 0);
  PatternGenerator::set_global_chaos(param_values[kParamChaosEnable] != 0);

  if (PatternGenerator::chaos_globally_enabled_)
  {
    // Chaos Amount parameter is 0-255. Assign directly.
    uint8_t chaos_val = param_values[kParamChaosAmount];
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
  alg->parameters = s_parameters;
  alg->parameterPages = &parameterPages;

  // Initialize parameters from specifications or defaults
  if (specifications)
  {
    for (unsigned i = 0; i < req.numParameters; ++i)
    {
      alg->v[i] = specifications[i];
    }
  }
  else
  {
    for (unsigned i = 0; i < req.numParameters; ++i)
    {
      alg->v[i] = s_parameters[i].def;
    }
  }

  // Initialize CV input/output pointers - THIS IS THE CHALLENGING PART with old API
  // For now, we assume the host somehow makes these available if parameters are set.
  // Or, nt_grids_step will have to read from busFrames for CVs.
  // alg->clock_in = ???
  // alg->reset_in = ???

  alg->prev_clock_cv_val = 0.0f;
  alg->prev_reset_cv_val = 0.0f;

  update_grids_from_params(alg->v);
  nt_grids_port::grids::PatternGenerator::Reset();
  return reinterpret_cast<_NT_algorithm *>(alg);
}

// Original Signature for parameterChanged
static void nt_grids_parameter_changed(_NT_algorithm *self_base, int p_idx)
{
  NtGridsAlgorithm *self = reinterpret_cast<NtGridsAlgorithm *>(self_base);
  if (p_idx >= 0 && p_idx < kNumParameters)
  {
    self->v[p_idx] = self_base->v[p_idx];
  }
  update_grids_from_params(self->v);
  // If CV routing params changed, might need to update clock_in/reset_in if they were manually mapped.
}

// Original Signature for step, but with new internal logic
static void nt_grids_step(_NT_algorithm *self_base, float *busFrames, int numFramesBy4)
{
  NtGridsAlgorithm *self = reinterpret_cast<NtGridsAlgorithm *>(self_base);
  using nt_grids_port::grids::PatternGenerator;
  int num_frames_total = numFramesBy4 * 4; // Total samples in the block for busFrames indexing

  // CV Input Handling (once per block) - reading from busFrames
  const float cv_threshold = 0.5f;
  float current_clock_cv = 0.0f;
  float current_reset_cv = 0.0f;

  int clock_bus_idx_param_val = self->v[kParamClockInput];
  if (clock_bus_idx_param_val > 0 && (clock_bus_idx_param_val - 1) < 28)
  {
    current_clock_cv = busFrames[(clock_bus_idx_param_val - 1) * num_frames_total + 0];
  }

  if (current_clock_cv > cv_threshold && self->prev_clock_cv_val <= cv_threshold)
  {
    PatternGenerator::TickClock(true);
  }
  self->prev_clock_cv_val = current_clock_cv;

  int reset_bus_idx_param_val = self->v[kParamResetInput];
  if (reset_bus_idx_param_val > 0 && (reset_bus_idx_param_val - 1) < 28)
  {
    current_reset_cv = busFrames[(reset_bus_idx_param_val - 1) * num_frames_total + 0];
  }

  if (current_reset_cv > cv_threshold && self->prev_reset_cv_val <= cv_threshold)
  {
    PatternGenerator::Reset();
  }
  self->prev_reset_cv_val = current_reset_cv;

  // Get current trigger state from PatternGenerator
  uint8_t current_pattern_state = PatternGenerator::state_;

  // Main processing loop for each sample in the block
  // Writing outputs to busFrames based on parameter routing
  const float trigger_on_voltage = 1.0f; // Or 5.0f, depending on Disting standards
  const float trigger_off_voltage = 0.0f;

  for (int s = 0; s < num_frames_total; ++s)
  {
    // Trig 1 Output (Bus defined by kParamOutputTrig1)
    int bus_idx_trig1 = self->v[kParamOutputTrig1] - 1; // Params are 1-based bus numbers
    if (bus_idx_trig1 >= 0 && bus_idx_trig1 < 28)
    {
      busFrames[bus_idx_trig1 * num_frames_total + s] = (current_pattern_state & (1 << 0)) ? trigger_on_voltage : trigger_off_voltage;
    }

    // Trig 2 Output (Bus defined by kParamOutputTrig2)
    int bus_idx_trig2 = self->v[kParamOutputTrig2] - 1;
    if (bus_idx_trig2 >= 0 && bus_idx_trig2 < 28)
    {
      busFrames[bus_idx_trig2 * num_frames_total + s] = (current_pattern_state & (1 << 1)) ? trigger_on_voltage : trigger_off_voltage;
    }

    // Trig 3 Output (Bus defined by kParamOutputTrig3)
    int bus_idx_trig3 = self->v[kParamOutputTrig3] - 1;
    if (bus_idx_trig3 >= 0 && bus_idx_trig3 < 28)
    {
      busFrames[bus_idx_trig3 * num_frames_total + s] = (current_pattern_state & (1 << 2)) ? trigger_on_voltage : trigger_off_voltage;
    }

    // Accent Output (Bus defined by kParamOutputAccent)
    int bus_idx_accent = self->v[kParamOutputAccent] - 1;
    if (bus_idx_accent >= 0 && bus_idx_accent < 28)
    {
      busFrames[bus_idx_accent * num_frames_total + s] = (current_pattern_state & nt_grids_port::grids::OUTPUT_BIT_ACCENT) ? trigger_on_voltage : trigger_off_voltage;
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
  NtGridsAlgorithm *self = reinterpret_cast<NtGridsAlgorithm *>(self_base);
  bool is_drum_mode = (self->v[kParamMode] == 1);
  if (is_drum_mode)
  {
    pots[0] = (self->v[kParamDrumDensity1] / 255.0f);
    pots[1] = (self->v[kParamDrumDensity2] / 255.0f);
    pots[2] = (self->v[kParamDrumDensity3] / 255.0f);
  }
  else // Euclidean Mode
  {
    pots[0] = (self->v[kParamEuclideanFill1] / 255.0f);
    pots[1] = (self->v[kParamEuclideanFill2] / 255.0f);
    pots[2] = (self->v[kParamEuclideanFill3] / 255.0f);
  }
}

static void nt_grids_custom_ui(_NT_algorithm *self_base, const _NT_uiData &data)
{
  NtGridsAlgorithm *self = reinterpret_cast<NtGridsAlgorithm *>(self_base);
  bool is_drum_mode = (self->v[kParamMode] == 1);
  uint32_t alg_idx = NT_algorithmIndex(self_base);
  uint32_t param_offset = NT_parameterOffset();

  if ((data.buttons & kNT_encoderButtonR) && !(data.lastButtons & kNT_encoderButtonR))
  {
    int32_t current_mode_val = self->v[kParamMode];
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
      current_val = self->v[param_idx_to_change];
      min_val = s_parameters[param_idx_to_change].min;
      max_val = s_parameters[param_idx_to_change].max;
      new_val = current_val + data.encoders[0];
    }
    else
    {
      param_idx_to_change = kParamChaosAmount;
      current_val = self->v[param_idx_to_change];
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
      current_val = self->v[param_idx_to_change];
      min_val = s_parameters[param_idx_to_change].min;
      max_val = s_parameters[param_idx_to_change].max;
      new_val = current_val + data.encoders[1];
    }
    else
    {
      param_idx_to_change = kParamEuclideanLength1;
      current_val = self->v[param_idx_to_change];
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
  NtGridsAlgorithm *self = reinterpret_cast<NtGridsAlgorithm *>(self_base);
  bool is_drum_mode = (self->v[kParamMode] == 1);
  char buffer[32];
  _NT_textSize textSize = kNT_textNormal;
  int current_y = 22;
  const int line_spacing = 10;

  NT_drawText(5, current_y, is_drum_mode ? "Mode: DRUM" : "Mode: EUCLIDEAN", 15, kNT_textLeft, textSize);
  current_y += line_spacing;

  bool chaos_on = self->v[kParamChaosEnable] != 0;
  NT_drawText(5, current_y, "Chaos:", 15, kNT_textLeft, textSize);
  NT_drawText(55, current_y, chaos_on ? "ON" : "OFF", 15, kNT_textLeft, textSize);
  NT_drawText(95, current_y, "Amt:", 15, kNT_textLeft, textSize);
  NT_intToString(buffer, self->v[kParamChaosAmount]);
  NT_drawText(130, current_y, buffer, 15, kNT_textLeft, textSize);
  current_y += line_spacing;

  if (is_drum_mode)
  {
    NT_drawText(5, current_y, "X:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamDrumMapX]);
    NT_drawText(25, current_y, buffer, 15, kNT_textLeft, textSize);
    NT_drawText(70, current_y, "Y:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamDrumMapY]);
    NT_drawText(90, current_y, buffer, 15, kNT_textLeft, textSize);
    current_y += line_spacing;

    NT_drawText(5, current_y, "Density1:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamDrumDensity1]);
    NT_drawText(100, current_y, buffer, 15, kNT_textLeft, textSize);
    current_y += line_spacing;

    NT_drawText(5, current_y, "Density2:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamDrumDensity2]);
    NT_drawText(100, current_y, buffer, 15, kNT_textLeft, textSize);
    current_y += line_spacing;

    NT_drawText(5, current_y, "Density3:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamDrumDensity3]);
    NT_drawText(100, current_y, buffer, 15, kNT_textLeft, textSize);
  }
  else
  {
    NT_drawText(5, current_y, "L1:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamEuclideanLength1]);
    NT_drawText(35, current_y, buffer, 15, kNT_textLeft, textSize);
    NT_drawText(75, current_y, "F1:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamEuclideanFill1]);
    NT_drawText(105, current_y, buffer, 15, kNT_textLeft, textSize);
    current_y += line_spacing;

    NT_drawText(5, current_y, "L2:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamEuclideanLength2]);
    NT_drawText(35, current_y, buffer, 15, kNT_textLeft, textSize);
    NT_drawText(75, current_y, "F2:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamEuclideanFill2]);
    NT_drawText(105, current_y, buffer, 15, kNT_textLeft, textSize);
    current_y += line_spacing;

    NT_drawText(5, current_y, "L3:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamEuclideanLength3]);
    NT_drawText(35, current_y, buffer, 15, kNT_textLeft, textSize);
    NT_drawText(75, current_y, "F3:", 15, kNT_textLeft, textSize);
    NT_intToString(buffer, self->v[kParamEuclideanFill3]);
    NT_drawText(105, current_y, buffer, 15, kNT_textLeft, textSize);
  }
  return false;
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