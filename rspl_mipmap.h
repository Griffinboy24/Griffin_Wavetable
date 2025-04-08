/******************************************************************************
    rspl_mipmap.h - Header-only conversion of the MipMapFlt module.
    
    This file combines:
      - MipMapFlt.h          [&#8203;:contentReference[oaicite:0]{index=0}]
      - MipMapFlt.hpp        [&#8203;:contentReference[oaicite:1]{index=1}]
      - MipMapFlt.cpp        [&#8203;:contentReference[oaicite:2]{index=2}]

    Copyright (c) 2003 Laurent de Soras
    Distributed under the GNU Lesser General Public License, version 2.1 or later.
*******************************************************************************/

#ifndef RSPL_MIPMAP_H
#define RSPL_MIPMAP_H

#include "rspl.h"      // Provides basic definitions: def.h, integer types, rspl_FORCEINLINE, min/max, etc.
#include <vector>
#include <cassert>

namespace rspl
{

//---------------------------------------------------------------------------
// MipMapFlt Class Declaration
//---------------------------------------------------------------------------

class MipMapFlt
{
public:
    MipMapFlt();
    ~MipMapFlt() { /* no dynamic memory to free beyond std::vector */ }

    // Initializes the sample.
    //   len            : Full length of the sample (in samples), must be >= 0.
    //   add_len_pre    : Extra data length required before sample (>= 0).
    //   add_len_post   : Extra data length required after sample (>= 0).
    //   nbr_tables     : Number of desired mip-map levels (>= 1).
    //   imp_ptr        : Pointer on FIR impulse data.
    //   nbr_taps       : Number of taps in the FIR filter (must be > 0 and odd).
    // Returns: true if more data are needed to fill the sample.
    inline bool init_sample(long len, long add_len_pre, long add_len_post, int nbr_tables, const double imp_ptr[], int nbr_taps);

    // Supplies a block of sample data. Must be called repeatedly until the entire sample is loaded.
    // Returns: true if more data are needed.
    inline bool fill_sample(const float data_ptr[], long nbr_spl);

    // Clears loaded sample data and releases allocated memory.
    inline void clear_sample();

    // Returns true if the sample has been fully loaded.
    inline bool is_ready() const;

    // Inlined getters from the mip-map (provided in MipMapFlt.hpp originally):
    inline long get_sample_len() const;
    inline long get_lev_len(int level) const;
    inline const int get_nbr_tables() const;
    inline const float * use_table(int table) const;

protected:
    // (No protected members)

private:
    // Nested class to hold table (level) data.
    class TableData
    {
    public:
        typedef std::vector<float> SplData;
        SplData _data;
        float * _data_ptr; // Points into _data at offset add_len_pre.
    };

    typedef TableData::SplData SplData;
    typedef std::vector<TableData> TableArr;

    // Resizes and clears the internal table storage.
    inline void resize_and_clear_tables();

    // Checks if the sample is complete and, if so, builds the mip-map levels.
    inline bool check_sample_and_build_mip_map();

    // Builds one level of the mip-map (level > 0).
    inline void build_mip_map_level(int level);

    // Applies a symmetric FIR filter to produce one interpolated sample.
    inline float filter_sample(const TableData::SplData &table, long pos) const;

    // Data members:
    TableArr _table_arr;    // Array of tables (one per mip-map level)
    SplData  _filter;       // FIR filter coefficients (stored from center to edge)
    long     _len;          // Full sample length; < 0 if not initialized.
    long     _add_len_pre;  // Extra samples required before the actual sample.
    long     _add_len_post; // Extra samples required after the actual sample.
    long     _filled_len;   // Number of samples already supplied.
    int      _nbr_tables;   // Number of mip-map levels.

    // Forbidden member functions:
    MipMapFlt(const MipMapFlt &other);
    MipMapFlt & operator=(const MipMapFlt &other);
    bool operator==(const MipMapFlt &other);
    bool operator!=(const MipMapFlt &other);
};

//---------------------------------------------------------------------------
// MipMapFlt Inline Implementations
//---------------------------------------------------------------------------

inline MipMapFlt::MipMapFlt()
: _table_arr(), _filter(), _len(-1), _add_len_pre(0), _add_len_post(0), _filled_len(0), _nbr_tables(0)
{
    // Constructor does not allocate sample data
}

inline bool MipMapFlt::init_sample(long len, long add_len_pre, long add_len_post, int nbr_tables, const double imp_ptr[], int nbr_taps)
{
    assert(len >= 0);
    assert(add_len_pre >= 0);
    assert(add_len_post >= 0);
    assert(nbr_tables > 0);
    assert(imp_ptr != 0);
    assert(nbr_taps > 0);
    assert((nbr_taps & 1) == 1);  // Must be odd

    // Store the FIR filter coefficients.
    const int half_fir_len = (nbr_taps - 1) / 2;
    _filter.resize(half_fir_len + 1);
    for (int pos = 0; pos <= half_fir_len; ++pos)
    {
        // Taking coefficients from the center (at index half_fir_len) to the end.
        _filter[pos] = static_cast<float>(imp_ptr[half_fir_len + pos]);
    }
    const long filter_sup = static_cast<long>(half_fir_len * 2);

    _len = len;
    // Use the maximum between user-specified additional length and the filter support.
    _add_len_pre  = max(add_len_pre, filter_sup);
    _add_len_post = max(add_len_post, filter_sup);
    _filled_len = 0;
    _nbr_tables = nbr_tables;

    resize_and_clear_tables();

    return check_sample_and_build_mip_map();
}

inline bool MipMapFlt::fill_sample(const float data_ptr[], long nbr_spl)
{
    assert(_len >= 0);
    assert(_nbr_tables > 0);
    assert(!_table_arr.empty());
    assert(data_ptr != 0);
    assert(nbr_spl > 0);
    assert(nbr_spl <= _len - _filled_len);

    TableData::SplData & sample = _table_arr[0]._data;
    const long offset = _add_len_pre + _filled_len;
    const long work_len = min(nbr_spl, _len - _filled_len);

    for (long pos = 0; pos < work_len; ++pos)
    {
        sample[offset + pos] = data_ptr[pos];
    }
    _filled_len += work_len;

    return check_sample_and_build_mip_map();
}

inline void MipMapFlt::clear_sample()
{
    _len = -1;
    _add_len_pre = 0;
    _add_len_post = 0;
    _filled_len = 0;
    _nbr_tables = 0;
    // Clear allocated memory:
    TableArr().swap(_table_arr);
    SplData().swap(_filter);
}

inline bool MipMapFlt::is_ready() const
{
    bool ready_flag = (_len >= 0);
    ready_flag &= (_nbr_tables > 0);
    ready_flag &= (_filled_len == _len);
    return ready_flag;
}

inline void MipMapFlt::resize_and_clear_tables()
{
    _table_arr.resize(_nbr_tables);
    for (int table_cnt = 0; table_cnt < _nbr_tables; ++table_cnt)
    {
        const long lev_len = get_lev_len(table_cnt);
        const long table_len = _add_len_pre + lev_len + _add_len_post;

        TableData & table = _table_arr[table_cnt];
        // Allocate the table data vector and fill with zeros.
        table._data = TableData::SplData(table_len, 0);
        table._data_ptr = &table._data[_add_len_pre];
    }
}

inline bool MipMapFlt::check_sample_and_build_mip_map()
{
    if (_filled_len == _len)
    {
        // Build successive mip-map levels from level 1 onward.
        for (int level = 1; level < _nbr_tables; ++level)
        {
            build_mip_map_level(level);
        }
        // Release the FIR filter as it is no longer needed.
        SplData().swap(_filter);
    }
    return (_filled_len < _len);
}

inline void MipMapFlt::build_mip_map_level(int level)
{
    assert(level > 0);
    assert(level < _nbr_tables);
    assert(!_table_arr.empty());

    TableData::SplData & ref_spl = _table_arr[level - 1]._data;
    TableData::SplData & new_spl = _table_arr[level]._data;
    const long lev_len = get_lev_len(level);

    // Determine the size of residual side data.
    const long filter_half_len = static_cast<long>(_filter.size());
    const long filter_quarter_len = filter_half_len / 2; // integer division
    const long end_pos = lev_len + filter_quarter_len;

    // For each output sample in the new level, apply the filter on the reference level.
    for (long pos = -filter_quarter_len; pos < end_pos; ++pos)
    {
        const long pos_ref = _add_len_pre + pos * 2;
        const float val = filter_sample(ref_spl, pos_ref);

        const long pos_new = _add_len_pre + pos;
        assert(pos_new >= 0);
        assert(pos_new < static_cast<long>(new_spl.size()));
        new_spl[pos_new] = val;
    }
}

inline float MipMapFlt::filter_sample(const TableData::SplData &table, long pos) const
{
    // Apply symmetric FIR filtering.
    const long filter_half_len = static_cast<long>(_filter.size()) - 1;
    assert(pos - filter_half_len >= 0);
    assert(pos + filter_half_len < static_cast<long>(table.size()));

    float sum = table[pos] * _filter[0];
    for (long fir_pos = 1; fir_pos <= filter_half_len; ++fir_pos)
    {
        const float two_spl = table[pos - fir_pos] + table[pos + fir_pos];
        sum += two_spl * _filter[fir_pos];
    }
    return sum;
}

//---------------------------------------------------------------------------
// Inline definitions from MipMapFlt.hpp
//---------------------------------------------------------------------------

inline long MipMapFlt::get_sample_len() const
{
    assert(is_ready());
    return _len;
}

inline const int MipMapFlt::get_nbr_tables() const
{
    assert(is_ready());
    return _nbr_tables;
}

inline long MipMapFlt::get_lev_len(int level) const
{
    assert(_len >= 0);
    assert(level >= 0);
    assert(level < _nbr_tables);
    const long scale = 1L << level;
    // Compute the level's length as the ceiling of (_len / scale)
    const long lev_len = (_len + scale - 1) >> level;
    return lev_len;
}

inline const float * MipMapFlt::use_table(int table) const
{
    assert(is_ready());
    assert(table >= 0);
    assert(table < _nbr_tables);
    return _table_arr[table]._data_ptr;
}

} // namespace rspl

#endif // RSPL_MIPMAP_H
