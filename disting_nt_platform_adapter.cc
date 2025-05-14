#include "disting_nt_platform_adapter.h"
#include "distingnt/api.h" // For actual NT_ calls and NT_globals
#include "nt_grids.h"      // For extern s_parameters (and ParameterIndex via nt_grids_parameter_defs.h indirectly)

// Parameter setting
void DistingNtPlatformAdapter::setParameterFromUi(uint32_t alg_idx, uint32_t param_idx_with_offset, int32_t value)
{
  NT_setParameterFromUi(alg_idx, param_idx_with_offset, value);
}

// Drawing
void DistingNtPlatformAdapter::drawText(int16_t x, int16_t y, const char *str, uint8_t c, _NT_textAlignment align, _NT_textSize size)
{
  NT_drawText(x, y, str, c, align, size);
}
void DistingNtPlatformAdapter::intToString(char *buffer, int32_t value)
{
  NT_intToString(buffer, value);
}

// Platform Info
uint32_t DistingNtPlatformAdapter::getAlgorithmIndex(_NT_algorithm *self_base)
{
  return NT_algorithmIndex(self_base);
}
uint32_t DistingNtPlatformAdapter::getParameterOffset()
{
  return NT_parameterOffset();
}
float DistingNtPlatformAdapter::getSampleRate()
{
  return NT_globals.sampleRate;
}

// Parameter Definitions
const _NT_parameter *DistingNtPlatformAdapter::getParameterDefinition(ParameterIndex p_idx)
{
  // kNumParameters is from nt_grids_parameter_defs.h, included via nt_grids.h or nt_platform_adapter.h
  if (p_idx >= 0 && p_idx < kNumParameters)
  {
    return &s_parameters[p_idx];
  }
  return nullptr; // Should ideally not happen if p_idx is always valid
}

uint16_t DistingNtPlatformAdapter::getPotButtonMask(int pot_index)
{
  if (pot_index == 0)
    return kNT_potButtonL;
  if (pot_index == 1)
    return kNT_potButtonC;
  if (pot_index == 2)
    return kNT_potButtonR;
  return 0; // Should not happen if pot_index is always 0, 1, or 2
}