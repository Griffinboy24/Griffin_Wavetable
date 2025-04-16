#ifndef RSPL_BIG_ARRAYS_H
#define RSPL_BIG_ARRAYS_H

namespace rspl {

	// FIR filter coefficients for single-rate interpolation (InterpPack)
	extern const double FIR_1X_COEF_ARR[];
	extern const int    FIR_1X_COEF_ARR_SIZE;

	// FIR filter coefficients for double-rate interpolation (InterpPack)
	extern const double FIR_2X_COEF_ARR[];
	extern const int    FIR_2X_COEF_ARR_SIZE;

	// Downsampler coefficients (Downsampler2Flt)
	extern const double DOWNSAMPLER_COEF_ARR[];
	extern const int    DOWNSAMPLER_COEF_ARR_SIZE;

	// Mip-map FIR coefficients (MipMapFlt/ResamplerFlt)
	extern const double MIP_MAP_FIR_COEF_ARR[];
	extern const int    MIP_MAP_FIR_COEF_ARR_SIZE;

} // namespace rspl

#endif // RSPL_BIG_ARRAYS_H
