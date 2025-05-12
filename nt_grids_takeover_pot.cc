#include "nt_grids_takeover_pot.h"
#include "nt_grids.h"      // Include for NtGridsAlgorithm definition and extern s_parameters
#include "distingnt/api.h" // Needed for NT_setParameterFromUi, _NT_uiData etc.

// Note: Now includes nt_grids.h instead of nt_grids.cc

// --- TakeoverPot Method Implementations ---
TakeoverPot::TakeoverPot() : m_algo(nullptr),
                             m_pot_index(0),
                             m_pot_button_mask(0),
                             m_primary_param(kParamMode), // Default to avoid uninitialized
                             m_alternate_param(kParamMode),
                             m_has_alternate(false),
                             m_primary_scale(255.0f),
                             m_alternate_scale(255.0f),
                             m_is_controlling_alternate(false),
                             m_takeover_active_primary(false),
                             m_takeover_active_alternate(false),
                             m_primary_takeover_value(0),
                             m_alternate_takeover_value(0),
                             m_prev_physical_value(-1.0f) // Initialize to invalid value
{
}

void TakeoverPot::init(NtGridsAlgorithm *algo, int pot_index)
{
  m_algo = algo;
  m_pot_index = pot_index;
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

void TakeoverPot::syncPhysicalValue(float physical_pot_value)
{
  // Called by setupUi to initialize the pot's physical value and disable takeover
  m_prev_physical_value = physical_pot_value;
  m_takeover_active_primary = false;
  m_takeover_active_alternate = false;
  m_is_controlling_alternate = false;
}

// Private helper to set parameter value, handling clamping and UI update call
void TakeoverPot::setParameter(ParameterIndex param_to_set, int32_t value)
{
  if (!m_algo)
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

  uint32_t alg_idx = NT_algorithmIndex(m_algo);
  uint32_t param_offset = NT_parameterOffset();
  NT_setParameterFromUi(alg_idx, param_to_set + param_offset, value);
}

// Main update logic called from nt_grids_custom_ui
void TakeoverPot::update(const _NT_uiData &data)
{
  if (!m_algo)
    return;

  float current_pot_physical_value = data.pots[m_pot_index];
  bool pot_changed = (data.potChange & (1 << m_pot_index));

  // --- Handle Button Transitions (Press/Release) ---
  // Determine which button press/release affects this pot's alternate state
  // and whether it results in switching TO alternate or FROM alternate.
  bool switchToAlternate = false;
  bool switchFromAlternate = false;
  bool current_alternate_state_active = m_is_controlling_alternate; // Default assumption

  if (m_has_alternate)
  {
    bool is_euclidean_mode = (m_algo->v[kParamMode] == 0);

    if (is_euclidean_mode)
    {
      // Euclidean: Pot R button toggles the global m_euclidean_controls_length state
      // This was handled in nt_grids_custom_ui. Here, we just need to know
      // if the button press *caused* a state change relevant to *this* pot.
      // We determine m_is_controlling_alternate based on the global flag.
      m_is_controlling_alternate = m_algo->m_euclidean_controls_length;

      // If Pot R button was just pressed/released, it toggled the state.
      // We need to activate takeover for the parameter we just switched *to*.
      bool potR_pressed = (data.buttons & kNT_potButtonR) && !(data.lastButtons & kNT_potButtonR);
      bool potR_released = !(data.buttons & kNT_potButtonR) && (data.lastButtons & kNT_potButtonR);

      if (potR_pressed || potR_released)
      { // State just toggled
        // activateTakeover is now handled by resetTakeoverForModeSwitch called from custom_ui
        // So, no explicit takeover logic needed here for Euclidean toggle itself.
      }
      current_alternate_state_active = m_is_controlling_alternate; // Reflect the global state
    }
    else // Drum Mode
    {
      // Drum: Only Pot C (index 1) has alternate (Chaos), controlled by its own button
      if (m_pot_index == 1)
      {
        bool potC_button_current = (data.buttons & kNT_potButtonC);
        bool potC_button_last = (data.lastButtons & kNT_potButtonC);
        switchToAlternate = potC_button_current && !potC_button_last;
        switchFromAlternate = !potC_button_current && potC_button_last;
        current_alternate_state_active = potC_button_current; // Alternate is active while button is held
      }
      // Other pots (L, R) in drum mode have no alternate behavior based on buttons.
      m_is_controlling_alternate = current_alternate_state_active; // Update internal state
    }

    // --- Activate Takeover Based on State Switches (Drum mode Pot C only) ---
    if (switchToAlternate)
    {
      m_takeover_active_primary = false;
      m_alternate_takeover_value = m_algo->v[m_alternate_param];
      m_takeover_active_alternate = true;
      // Check if pot is already at the target value
      int32_t alternate_value_at_pot = static_cast<int32_t>((current_pot_physical_value * m_alternate_scale) + 0.5f);
      if (alternate_value_at_pot == m_alternate_takeover_value)
      {
        m_takeover_active_alternate = false; // Already there
      }
    }
    else if (switchFromAlternate)
    {
      m_takeover_active_alternate = false;
      m_primary_takeover_value = m_algo->v[m_primary_param];
      m_takeover_active_primary = true;
      // Check if pot is already at the target value
      int32_t primary_value_at_pot = static_cast<int32_t>((current_pot_physical_value * m_primary_scale) + 0.5f);
      if (primary_value_at_pot == m_primary_takeover_value)
      {
        m_takeover_active_primary = false; // Already there
      }
    }
    // Note: Euclidean takeover is handled via resetTakeoverForModeSwitch in custom_ui
  }
  else
  {
    // No alternate configured for this pot/mode
    m_is_controlling_alternate = false;
  }

  // --- Handle Pot Movement ---
  if (pot_changed)
  {
    int32_t new_param_value_scaled = 0;
    int32_t prev_param_value_scaled = 0; // Needed for takeover check

    if (m_is_controlling_alternate) // Check the *current* control state
    {                               // Potentiometer controls the alternate parameter
      new_param_value_scaled = static_cast<int32_t>((current_pot_physical_value * m_alternate_scale) + 0.5f);
      if (m_takeover_active_alternate)
      { // Takeover active: check if pot value crosses the stored takeover value
        prev_param_value_scaled = static_cast<int32_t>((m_prev_physical_value * m_alternate_scale) + 0.5f);
        bool crossed_up = (prev_param_value_scaled <= m_alternate_takeover_value && new_param_value_scaled >= m_alternate_takeover_value);
        bool crossed_down = (prev_param_value_scaled >= m_alternate_takeover_value && new_param_value_scaled <= m_alternate_takeover_value);
        if (crossed_up || crossed_down)
        {
          m_takeover_active_alternate = false;
          setParameter(m_alternate_param, new_param_value_scaled);
        }
      }
      else
      { // Takeover not active: directly set the parameter
        setParameter(m_alternate_param, new_param_value_scaled);
      }
    }
    else
    { // Potentiometer controls the primary parameter
      new_param_value_scaled = static_cast<int32_t>((current_pot_physical_value * m_primary_scale) + 0.5f);
      if (m_takeover_active_primary)
      { // Takeover active: check if pot value crosses the stored takeover value
        prev_param_value_scaled = static_cast<int32_t>((m_prev_physical_value * m_primary_scale) + 0.5f);
        bool crossed_up = (prev_param_value_scaled <= m_primary_takeover_value && new_param_value_scaled >= m_primary_takeover_value);
        bool crossed_down = (prev_param_value_scaled >= m_primary_takeover_value && new_param_value_scaled <= m_primary_takeover_value);
        if (crossed_up || crossed_down)
        {
          m_takeover_active_primary = false;
          setParameter(m_primary_param, new_param_value_scaled);
        }
      }
      else
      { // Takeover not active: directly set the parameter
        setParameter(m_primary_param, new_param_value_scaled);
      }
    }
  }

  // Update previous physical value for next cycle's takeover check
  m_prev_physical_value = current_pot_physical_value;
}