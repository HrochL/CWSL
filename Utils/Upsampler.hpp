// David Mittiga
// Alex Ranaldi  W2AXR   alexranaldi@gmail.com


// LICENSE: GNU General Public License v3.0
// THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.

#ifndef UPSAMPLER_HPP
#   define UPSAMPLER_HPP

#include "LowPass.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>


// An efficient power of 2 real upsampler.
template<class PRECISION = double>
class Upsampler {

public:

    // Constructor
    Upsampler(const size_t ratio_log2) :
        ratio(1 << ratio_log2), latency(1 << (ratio_log2 + 3))
    {

        // Check inputs
        if (ratio_log2 < 1)
            throw std::invalid_argument("log2(Ratio) must >= 1");
        if (16 < ratio_log2)
            throw std::invalid_argument("log2(Ratio) must be <= 16");

        // Create the lowpass filter taps
        FiltOrder = latency*2;
        filter = BuildLowPass<PRECISION>(FiltOrder, 1/(double)ratio);

        // Create the workspace
        workspace = new PRECISION[FiltOrder];
        index_max = FiltOrder-1;
        index = 0;

        // Reset the workspace
        Reset();

    }

    // Destructor
    ~Upsampler(void)
    {

        // Free allocated memory
        delete[] filter;
        delete[] workspace;

    }

    // Clears the buffer
    void Reset(void)
    {

        // Clear the buffer and reset the output sample index
        std::fill(workspace, workspace+FiltOrder, static_cast<PRECISION>(0.0));
        index = 0;

    }

    // Consumes one input sample from the provided pointer and
    // generates <ratio> output samples at the provided pointer.
    template<class TYPE = double>
    void Iterate(const TYPE *in, TYPE *out)
    {

        // Update the workspace
        for (size_t n = 0; n < FiltOrder; ++n)
            workspace[(n+index)&index_max] +=
                    static_cast<PRECISION>(in[0])*filter[n];

        // Retrieve the output samples and reset them in the workspace
        for (size_t n = 0; n < ratio; ++n) {
            out[n] = static_cast<TYPE>(workspace[index+n]);
            workspace[index+n] = static_cast<PRECISION>(0.0);
        }

        // Update the output index for next time
        index = (index+ratio) & index_max;

    }

    // Returns the upsample ratio
    inline size_t GetRatio(void) const {return ratio;}
    // Returns the output delay of this filter (at the output rate)
    inline size_t GetDelay(void) const {return latency;}

private:

    // Weighted low-pass filter
    PRECISION *filter;
    // Low-pass filter order
    size_t FiltOrder;
    // Upsample ratio
    const size_t ratio;
    // Output sample latency of upsampler
    const size_t latency;

    // Convolution workspace
    PRECISION *workspace;
    // Index of next output sample
    size_t index;
    // Used to bit-mask instead of modular arithmetic: CPU savings
    size_t index_max;

}; // class Upsampler


#endif // UPSAMPLER_HPP
