#pragma once

#include "distingnt/api.h"           // Explicitly include for NT types
#include "nt_platform_adapter.h"     // For INtPlatformAdapter interface
#include "nt_grids_parameter_defs.h" // For ParameterIndex

// Forward declaration
struct _NT_algorithm;

// Forward declaration to resolve circular dependency
class NtGridsAlgorithm;

class DistingNtPlatformAdapter : public INtPlatformAdapter
{
public:
  DistingNtPlatformAdapter(NtGridsAlgorithm *alg);
  ~DistingNtPlatformAdapter() override = default;

  // Parameter setting
  void setParameterFromUi(uint32_t alg_idx, uint32_t param_idx_with_offset, int32_t value) override;

  // Drawing
  void drawText(int16_t x, int16_t y, const char *str, uint8_t c, _NT_textAlignment align, _NT_textSize size) override;
  void intToString(char *buffer, int32_t value) override;

  // Platform Info
  uint32_t getAlgorithmIndex(_NT_algorithm *self_base) override;
  uint32_t getParameterOffset() override;
  float getSampleRate() override;

  // Parameter Definitions
  const _NT_parameter *getParameterDefinition(ParameterIndex p_idx) override;
  uint16_t getPotButtonMask(int pot_index) override;

private:
  NtGridsAlgorithm *m_algorithm;
};