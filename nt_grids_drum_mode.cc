#include "nt_grids_drum_mode.h"
#include "nt_grids.h"                // For NtGridsAlgorithm, ParameterIndex, etc.
#include "nt_grids_parameter_defs.h" // For kParamOff and other parameter definitions
#include <cstring>

#ifdef TESTING_BUILD
#include <cstdio> // For snprintf
#endif

// Handle encoder input for Drum mode (Map X and Map Y)
void DrumModeStrategy::handleEncoderInput(NtGridsAlgorithm *self, const _NT_uiData &data, int encoder_idx)
{
  if (!self || !self->m_platform_adapter)
  {
#ifdef TESTING_BUILD
    if (self && self->m_platform_adapter)
      self->m_platform_adapter->drawText(0, 0, "DrumEnc: Early exit, self or adapter null", 0, kNT_textLeft, kNT_textTiny);
#endif
    return;
  }

  int encoder_delta = data.encoders[encoder_idx];
  bool changed_flag = (encoder_delta != 0); // Check if encoder_delta itself is non-zero

#ifdef TESTING_BUILD
  char msg_buf[200];
  snprintf(msg_buf, sizeof(msg_buf), "DrumEnc Idx:%d Delta:%d Changed:%s", encoder_idx, encoder_delta, changed_flag ? "T" : "F");
  self->m_platform_adapter->drawText(0, 0, msg_buf, 0, kNT_textLeft, kNT_textTiny);
#endif

  if (!changed_flag || encoder_delta == 0)
  {
#ifdef TESTING_BUILD
    self->m_platform_adapter->drawText(0, 0, "DrumEnc: Early exit, no change or zero delta", 0, kNT_textLeft, kNT_textTiny);
#endif
    return; // No change or no movement for this encoder
  }

  ParameterIndex target_param = kParamOff;
  if (encoder_idx == 0)
    target_param = kParamDrumMapX;
  else if (encoder_idx == 1)
    target_param = kParamDrumMapY;
  else
    return; // Invalid encoder index

  const _NT_parameter *param_def = self->m_platform_adapter->getParameterDefinition(target_param);
  if (!param_def)
    return;
  int current_val = self->v[target_param];
  int min_val = param_def->min;
  int max_val = param_def->max;
  int new_val = current_val + encoder_delta;
  if (new_val < min_val)
    new_val = min_val;
  if (new_val > max_val)
    new_val = max_val;
  uint32_t alg_idx = self->m_platform_adapter->getAlgorithmIndex(self);
  uint32_t param_offset = self->m_platform_adapter->getParameterOffset();
  self->m_platform_adapter->setParameterFromUi(alg_idx, target_param + param_offset, new_val);
}

// Process pots for Drum mode (Density1, Density2, Density3, Chaos on R)
void DrumModeStrategy::processPotsUI(NtGridsAlgorithm *self, const _NT_uiData &data)
{
  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_param_idx, alternate_param_idx;
    bool has_alternate;
    float primary_scale, alternate_scale;
    determinePotConfig(self, i, primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale);
    self->m_pots[i].configure(primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale);
    self->m_pots[i].update(data);
  }
}

// Set up initial pot positions for Drum mode (Density1-3)
void DrumModeStrategy::setupTakeoverPots(NtGridsAlgorithm *self, _NT_float3 &pots)
{
  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_param_idx;
    float primary_scale;
    // Only Density1,2,3 for Drum mode
    switch (i)
    {
    case 0:
      primary_param_idx = kParamDrumDensity1;
      break;
    case 1:
      primary_param_idx = kParamDrumDensity2;
      break;
    case 2:
      primary_param_idx = kParamDrumDensity3;
      break;
    default:
      primary_param_idx = kParamDrumDensity1;
      break;
    }
    primary_scale = 255.0f;
    pots[i] = (primary_scale > 0) ? ((float)self->v[primary_param_idx] / primary_scale) : 0.0f;
    self->m_pots[i].syncPhysicalValue(pots[i]);
  }
}

// Draw Drum mode UI (densities, Map X/Y, Chaos)
void DrumModeStrategy::drawModeUI(NtGridsAlgorithm *self, int y_start, _NT_textSize text_size, int line_spacing, char *buffer)
{
  if (!self || !self->m_platform_adapter)
    return;
  int current_y = y_start;
  // Densities
  self->m_platform_adapter->drawText(10, current_y, "D1:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter->intToString(buffer, self->v[kParamDrumDensity1]);
  self->m_platform_adapter->drawText(35, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter->drawText(95, current_y, "D2:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter->intToString(buffer, self->v[kParamDrumDensity2]);
  self->m_platform_adapter->drawText(120, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter->drawText(175, current_y, "D3:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter->intToString(buffer, self->v[kParamDrumDensity3]);
  self->m_platform_adapter->drawText(200, current_y, buffer, 15, kNT_textLeft, text_size);
  current_y += line_spacing;
  // Map X, Map Y, Chaos
  self->m_platform_adapter->drawText(70, current_y, "X:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter->intToString(buffer, self->v[kParamDrumMapX]);
  self->m_platform_adapter->drawText(95, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter->drawText(135, current_y, "Y:", 15, kNT_textLeft, text_size);
  self->m_platform_adapter->intToString(buffer, self->v[kParamDrumMapY]);
  self->m_platform_adapter->drawText(160, current_y, buffer, 15, kNT_textLeft, text_size);
  self->m_platform_adapter->drawText(200, current_y, "Chaos:", 15, kNT_textLeft, text_size);
  if (self->v[kParamChaosEnable])
  {
    self->m_platform_adapter->intToString(buffer, self->v[kParamChaosAmount]);
    self->m_platform_adapter->drawText(255, current_y, buffer, 15, kNT_textRight, text_size);
  }
  else
  {
    self->m_platform_adapter->drawText(255, current_y, "Off", 15, kNT_textRight, text_size);
  }
}

// Called when Drum mode is activated
void DrumModeStrategy::onModeActivated(NtGridsAlgorithm * /*self*/)
{
  // No special state to reset for Drum mode
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