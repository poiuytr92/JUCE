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
    void doUpdateAudioSourceContent (ARAAudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARAContentUpdateScopes scopeFlags) noexcept override;

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

class ARAPlaybackRegionReader : public AudioFormatReader,
                                public ARAPlaybackRegion::Listener
{
public:
    ARAPlaybackRegionReader (ARAPlaybackRenderer* renderer, std::vector<ARAPlaybackRegion*> const& playbackRegions);
    virtual ~ARAPlaybackRegionReader();

    // TODO JUCE_ARA temporary api - do we want to keep that? If so, shouldn't we return some proper range type?
    double getRegionsStartTime() const { return regionsStartTime; }
    double getRegionsEndTime() const  { return regionsEndTime; }

    bool isValid() const { return (playbackRenderer != nullptr); }
    void invalidate();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) override;
    void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) noexcept override;

private:
    std::unique_ptr<ARAPlaybackRenderer> playbackRenderer;
    ReadWriteLock lock;
    double regionsStartTime;
    double regionsEndTime;
};

//==============================================================================

class ARARegionSequenceReader : public ARAPlaybackRegionReader,
                                ARARegionSequence::Listener
{
public:
    ARARegionSequenceReader (ARAPlaybackRenderer* playbackRenderer, ARARegionSequence* regionSequence);
    virtual ~ARARegionSequenceReader();

    void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) override;
    void didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) override;
    void willDestroyRegionSequence (ARARegionSequence* regionSequence) override;

private:
    ARARegionSequence* sequence;
};

} // namespace juce
