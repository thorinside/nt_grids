#pragma once
#include "nt_grids_parameter_defs.h"
#include "distingnt/api.h"

// Forward declarations
struct NtGridsAlgorithm;
struct _NT_uiData;

// Interface for mode-specific UI and control logic
class IModeStrategy
{
public:
  virtual ~IModeStrategy() = default;

  // Handle encoder input for this mode
  virtual void handleEncoderInput(NtGridsAlgorithm *self, const _NT_uiData &data, int encoder_index) = 0;

  // Handle pot input and configuration for this mode
  virtual void processPotsUI(NtGridsAlgorithm *self, const _NT_uiData &data) = 0;
  virtual void setupTakeoverPots(NtGridsAlgorithm *self, _NT_float3 &pots) = 0;

  // Draw mode-specific UI elements
  virtual void drawModeUI(NtGridsAlgorithm *self, int y_start, _NT_textSize text_size, int line_spacing, char *buffer) = 0;

  // Called when this mode becomes active
  virtual void onModeActivated(NtGridsAlgorithm *self) = 0;
  // Called when this mode is deactivated
  virtual void onModeDeactivated(NtGridsAlgorithm *self) = 0;

  // Parameter mapping helpers
  virtual void determinePotConfig(NtGridsAlgorithm *self, int pot_index, ParameterIndex &primary_idx, ParameterIndex &alternate_idx, bool &has_alternate, float &primary_scale, float &alternate_scale) = 0;

  // Mode name for display/debug
  virtual const char *getModeName() const = 0;
};