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
                             m_state(TakeoverState::DIRECT_CONTROL), // Start in direct control or an initial hold?
                                                                     // Let's assume DIRECT_CONTROL until first configure/sync.
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
  m_state = TakeoverState::DIRECT_CONTROL; // Or HOLDING_WAIT_FOR_MOVE if params are known?
                                           // For now, DIRECT_CONTROL, syncPhysicalValue will set it up.
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
  // When configured, assume the primary parameter is now active. Potentially enter HOLDING state.
  // This will be properly set by syncPhysicalValue or resetTakeoverForModeSwitch typically called after configure.
  m_needs_initial_sync_after_config = true; // Set flag when configured
}

void TakeoverPot::resetTakeoverForModeSwitch(int16_t current_value_of_new_primary_param)
{
  // This function is called when the mode changes. The new primary parameter for this pot
  // in the new mode will be determined by the strategy's determinePotConfig, and then
  // configure() will be called by onModeActivated. So, this function might be redundant
  // if onModeActivated reliably calls configure().
  // However, to be safe and ensure m_needs_initial_sync_after_config is set upon mode switch:
  // We assume configure() will be called externally by the mode strategy after this.
  // For now, let's ensure the flag is set if this pot is re-evaluated.
  // The actual parameter values for m_held_parameter_value will be picked up in the first update().
  m_is_controlling_alternate = false; // Default to primary after mode switch
  m_needs_initial_sync_after_config = true;
}

void TakeoverPot::resetTakeoverForNewPrimary(int16_t current_value_of_new_primary_param)
{
  // Similar to above, configure() called by the strategy (e.g. Euclidean when toggling Length/Fill)
  // is the main path. This function primarily ensures state is ready for that re-config.
  m_is_controlling_alternate = false;
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
void TakeoverPot::setParameter(ParameterIndex param_to_set, int32_t value)
{
  if (!m_algo || !m_platform_adapter)
    return;

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
    return; // Invalid parameter index
  }

  uint32_t alg_idx = m_platform_adapter->getAlgorithmIndex(m_algo);
  uint32_t param_offset = m_platform_adapter->getParameterOffset();
  m_platform_adapter->setParameterFromUi(alg_idx, param_to_set + param_offset, value);
}

// Main update logic called from nt_grids_custom_ui
void TakeoverPot::update(const _NT_uiData &data)
{
  if (!m_algo || !m_platform_adapter || m_pot_index < 0 || m_pot_index > 2)
    return;

  float current_physical_value = data.pots[m_pot_index];
  ParameterIndex active_param_for_this_cycle = m_is_controlling_alternate && m_has_alternate ? m_alternate_param : m_primary_param;
  float active_scale_for_this_cycle = m_is_controlling_alternate && m_has_alternate ? m_alternate_scale : m_primary_scale;

  if (m_needs_initial_sync_after_config)
  {
    // This is the first update after this pot was configured (or re-configured).
    // active_param_for_this_cycle should be correct based on initial m_is_controlling_alternate (false).
    m_held_parameter_value = m_algo->v[active_param_for_this_cycle]; // Should be preset value by now.
    m_physical_pot_at_hold_start = current_physical_value;           // Uses data.pots[] which was set by setupUi.
    m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    m_needs_initial_sync_after_config = false;
    // m_prev_physical_value will be updated at the end of this function.
    // No actual parameter change here, just setup for hold.
  }

  // --- Determine active parameter and its current value from algo state (can change due to buttons) ---
  // Re-evaluate active_param for button logic, m_held_parameter_value will update if state changes
  active_param_for_this_cycle = m_primary_param;
  active_scale_for_this_cycle = m_primary_scale;
  // int16_t current_param_value_from_algo = m_algo->v[m_primary_param]; // Not needed if m_held_parameter_value is source of truth

  bool was_controlling_alternate = m_is_controlling_alternate;

  // --- Handle Button Transitions for Pot R (Chaos in Drum Mode) ---
  bool is_drum_mode = (m_algo->v[kParamMode] == 1);
  if (is_drum_mode && m_has_alternate && m_pot_index == 2)
  {
    bool potR_button_current = (data.buttons & kNT_potButtonR);
    bool potR_button_last = (data.lastButtons & kNT_potButtonR);
    bool button_just_pressed = potR_button_current && !potR_button_last;
    bool button_just_released = !potR_button_current && potR_button_last;

    if (button_just_pressed)
    {
      m_is_controlling_alternate = true;
      active_param_for_this_cycle = m_alternate_param;
      active_scale_for_this_cycle = m_alternate_scale;
      m_held_parameter_value = m_algo->v[active_param_for_this_cycle];
      m_physical_pot_at_hold_start = current_physical_value;
      m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    }
    else if (button_just_released)
    {
      m_is_controlling_alternate = false;
      active_param_for_this_cycle = m_primary_param;
      active_scale_for_this_cycle = m_primary_scale;
      m_held_parameter_value = m_algo->v[active_param_for_this_cycle];
      m_physical_pot_at_hold_start = current_physical_value;
      m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    }
  }
  else
  {
    // If not the special chaos pot, or mode changed, ensure we target primary.
    if (m_is_controlling_alternate)
    { // Was controlling alternate, but context changed (e.g. mode switch)
      m_is_controlling_alternate = false;
      active_param_for_this_cycle = m_primary_param; // Fallback to primary
      active_scale_for_this_cycle = m_primary_scale;
      // This situation implies a mode switch happened. The m_needs_initial_sync_after_config path
      // (triggered by configure() in onModeActivated) should correctly re-initialize based on primary param.
      // So, no need to explicitly set m_held_parameter_value or m_state here, as the sync flag handles it.
    }
  }

  // If button press/release just changed m_is_controlling_alternate, active_param_for_this_cycle is now updated.
  // If m_needs_initial_sync_after_config was true, it's now false, and state is HOLDING_WAIT_FOR_MOVE.
  // m_held_parameter_value and m_physical_pot_at_hold_start are set correctly for the current target.

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
        setParameter(active_param_for_this_cycle, new_val);
        m_held_parameter_value = new_val;
        if (new_val == scaled_physical_target)
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
      setParameter(active_param_for_this_cycle, scaled_physical_target);
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
        setParameter(active_param_for_this_cycle, new_val);
        m_held_parameter_value = new_val;
        if (new_val == scaled_physical_target)
        {
          m_state = TakeoverState::DIRECT_CONTROL;
        }
      }
    }
    break;

  case TakeoverState::DIRECT_CONTROL:
    if (current_physical_value != m_prev_physical_value || m_prev_physical_value == -1.0f)
    {
      setParameter(active_param_for_this_cycle, scaled_physical_target);
      m_held_parameter_value = scaled_physical_target;
    }
    break;
  }

  m_prev_physical_value = current_physical_value;
}