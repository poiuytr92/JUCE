#pragma once

#include "juce_ARAModelObjects.h"

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
//
// When reading playback regions (directly or through a region sequnece reader), the reader will
// represent the regions as a single source object that covers the union of all affected regions.
// The first sample produced by the reader thus will be the first sample of the earliest region.
// This means that the location of this region has to be taken into account by the calling code if
// it wants to relate the samples to the model or any other reader output.
//==============================================================================
/**
    Subclass of AudioFormatReader that reads samples from a single ARA audio source. 

    The reader becomes invalidated if 
        - the audio source content is updated in a way that affects its samples
        - the audio source sample access is disabled
        - The audio source being read is destroyed

    @tags{ARA}
*/
class ARAAudioSourceReader  : public AudioFormatReader,
                              private ARAAudioSource::Listener
{
public:
    /** Use an ARAAudioSource to construct an audio source reader that reads either 32 or 64 bit samples. */
    ARAAudioSourceReader (ARAAudioSource* audioSource, bool use64BitSamples = false);
    virtual ~ARAAudioSourceReader();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    /** Returns true as long as the reader's underlying ARAAudioSource remains accessible and its sample content is not changed. */
    bool isValid() const { return audioSourceBeingRead != nullptr; }
    /** Invalidate the reader - the reader will call this internally if needed, but can also be invalidated from the outside. */
    void invalidate();

    void willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties) override;
    void doUpdateAudioSourceContent (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags) override;
    void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) override;
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) override;
    void willDestroyAudioSource (ARAAudioSource* audioSource) override;

private:
    ARAAudioSource* audioSourceBeingRead;
    std::unique_ptr<ARA::PlugIn::HostAudioReader> araHostReader;
    ReadWriteLock lock;
    std::vector<void*> tmpPtrs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAAudioSourceReader)
};

//==============================================================================
/**
    Subclass of AudioFormatReader that reads samples from a group of playback regions. 

    In order to read from playback regions, the reader requires a playback renderer. 
    The reader instance will take care of adding all regions being read to the renderer
    and invoke its processBlock function in order to read the region samples. 

    The reader becomes invalid if 
        - any region properties are updated in a way that would affect its samples
        - any region content is updated in a way that would affect its samples
        - any of its regions are destroyed

    @tags{ARA}
*/
class ARAPlaybackRegionReader  : public AudioFormatReader,
                                 private ARAPlaybackRegion::Listener
{
public:
    /** Use a std::vector<ARAPlaybackRegion*> to construct a playback region reader. 
        @param playbackRenderer The ARAPlaybackRenderer instance through which the region samples will be rendered
        @param playbackRegions The vector of playback regions that can be read by the reader
        @param nonRealtime Whether or not the samples need to be read in real time (the \p playbackRenderer will be configured appropriately)
        @param playbackSampleRate The sample rate at which \p playbackRenderer should render playback regions (the first region's audio source sample rate will be used if left 0.0)
        @param channelCount The channel count of the playback region data (the first region's audio source channel count will be used if left 0)
        @param use64BitSamples A flag indicating whether or not the playback region samples should be read in 64 or 32 bit floating point
    */
    ARAPlaybackRegionReader (ARAPlaybackRenderer* playbackRenderer, std::vector<ARAPlaybackRegion*> const& playbackRegions, bool nonRealtime,
                             double playbackSampleRate = 0.0, int channelCount = 0, bool use64BitSamples = false);
    virtual ~ARAPlaybackRegionReader();

    /** Returns true if any of the reader's underlying playback region's have been invalidated. */
    bool isValid() const { return (playbackRenderer != nullptr); }
    /** Invalidate the reader - this should be called if the sample content of any of the reader's ARAPlaybackRegions changes. */
    void invalidate();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) override;
    void didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags) override;
    void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) override;

    /** The starting point of the reader in playback samples */
    int64 startInSamples = 0;
    /** Whether or not the underlying ARAPlaybackRenderer has been configured for real-time playback */
    bool isNonRealtime = false;

private:
    std::unique_ptr<ARAPlaybackRenderer> playbackRenderer;
    ReadWriteLock lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAPlaybackRegionReader)
};

//==============================================================================
/**
    Subclass of ARAPlaybackRegionReader that reads all playback regions in a region sequence. 

    Like the ARAPlaybackRegionReader this class uses a playback renderer instance to read
    playback region samples. 

    In addition to the reasons that an ARAPlaybackRegionReader would become invalidated, 
    this reader invalidates if 
        - any playback regions are added or removed from the sequence
        - the region sequence is destroyed

    @tags{ARA}
*/
class ARARegionSequenceReader  : public ARAPlaybackRegionReader,
                                 private ARARegionSequence::Listener
{
public:
    /** Use an ARARegionSequence to construct a region sequence reader. 
        @param playbackRenderer The ARAPlaybackRenderer instance through which all region sequence samples will be rendered
        @param regionSequence The region sequence being read - all playback regions on this sequence will be read
        @param nonRealtime Whether or not the samples need to be read in real time (the \p playbackRenderer will be configured appropriately)
        @param playbackSampleRate The sample rate at which \p playbackRenderer should render playback regions (will use the first region's audio source sample rate if left 0.0)
        @param channelCount The channel count of the playback region data (will use the first region's audio source channel count if left 0)
        @param use64BitSamples The precision of the floating point sample data generated by the reader (assumed to be 32 bit)
    */
    ARARegionSequenceReader (ARAPlaybackRenderer* playbackRenderer, ARARegionSequence* regionSequence, bool nonRealtime,
                             double playbackSampleRate = 0.0, int channelCount = 0, bool use64BitSamples = false);
    virtual ~ARARegionSequenceReader();

    void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) override;
    void didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) override;
    void willDestroyRegionSequence (ARARegionSequence* regionSequence) override;

private:
    ARARegionSequence* sequence;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARARegionSequenceReader)
};

} // namespace juce
