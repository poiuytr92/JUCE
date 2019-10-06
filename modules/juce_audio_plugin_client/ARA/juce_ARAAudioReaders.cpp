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

void ARAAudioSourceReader::didUpdateAudioSourceContent (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags)
{
    jassert (audioSourceBeingRead == audioSource);

    // don't invalidate if the audio signal is unchanged
    if (scopeFlags.affectSamples())
        invalidate();
}

void ARAAudioSourceReader::willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable)
{
    jassert (audioSourceBeingRead == audioSource);

    // invalidate our reader if sample access is disabled
    if (! enable)
    {
        ScopedWriteLock scopedLock (lock);
        araHostReader.reset();
    }
}

void ARAAudioSourceReader::didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable)
{
    jassert (audioSourceBeingRead == audioSource);

    // recreate our reader if sample access is enabled
    if (enable && isValid())
    {
        ScopedWriteLock scopedLock (lock);
        araHostReader.reset (new ARA::PlugIn::HostAudioReader (audioSourceBeingRead));
    }
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

MidiBuffer ARAPlaybackRegionReader::dummyMidiBuffer;

ARAPlaybackRegionReader::ARAPlaybackRegionReader (ARADocumentController* documentController, AudioProcessor* processor,
                                                  std::vector<ARAPlaybackRegion*> const& playbackRegions,
                                                  bool alwaysNonRealtime, double playbackSampleRate, int channelCount, bool use64BitSamples)
    : AudioFormatReader (nullptr, "ARAPlaybackRegionReader")
{
    audioProcessor.reset (processor);
    audioProcessorAraExtension = dynamic_cast<AudioProcessorARAExtension*> (processor);
    jassert (audioProcessorAraExtension != nullptr);
    audioProcessorAraExtension->bindToARA (ARA::PlugIn::toRef (documentController),
                                           ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole, ARA::kARAPlaybackRendererRole);

    sampleRate = (playbackSampleRate > 0.0) ? playbackSampleRate :
                                              playbackRegions.empty() ? 44100.0 : playbackRegions.front()->getAudioModification()->getAudioSource()->getSampleRate();
    numChannels = (channelCount > 0) ? channelCount :
                                              playbackRegions.empty() ? 1 : (unsigned int)playbackRegions.front()->getAudioModification()->getAudioSource()->getChannelCount();
    bitsPerSample = use64BitSamples ? 64 : 32;
    usesFloatingPointData = true;

    if (playbackRegions.empty())
    {
        startInSamples = 0;
        lengthInSamples = 0;
    }
    else
    {
        double regionsStartTime = std::numeric_limits<double>::max();
        double regionsEndTime = std::numeric_limits<double>::lowest();

        for (auto playbackRegion : playbackRegions)
        {
            auto playbackRegionTimeRange = playbackRegion->getTimeRange (true);
            regionsStartTime = jmin (regionsStartTime, playbackRegionTimeRange.getStart());
            regionsEndTime = jmax (regionsEndTime, playbackRegionTimeRange.getEnd());

            audioProcessorAraExtension->getARAPlaybackRenderer()->addPlaybackRegion (ARA::PlugIn::toRef (playbackRegion));
            playbackRegion->addListener (this);
        }

        startInSamples = (int64) (regionsStartTime * sampleRate + 0.5);
        lengthInSamples = (int64) ((regionsEndTime - regionsStartTime) * sampleRate + 0.5);
    }

    auto channelSet = AudioChannelSet::canonicalChannelSet (numChannels);
    for (int i = 0; i < audioProcessor->getBusCount (false); i++)
        audioProcessor->setChannelLayoutOfBus (false, i, channelSet);

    audioProcessor->setProcessingPrecision (use64BitSamples ? AudioProcessor::doublePrecision : AudioProcessor::singlePrecision);
    audioProcessor->setPlayHead (this);
    audioProcessor->setNonRealtime (alwaysNonRealtime);
    audioProcessorAraExtension->setAlwaysNonRealtime (alwaysNonRealtime);
    audioProcessor->setRateAndBufferSizeDetails (sampleRate, 16*1024);
    audioProcessor->prepareToPlay (sampleRate, 16*1024);
}

ARAPlaybackRegionReader::ARAPlaybackRegionReader (std::vector<ARAPlaybackRegion*> const& playbackRegions, bool alwaysNonRealtime,
                                                  double playbackSampleRate /*= 0.0*/, int channelCount /*= 0*/, bool use64BitSamples /*= false*/)
    : ARAPlaybackRegionReader (playbackRegions.front()->getDocumentController<ARADocumentController>(),
                               createPluginFilterOfType (PluginHostType::getPluginLoadedAs()), playbackRegions, alwaysNonRealtime, playbackSampleRate, channelCount, use64BitSamples)
{
}

ARAPlaybackRegionReader::ARAPlaybackRegionReader (AudioProcessor* processor, std::vector<ARAPlaybackRegion*> const& playbackRegions,
                                                  bool alwaysNonRealtime, double playbackSampleRate, int channelCount, bool use64BitSamples)
    : ARAPlaybackRegionReader (playbackRegions.front()->getDocumentController<ARADocumentController>(),
                               processor, playbackRegions, alwaysNonRealtime, playbackSampleRate, channelCount, use64BitSamples)
{
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

    for (auto playbackRegion : audioProcessorAraExtension->getARAPlaybackRenderer()->getPlaybackRegions<ARAPlaybackRegion>())
        playbackRegion->removeListener (this);

    audioProcessor->releaseResources();
    audioProcessorAraExtension = nullptr;
    audioProcessor.reset();
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
            renderPosition = startSampleInFile + startInSamples;
            while (numSamples > 0)
            {
                int numSliceSamples = jmin (numSamples, audioProcessor->getBlockSize());
                AudioBuffer<float> buffer ((float **) destSamples, numDestChannels, startOffsetInDestBuffer, numSliceSamples);
                audioProcessor->processBlock (buffer, dummyMidiBuffer);
                jassert (dummyMidiBuffer.getNumEvents() == 0);
                success &= audioProcessorAraExtension->didProcessBlockSucceed();
                numSamples -= numSliceSamples;
                startOffsetInDestBuffer += numSliceSamples;
                renderPosition += numSliceSamples;
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

bool ARAPlaybackRegionReader::getCurrentPosition (CurrentPositionInfo& result)
{
    // we're only providing the minimal set of meaningful values, since the ARA renderer
    // should only look at the time position and the playing state, and read any related
    // tempo or bar signature information from the ARA model directly (MusicalContext)
    result.resetToDefault();

    result.timeInSamples = renderPosition;
    result.timeInSeconds = static_cast<double> (renderPosition) / sampleRate;
    result.isPlaying = true;
    return true;
}

void ARAPlaybackRegionReader::didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags)
{
    jassert (ARA::contains (audioProcessorAraExtension->getARAPlaybackRenderer()->getPlaybackRegions(), playbackRegion));

    // invalidate if the audio signal is changed
    if (scopeFlags.affectSamples())
        invalidate();
}

void ARAPlaybackRegionReader::willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion)
{
    jassert (ARA::contains (audioProcessorAraExtension->getARAPlaybackRenderer()->getPlaybackRegions(), playbackRegion));

    invalidate();
}

//==============================================================================

ARARegionSequenceReader::ARARegionSequenceReader (ARARegionSequence* regionSequence, bool nonRealtime,
                                                  double playbackSampleRate /*= 0.0*/, int channelCount /*= 0*/, bool use64BitSamples /*= false*/)
    : ARARegionSequenceReader (createPluginFilterOfType (PluginHostType::getPluginLoadedAs()), regionSequence, nonRealtime,
                               playbackSampleRate, channelCount, use64BitSamples)
{
}

ARARegionSequenceReader::ARARegionSequenceReader (AudioProcessor* audioProcessor, ARARegionSequence* regionSequence, bool nonRealtime,
                                                  double playbackSampleRate /*= 0.0*/, int channelCount /*= 0*/, bool use64BitSamples /*= false*/)
    : ARAPlaybackRegionReader (regionSequence->getDocumentController<ARADocumentController>(), audioProcessor,
                               regionSequence->getPlaybackRegions<ARAPlaybackRegion>(),
                               nonRealtime, playbackSampleRate, channelCount, use64BitSamples),
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

} // namespace juce
