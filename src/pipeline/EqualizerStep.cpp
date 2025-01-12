//=========================================================================
// Name:            EqualizerStep.cpp
// Purpose:         Describes an equalizer step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================


#include "EqualizerStep.h"

#include "../sox_biquad.h"
#include <assert.h>

// TBD -- use std::mutex instead of wxMutex to remove wxWidgets dependency.
#include <wx/wx.h>
extern wxMutex g_mutexProtectingCallbackData;

EqualizerStep::EqualizerStep(int sampleRate, bool* enableFilter, void** bassFilter, void** midFilter, void** trebleFilter, void** volFilter)
    : sampleRate_(sampleRate)
    , enableFilter_(enableFilter)
    , bassFilter_(bassFilter)
    , midFilter_(midFilter)
    , trebleFilter_(trebleFilter)
    , volFilter_(volFilter)
{
    // empty
}

EqualizerStep::~EqualizerStep()
{
    // empty
}

int EqualizerStep::getInputSampleRate() const
{
    return sampleRate_;
}

int EqualizerStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> EqualizerStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    short* outputSamples = new short[numInputSamples];
    assert(outputSamples != nullptr);
    
    memcpy(outputSamples, inputSamples.get(), sizeof(short)*numInputSamples);
    
    g_mutexProtectingCallbackData.Lock();
    if (*enableFilter_)
    {
        sox_biquad_filter(*bassFilter_, outputSamples, outputSamples, numInputSamples);
        sox_biquad_filter(*trebleFilter_, outputSamples, outputSamples, numInputSamples);
        sox_biquad_filter(*midFilter_, outputSamples, outputSamples, numInputSamples);
        if (*volFilter_)
        {
            sox_biquad_filter(*volFilter_, outputSamples, outputSamples, numInputSamples);
        }
    }
    g_mutexProtectingCallbackData.Unlock();
    
    *numOutputSamples = numInputSamples;
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}