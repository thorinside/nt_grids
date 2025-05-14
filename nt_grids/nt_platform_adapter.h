#ifndef NT_PLATFORM_ADAPTER_H
#define NT_PLATFORM_ADAPTER_H

#include "distingnt/api.h"
#include "nt_grids_parameter_defs.h"

class NtPlatformAdapter
{
public:
  virtual ~NtPlatformAdapter() = default;

  virtual void setParameterFromUi(uint32_t alg_idx, uint32_t param_idx, int32_t value) = 0;
  virtual void drawText(int x, int y, const char *text, int color, int align, int size) = 0;
  virtual int getAlgorithmIndex(void *algo) = 0;
  virtual uint16_t getPotButtonMask(int pot_index) = 0;
  virtual const _NT_parameter *getParameterDefinition(int p_idx) = 0;
};

#endif // NT_PLATFORM_ADAPTER_H