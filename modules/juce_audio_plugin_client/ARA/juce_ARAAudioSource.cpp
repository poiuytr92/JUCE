#include "juce_ARAAudioSource.h"

namespace juce
{

class ARAAudioSource::Reader : public AudioFormatReader
{
public:
    Reader (ARAAudioSource*);
    ~Reader();

    void createHostAudioReaderForSource (ARA::PlugIn::AudioSource* audioSource);
    void invalidate();

    bool readSamples (
        int** destSamples,
        int numDestChannels,
        int startOffsetInDestBuffer,
        int64 startSampleInFile,
        int numSamples) override;

private:
    Ref::Ptr ref;
    std::vector<void*> tmpPtrs;

    // When readSamples is not reading all channels,
    // we still need to provide pointers to all channels to the ARA read call.
    // So we'll read the other channels into this dummy buffer.
    std::vector<float> dummyBuffer;

    std::unique_ptr<ARA::PlugIn::HostAudioReader> araHostReader;
};

ARAAudioSource::ARAAudioSource (ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef)
: ARA::PlugIn::AudioSource (document, hostRef),
  ARAAudioSourceUpdateListener (document->getDocumentController ()),
  ref (new Ref (this))
{}

ARAAudioSource::~ARAAudioSource()
{
    invalidateReaders();
}

void ARAAudioSource::invalidateReaders()
{
    {
        ScopedWriteLock l (readersLock);
        for (auto& reader : readers)
            reader->invalidate();
        readers.clear();
    }
    ref->reset();
}

AudioFormatReader* ARAAudioSource::newReader()
{
    return new Reader (this);
}

void ARAAudioSource::willUpdateAudioSourceProperties (ARA::PlugIn::AudioSource* audioSource, ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties> newProperties) noexcept
{
    if (audioSource != this)
        return;

   #if JUCE_DEBUG
    jassert (! stateUpdateProperties);
    stateUpdateProperties = true;
   #endif

    // TODO JUCE_ARA
    // We can check individual values to see if we need to invalidate, 
    // but according to ARAInterface.h line 2607 isn't it up to the 
    // ARA host to disable audio source sample access when appropriate?
    if (getSampleCount () != newProperties->sampleCount ||
        getSampleRate () != newProperties->sampleRate ||
        getChannelCount () != newProperties->channelCount)
    {
        invalidateReaders ();
    }
}

void ARAAudioSource::didUpdateAudioSourceProperties (ARA::PlugIn::AudioSource* audioSource) noexcept
{
    if (audioSource != this)
        return;

   #if JUCE_DEBUG
    jassert (stateUpdateProperties);
    stateUpdateProperties = false;
   #endif

    if (ref->get () == nullptr)
        ref = new Ref (this);
}

void ARAAudioSource::willEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    if (audioSource != this)
        return;

   #if JUCE_DEBUG
    jassert (! stateEnableSamplesAccess);
    stateEnableSamplesAccess = true;
   #endif

    ref->lock.enterWrite();
    if (! enable)
    {
        ScopedReadLock l (readersLock);
        for (auto& reader : readers)
            reader->invalidate();
    }
}

void ARAAudioSource::didEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    if (audioSource != this)
        return;

   #if JUCE_DEBUG
    jassert (stateEnableSamplesAccess);
    stateEnableSamplesAccess = false;
   #endif

    if (enable)
    {
        ScopedReadLock l (readersLock);
        for (auto& reader : readers)
            reader->createHostAudioReaderForSource (this);
    }
    ref->lock.exitWrite();
}

void ARAAudioSource::doUpdateAudioSourceContent (ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* /*range*/, ARA::ARAContentUpdateFlags flags) noexcept
{
    if (audioSource != this)
        return;

    // don't invalidate if the audio signal is unchanged
    if ((flags & ARA::kARAContentUpdateSignalScopeRemainsUnchanged) != 0)
        return;

    invalidateReaders ();
}

std::unique_ptr<BufferingAudioSource> ARAAudioSource::createBufferingAudioSource (TimeSliceThread& thread, int bufferSize)
{
    return std::unique_ptr<BufferingAudioSource> (new BufferingAudioSource (new AudioFormatReaderSource (new ARAAudioSourceReader (this), true), thread, true, bufferSize));
}

ARAAudioSource::Reader::Reader (ARAAudioSource* source)
: AudioFormatReader (nullptr, "ARAAudioSourceReader"),
  ref (source->ref)
{
    if (source->isSampleAccessEnabled())
        araHostReader.reset (new ARA::PlugIn::HostAudioReader (source));
    bitsPerSample = 32;
    usesFloatingPointData = true;
    sampleRate = source->getSampleRate();
    numChannels = source->getChannelCount();
    lengthInSamples = source->getSampleCount();
    tmpPtrs.resize (numChannels);
    ScopedWriteLock l (source->readersLock);
    source->readers.push_back (this);
}

ARAAudioSource::Reader::~Reader()
{
    if (Ref::ScopedAccess source{ ref })
    {
        ScopedWriteLock l (source->readersLock);
        ARA::find_erase (source->readers, this);
    }
}

void ARAAudioSource::Reader::createHostAudioReaderForSource (ARA::PlugIn::AudioSource* audioSource)
{
    // TODO JUCE_ARA should we assert these conditions instead of treating them as a case for invalidation?
    if (audioSource == nullptr || audioSource->isSampleAccessEnabled() == false)
        invalidate();
    else
        araHostReader.reset (new ARA::PlugIn::HostAudioReader (audioSource));
}

void ARAAudioSource::Reader::invalidate()
{
    araHostReader.reset();
}

bool ARAAudioSource::Reader::readSamples (
    int** destSamples,
    int numDestChannels,
    int startOffsetInDestBuffer,
    int64 startSampleInFile,
    int numSamples)
{
    Ref::ScopedAccess source (ref, true);
    if (! source || araHostReader == nullptr)
    {
        for (int chan_i = 0; chan_i < (int) tmpPtrs.size (); ++chan_i)
            FloatVectorOperations::clear ((float *) destSamples[chan_i], numSamples);
        return false;
    }

    for (int chan_i = 0; chan_i < (int) tmpPtrs.size(); ++chan_i)
        if (chan_i < numDestChannels && destSamples[chan_i] != nullptr)
            tmpPtrs[chan_i] = (void*) (destSamples[chan_i] + startOffsetInDestBuffer);
        else
        {
            if (numSamples > (int) dummyBuffer.size())
                dummyBuffer.resize (numSamples);
            tmpPtrs[chan_i] = (void*) dummyBuffer.data();
        }
    return araHostReader->readAudioSamples (startSampleInFile, numSamples, tmpPtrs.data());
}

// =============================================================================================================

ARAAudioSourceReader::ARAAudioSourceReader (ARA::PlugIn::AudioSource* source, bool use64BitSamples)
: AudioFormatReader (nullptr, "ARAAudioSourceReader"),
  ARAAudioSourceUpdateListener (source->getDocument ()->getDocumentController ())
{
    bitsPerSample = use64BitSamples ? 64 : 32;
    usesFloatingPointData = true;
    sampleRate = source->getSampleRate ();
    numChannels = source->getChannelCount ();
    lengthInSamples = source->getSampleCount ();
    tmpPtrs.resize (numChannels);
    audioSourceBeingRead = source;

    if (audioSourceBeingRead->isSampleAccessEnabled ())
        recreate ();
}

ARAAudioSourceReader::~ARAAudioSourceReader ()
{
    ScopedWriteLock l (lock);
    invalidate ();
}

void ARAAudioSourceReader::willEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    if (audioSource != audioSourceBeingRead)
        return;

    // unlocked in didEnableAudioSourceSamplesAccess
    lock.enterWrite ();

    // invalidate our reader if sample access is disabled
    if (enable == false)
        invalidate ();
}

void ARAAudioSourceReader::didEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
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

void ARAAudioSourceReader::willDestroyAudioSource (ARA::PlugIn::AudioSource* audioSource) noexcept
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

void ARAAudioSourceReader::doUpdateAudioSourceContent (ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* /*range*/, ARA::ARAContentUpdateFlags flags) noexcept
{
    if (audioSource != audioSourceBeingRead)
        return;

    // don't invalidate if the audio signal is unchanged
    if ((flags & ARA::kARAContentUpdateSignalScopeRemainsUnchanged) != 0)
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
    if (! lock.tryEnterRead () || araHostReader == nullptr)
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
} // namespace juce
