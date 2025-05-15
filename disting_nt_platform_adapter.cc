#include "disting_nt_platform_adapter.h"
#include "distingnt/api.h" // For actual NT_ calls and NT_globals
#include "nt_grids.h"      // For extern s_parameters (and ParameterIndex via nt_grids_parameter_defs.h indirectly)

// Constructor
DistingNtPlatformAdapter::DistingNtPlatformAdapter(NtGridsAlgorithm *alg)
//: m_algorithm(alg) // Comment out member initialization if any complex logic depends on it here
{
  // Comment out entire body for testing
  // if (alg) { // Basic null check
  //     // Any other simple, safe initializations can go here if needed
  //     // For example, if you had simple members that don't depend on alg->v or other complex state.
  // }
  // m_algorithm is the main thing to initialize. If alg is nullptr, it implies an issue elsewhere.
  m_algorithm = alg; // Keep this minimal assignment for now, but be wary if alg itself is problematic
}

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
const _NT_parameter *DistingNtPlatformAdapter::getParameterDefinition(ParameterIndex param_idx)
{
  if (param_idx >= 0 && param_idx < kNumParameters)
  {
    if (m_algorithm && m_algorithm->parameters)
    {
      return &m_algorithm->parameters[param_idx];
    }
  }
  return nullptr;
}

uint16_t DistingNtPlatformAdapter::getPotButtonMask(int pot_index)
{
  if (pot_index == 0)
    return kNT_potButtonL;
  if (pot_index == 1)
    return kNT_potButtonC;
  if (pot_index == 2)
    return kNT_potButtonR;
  return 0;
}