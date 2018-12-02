#include "juce_ARAAudioReaders.h"

namespace juce
{

ARAAudioSourceReader::ARAAudioSourceReader (ARA::PlugIn::AudioSource* source, bool use64BitSamples)
    : AudioFormatReader (nullptr, "ARAAudioSourceReader")
{
    bitsPerSample = use64BitSamples ? 64 : 32;
    usesFloatingPointData = true;
    sampleRate = source->getSampleRate ();
    numChannels = source->getChannelCount ();
    lengthInSamples = source->getSampleCount ();
    tmpPtrs.resize (numChannels);
    audioSourceBeingRead = static_cast<ARAAudioSource*> (source);

    audioSourceBeingRead->addListener (this);

    if (audioSourceBeingRead->isSampleAccessEnabled ())
        recreate ();
}

ARAAudioSourceReader::~ARAAudioSourceReader ()
{
    // TODO JUCE_ARA
    // should we do this before the lock? after unlock?
    audioSourceBeingRead->removeListener (this);

    ScopedWriteLock l (lock);
    invalidate ();
}

void ARAAudioSourceReader::willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept
{
    if (audioSource != audioSourceBeingRead)
        return;

    // unlocked in didEnableAudioSourceSamplesAccess
    lock.enterWrite ();

    // invalidate our reader if sample access is disabled
    if (enable == false)
        invalidate ();
}

void ARAAudioSourceReader::didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept
{
    // following the invalidation above we can recreate any readers
    // we had before access was disabled
    if (audioSource != audioSourceBeingRead)
        return;

    // recreate our reader if sample access is enabled
    if (enable)
        recreate ();

    lock.exitWrite ();
}

void ARAAudioSourceReader::willDestroyAudioSource (ARAAudioSource* audioSource) noexcept
{
    if (audioSource != audioSourceBeingRead)
        return;

    // TODO JUCE_ARA
    // should this ever happen? ideally someone delete us instead...
    // jassertfalse;
    ScopedWriteLock scopedLock (lock);
    invalidate ();
    audioSourceBeingRead = nullptr;
}

void ARAAudioSourceReader::doUpdateAudioSourceContent (ARAAudioSource* audioSource, const ARA::ARAContentTimeRange* /*range*/, ARA::ContentUpdateScopes scopeFlags) noexcept
{
    if (audioSource != audioSourceBeingRead)
        return;

    // don't invalidate if the audio signal is unchanged
    if (! scopeFlags.affectSamples())
        return;

    ScopedWriteLock scopedLock (lock);
    invalidate ();
}

void ARAAudioSourceReader::recreate ()
{
    // TODO JUCE_ARA
    // it shouldnt' be possible for araHostReader to contain data at this point, 
    // but should we assert that?
    jassert (audioSourceBeingRead->isSampleAccessEnabled ());
    araHostReader.reset (new ARA::PlugIn::HostAudioReader (audioSourceBeingRead));
}

void ARAAudioSourceReader::invalidate ()
{
    araHostReader.reset ();
}

bool ARAAudioSourceReader::readSamples (
    int** destSamples,
    int numDestChannels,
    int startOffsetInDestBuffer,
    int64 startSampleInFile,
    int numSamples)
{
    // If we can't enter the lock or we don't have a reader, zero samples and return false
    if (!lock.tryEnterRead () || araHostReader == nullptr)
    {
        for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
            FloatVectorOperations::clear ((float *) destSamples[chan_i], numSamples);
        return false;
    }

    jassert (audioSourceBeingRead != nullptr);

    for (int chan_i = 0; chan_i < (int) tmpPtrs.size (); ++chan_i)
        if (chan_i < numDestChannels && destSamples[chan_i] != nullptr)
            tmpPtrs[chan_i] = (void*) (destSamples[chan_i] + startOffsetInDestBuffer);
        else
        {
            if (numSamples > (int) dummyBuffer.size ())
                dummyBuffer.resize (numSamples);
            tmpPtrs[chan_i] = (void*) dummyBuffer.data ();
        }

    bool success = araHostReader->readAudioSamples (startSampleInFile, numSamples, tmpPtrs.data ());
    lock.exitRead ();
    return success;
}

//==============================================================================

ARAPlaybackRegionReader::ARAPlaybackRegionReader (ARAPlaybackRenderer* playbackRenderer, std::vector<ARAPlaybackRegion*> playbackRegions)
: AudioFormatReader (nullptr, "ARAAudioSourceReader"),
  playbackRenderer (playbackRenderer)
{
    // TODO JUCE_ARA
    // deal with single and double precision floats
    bitsPerSample = 32;
    usesFloatingPointData = true;
    numChannels = 0;
    lengthInSamples = 0;
    sampleRate = 0;

    for (ARAPlaybackRegion* playbackRegion : playbackRegions)
    {
        ARA::PlugIn::AudioModification* modification = playbackRegion->getAudioModification();
        ARA::PlugIn::AudioSource* source = modification->getAudioSource ();

        if (sampleRate == 0.0)
            sampleRate = source->getSampleRate();

        if (sampleRate != source->getSampleRate())
        {
            // Skip regions with mis-matching sample-rates!
            continue;
        }

        numChannels = std::max (numChannels, (unsigned int) source->getChannelCount());
        lengthInSamples = std::max (lengthInSamples, playbackRegion->getEndInPlaybackSamples (sampleRate));

        playbackRenderer->addPlaybackRegion (playbackRegion);
    }
}

ARAPlaybackRegionReader::~ARAPlaybackRegionReader()
{
    ScopedWriteLock scopedWrite (lock);
    delete playbackRenderer;
}

bool ARAPlaybackRegionReader::readSamples (
    int** destSamples,
    int numDestChannels,
    int startOffsetInDestBuffer,
    int64 startSampleInFile,
    int numSamples)
{
    // render our ARA playback regions for this time duration using the ARA playback renderer instance
    if (! lock.tryEnterRead())
    {
        for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
            FloatVectorOperations::clear ((float *) destSamples[chan_i], numSamples);
        return false;
    }

    AudioBuffer<float> buffer ((float **) destSamples, numDestChannels, startOffsetInDestBuffer, numSamples);
    playbackRenderer->renderSamples (buffer, sampleRate, startSampleInFile, true);
    lock.exitRead();
    return true;
}

//==============================================================================

ARARegionSequenceReader::ARARegionSequenceReader (ARAPlaybackRenderer* playbackRenderer, ARARegionSequence* regionSequence)
    : ARAPlaybackRegionReader (playbackRenderer, {}),
    sequence (regionSequence)
{
    // TODO JUCE_ARA
     // deal with single and double precision floats
    bitsPerSample = 32;
    usesFloatingPointData = true;
    numChannels = 0;
    lengthInSamples = 0;
    sampleRate = 0;

    for (ARA::PlugIn::PlaybackRegion* playbackRegion : sequence->getPlaybackRegions ())
    {
        ARAPlaybackRegion* araPlaybackRegion = static_cast<ARAPlaybackRegion*> (playbackRegion);
        ARA::PlugIn::AudioModification* modification = playbackRegion->getAudioModification ();
        ARA::PlugIn::AudioSource* source = modification->getAudioSource ();

        if (sampleRate == 0.0)
            sampleRate = source->getSampleRate ();

        if (sampleRate != source->getSampleRate ())
        {
            // Skip regions with mis-matching sample-rates!
            continue;
        }

        numChannels = std::max (numChannels, (unsigned int) source->getChannelCount ());
        lengthInSamples = std::max (lengthInSamples, playbackRegion->getEndInPlaybackSamples (sampleRate));

        playbackRenderer->addPlaybackRegion (araPlaybackRegion);
        araPlaybackRegion->addListener (this);
    }
}

ARARegionSequenceReader::~ARARegionSequenceReader ()
{
    ScopedWriteLock scopedWrite (lock);
    for (ARA::PlugIn::PlaybackRegion* playbackRegion : playbackRenderer->getPlaybackRegions ())
        static_cast<ARAPlaybackRegion*> (playbackRegion)->removeListener (this);
}

void ARARegionSequenceReader::willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties> newProperties) noexcept
{
    if (ARA::contains (playbackRenderer->getPlaybackRegions(), static_cast<ARA::PlugIn::PlaybackRegion*> (playbackRegion)))
    {
        if (newProperties->regionSequenceRef != ARA::PlugIn::toRef (sequence))
        {
            ScopedWriteLock scopedWrite (lock);
            playbackRegion->removeListener (this);
            playbackRenderer->removePlaybackRegion (playbackRegion);
        }
    }
    else if (newProperties->regionSequenceRef == ARA::PlugIn::toRef (sequence))
    {
        ScopedWriteLock scopedWrite (lock);
        playbackRegion->addListener (this);
        playbackRenderer->addPlaybackRegion (playbackRegion);
    }
}

void ARARegionSequenceReader::willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) noexcept
{
    if (ARA::contains (playbackRenderer->getPlaybackRegions(), static_cast<ARA::PlugIn::PlaybackRegion*> (playbackRegion)))
    {
        ScopedWriteLock scopedWrite (lock);
        playbackRegion->removeListener (this);
        playbackRenderer->removePlaybackRegion (playbackRegion);
    }
}

}
