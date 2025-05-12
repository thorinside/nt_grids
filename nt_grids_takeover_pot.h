#ifndef NT_GRIDS_TAKEOVER_POT_H
#define NT_GRIDS_TAKEOVER_POT_H

#include "distingnt/api.h" // Include necessary API definitions (_NT_uiData, kNT_potButtonX, etc.)

// Shared parameter index definitions
// TODO: Consider moving to a more central location if more shared enums arise.
enum ParameterIndex
{
  // Mode & Global
  kParamMode,
  kParamChaosEnable,
  kParamChaosAmount,
  // CV Inputs
  kParamClockInput,
  kParamResetInput,
  // Drum Mode Specific
  kParamDrumMapX,
  kParamDrumMapY,
  kParamDrumDensity1,
  kParamDrumDensity2,
  kParamDrumDensity3,
  // Euclidean Mode Specific
  kParamEuclideanLength1,
  kParamEuclideanLength2,
  kParamEuclideanLength3,
  kParamEuclideanFill1,
  kParamEuclideanFill2,
  kParamEuclideanFill3,
  // Outputs
  kParamOutputTrig1,
  kParamOutputTrig1Mode,
  kParamOutputTrig2,
  kParamOutputTrig2Mode,
  kParamOutputTrig3,
  kParamOutputTrig3Mode,
  kParamOutputAccent,
  kParamOutputAccentMode,
  kNumParameters
};

// Forward declare the main algorithm struct to use a pointer to it
struct NtGridsAlgorithm;

class TakeoverPot
{
public:
  TakeoverPot();
  void init(NtGridsAlgorithm *algo, int pot_index);
  void configure(ParameterIndex primary_param, ParameterIndex alternate_param, bool has_alternate,
                 float primary_scale, float alternate_scale);
  void resetTakeoverForModeSwitch(int16_t new_primary_param_value);
  void resetTakeoverForNewPrimary(int16_t new_primary_param_value);
  void syncPhysicalValue(float physical_pot_value);
  void update(const _NT_uiData &data);

private:
  // Private helper function needs access to NT_setParameterFromUi, s_parameters, kNumParameters etc.
  // Implementation will be in the .cc file.
  void setParameter(ParameterIndex param_to_set, int32_t value);

  NtGridsAlgorithm *m_algo;   // Pointer to the main algorithm state
  int m_pot_index;            // 0, 1, or 2 for L, C, R
  uint16_t m_pot_button_mask; // e.g., kNT_potButtonL
  ParameterIndex m_primary_param;
  ParameterIndex m_alternate_param;
  bool m_has_alternate;
  float m_primary_scale;
  float m_alternate_scale;

  bool m_is_controlling_alternate;    // True if the alternate parameter is currently being controlled
  bool m_takeover_active_primary;     // True if primary parameter takeover is active
  bool m_takeover_active_alternate;   // True if alternate parameter takeover is active
  int16_t m_primary_takeover_value;   // Value the primary parameter had when takeover started
  int16_t m_alternate_takeover_value; // Value the alternate parameter had when takeover started
  float m_prev_physical_value;        // Last known physical pot position (0.0-1.0)
};

#endif // NT_GRIDS_TAKEOVER_POT_H