#include "nt_grids_takeover_pot.h"
#include "nt_grids.h"                    // Include for NtGridsAlgorithm definition and extern s_parameters
#include "distingnt/api.h"               // Needed for NT_setParameterFromUi, _NT_uiData etc.
#include "disting_nt_platform_adapter.h" // Required for the new member type

// Note: Now includes nt_grids.h instead of nt_grids.cc

// --- TakeoverPot Method Implementations ---
TakeoverPot::TakeoverPot() : m_algo(nullptr),
                             m_platform_adapter(nullptr),
                             m_pot_index(-1),
                             m_pot_button_mask(0),
                             m_primary_param(kParamMode),   // Default, will be overwritten by configure
                             m_alternate_param(kParamMode), // Default, will be overwritten by configure
                             m_has_alternate(false),
                             m_primary_scale(255.0f),
                             m_alternate_scale(255.0f),
                             m_is_controlling_alternate(false),
                             m_prev_physical_value(-1.0f),
                             m_state(TakeoverState::DIRECT_CONTROL), // Initial state; first update() call after configure() establishes proper HOLDING state.
                             m_held_parameter_value(0),
                             m_physical_pot_at_hold_start(0.0f),
                             m_needs_initial_sync_after_config(true) // Initialize to true
{
}

void TakeoverPot::init(NtGridsAlgorithm *algo, int pot_index, DistingNtPlatformAdapter *platform_adapter)
{
  m_algo = algo;
  m_pot_index = pot_index;
  m_platform_adapter = platform_adapter;

  if (pot_index == 0)
    m_pot_button_mask = kNT_potButtonL;
  else if (pot_index == 1)
    m_pot_button_mask = kNT_potButtonC;
  else // pot_index == 2
    m_pot_button_mask = kNT_potButtonR;

  // Reset state
  m_is_controlling_alternate = false;
  m_prev_physical_value = -1.0f;
  m_state = TakeoverState::DIRECT_CONTROL; // Initial state; first update() call after configure() establishes proper HOLDING state.
  m_held_parameter_value = 0;
  m_physical_pot_at_hold_start = 0.0f;
  m_needs_initial_sync_after_config = true; // Initialize to true
}

void TakeoverPot::configure(ParameterIndex p, ParameterIndex a, bool h, float ps, float as)
{
  m_primary_param = p;
  m_alternate_param = a;
  m_has_alternate = h;
  m_primary_scale = (ps > 0) ? ps : 1.0f;
  m_alternate_scale = (as > 0) ? as : 1.0f;
  // This pot is now (re)configured. Flag it so the next update() performs a full sync
  // to the current parameter value and physical pot position, establishing a new hold state.
  m_needs_initial_sync_after_config = true;
}

// Called when mode changes or primary parameter focus might change (e.g., Euclidean Length/Fill toggle).
// Ensures the pot is flagged for re-synchronization on its next update().
// The mode strategy's onModeActivated() or relevant button handlers are responsible for calling
// configure() first, which sets m_needs_initial_sync_after_config.
// These reset methods ensure m_is_controlling_alternate is appropriately set to false and
// redundantly set the sync flag for robustness, anticipating a configure() call from the strategy.
void TakeoverPot::resetTakeoverForModeSwitch()
{
  m_is_controlling_alternate = false; // Default to primary after mode switch
  m_needs_initial_sync_after_config = true;
}

void TakeoverPot::resetTakeoverForNewPrimary()
{
  m_is_controlling_alternate = false; // Default to primary when focus changes (e.g. Length/Fill toggle)
  m_needs_initial_sync_after_config = true;
}

void TakeoverPot::syncPhysicalValue(float physical_pot_value)
{
  // Explicit external sync request.
  ParameterIndex param_to_sync = m_is_controlling_alternate && m_has_alternate ? m_alternate_param : m_primary_param;
  m_held_parameter_value = m_algo->v[param_to_sync];
  m_physical_pot_at_hold_start = physical_pot_value;
  m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
  m_prev_physical_value = physical_pot_value;
  m_needs_initial_sync_after_config = false; // Sync is done
}

// Private helper to set parameter value, handling clamping and UI update call
int32_t TakeoverPot::setParameter(ParameterIndex param_to_set, int32_t value)
{
  if (!m_algo || !m_platform_adapter)
    return value; // Or some indicator of error / unchanged value if preferred for non-call

  // Clamp value using s_parameters
  if (param_to_set >= 0 && param_to_set < kNumParameters)
  {
    int32_t min_val = s_parameters[param_to_set].min;
    int32_t max_val = s_parameters[param_to_set].max;

    // --- REMOVED Fill Capping Logic ---
    // Fill parameters now use their full range (0-255) regardless of Length.

    // Standard clamping to parameter min/max
    if (value < min_val)
      value = min_val;
    if (value > max_val)
      value = max_val;
  }
  else
  {
    return value; // Invalid parameter index, return original value (or handle error)
  }

  uint32_t alg_idx = m_platform_adapter->getAlgorithmIndex(m_algo);
  uint32_t param_offset = m_platform_adapter->getParameterOffset();
  m_platform_adapter->setParameterFromUi(alg_idx, param_to_set + param_offset, value);
  return value; // Return the (potentially clamped) value that was set
}

// Main update logic called from nt_grids_custom_ui
void TakeoverPot::update(const _NT_uiData &data)
{
  if (!m_algo || !m_platform_adapter || m_pot_index < 0 || m_pot_index > 2)
    return;

  float current_physical_value = data.pots[m_pot_index];

  // Handle initial sync after configuration.
  // m_is_controlling_alternate should be correctly set by configure() or resetTakeover...()
  // before this update is called for the first time after config.
  if (m_needs_initial_sync_after_config)
  {
    ParameterIndex param_for_initial_sync = (m_is_controlling_alternate && m_has_alternate) ? m_alternate_param : m_primary_param;
    m_held_parameter_value = m_algo->v[param_for_initial_sync];
    m_physical_pot_at_hold_start = current_physical_value;
    m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    m_needs_initial_sync_after_config = false;
    // m_prev_physical_value will be updated at the end of this function.
  }

  // --- Handle Button Logic and Update m_is_controlling_alternate ---
  // This section solely focuses on setting m_is_controlling_alternate
  // and re-syncing takeover state (m_held_parameter_value, etc.) if a switch occurs.
  bool is_drum_mode = (m_algo->v[kParamMode] == 1);

  if (is_drum_mode && m_has_alternate && m_pot_index == 2) // Pot R (Chaos)
  {
    bool button_currently_pressed = (data.buttons & kNT_potButtonR);
    bool button_pressed_last_cycle = (data.lastButtons & kNT_potButtonR);
    bool button_just_became_pressed = button_currently_pressed && !button_pressed_last_cycle;
    bool button_just_became_released = !button_currently_pressed && button_pressed_last_cycle;

    if (button_just_became_pressed)
    {
      if (!m_is_controlling_alternate) // If not already true
      {
        m_is_controlling_alternate = true;
        // Sync state for newly targeted alternate parameter
        m_held_parameter_value = m_algo->v[m_alternate_param];
        m_physical_pot_at_hold_start = current_physical_value;
        m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
      }
    }
    else if (button_just_became_released)
    {
      if (m_is_controlling_alternate) // If it was true
      {
        m_is_controlling_alternate = false;
        // Sync state for newly targeted primary parameter
        m_held_parameter_value = m_algo->v[m_primary_param];
        m_physical_pot_at_hold_start = current_physical_value;
        m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
      }
    }
    // If button is held (button_currently_pressed and not just_became_pressed),
    // m_is_controlling_alternate remains as set by the press. State is already synced.
  }
  else // Not the Chaos pot configuration (e.g. different pot, different mode, or m_has_alternate is false for this pot)
  {
    if (m_is_controlling_alternate) // If we were controlling alternate but context changed
    {
      m_is_controlling_alternate = false;
      // A full configure() via onModeActivated() should have set m_needs_initial_sync_after_config.
      // The sync block at the start of update() handles this.
      // If for some edge case sync flag isn't set, manually re-sync to primary:
      if (!m_needs_initial_sync_after_config)
      {
        m_held_parameter_value = m_algo->v[m_primary_param];
        m_physical_pot_at_hold_start = current_physical_value;
        m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
      }
    }
  }

  // --- Now, derive active_param_for_this_cycle and active_scale_for_this_cycle ---
  // This is based on the final state of m_is_controlling_alternate after button/context logic.
  ParameterIndex active_param_for_this_cycle;
  float active_scale_for_this_cycle;

  if (m_is_controlling_alternate && m_has_alternate) // Ensure m_has_alternate is still true
  {
    active_param_for_this_cycle = m_alternate_param;
    active_scale_for_this_cycle = m_alternate_scale;
  }
  else
  {
    active_param_for_this_cycle = m_primary_param;
    active_scale_for_this_cycle = m_primary_scale;
    if (m_is_controlling_alternate)
    {                                     // If m_is_controlling_alternate is true but m_has_alternate is false
      m_is_controlling_alternate = false; // Correct the flag
      // And re-sync to primary if not already handled by initial sync
      if (!m_needs_initial_sync_after_config)
      {
        m_held_parameter_value = m_algo->v[m_primary_param];
        m_physical_pot_at_hold_start = current_physical_value;
        m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
      }
    }
  }

  // --- Main State Machine for Pot Value Handling ---
  int32_t scaled_physical_target = static_cast<int32_t>((current_physical_value * active_scale_for_this_cycle) + 0.5f);

  switch (m_state)
  {
  case TakeoverState::HOLDING_WAIT_FOR_MOVE:
    // Parameter value is m_held_parameter_value. No change until pot moves from m_physical_pot_at_hold_start.
    if (current_physical_value != m_physical_pot_at_hold_start)
    {
      m_state = TakeoverState::RELATIVELY_ADJUSTING;
      int32_t new_val = m_held_parameter_value;
      if (current_physical_value > m_physical_pot_at_hold_start)
        new_val++;
      else if (current_physical_value < m_physical_pot_at_hold_start)
        new_val--;

      if (new_val != m_held_parameter_value)
      {
        m_held_parameter_value = setParameter(active_param_for_this_cycle, new_val);
        if (m_held_parameter_value == scaled_physical_target)
        {
          m_state = TakeoverState::DIRECT_CONTROL;
        }
      }
      else if (m_held_parameter_value == scaled_physical_target)
      {
        m_state = TakeoverState::DIRECT_CONTROL;
      }
    }
    break;

  case TakeoverState::RELATIVELY_ADJUSTING:
    if (m_held_parameter_value == scaled_physical_target)
    {
      m_state = TakeoverState::DIRECT_CONTROL;
      m_held_parameter_value = setParameter(active_param_for_this_cycle, scaled_physical_target);
    }
    else if (current_physical_value != m_prev_physical_value)
    {
      int32_t new_val = m_held_parameter_value;
      if (current_physical_value > m_prev_physical_value)
        new_val++;
      else if (current_physical_value < m_prev_physical_value)
        new_val--;

      if (new_val != m_held_parameter_value)
      {
        m_held_parameter_value = setParameter(active_param_for_this_cycle, new_val);
        if (m_held_parameter_value == scaled_physical_target)
        {
          m_state = TakeoverState::DIRECT_CONTROL;
        }
      }
    }
    break;

  case TakeoverState::DIRECT_CONTROL:
    if (current_physical_value != m_prev_physical_value || m_prev_physical_value == -1.0f)
    {
      m_held_parameter_value = setParameter(active_param_for_this_cycle, scaled_physical_target);
    }
    break;
  }

  m_prev_physical_value = current_physical_value;
}