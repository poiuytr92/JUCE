#pragma once

#include "juce_ARAAudioSource.h"

namespace juce
{

// All these readers follow a common pattern of "invalidation":
//
// Whenever the samples they are reading are altered, the readers become invalid and will stop
// accessing the model graph. These alterations are model edits such as property changes,
// content changes (if affecting sample scope), or the deletion of some model object involved
// in the read process.
// Since these edits are performed on the document controller thread, reader validity can immediately
// be checked after the edit has conluded, and any reader that has become invalid can be recreated.
//
// Note that encountering a failure in any individual read call does not invalidate the reader,
// so that the entity using the reader can decide whether to retry or to back out.
// This includes trying to read an audio source for which the host has currently disabled access:
// the failure will be immediately visible, but the reader will remain valid.
// This ensures that for example a realtime renderer can just keep go reading and will be seeing
// proper samples again once sample access is reenabled.
//
// If desired, the code calling readSamples() can also implement proper signaling of any read error
// to the document controller thread to trigger rebuilding the reader as needed.
// This will typically be done when implementing audio source analysis: if there is an error upon
// reading the samples that cannot be resolved within a reasonable timeout, then the anaylsis would
// be aborted. The document controller code that monitors the analysis tasks can evaluate this and
// re-launch a new analysis when appropriate (e.g. when access is re-enabled).

class ARAAudioSourceReader : public AudioFormatReader,
                             ARAAudioSource::Listener
{
public:
    ARAAudioSourceReader (ARAAudioSource* audioSource, bool use64BitSamples = false);
    virtual ~ARAAudioSourceReader();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    bool getIsValid() const { return isValid; }

    // TODO JUCE_ARA
    // do we need to handle property updates?
    // any other invalidation hooks? 
    void willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties) noexcept override;
    void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept override;
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept override;
    void willDestroyAudioSource (ARAAudioSource* audioSource) noexcept override;
    void doUpdateAudioSourceContent (ARAAudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARAContentUpdateScopes scopeFlags) noexcept override;

private:
    bool isValid;
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
    ARAPlaybackRegionReader (ARAPlaybackRenderer* renderer, std::vector<ARAPlaybackRegion*> const& playbackRegions, bool nonRealtime);
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
    bool isNonRealtime;
};

//==============================================================================

class ARARegionSequenceReader : public ARAPlaybackRegionReader,
                                ARARegionSequence::Listener
{
public:
    ARARegionSequenceReader (ARAPlaybackRenderer* playbackRenderer, ARARegionSequence* regionSequence, bool nonRealtime);
    virtual ~ARARegionSequenceReader();

    void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) override;
    void didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) override;
    void willDestroyRegionSequence (ARARegionSequence* regionSequence) override;

private:
    ARARegionSequence* sequence;
};

} // namespace juce
