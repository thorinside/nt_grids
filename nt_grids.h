#ifndef NT_GRIDS_H
#define NT_GRIDS_H

#include "distingnt/api.h"
#include "nt_grids_takeover_pot.h" // Need TakeoverPot definition for the member array

// --- NtGridsAlgorithm Struct Definition ---
// Moved here from nt_grids.cc so it can be included by other .cc files
struct NtGridsAlgorithm : _NT_algorithm // Inherit from _NT_algorithm
{
  const float *clock_in;
  const float *reset_in;

  float prev_clock_cv_val;
  float prev_reset_cv_val;

  uint32_t trigger_on_samples_remaining[4];

  bool debug_recent_clock_tick;
  bool debug_recent_reset;
  bool debug_param_changed_flags[kNumParameters]; // kNumParameters comes from nt_grids_takeover_pot.h

  // TakeoverPot objects are now defined via the included header
  TakeoverPot m_pots[3];

  // Track the last known mode to detect changes
  int16_t m_last_mode; // Use int16_t to match parameter value type

  bool m_euclidean_controls_length; // True if pots control Length 1-3 in Euclidean mode

  // Add a default constructor maybe? Or rely on placement new initialization?
  // Relying on placement new for now.

  void resetTakeoverForModeSwitch(int16_t new_primary_param_value); // For Drum/Euclid mode switch
  void syncPhysicalValue(float physical_pot_value);                 // For setupUi
  void resetTakeoverForNewPrimary(int16_t new_primary_param_value); // For Euclid Length/Fill toggle
  void update(const _NT_uiData &data);                              // The main update logic
};

// --- Extern declaration for s_parameters ---
// Allows nt_grids_takeover_pot.cc to access the parameter definitions
extern const _NT_parameter s_parameters[];

#endif // NT_GRIDS_H