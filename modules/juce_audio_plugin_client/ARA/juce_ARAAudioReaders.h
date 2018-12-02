#pragma once

#include "juce_ARAAudioSource.h"

namespace juce
{
    
class ARAAudioSourceReader : public AudioFormatReader,
                             ARAAudioSource::Listener
{
public:
    ARAAudioSourceReader (ARAAudioSource* audioSource, bool use64BitSamples = false);
    virtual ~ARAAudioSourceReader();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    // TODO JUCE_ARA
    // do we need to handle property updates?
    // any other invalidation hooks? 
    void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept override;
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept override;
    void willDestroyAudioSource (ARAAudioSource* audioSource) noexcept override;
    void doUpdateAudioSourceContent (ARAAudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;

private:
    void recreate();
    void invalidate();

private:
    ARAAudioSource* audioSourceBeingRead;
    std::unique_ptr<ARA::PlugIn::HostAudioReader> araHostReader;

    // per reader locks means we can create readers while others are reading
    ReadWriteLock lock;

    std::vector<void*> tmpPtrs;
};

//==============================================================================

class ARAPlaybackRegionReader : public AudioFormatReader
{
public:
    ARAPlaybackRegionReader (ARAPlaybackRenderer* playbackRenderer, std::vector<ARAPlaybackRegion*> const& playbackRegions);
    virtual ~ARAPlaybackRegionReader();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

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
    virtual ~ARARegionSequenceReader();

    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) noexcept override;
    void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) noexcept override;

private:
    ARARegionSequence* sequence;
};

} // namespace juce
