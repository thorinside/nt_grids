#ifndef DISTING_NT_PLATFORM_ADAPTER_H
#define DISTING_NT_PLATFORM_ADAPTER_H

#include "distingnt/api.h"       // Explicitly include for NT types like _NT_textAlign
#include "nt_platform_adapter.h" // For INtPlatformAdapter interface

class DistingNtPlatformAdapter : public INtPlatformAdapter
{
public:
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
};

#endif // DISTING_NT_PLATFORM_ADAPTER_H