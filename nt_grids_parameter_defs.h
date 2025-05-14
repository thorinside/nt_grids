#ifndef NT_GRIDS_PARAMETER_DEFS_H
#define NT_GRIDS_PARAMETER_DEFS_H

// Shared parameter index definitions
enum ParameterIndex
{
  kParamOff, // Represents an invalid or unassigned parameter
  // Mode & Global
  kParamMode,
  kParamChaosEnable,
  kParamChaosAmount,
  // CV Inputs
  kParamClockInput,
  kParamResetInput,
  // Drum Mode Specific
  kParamDrumMapX,
  kParamDrumMapY,
  kParamDrumDensity1,
  kParamDrumDensity2,
  kParamDrumDensity3,
  // Euclidean Mode Specific
  kParamEuclideanControlsLength,
  kParamEuclideanLength1,
  kParamEuclideanLength2,
  kParamEuclideanLength3,
  kParamEuclideanFill1,
  kParamEuclideanFill2,
  kParamEuclideanFill3,
  // Outputs
  kParamOutputTrig1,
  kParamOutputTrig1Mode,
  kParamOutputTrig2,
  kParamOutputTrig2Mode,
  kParamOutputTrig3,
  kParamOutputTrig3Mode,
  kParamOutputAccent,
  kParamOutputAccentMode,
  kNumParameters // Represents the total number of parameters
};

#endif // NT_GRIDS_PARAMETER_DEFS_H