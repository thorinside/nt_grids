#include "nt_grids_drum_mode.h"
#include "nt_grids.h"                // For NtGridsAlgorithm, ParameterIndex, etc.
#include "nt_grids_parameter_defs.h" // For kParamOff and other parameter definitions
#include <cstring>

// Manual MIN/MAX macros if not available
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

// DrumModeStrategy constructor - ensure it doesn't configure pots.
// Default constructor is fine if it does no work related to self_algo_for_init->m_pots.
// Removed definition here, as it's defaulted in the header.
// DrumModeStrategy::DrumModeStrategy()
// {
//   // Constructor should be empty or only initialize its own members not dependent on NtGridsAlgorithm state.
// }

// Handle encoder input for Drum mode (Map X and Map Y)
void DrumModeStrategy::handleEncoderInput(NtGridsAlgorithm *self, const _NT_uiData &data, int encoder_idx)
{
  if (!self)
    return; // self pointer check is still valid

  int encoder_delta = data.encoders[encoder_idx];
  if (encoder_delta == 0)
    return;

  ParameterIndex target_param = kParamMode; // Original initialization
  if (encoder_idx == 0)
  { // Left Encoder typically controls X
    target_param = kParamDrumMapX;
  }
  else if (encoder_idx == 1)
  { // Right Encoder typically controls Y
    target_param = kParamDrumMapY;
  }
  else
  {
    return; // Should not happen with valid encoder_idx
  }

  const _NT_parameter *param_def = self->m_platform_adapter.getParameterDefinition(target_param);
  if (!param_def)
    return;

  int32_t current_val = self->v[target_param];
  int32_t new_val = current_val + encoder_delta; // Use original encoder_delta

  new_val = MAX(param_def->min, MIN(param_def->max, new_val));

  if (new_val != current_val)
  {
    uint32_t alg_idx = self->m_platform_adapter.getAlgorithmIndex(self);
    uint32_t param_offset = self->m_platform_adapter.getParameterOffset();
    self->m_platform_adapter.setParameterFromUi(alg_idx, target_param + param_offset, new_val);
  }
}

// Process pots for Drum mode (Density1, Density2, Density3, Chaos on R)
void DrumModeStrategy::processPotsUI(NtGridsAlgorithm *self, const _NT_uiData &data)
{
  for (int i = 0; i < 3; ++i)
  {
    // ParameterIndex primary_param_idx, alternate_param_idx;
    // bool has_alternate;
    // float primary_scale, alternate_scale;
    // determinePotConfig(self, i, primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale);
    // self->m_pots[i].configure(primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale); // Configure is done in onModeActivated
    self->m_pots[i].update(data);
  }
}

// Set up initial pot positions for Drum mode (Density1-3)
void DrumModeStrategy::setupTakeoverPots(NtGridsAlgorithm *self, _NT_float3 &pots_out)
{
  if (!self)
    return;

  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_idx;
    ParameterIndex alternate_idx;
    bool has_alternate;
    float primary_scale;
    float alternate_scale;

    determinePotConfig(self, i, primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
    // self->m_pots[i].configure(primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
    // configure() is now called in onModeActivated. Here we only care about sync for initial UI draw.

    // Calculate the ideal physical pot position (0.0-1.0) that represents the current parameter value.
    // This current parameter value SHOULD be the preset value at this stage.
    float ideal_physical_pos = 0.0f;
    if (primary_scale > 0.001f)
    { // Avoid division by zero or tiny scales
      ideal_physical_pos = static_cast<float>(self->v[primary_idx]) / primary_scale;
    }
    // Clamp to 0.0 - 1.0, though scaled parameters should ideally fall within this.
    if (ideal_physical_pos < 0.0f)
      ideal_physical_pos = 0.0f;
    if (ideal_physical_pos > 1.0f)
      ideal_physical_pos = 1.0f;

    pots_out[i] = ideal_physical_pos; // Tell the firmware what data.pots[i] should be for the first custom_ui call.

    // Now sync the TakeoverPot with this ideal physical position.
    // m_held_parameter_value will be set from self->v[primary_idx] (the preset value).
    // m_physical_pot_at_hold_start will be ideal_physical_pos.
    // State will be HOLDING_WAIT_FOR_MOVE.
    // The first custom_ui call will see data.pots[i] == ideal_physical_pos, so no change will occur until user moves the actual hardware.
    // self->m_pots[i].syncPhysicalValue(ideal_physical_pos); // REMOVED - Initial sync is handled by configure() + first update()
  }
}

// Draw Drum mode UI (densities, Map X/Y, Chaos)
void DrumModeStrategy::drawModeUI(NtGridsAlgorithm *self, int y_start, _NT_textSize text_size, int line_spacing, char *buffer)
{
  if (!self)
    return; // self pointer check is still valid

  int current_y = y_start;
  // Densities
  self->m_platform_adapter.drawText(10, current_y, "D1:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter.intToString(buffer, self->v[kParamDrumDensity1]);
  self->m_platform_adapter.drawText(35, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter.drawText(95, current_y, "D2:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter.intToString(buffer, self->v[kParamDrumDensity2]);
  self->m_platform_adapter.drawText(120, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter.drawText(175, current_y, "D3:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter.intToString(buffer, self->v[kParamDrumDensity3]);
  self->m_platform_adapter.drawText(200, current_y, buffer, 15, kNT_textLeft, text_size);
  current_y += line_spacing;
  // Map X, Map Y, Chaos
  self->m_platform_adapter.drawText(70, current_y, "X:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter.intToString(buffer, self->v[kParamDrumMapX]);
  self->m_platform_adapter.drawText(95, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter.drawText(135, current_y, "Y:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter.intToString(buffer, self->v[kParamDrumMapY]);
  self->m_platform_adapter.drawText(160, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter.drawText(200, current_y, "Chaos:", 15, kNT_textLeft, text_size);

  bool chaos_enabled = self->v[kParamChaosEnable];
  if (chaos_enabled)
  {
    self->m_platform_adapter.intToString(buffer, self->v[kParamChaosAmount]);
    self->m_platform_adapter.drawText(255, current_y, buffer, 15, kNT_textRight, text_size);
  }
  else
  {
    self->m_platform_adapter.drawText(255, current_y, "Off", 15, kNT_textRight, text_size);
  }
}

// Called when Drum mode is activated
void DrumModeStrategy::onModeActivated(NtGridsAlgorithm *self)
{
  // No special state to reset for Drum mode
  if (!self)
    return;

  // Configure pots when mode is activated
  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_idx;
    ParameterIndex alternate_idx;
    bool has_alternate;
    float primary_scale;
    float alternate_scale;
    // 'self' is not used by DrumModeStrategy::determinePotConfig, but pass for consistency if it changes.
    this->determinePotConfig(self, i, primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
    self->m_pots[i].configure(primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
  }
}

// Called when Drum mode is deactivated
void DrumModeStrategy::onModeDeactivated(NtGridsAlgorithm * /*self*/)
{
  // No special state to reset for Drum mode
}

// Map pots to Drum mode parameters (Density1, Density2, Density3, Chaos on R)
void DrumModeStrategy::determinePotConfig(NtGridsAlgorithm * /*self*/, int pot_index, ParameterIndex &primary_idx, ParameterIndex &alternate_idx, bool &has_alternate, float &primary_scale, float &alternate_scale)
{
  primary_scale = 255.0f;
  alternate_scale = 255.0f;
  has_alternate = false;
  switch (pot_index)
  {
  case 0:
    primary_idx = kParamDrumDensity1;
    alternate_idx = kParamMode; // Unused
    break;
  case 1:
    primary_idx = kParamDrumDensity2;
    alternate_idx = kParamMode; // Unused
    break;
  case 2:
    primary_idx = kParamDrumDensity3;
    alternate_idx = kParamChaosAmount;
    has_alternate = true;
    break;
  default:
    primary_idx = kParamDrumDensity1;
    alternate_idx = kParamMode;
    break;
  }
}

// Return mode name
const char *DrumModeStrategy::getModeName() const
{
  return "Drums";
}