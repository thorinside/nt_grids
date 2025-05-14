#ifndef NT_GRIDS_MODE_STRATEGY_H
#define NT_GRIDS_MODE_STRATEGY_H

#include "nt_grids_parameter_defs.h"
#include "distingnt/api.h"

class NtGridsAlgorithm;

class ModeStrategy
{
public:
  virtual ~ModeStrategy() = default;
  virtual void handleEncoderInput(NtGridsAlgorithm *self, int encoder_idx, int encoder_delta) = 0;
  virtual void handlePotInput(NtGridsAlgorithm *self, int pot_index, float pot_value) = 0;
  virtual void drawUi(NtGridsAlgorithm *self) = 0;
  virtual void onModeActivated(NtGridsAlgorithm *self) = 0;
  virtual void onModeDeactivated(NtGridsAlgorithm *self) = 0;
  virtual void determinePotConfig(NtGridsAlgorithm *self, int pot_index, ParameterIndex &primary_idx, ParameterIndex &alternate_idx, bool &has_alternate, float &primary_scale, float &alternate_scale) = 0;
  virtual const char *modeName() const = 0;
};

#endif // NT_GRIDS_MODE_STRATEGY_H