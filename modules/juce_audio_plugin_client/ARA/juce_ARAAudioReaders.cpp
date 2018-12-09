#include "juce_ARAAudioReaders.h"

namespace juce
{

ARAAudioSourceReader::ARAAudioSourceReader (ARAAudioSource* audioSource, bool use64BitSamples)
: AudioFormatReader (nullptr, "ARAAudioSourceReader"),
  audioSourceBeingRead (audioSource)
{
    jassert (audioSourceBeingRead != nullptr);

    bitsPerSample = use64BitSamples ? 64 : 32;
    usesFloatingPointData = true;
    sampleRate = audioSourceBeingRead->getSampleRate();
    numChannels = audioSourceBeingRead->getChannelCount();
    lengthInSamples = audioSourceBeingRead->getSampleCount();
    tmpPtrs.resize (numChannels);

    audioSourceBeingRead->addListener (this);
    if (audioSourceBeingRead->isSampleAccessEnabled())
        araHostReader.reset (new ARA::PlugIn::HostAudioReader (audioSourceBeingRead));
}

ARAAudioSourceReader::~ARAAudioSourceReader()
{
    invalidate();
}

void ARAAudioSourceReader::invalidate()
{
    ScopedWriteLock scopedLock (lock);

    if (! isValid())
        return;

    araHostReader.reset();

    audioSourceBeingRead->removeListener (this);
    audioSourceBeingRead = nullptr;
}

void ARAAudioSourceReader::willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties)
{
    if (audioSource->getSampleCount() != newProperties->sampleCount ||
        audioSource->getSampleRate() != newProperties->sampleRate ||
        audioSource->getChannelCount() != newProperties->channelCount)
    {
        invalidate();
    }
}

void ARAAudioSourceReader::doUpdateAudioSourceContent (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags)
{
    jassert (audioSourceBeingRead == audioSource);

    // don't invalidate if the audio signal is unchanged
    if (scopeFlags.affectSamples())
        invalidate();
}

void ARAAudioSourceReader::willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable)
{
    jassert (audioSourceBeingRead == audioSource);

    // unlocked in didEnableAudioSourceSamplesAccess
    lock.enterWrite();

    // invalidate our reader if sample access is disabled
    if (! enable)
        araHostReader.reset();
}

void ARAAudioSourceReader::didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable)
{
    jassert (audioSourceBeingRead == audioSource);

    // following the invalidation above we can recreate any readers
    // we had before access was disabled

    // recreate our reader if sample access is enabled
    if (enable && isValid())
        araHostReader.reset (new ARA::PlugIn::HostAudioReader (audioSourceBeingRead));

    lock.exitWrite();
}

void ARAAudioSourceReader::willDestroyAudioSource (ARAAudioSource* audioSource)
{
    jassert (audioSourceBeingRead == audioSource);

    invalidate();
}

bool ARAAudioSourceReader::readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                                        int64 startSampleInFile, int numSamples)
{
    int destSize = (bitsPerSample / 8) * numSamples;
    int bufferOffset = (bitsPerSample / 8) * startOffsetInDestBuffer;

    // If we're invalid or can't enter the lock or audio source access is currently disabled, zero samples and return false
    bool gotReadlock = isValid() ? lock.tryEnterRead() : false;
    if (! isValid() || ! gotReadlock || (araHostReader == nullptr))
    {
        if (gotReadlock)
            lock.exitRead();

        for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
        {
            if (destSamples[chan_i] != nullptr)
                zeromem (((uint8_t*) destSamples[chan_i]) + bufferOffset, destSize);
        }
        return false;
    }

    for (int chan_i = 0; chan_i < (int) tmpPtrs.size(); ++chan_i)
    {
        if ((chan_i < numDestChannels) && (destSamples[chan_i] != nullptr))
        {
            tmpPtrs[chan_i] = ((uint8_t*) destSamples[chan_i]) + bufferOffset;
        }
        else
        {
            // When readSamples is not reading all channels,
            // we still need to provide pointers to all channels to the ARA read call.
            // So we'll read the other channels into this dummy buffer.
            static ThreadLocalValue<std::vector<uint8_t>> dummyBuffer;
            if (destSize > (int) dummyBuffer.get().size())
                dummyBuffer.get().resize (destSize);

            tmpPtrs[chan_i] = dummyBuffer.get().data();
        }
    }

    bool success = araHostReader->readAudioSamples (startSampleInFile, numSamples, tmpPtrs.data());

    lock.exitRead();

    return success;
}

//==============================================================================

ARAPlaybackRegionReader::ARAPlaybackRegionReader (ARAPlaybackRenderer* renderer, std::vector<ARAPlaybackRegion*> const& playbackRegions, bool nonRealtime)
: AudioFormatReader (nullptr, "ARAPlaybackRegionReader"),
  isNonRealtime (nonRealtime),
  playbackRenderer (renderer)
{
    // TODO JUCE_ARA
    // Make sampleRate, numChannels and use64BitSamples available as c'tor parameters instead
    // of deducing it here. Since regions can start anywhere on the timeline, maybe also define
    // which time range should be considered as "range to be read by this reader".
    bitsPerSample = 32;
    usesFloatingPointData = true;
    numChannels = 1;

    if (playbackRegions.size() == 0)
    {
        startInSamples = 0;
        lengthInSamples = 0;
        sampleRate = 44100.0;
    }
    else
    {
        sampleRate = 0.0;
        double regionsStartTime = std::numeric_limits<double>::max();
        double regionsEndTime = std::numeric_limits<double>::lowest();

        for (auto playbackRegion : playbackRegions)
        {
            ARA::PlugIn::AudioModification* modification = playbackRegion->getAudioModification();
            ARA::PlugIn::AudioSource* source = modification->getAudioSource();

            if (sampleRate == 0.0)
                sampleRate = source->getSampleRate();

            numChannels = jmax (numChannels, (unsigned int) source->getChannelCount());

            regionsStartTime = jmin (regionsStartTime, playbackRegion->getStartInPlaybackTime() - playbackRegion->getHeadTime());
            regionsEndTime = jmax (regionsEndTime, playbackRegion->getEndInPlaybackTime() + playbackRegion->getTailTime());

            playbackRenderer->addPlaybackRegion (playbackRegion);
            playbackRegion->addListener (this);
        }

        startInSamples = (int64) (regionsStartTime * sampleRate + 0.5);
        lengthInSamples = (int64) ((regionsEndTime - regionsStartTime) * sampleRate + 0.5);
    }

    playbackRenderer->prepareToPlay (sampleRate, numChannels, 16*1024, ! isNonRealtime);
}

ARAPlaybackRegionReader::~ARAPlaybackRegionReader()
{
    invalidate();
}

void ARAPlaybackRegionReader::invalidate()
{
    ScopedWriteLock scopedWrite (lock);

    if (! isValid())
        return;

    for (auto playbackRegion : playbackRenderer->getPlaybackRegions())
        static_cast<ARAPlaybackRegion*>(playbackRegion)->removeListener (this);

    playbackRenderer.reset();
}

bool ARAPlaybackRegionReader::readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                                           int64 startSampleInFile, int numSamples)
{
    bool success = false;
    bool needClearSamples = true;
    if (lock.tryEnterRead())
    {
        if (isValid())
        {
            success = true;
            needClearSamples = false;
            startSampleInFile += startInSamples;
            while (numSamples > 0)
            {
                int numSliceSamples = jmin(numSamples, playbackRenderer->getMaxSamplesPerBlock());
                AudioBuffer<float> buffer ((float **) destSamples, numDestChannels, startOffsetInDestBuffer, numSliceSamples);
                success &= playbackRenderer->processBlock (buffer, startSampleInFile, true, isNonRealtime);
                numSamples -= numSliceSamples;
                startOffsetInDestBuffer += numSliceSamples;
                startSampleInFile += numSliceSamples;
            }
        }

        lock.exitRead();
    }

    if (needClearSamples)
    {
        for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
            FloatVectorOperations::clear ((float *) destSamples[chan_i], numSamples);
    }

    return success;
}

void ARAPlaybackRegionReader::willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties)
{
    jassert (ARA::contains (playbackRenderer->getPlaybackRegions(), playbackRegion));

    // TODO JUCE_ARA most of these tests should be unnecessary now that we're listening to contentChanged...?
    if ((playbackRegion->getStartInAudioModificationTime() != newProperties->startInModificationTime) ||
        (playbackRegion->getDurationInAudioModificationTime() != newProperties->durationInModificationTime) ||
        (playbackRegion->getStartInPlaybackTime() != newProperties->startInPlaybackTime) ||
        (playbackRegion->getDurationInPlaybackTime() != newProperties->durationInPlaybackTime) ||
        (playbackRegion->isTimestretchEnabled() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationTimestretch) != 0)) ||
        (playbackRegion->isTimeStretchReflectingTempo() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationTimestretchReflectingTempo) != 0)) ||
        (playbackRegion->hasContentBasedFadeAtHead() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationContentBasedFadeAtHead) != 0)) ||
        (playbackRegion->hasContentBasedFadeAtTail() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationContentBasedFadeAtTail) != 0)))
    {
        invalidate();
    }
}

void ARAPlaybackRegionReader::didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags)
{
    jassert (ARA::contains (playbackRenderer->getPlaybackRegions(), playbackRegion));

    // don't invalidate if the audio signal is unchanged
    if (scopeFlags.affectSamples())
        invalidate();
}

void ARAPlaybackRegionReader::willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion)
{
    jassert (ARA::contains (playbackRenderer->getPlaybackRegions(), playbackRegion));

    invalidate();
}

//==============================================================================

ARARegionSequenceReader::ARARegionSequenceReader (ARAPlaybackRenderer* playbackRenderer, ARARegionSequence* regionSequence, bool nonRealtime)
: ARAPlaybackRegionReader (playbackRenderer, reinterpret_cast<std::vector<ARAPlaybackRegion*> const&> (regionSequence->getPlaybackRegions()), nonRealtime),
  sequence (regionSequence)
{
    sequence->addListener (this);
}

ARARegionSequenceReader::~ARARegionSequenceReader()
{
    if (sequence != nullptr)
        sequence->removeListener (this);
}

void ARARegionSequenceReader::willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion)
{
    jassert (sequence == regionSequence);
    jassert (ARA::contains (sequence->getPlaybackRegions(), playbackRegion));

    invalidate();
}

void ARARegionSequenceReader::didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion)
{
    jassert (sequence == regionSequence);
    jassert (ARA::contains (sequence->getPlaybackRegions(), playbackRegion));

    invalidate();
}

void ARARegionSequenceReader::willDestroyRegionSequence (ARARegionSequence* regionSequence)
{
    jassert (sequence == regionSequence);

    invalidate();

    sequence->removeListener (this);
    sequence = nullptr;
}

ARARegionSequenceSourceReader::ARARegionSequenceSourceReader (
    ARARegionSequence* regionSequence, double sampleRate_, int numChannels_)
: AudioFormatReader (nullptr, "ARARegionSequenceSourceReader"),
  sequence (regionSequence), sampleBuffer (numChannels_, 256)
{
    jassert (sequence != nullptr);

    bitsPerSample = 32;
    usesFloatingPointData = true;
    sampleRate = sampleRate_;
    numChannels = numChannels_;


    sequence->addListener (this);

    for (auto playbackRegion : sequence->getPlaybackRegions())
    {
        ARAPlaybackRegion* region = static_cast<ARAPlaybackRegion*> (playbackRegion);
        auto modification = region->getAudioModification();
        ARAAudioSource* source = static_cast<ARAAudioSource*> (modification->getAudioSource());

        if (sampleRate != source->getSampleRate())
            continue;

        regions.push_back (region);
        region->addListener (this);

        if (sourceReaders.find (source) == sourceReaders.end())
        {
            sourceReaders[source] = new ARAAudioSourceReader (source);
            source->addListener (this);
        }

        lengthInSamples = std::max (lengthInSamples, region->getEndInPlaybackSamples (sampleRate));
    }
}

ARARegionSequenceSourceReader::~ARARegionSequenceSourceReader()
{
    invalidate();
}

void ARARegionSequenceSourceReader::invalidate()
{
    ScopedWriteLock scopedWrite (lock);

    if (! isValid())
        return;

    for (auto src : sourceReaders)
    {
        src.first->removeListener (this);
        delete src.second;
    }

    for (auto region : regions)
        region->removeListener (this);

    sequence->removeListener (this);
    sequence = nullptr;
}

bool ARARegionSequenceSourceReader::readSamples (
    int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
    int64 startSampleInFile, int numSamples)
{
    int destSize = (bitsPerSample / 8) * numSamples;
    int bufferOffset = (bitsPerSample / 8) * startOffsetInDestBuffer;

    for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
        if (destSamples[chan_i] != nullptr)
            zeromem (((uint8_t*) destSamples[chan_i]) + bufferOffset, destSize);

    if (! lock.tryEnterRead())
        return false;

    if (! isValid())
    {
        lock.exitRead();
        return false;
    }

    const double start = (double) startSampleInFile / sampleRate;
    const double stop = (double) (startSampleInFile + (int64) numSamples) / sampleRate;

    if (sampleBuffer.getNumSamples() < numSamples)
        sampleBuffer.setSize (numDestChannels, numSamples, false, false, true);

    // Fill in content from relevant regions
    for (auto region : regions)
    {
        if (region->getEndInPlaybackTime() <= start || region->getStartInPlaybackTime() >= stop)
            continue;

        const int64 regionStartSample = region->getStartInPlaybackSamples (sampleRate);

        const int64 startSampleInRegion = std::max ((int64) 0, startSampleInFile - regionStartSample);
        const int destOffest = (int) std::max ((int64) 0, regionStartSample - startSampleInFile);
        const int numRegionSamples = std::min (
                (int) (region->getDurationInPlaybackSamples (sampleRate) - startSampleInRegion),
                numSamples - destOffest);

        ARAAudioSource* source = static_cast<ARAAudioSource*> (region->getAudioModification()->getAudioSource());
        AudioFormatReader* sourceReader = sourceReaders[source];
        jassert (sourceReader != nullptr);

        if (! sourceReader->read (
            (int**) sampleBuffer.getArrayOfWritePointers(),
            numDestChannels,
            region->getStartInAudioModificationSamples() + startSampleInRegion,
            numRegionSamples,
            false))
        {
            lock.exitRead();
            return false;
        }

        for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
            if (destSamples[chan_i] != nullptr)
                FloatVectorOperations::add (
                    (float*) (destSamples[chan_i]) + startOffsetInDestBuffer + destOffest,
                    sampleBuffer.getReadPointer (chan_i), numRegionSamples);
    }

    lock.exitRead();

    return true;
}

void ARARegionSequenceSourceReader::willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion)
{
    jassert (sequence == regionSequence);
    jassert (ARA::contains (sequence->getPlaybackRegions(), playbackRegion));

    invalidate();
}

void ARARegionSequenceSourceReader::didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion)
{
    jassert (sequence == regionSequence);
    jassert (ARA::contains (sequence->getPlaybackRegions(), playbackRegion));

    invalidate();
}

void ARARegionSequenceSourceReader::willDestroyRegionSequence (ARARegionSequence* regionSequence)
{
    jassert (sequence == regionSequence);

    invalidate();
}

void ARARegionSequenceSourceReader::willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties)
{
    jassert (ARA::contains (regions, playbackRegion));

    // TODO JUCE_ARA most of these tests should be unnecessary now that we're listening to contentChanged...?
    if ((playbackRegion->getStartInAudioModificationTime() != newProperties->startInModificationTime) ||
        (playbackRegion->getDurationInAudioModificationTime() != newProperties->durationInModificationTime) ||
        (playbackRegion->getStartInPlaybackTime() != newProperties->startInPlaybackTime) ||
        (playbackRegion->getDurationInPlaybackTime() != newProperties->durationInPlaybackTime) ||
        (playbackRegion->isTimestretchEnabled() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationTimestretch) != 0)) ||
        (playbackRegion->isTimeStretchReflectingTempo() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationTimestretchReflectingTempo) != 0)) ||
        (playbackRegion->hasContentBasedFadeAtHead() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationContentBasedFadeAtHead) != 0)) ||
        (playbackRegion->hasContentBasedFadeAtTail() != ((newProperties->transformationFlags & ARA::kARAPlaybackTransformationContentBasedFadeAtTail) != 0)))
        invalidate();
}

void ARARegionSequenceSourceReader::didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags)
{
    jassert (ARA::contains (regions, playbackRegion));

    // don't invalidate if the audio signal is unchanged
    if (scopeFlags.affectSamples())
        invalidate();
}

void ARARegionSequenceSourceReader::willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties)
{
    if (audioSource->getSampleCount() != newProperties->sampleCount ||
        audioSource->getSampleRate() != newProperties->sampleRate ||
        audioSource->getChannelCount() != newProperties->channelCount)
    {
        invalidate();
    }
}

void ARARegionSequenceSourceReader::doUpdateAudioSourceContent (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags)
{
    // don't invalidate if the audio signal is unchanged
    if (scopeFlags.affectSamples())
        invalidate();
}

} // namespace juce
