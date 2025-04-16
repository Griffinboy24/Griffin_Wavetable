/******************************************************************************
    rspl_downsampler2flt.h - Header-only conversion of the Downsampler2Flt module.
    
    This file combines:
      - Downsampler2Flt.cpp  [&#8203;:contentReference[oaicite:0]{index=0}]
      - Downsampler2Flt.h    [&#8203;:contentReference[oaicite:1]{index=1}]
      - Downsampler2Flt.hpp  [&#8203;:contentReference[oaicite:2]{index=2}]

    Copyright (c) 2003 Laurent de Soras
    Distributed under the GNU Lesser General Public License, version 2.1 or later.
*******************************************************************************/

#ifndef RSPL_DOWNSAMPLER2FLT_H
#define RSPL_DOWNSAMPLER2FLT_H


#include <cassert>

namespace rspl
{

class Downsampler2Flt
{
public:
    enum { NBR_COEFS = 7 };

    Downsampler2Flt();
    ~Downsampler2Flt() {}

    // Set the filter coefficients. The pointer must refer to an array of NBR_COEFS doubles.
    void set_coefs(const double coef_ptr[NBR_COEFS]);

    // Clear the internal state buffers.
    void clear_buffers();

    // Downsample a block of samples (input sample count must be 2*nbr_spl).
    void downsample_block(float dest_ptr[], const float src_ptr[], long nbr_spl);

    // Adjust the phase of a signal by inserting zeros between samples.
    void phase_block(float dest_ptr[], const float src_ptr[], long nbr_spl);

private:
    // Magic constant to verify that the coefficients have been set.
    enum { CHK_COEFS_NOT_SET = 12345 };

    // Process a pair of samples and return the downsampled output.
    rspl_FORCEINLINE float process_sample(float path_0, float path_1);

    float _coef_arr[NBR_COEFS];
    float _x_arr[2];
    float _y_arr[NBR_COEFS];

private:
    // Forbidden member functions
    Downsampler2Flt(const Downsampler2Flt &other);
    Downsampler2Flt &operator=(const Downsampler2Flt &other);
    bool operator==(const Downsampler2Flt &other);
    bool operator!=(const Downsampler2Flt &other);
};

//---------------------------------------------------------------------------
// Inline implementations
//---------------------------------------------------------------------------

inline Downsampler2Flt::Downsampler2Flt()
: _coef_arr(), _x_arr(), _y_arr()
{
    _coef_arr[0] = static_cast<float>(CHK_COEFS_NOT_SET);
    clear_buffers();
}

inline void Downsampler2Flt::set_coefs(const double coef_ptr[NBR_COEFS])
{
    assert(coef_ptr != 0);
    for (int mem = 0; mem < NBR_COEFS; ++mem)
    {
        float coef = static_cast<float>(coef_ptr[mem]);
        assert(coef > 0);
        assert(coef < 1);
        _coef_arr[mem] = coef;
    }
}

inline void Downsampler2Flt::clear_buffers()
{
    _x_arr[0] = 0;
    _x_arr[1] = 0;
    for (int mem = 0; mem < NBR_COEFS; ++mem)
    {
        _y_arr[mem] = 0;
    }
}

inline void Downsampler2Flt::downsample_block(float dest_ptr[], const float src_ptr[], long nbr_spl)
{
    assert(_coef_arr[0] != static_cast<float>(CHK_COEFS_NOT_SET));
    assert(dest_ptr != 0);
    assert(src_ptr != 0);
    assert(nbr_spl > 0);

    long pos = 0;
    do
    {
        const float path_0 = src_ptr[pos * 2 + 1];
        const float path_1 = src_ptr[pos * 2    ];
        dest_ptr[pos] = process_sample(path_0, path_1);
        ++pos;
    }
    while (pos < nbr_spl);
}

inline void Downsampler2Flt::phase_block(float dest_ptr[], const float src_ptr[], long nbr_spl)
{
    assert(_coef_arr[0] != static_cast<float>(CHK_COEFS_NOT_SET));
    assert(dest_ptr != 0);
    assert(src_ptr != 0);
    assert(nbr_spl > 0);

    long pos = 0;
    do
    {
        float path_1 = src_ptr[pos];
        dest_ptr[pos] = process_sample(0.0f, path_1);
        ++pos;
    }
    while (pos < nbr_spl);

    // Adjust for potential denormals on path 0:
    _y_arr[0] += ANTI_DENORMAL_FLT;
    _y_arr[2] += ANTI_DENORMAL_FLT;
    _y_arr[4] += ANTI_DENORMAL_FLT;
    _y_arr[6] += ANTI_DENORMAL_FLT;

    _y_arr[0] -= ANTI_DENORMAL_FLT;
    _y_arr[2] -= ANTI_DENORMAL_FLT;
    _y_arr[4] -= ANTI_DENORMAL_FLT;
    _y_arr[6] -= ANTI_DENORMAL_FLT;
}

rspl_FORCEINLINE float Downsampler2Flt::process_sample(float path_0, float path_1)
{
    float tmp_0 = _x_arr[0];
    float tmp_1 = _x_arr[1];
    _x_arr[0] = path_0;
    _x_arr[1] = path_1;

    path_0 = (path_0 - _y_arr[0]) * _coef_arr[0] + tmp_0;
    path_1 = (path_1 - _y_arr[1]) * _coef_arr[1] + tmp_1;
    tmp_0 = _y_arr[0];
    tmp_1 = _y_arr[1];
    _y_arr[0] = path_0;
    _y_arr[1] = path_1;

    path_0 = (path_0 - _y_arr[2]) * _coef_arr[2] + tmp_0;
    path_1 = (path_1 - _y_arr[3]) * _coef_arr[3] + tmp_1;
    tmp_0 = _y_arr[2];
    tmp_1 = _y_arr[3];
    _y_arr[2] = path_0;
    _y_arr[3] = path_1;

    path_0 = (path_0 - _y_arr[4]) * _coef_arr[4] + tmp_0;
    path_1 = (path_1 - _y_arr[5]) * _coef_arr[5] + tmp_1;
    tmp_0 = _y_arr[4];
    _y_arr[4] = path_0;
    _y_arr[5] = path_1;

    path_0 = (path_0 - _y_arr[6]) * _coef_arr[6] + tmp_0;
    _y_arr[6] = path_0;
    assert(NBR_COEFS == 7);

    return (path_0 + path_1);
}

} // namespace rspl

#endif // RSPL_DOWNSAMPLER2FLT_H
