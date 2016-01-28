//
//  Biquad.h
//
//  Created by Nigel Redmon on 11/24/12
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the Biquad code:
//  http://www.earlevel.com/main/2012/11/25/biquad-c-source-code/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code
//  for your own purposes, free or commercial.
//

#ifndef Biquad_h
#define Biquad_h

#include <vector>

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

enum BiquadType {
    bq_type_lowpass = 0,
    bq_type_highpass,
    bq_type_bandpass,
    bq_type_notch,
    bq_type_peak,
    bq_type_lowshelf,
    bq_type_highshelf
};

// BK Notes
// --------
// Coeffs ai, bi are swapped notationally relative to most sources,
// but we leave that as it is because it functions correctly and it
// matches original author's discussion.
//
// Fc is a normalized cutoff frequency. E.g., for sample rate 20kHz
// and desired cutoff 300Hz, set Fc = 300/20000.
//
// The bandpass option doesn't allow separate low/high edge specs.,
// so instead, run highpass followed by lowpass. This is tested and
// works correctly.
//
class Biquad
{
private:
    std::vector<double> vz1, vz2;
    double  z1, z2;
    double  a0, a1, a2, b1, b2;
    double  Fc, Q, G;
    int     type;

public:
    Biquad();
    Biquad(
        int     type,
        double  Fc,
        double  Q = 0,
        double  peakGainDB = 0 );

    void setType( int type );
    void setQ( double Q );
    void setFc( double Fc );
    void setPeakGain( double peakGainDB );
    void setBiquad(
        int     type,
        double  Fc,
        double  Q = 0,
        double  peakGainDB = 0 );

    float process( float in );

    void clearMem()  {vz1.clear(); vz2.clear();}

    // Apply filter in-place to (ntpts) worth of data, starting at
    // address (data). (nchans) includes (neural + aux) channels,
    // so is the array stride between timepoints. Filter will only
    // be applied to channel range [c0,cLim). Class retains state
    // data for each channel in the filtered range between calls.
    void applyBlockwiseMem(
        short   *data,
        int     ntpts,
        int     nchans,
        int     c0,
        int     cLim );

    // Apply filter in-place to (ntpts) worth of data, starting at
    // address (data). (nchans) includes (neural + aux) channels,
    // so is the array stride between timepoints. Filter will only
    // be applied to channel (ichan). Class retains state data for
    // all of the (nchans) between calls.
    void apply1BlockwiseMemAll(
        short   *data,
        int     ntpts,
        int     nchans,
        int     ichan );

    // Apply filter in-place to (ntpts) worth of data, starting at
    // address (data). (nchans) includes (neural + aux) channels,
    // so is the array stride between timepoints. Filter will only
    // be applied to channel (ichan). Class retains state data for
    // ichan between calls.
    void apply1BlockwiseMem1(
        short   *data,
        int     ntpts,
        int     nchans,
        int     ichan );

    // Apply filter in-place to (ntpts) worth of data, starting at
    // address (data). (nchans) includes (neural + aux) channels,
    // so is the array stride between timepoints. Filter will only
    // be applied to channel (ichan). No state data are retained
    // between calls.
    void apply1BlockwiseNoMem(
        short   *data,
        int     ntpts,
        int     nchans,
        int     ichan );

private:
    void calcBiquad();
};

inline float Biquad::process( float in ) {
    double  out = in * a0 + z1;
    z1 = in * a1 + z2 - b1 * out;
    z2 = in * a2 - b2 * out;
    return (float)out;
}

#endif  // Biquad_h

