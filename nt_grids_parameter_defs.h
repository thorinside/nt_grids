#ifndef NT_GRIDS_PARAMETER_DEFS_H
#define NT_GRIDS_PARAMETER_DEFS_H

// Shared parameter index definitions
enum ParameterIndex
{
  // Mode & Global
  kParamMode,
  kParamChaosEnable,
  kParamChaosAmount,
  // Drum Mode Specific
  kParamDrumMapX,
  kParamDrumMapY,
  kParamDrumDensity1,
  kParamDrumDensity2,
  kParamDrumDensity3,
  // Euclidean Mode Specific
  kParamEuclideanControlsLength,
  kParamEuclideanLength1,
  kParamEuclideanFill1,
  kParamEuclideanShift1,
  kParamEuclideanLength2,
  kParamEuclideanFill2,
  kParamEuclideanShift2,
  kParamEuclideanLength3,
  kParamEuclideanFill3,
  kParamEuclideanShift3,
  // CV Inputs / Routing
  kParamClockInput,
  kParamResetInput,
  // Outputs / Routing
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