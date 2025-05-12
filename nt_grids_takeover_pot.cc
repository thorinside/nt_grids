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

  // --- Handle Button Transitions & State ---
  bool switchToAlternate = false;
  bool switchFromAlternate = false;
  bool current_alternate_state_active = false; // Is the alternate param controlled *right now*?

  bool is_drum_mode = (m_algo->v[kParamMode] == 1);

  // --- Determine current control state and handle button transitions ONLY for Drum/PotR ---
  if (is_drum_mode && m_has_alternate && m_pot_index == 2) // Drum mode, Pot R, Alternate configured (Chaos)
  {
    bool potR_button_current = (data.buttons & kNT_potButtonR);
    bool potR_button_last = (data.lastButtons & kNT_potButtonR);
    switchToAlternate = potR_button_current && !potR_button_last;   // Just pressed
    switchFromAlternate = !potR_button_current && potR_button_last; // Just released
    current_alternate_state_active = potR_button_current;           // Control alternate WHILE held

    // --- Activate Takeover Based on State Switches (Drum mode Pot R only) ---
    if (switchToAlternate)
    {
      m_takeover_active_primary = false;                         // Stop takeover for primary
      m_alternate_takeover_value = m_algo->v[m_alternate_param]; // Store current alternate value
      m_takeover_active_alternate = true;                        // Start takeover for alternate
      // Optional check if pot is already at the target value
      int32_t alternate_value_at_pot = static_cast<int32_t>((current_pot_physical_value * m_alternate_scale) + 0.5f);
      if (alternate_value_at_pot == m_alternate_takeover_value)
      {
        m_takeover_active_alternate = false; // Already there
      }
    }
    else if (switchFromAlternate)
    {
      m_takeover_active_alternate = false;                   // Stop takeover for alternate
      m_primary_takeover_value = m_algo->v[m_primary_param]; // Store current primary value
      m_takeover_active_primary = true;                      // Start takeover for primary
      // Optional check if pot is already at the target value
      int32_t primary_value_at_pot = static_cast<int32_t>((current_pot_physical_value * m_primary_scale) + 0.5f);
      if (primary_value_at_pot == m_primary_takeover_value)
      {
        m_takeover_active_primary = false; // Already there
      }
    }

    // Update internal state reflecting current control target for Drum/PotR
    m_is_controlling_alternate = current_alternate_state_active;
  }
  else
  {
    // Not Drum/PotR: No button-hold alternate logic applies here.
    // Euclidean toggling sets the *primary* parameter and activates takeover via resetTakeoverForNewPrimary.
    // Therefore, the pot should always be considered controlling the primary.
    m_is_controlling_alternate = false;
    // Ensure alternate takeover isn't spuriously active if mode switched while button held etc.
    // Although resetTakeoverForModeSwitch should handle this.
    // m_takeover_active_alternate = false; // Probably safe to ensure this
  }

  // --- Handle Pot Movement ---
  if (pot_changed)
  {
    int32_t new_param_value_scaled = 0;
    int32_t prev_param_value_scaled = 0; // Needed for takeover check if takeover is active

    // Check m_is_controlling_alternate which reflects the *current* target parameter (only true for Drum/PotR-Hold)
    if (m_is_controlling_alternate) // Controlling Alternate (Drum mode, Pot R held, controlling Chaos)
    {
      // WHILE the button is held, the pot DIRECTLY controls the alternate parameter.
      // The alternate takeover state is only used to trigger primary takeover upon release.
      new_param_value_scaled = static_cast<int32_t>((current_pot_physical_value * m_alternate_scale) + 0.5f);
      setParameter(m_alternate_param, new_param_value_scaled);
    }
    else // Controlling Primary (All pots in Euclid, Pots L/C in Drum, Pot R in Drum when button not held)
    {
      new_param_value_scaled = static_cast<int32_t>((current_pot_physical_value * m_primary_scale) + 0.5f);

      if (m_takeover_active_primary) // Takeover active for primary?
      {
        // Check if pot value crosses the stored takeover value
        prev_param_value_scaled = static_cast<int32_t>((m_prev_physical_value * m_primary_scale) + 0.5f);
        bool crossed_up = (prev_param_value_scaled <= m_primary_takeover_value && new_param_value_scaled >= m_primary_takeover_value);
        bool crossed_down = (prev_param_value_scaled >= m_primary_takeover_value && new_param_value_scaled <= m_primary_takeover_value);
        if (crossed_up || crossed_down)
        {
          m_takeover_active_primary = false; // Takeover complete
          setParameter(m_primary_param, new_param_value_scaled);
        }
        // else: Pot moved but hasn't reached takeover value yet, do nothing
      }
      else
      { // Takeover not active for primary: directly set the primary parameter
        setParameter(m_primary_param, new_param_value_scaled);
      }
    }
  }

  // Update previous physical value for next cycle's takeover check
  m_prev_physical_value = current_pot_physical_value;
}