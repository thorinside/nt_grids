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
                             m_physical_pot_at_hold_start(0.0f)
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
}

void TakeoverPot::resetTakeoverForModeSwitch(int16_t current_value_of_new_primary_param)
{
  m_is_controlling_alternate = false;
  m_held_parameter_value = current_value_of_new_primary_param;
  // m_prev_physical_value should hold the last known pot position. If it's -1.0 (first time),
  // assume a default like 0.0 for m_physical_pot_at_hold_start or get it from hardware if possible.
  // For now, if -1.0, it implies that the first update() will use it.
  m_physical_pot_at_hold_start = (m_prev_physical_value == -1.0f) ? 0.0f : m_prev_physical_value;
  m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
}

void TakeoverPot::resetTakeoverForNewPrimary(int16_t current_value_of_new_primary_param)
{
  m_is_controlling_alternate = false;
  m_held_parameter_value = current_value_of_new_primary_param;
  m_physical_pot_at_hold_start = (m_prev_physical_value == -1.0f) ? 0.0f : m_prev_physical_value;
  m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
}

void TakeoverPot::syncPhysicalValue(float physical_pot_value)
{
  // This is called when UI is first drawn or needs to re-sync.
  // The parameter value should NOT change here. We just set up the hold state.
  m_prev_physical_value = physical_pot_value; // Update this first.
  m_physical_pot_at_hold_start = physical_pot_value;

  if (m_is_controlling_alternate && m_has_alternate)
  {
    m_held_parameter_value = m_algo->v[m_alternate_param];
  }
  else
  {
    m_held_parameter_value = m_algo->v[m_primary_param];
  }
  m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
  // Parameter value itself (m_algo->v[...]) remains untouched.
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
  ParameterIndex active_param = m_primary_param;
  float active_scale = m_primary_scale;
  int16_t current_param_value_from_algo = m_algo->v[m_primary_param];

  // --- Determine active parameter and its current value from algo state ---
  if (m_is_controlling_alternate && m_has_alternate)
  {
    active_param = m_alternate_param;
    active_scale = m_alternate_scale;
    current_param_value_from_algo = m_algo->v[m_alternate_param];
  }

  // --- Handle Button Transitions for Pot R (Chaos in Drum Mode) ---
  bool is_drum_mode = (m_algo->v[kParamMode] == 1);
  if (is_drum_mode && m_has_alternate && m_pot_index == 2)
  {
    bool potR_button_current = (data.buttons & kNT_potButtonR);
    bool potR_button_last = (data.lastButtons & kNT_potButtonR);
    bool button_just_pressed = potR_button_current && !potR_button_last;
    bool button_just_released = !potR_button_current && potR_button_last;

    if (button_just_pressed) // Switched to controlling alternate (Chaos)
    {
      m_is_controlling_alternate = true;
      active_param = m_alternate_param; // Update local active_param for this cycle
      active_scale = m_alternate_scale;
      current_param_value_from_algo = m_algo->v[m_alternate_param]; // Get fresh value

      m_held_parameter_value = current_param_value_from_algo;
      m_physical_pot_at_hold_start = current_physical_value;
      m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    }
    else if (button_just_released) // Switched back to controlling primary (D3)
    {
      m_is_controlling_alternate = false;
      active_param = m_primary_param; // Update local active_param
      active_scale = m_primary_scale;
      current_param_value_from_algo = m_algo->v[m_primary_param]; // Get fresh value

      m_held_parameter_value = current_param_value_from_algo;
      m_physical_pot_at_hold_start = current_physical_value;
      m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    }
    // If button state hasn't changed, m_is_controlling_alternate, active_param, active_scale,
    // current_param_value_from_algo (from start of update) and m_state persist correctly.
  }
  else
  {
    // If not the special chaos pot, or mode changed, ensure we target primary.
    // resetTakeoverForModeSwitch would have set up the HOLDING state for primary.
    // This block mainly ensures m_is_controlling_alternate is correct if we fell out of the above IF.
    if (m_is_controlling_alternate)
    { // Was controlling alternate, but now not drum mode pot R
      m_is_controlling_alternate = false;
      // State change to HOLDING_WAIT_FOR_MOVE for primary param should be handled by resetTakeoverForModeSwitch.
      // Re-fetch active_param etc. for safety, though resetTakeover should make it consistent.
      active_param = m_primary_param;
      active_scale = m_primary_scale;
      current_param_value_from_algo = m_algo->v[m_primary_param];
      // Re-initialize hold for primary param, assuming mode switch logic dictates it
      m_held_parameter_value = current_param_value_from_algo;
      m_physical_pot_at_hold_start = current_physical_value;
      m_state = TakeoverState::HOLDING_WAIT_FOR_MOVE;
    }
  }

  // --- Main State Machine for Pot Value Handling ---
  int32_t scaled_physical_target = static_cast<int32_t>((current_physical_value * active_scale) + 0.5f);

  switch (m_state)
  {
  case TakeoverState::HOLDING_WAIT_FOR_MOVE:
    // Parameter value in m_algo->v[active_param] is currently m_held_parameter_value (or should be).
    // No change to parameter value until pot moves from its initial activation position.
    if (current_physical_value != m_physical_pot_at_hold_start || m_prev_physical_value == -1.0f) // Pot moved or first update
    {
      // Pot has moved for the first time since hold started.
      m_state = TakeoverState::RELATIVELY_ADJUSTING;
      // The value to change is m_held_parameter_value, then apply it.
      // The first adjustment is based on new physical vs. physical_at_hold_start.
      int32_t new_val = m_held_parameter_value;
      if (current_physical_value > m_physical_pot_at_hold_start)
        new_val++;
      else if (current_physical_value < m_physical_pot_at_hold_start)
        new_val--;
      // else: pot moved but ended up at same scaled step - rare, no change yet or handle as direct? For now, no change.

      if (new_val != m_held_parameter_value)
      { // Only if a change actually occurred
        setParameter(active_param, new_val);
        m_held_parameter_value = new_val; // Update m_held_parameter_value for next relative step if still needed

        // Check if this first relative step already met the physical pot's target
        if (new_val == scaled_physical_target)
        {
          m_state = TakeoverState::DIRECT_CONTROL;
        }
      }
      else if (m_held_parameter_value == scaled_physical_target)
      {
        // Pot moved slightly but resulted in no change, and we happen to match target.
        m_state = TakeoverState::DIRECT_CONTROL;
      }
    }
    // Else, pot hasn't moved from m_physical_pot_at_hold_start, parameter remains held.
    break;

  case TakeoverState::RELATIVELY_ADJUSTING:
    // Parameter value is m_held_parameter_value (which is also m_algo->v[active_param])
    if (m_held_parameter_value == scaled_physical_target)
    {
      m_state = TakeoverState::DIRECT_CONTROL;
      // Value already matches, ensure it's set if pot just moved to this value.
      // The DIRECT_CONTROL case will handle setting if pot moves again.
      setParameter(active_param, scaled_physical_target); // Ensure exact match
    }
    else if (current_physical_value != m_prev_physical_value) // Pot is still moving
    {
      int32_t new_val = m_held_parameter_value;
      if (current_physical_value > m_prev_physical_value)
        new_val++;
      else if (current_physical_value < m_prev_physical_value)
        new_val--;

      if (new_val != m_held_parameter_value)
      {
        setParameter(active_param, new_val);
        m_held_parameter_value = new_val;

        // Check if this relative step met the physical pot's target
        if (new_val == scaled_physical_target)
        {
          m_state = TakeoverState::DIRECT_CONTROL;
        }
      }
    }
    // Else, pot stopped moving but not yet at target, parameter value remains where it was last set.
    break;

  case TakeoverState::DIRECT_CONTROL:
    if (current_physical_value != m_prev_physical_value || m_prev_physical_value == -1.0f) // Pot moved or first update
    {
      setParameter(active_param, scaled_physical_target);
      m_held_parameter_value = scaled_physical_target; // Keep this in sync for potential next HOLD state
    }
    break;
  }

  m_prev_physical_value = current_physical_value;
}