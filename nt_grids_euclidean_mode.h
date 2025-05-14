#pragma once
#include "nt_grids_mode_strategy.h"

class EuclideanModeStrategy : public IModeStrategy
{
public:
  EuclideanModeStrategy(NtGridsAlgorithm *self_algo_for_init);
  ~EuclideanModeStrategy() override = default;

  void handleEncoderInput(NtGridsAlgorithm *self, const _NT_uiData &data, int encoder_index) override;
  void processPotsUI(NtGridsAlgorithm *self, const _NT_uiData &data) override;
  void setupTakeoverPots(NtGridsAlgorithm *self, _NT_float3 &pots) override;
  void drawModeUI(NtGridsAlgorithm *self, int y_start, _NT_textSize text_size, int line_spacing, char *buffer) override;
  void onModeActivated(NtGridsAlgorithm *self) override;
  void onModeDeactivated(NtGridsAlgorithm *self) override;
  void determinePotConfig(NtGridsAlgorithm *self, int pot_index, ParameterIndex &primary_idx, ParameterIndex &alternate_idx, bool &has_alternate, float &primary_scale, float &alternate_scale) override;
  const char *getModeName() const override;

private:
  bool m_euclidean_controls_length = false;
};