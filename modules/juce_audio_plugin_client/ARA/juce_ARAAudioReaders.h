#pragma once

#include "juce_ARAAudioSource.h"

namespace juce
{
    
class ARAAudioSourceReader : public AudioFormatReader,
                             ARAAudioSource::Listener
{
public:
    ARAAudioSourceReader (ARA::PlugIn::AudioSource* audioSource, bool use64BitSamples = false);
    virtual ~ARAAudioSourceReader ();

    void recreate ();
    void invalidate ();

    bool readSamples (
        int** destSamples,
        int numDestChannels,
        int startOffsetInDestBuffer,
        int64 startSampleInFile,
        int numSamples) override;

    // TODO JUCE_ARA
    // do we need to handle property updates?
    // any other invalidation hooks? 
    void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept override;
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept override;
    void willDestroyAudioSource (ARAAudioSource* audioSource) noexcept override;
    void doUpdateAudioSourceContent (ARAAudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;

private:
    std::vector<void*> tmpPtrs;

    // per reader locks means we can create readers while others are reading
    ReadWriteLock lock;

    // When readSamples is not reading all channels,
    // we still need to provide pointers to all channels to the ARA read call.
    // So we'll read the other channels into this dummy buffer.
    std::vector<float> dummyBuffer;
    ARAAudioSource* audioSourceBeingRead; // TODO JUCE_ARA make this const
    std::unique_ptr<ARA::PlugIn::HostAudioReader> araHostReader;
};

//==============================================================================

class ARAPlaybackRegionReader : public AudioFormatReader
{
public:
    ARAPlaybackRegionReader (ARAPlaybackRenderer* playbackRenderer, std::vector<ARAPlaybackRegion*> playbackRegions);
    virtual ~ARAPlaybackRegionReader ();

    bool readSamples (
        int** destSamples,
        int numDestChannels,
        int startOffsetInDestBuffer,
        int64 startSampleInFile,
        int numSamples) override;

protected:
    ARAPlaybackRenderer* playbackRenderer;
    ReadWriteLock lock;
};

//==============================================================================

class ARARegionSequenceReader : public ARAPlaybackRegionReader,
                                ARAPlaybackRegion::Listener
{
public:
    ARARegionSequenceReader (ARAPlaybackRenderer* playbackRenderer, ARARegionSequence* regionSequence);
    virtual ~ARARegionSequenceReader ();

    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties> newProperties) noexcept override;
    void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) noexcept override;

private:
    ARARegionSequence* sequence;
};

} // namespace juce
