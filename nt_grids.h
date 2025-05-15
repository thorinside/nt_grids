#ifndef NT_GRIDS_H
#define NT_GRIDS_H

#include "distingnt/api.h"
#include "nt_grids_takeover_pot.h"
#include "disting_nt_platform_adapter.h" // Include the new platform adapter
#include "nt_grids_drum_mode.h"          // Include Drum mode strategy
#include "nt_grids_euclidean_mode.h"     // Include Euclidean mode strategy
// IModeStrategy is included by the concrete strategy headers if they are used

// --- NtGridsAlgorithm Struct Definition ---
// Moved here from nt_grids.cc so it can be included by other .cc files
struct NtGridsAlgorithm : _NT_algorithm // Inherit from _NT_algorithm
{
  const float *clock_in;
  const float *reset_in;

  float prev_clock_cv_val;
  float prev_reset_cv_val;

  int8_t trigger_active_steps_remaining[4]; // Added to track trigger duration over steps

  bool debug_recent_clock_tick;
  bool debug_recent_reset;
  bool debug_param_changed_flags[kNumParameters]; // kNumParameters comes from nt_grids_takeover_pot.h

  // TakeoverPot objects are now defined via the included header
  TakeoverPot m_pots[3];

  // Track the last known mode to detect changes
  int16_t m_last_mode; // Use int16_t to match parameter value type

  // Platform Adapter and Mode Strategies
  DistingNtPlatformAdapter m_platform_adapter; // Changed from pointer to direct member
  DrumModeStrategy m_drum_mode_strategy;
  EuclideanModeStrategy m_euclidean_mode_strategy;
  IModeStrategy *m_current_mode_strategy;

  // Constructor declaration, if needed for strategies that take 'this'
  NtGridsAlgorithm();

  // Removed methods that are now part of TakeoverPot or will be strategy-dependent
  // void resetTakeoverForModeSwitch(int16_t new_primary_param_value);
  // void syncPhysicalValue(float physical_pot_value);
  // void resetTakeoverForNewPrimary(int16_t new_primary_param_value);
  // void update(const _NT_uiData &data);
};

// --- Extern declaration for s_parameters ---
// Allows nt_grids_takeover_pot.cc to access the parameter definitions
extern const _NT_parameter s_parameters[];

#endif // NT_GRIDS_H