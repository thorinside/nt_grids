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
                             m_takeover_active_primary(false),
                             m_takeover_active_alternate(false),
                             m_primary_takeover_value(0),
                             m_alternate_takeover_value(0),
                             m_prev_physical_value(-1.0f),
                             m_physical_value(0.0f),
                             m_state(TakeoverState::INACTIVE),
                             m_cached_primary_value(-1),
                             m_cached_alternate_value(-1)
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
  m_takeover_active_primary = false;
  m_takeover_active_alternate = false;
  m_primary_takeover_value = 0;
  m_alternate_takeover_value = 0;
  m_prev_physical_value = -1.0f; // Mark as uninitialized
  m_cached_primary_value = -1;
  m_cached_alternate_value = -1;
  m_state = TakeoverState::INACTIVE;
}

void TakeoverPot::configure(ParameterIndex p, ParameterIndex a, bool h, float ps, float as)
{
  m_primary_param = p;
  m_alternate_param = a;
  m_has_alternate = h;
  m_primary_scale = (ps > 0) ? ps : 1.0f; // Avoid division by zero later
  m_alternate_scale = (as > 0) ? as : 1.0f;
}

void TakeoverPot::resetTakeoverForModeSwitch(int16_t new_primary_param_value)
{
  // When mode switches, assume the pot controls the primary parameter for the new mode
  m_is_controlling_alternate = false;
  m_takeover_active_alternate = false; // Disable alternate takeover
  m_alternate_takeover_value = 0;

  // Activate takeover for the primary parameter, using its current value
  m_primary_takeover_value = new_primary_param_value;
  m_takeover_active_primary = true;
}

void TakeoverPot::resetTakeoverForNewPrimary(int16_t new_primary_param_value)
{
  // Called when Euclidean mode toggles between Length and Fill control.
  // The pot now controls a new primary parameter. Activate takeover for it.
  m_is_controlling_alternate = false; // Pot is now controlling the new primary
  m_takeover_active_alternate = false;
  m_alternate_takeover_value = 0;

  m_primary_takeover_value = new_primary_param_value;
  m_takeover_active_primary = true;
  // Optional: Add check here to see if physical pot is already near the new value and disable takeover if so.
}

void TakeoverPot::syncPhysicalValue(float physical_pot_value)
{
  // Called by setupUi to initialize the pot's physical value and initiate takeover
  m_prev_physical_value = physical_pot_value;

  // Determine which parameter the pot is initially focused on.
  // m_is_controlling_alternate should ideally be determined by the current UI state
  // (e.g. if a button is held that would activate alternate mode).
  // For now, assume primary unless explicitly set otherwise by button logic before UI draw.
  // If Drum/PotR button is held on UI entry, m_is_controlling_alternate might be true.
  // The button handling logic in update() will manage m_is_controlling_alternate subsequently.

  if (m_is_controlling_alternate && m_has_alternate)
  {
    m_alternate_takeover_value = m_algo->v[m_alternate_param];
    m_takeover_active_alternate = true;
    m_takeover_active_primary = false;
  }
  else
  {
    m_primary_takeover_value = m_algo->v[m_primary_param];
    m_takeover_active_primary = true;
    m_takeover_active_alternate = false;
  }
  // m_state remains INACTIVE, it seems unused in the current active logic paths
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
  // bool button_pressed_now = (data.buttons & m_pot_button_mask); // Not directly used here, but button logic below sets m_is_controlling_alternate

  // --- Handle Button Transitions & State ---
  // This section determines m_is_controlling_alternate and sets up m_takeover_active_X flags
  // for Drum Mode Pot R (Chaos).
  bool is_drum_mode = (m_algo->v[kParamMode] == 1);

  if (is_drum_mode && m_has_alternate && m_pot_index == 2) // Drum mode, Pot R, Alternate configured (Chaos)
  {
    bool potR_button_current = (data.buttons & kNT_potButtonR);
    bool potR_button_last = (data.lastButtons & kNT_potButtonR);
    bool switchToAlternate = potR_button_current && !potR_button_last;   // Just pressed
    bool switchFromAlternate = !potR_button_current && potR_button_last; // Just released

    m_is_controlling_alternate = potR_button_current; // Control alternate WHILE HELD

    if (switchToAlternate)
    {
      m_takeover_active_primary = false;
      m_alternate_takeover_value = m_algo->v[m_alternate_param];
      m_takeover_active_alternate = true;
      // Optional: If pot is already at target, deactivate takeover immediately
      // int32_t alternate_value_at_pot = static_cast<int32_t>((current_physical_value * m_alternate_scale) + 0.5f);
      // if (alternate_value_at_pot == m_alternate_takeover_value) { m_takeover_active_alternate = false; }
    }
    else if (switchFromAlternate)
    {
      m_takeover_active_alternate = false;
      m_primary_takeover_value = m_algo->v[m_primary_param];
      m_takeover_active_primary = true;
      // Optional: If pot is already at target, deactivate takeover immediately
      // int32_t primary_value_at_pot = static_cast<int32_t>((current_physical_value * m_primary_scale) + 0.5f);
      // if (primary_value_at_pot == m_primary_takeover_value) { m_takeover_active_primary = false; }
    }
    // If no switch, m_is_controlling_alternate reflects current button state.
    // Takeover flags (m_takeover_active_X) persist from the switch or from syncPhysicalValue.
  }
  else
  {
    // Not Drum/PotR with alternate: Pot controls its primary parameter.
    // m_is_controlling_alternate should be false here.
    // resetTakeoverForModeSwitch or resetTakeoverForNewPrimary would have set m_takeover_active_primary = true.
    // syncPhysicalValue also sets m_takeover_active_primary = true if not controlling alternate.
    m_is_controlling_alternate = false;
  }

  // --- Handle Pot Movement ---
  // Only proceed if pot has moved, or if it's the first update (m_prev_physical_value == -1.0f)
  if (current_physical_value == m_prev_physical_value && m_prev_physical_value != -1.0f)
  {
    // Pot hasn't moved, nothing to do regarding value setting
    // m_prev_physical_value is updated at the end
  }
  else
  {
    if (m_is_controlling_alternate && m_has_alternate)
    { // Pot is currently set to control the ALTERNATE parameter
      int32_t new_scaled_value = static_cast<int32_t>((current_physical_value * m_alternate_scale) + 0.5f);
      if (m_takeover_active_alternate)
      {
        int32_t prev_scaled_value = static_cast<int32_t>((m_prev_physical_value * m_alternate_scale) + 0.5f);
        bool crossed_up = (m_prev_physical_value != -1.0f && prev_scaled_value <= m_alternate_takeover_value && new_scaled_value >= m_alternate_takeover_value);
        bool crossed_down = (m_prev_physical_value != -1.0f && prev_scaled_value >= m_alternate_takeover_value && new_scaled_value <= m_alternate_takeover_value);

        if (crossed_up || crossed_down || m_prev_physical_value == -1.0f) // Takeover condition met or first update
        {
          m_takeover_active_alternate = false; // Takeover complete for this interaction
          setParameter(m_alternate_param, new_scaled_value);
        }
        // else: Pot moved but hasn't reached takeover value yet, do nothing to the parameter
      }
      else
      { // Takeover NOT active for alternate: directly set the alternate parameter
        setParameter(m_alternate_param, new_scaled_value);
      }
    }
    else
    { // Pot is currently set to control the PRIMARY parameter
      int32_t new_scaled_value = static_cast<int32_t>((current_physical_value * m_primary_scale) + 0.5f);
      if (m_takeover_active_primary)
      {
        int32_t prev_scaled_value = static_cast<int32_t>((m_prev_physical_value * m_primary_scale) + 0.5f);
        bool crossed_up = (m_prev_physical_value != -1.0f && prev_scaled_value <= m_primary_takeover_value && new_scaled_value >= m_primary_takeover_value);
        bool crossed_down = (m_prev_physical_value != -1.0f && prev_scaled_value >= m_primary_takeover_value && new_scaled_value <= m_primary_takeover_value);

        if (crossed_up || crossed_down || m_prev_physical_value == -1.0f) // Takeover condition met or first update
        {
          m_takeover_active_primary = false; // Takeover complete for this interaction
          setParameter(m_primary_param, new_scaled_value);
        }
        // else: Pot moved but hasn't reached takeover value yet, do nothing to the parameter
      }
      else
      { // Takeover NOT active for primary: directly set the primary parameter
        setParameter(m_primary_param, new_scaled_value);
      }
    }
  }

  // Update previous physical value for next cycle's takeover check
  m_prev_physical_value = current_physical_value;
}