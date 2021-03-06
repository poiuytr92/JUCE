#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "juce_ARAModelObjects.h"

namespace juce
{

class AudioProcessor;

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

    Plug-Ins typically use this from their rendering code, wrapped in a BufferingAudioReader
    to bridge between realtime rendering and non-realtime audio reading.

    The reader becomes invalidated if
        - the audio source content is updated in a way that affects its samples
        - the audio source sample access is disabled
        - The audio source being read is destroyed

    @tags{ARA}
*/
class JUCE_API  ARAAudioSourceReader  : public AudioFormatReader,
                                        private ARAAudioSource::Listener
{
public:
    /** Use an ARAAudioSource to construct an audio source reader for the given \p audioSource. */
    ARAAudioSourceReader (ARAAudioSource* audioSource);
    virtual ~ARAAudioSourceReader() override;

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    /** Returns true as long as the reader's underlying ARAAudioSource remains accessible and its sample content is not changed. */
    bool isValid() const { return audioSourceBeingRead != nullptr; }
    /** Invalidate the reader - the reader will call this internally if needed, but can also be invalidated from the outside (from message thread only!). */
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

    Plug-Ins typically use this to draw the output of a playback region in their UI.

    In order to read from playback regions, the reader requires an audio processor that
    acts as ARA playback renderer.
    Configuring the audio processor for real-time operation results in the reader being
    real-time capable too, unlike most other AudioFormatReaders.
    The reader instance will take care of adding all regions being read to the renderer
    and invoke its processBlock function in order to read the region samples.

    The reader becomes invalid if
        - any region properties are updated in a way that would affect its samples
        - any region content is updated in a way that would affect its samples
        - any of its regions are destroyed

    @tags{ARA}
*/
class JUCE_API  ARAPlaybackRegionReader   : public AudioFormatReader,
                                            private AudioPlayHead,
                                            private ARAPlaybackRegion::Listener
{
protected:
    ARAPlaybackRegionReader (ARADocumentController* documentController, std::unique_ptr<AudioProcessor> audioProcessor,
                             std::vector<ARAPlaybackRegion*> const& playbackRegions);

public:
    /** Create an ARAPlaybackRegionReader instance to read the given \p playbackRegions
        @param audioProcessor A custom ARA-compatible audio processor used for rendering the \p playbackRegions,
                              pre-configured appropriately for the intended use case (sample rate, output format, realtime, etc.)
                              The reader takes ownership and binds it to the document controller of the \p playbackRegions.
        @param playbackRegions The vector of playback regions that shall be read - must not be empty!
                               All regions must be part of the same ARADocument.
    */
    ARAPlaybackRegionReader (std::unique_ptr<AudioProcessor> audioProcessor, std::vector<ARAPlaybackRegion*> const& playbackRegions);

    virtual ~ARAPlaybackRegionReader() override;

    /** Returns true as long as any of the reader's underlying playback region's haven't changed. */
    bool isValid() const { return (audioProcessor != nullptr); }
    /** Invalidate the reader - this should be called if the sample content of any of the reader's ARAPlaybackRegions changes. */
    void invalidate();

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override;

    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) override;
    void didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags) override;
    void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) override;

    /** The starting point of the reader in playback samples */
    int64 startInSamples { 0 };

protected:
    bool getCurrentPosition (CurrentPositionInfo& result) override;

private:
    std::unique_ptr<AudioProcessor> audioProcessor;
    AudioProcessorARAExtension* audioProcessorAraExtension; // cache of dynamic_cast<AudioProcessorARAExtension*> (audioProcessor)
    int64 renderPosition { 0 };
    static MidiBuffer dummyMidiBuffer;
    ReadWriteLock lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAPlaybackRegionReader)
};

} // namespace juce
