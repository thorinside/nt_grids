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
  HOLDING_WAIT_FOR_MOVE, // Parameter selected, value held, waiting for first pot movement
  RELATIVELY_ADJUSTING,  // Pot moved, parameter changing relatively, seeking to meet physical pot value
  DIRECT_CONTROL         // Parameter value has met physical pot, direct control engaged
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

  bool m_is_controlling_alternate; // True if the alternate parameter is currently being targeted
  float m_prev_physical_value;     // Last known physical pot position (0.0-1.0)

  // State for takeover logic
  TakeoverState m_state;
  int16_t m_held_parameter_value;         // Stores the parameter's value when HOLDING_WAIT_FOR_MOVE begins
  float m_physical_pot_at_hold_start;     // Physical pot position (0.0-1.0) when HOLDING_WAIT_FOR_MOVE began
  bool m_needs_initial_sync_after_config; // Flag to trigger full sync on first update() after configure()
};

#endif // NT_GRIDS_TAKEOVER_POT_H