#ifndef NT_PLATFORM_ADAPTER_H
#define NT_PLATFORM_ADAPTER_H

#include "distingnt/api.h"           // For _NT_algorithm, _NT_parameter, _NT_uiData, _NT_float3 etc.
#include "nt_grids_parameter_defs.h" // For ParameterIndex enum

// Forward declarations for types used in the interface that might have declaration order issues
enum _NT_textSize;

// struct _NT_algorithm; // Already in distingnt/api.h
// struct _NT_parameter; // Already in distingnt/api.h

struct INtPlatformAdapter
{
  virtual ~INtPlatformAdapter() = default;

  // Parameter setting
  virtual void setParameterFromUi(uint32_t alg_idx, uint32_t param_idx_with_offset, int32_t value) = 0;

  // Drawing
  virtual void drawText(int16_t x, int16_t y, const char *str, uint8_t c, _NT_textAlignment align, _NT_textSize size) = 0;
  virtual void intToString(char *buffer, int32_t value) = 0;

  // Platform Info
  virtual uint32_t getAlgorithmIndex(_NT_algorithm *self_base) = 0;
  virtual uint32_t getParameterOffset() = 0;
  virtual float getSampleRate() = 0;

  // Parameter Definitions
  virtual const _NT_parameter *getParameterDefinition(ParameterIndex p_idx) = 0;
  virtual uint16_t getPotButtonMask(int pot_index) = 0; // For kNT_potButtonL/C/R constants

  // Potentially add methods for other NT_globals access if needed in future
  // Potentially add methods for other _NT_uiData fields if algorithm needs direct access beyond pot/encoder values
};

#endif // NT_PLATFORM_ADAPTER_H