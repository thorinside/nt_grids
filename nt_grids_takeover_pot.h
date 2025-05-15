#ifndef NT_GRIDS_TAKEOVER_POT_H
#define NT_GRIDS_TAKEOVER_POT_H

#include "distingnt/api.h"           // Include necessary API definitions
#include "nt_grids_parameter_defs.h" // Include the canonical ParameterIndex definition

// Shared parameter index definitions
// TODO: Consider moving to a more central location if more shared enums arise.

// Forward declare the main algorithm struct to use a pointer to it
struct NtGridsAlgorithm;
class DistingNtPlatformAdapter; // Forward declare

// Enum for TakeoverPot state
enum class TakeoverState
{
  INACTIVE,        // Pot is not yet in control, value hasn't crossed stored param value
  ACTIVE_PRIMARY,  // Pot is controlling the primary parameter
  ACTIVE_ALTERNATE // Pot is controlling the alternate parameter
};

class TakeoverPot
{
public:
  TakeoverPot();
  void init(NtGridsAlgorithm *algo, int pot_index, DistingNtPlatformAdapter *platform_adapter);
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

  NtGridsAlgorithm *m_algo;                     // Pointer to the main algorithm state
  DistingNtPlatformAdapter *m_platform_adapter; // Added member for platform adapter
  int m_pot_index;                              // 0, 1, or 2 for L, C, R
  uint16_t m_pot_button_mask;                   // e.g., kNT_potButtonL
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
  float m_physical_value;             // Current physical position (0.0 to 1.0)

  // Added missing members
  TakeoverState m_state;
  int16_t m_cached_primary_value;
  int16_t m_cached_alternate_value;
};

#endif // NT_GRIDS_TAKEOVER_POT_H