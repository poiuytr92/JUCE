#include "juce_ARAModelObjects.h"

namespace juce
{

//==============================================================================

ARADocument::ARADocument (ARADocumentController* documentController)
    : ARA::PlugIn::Document (documentController)
{}

//==============================================================================

ARAMusicalContext::ARAMusicalContext (ARADocument* document, ARA::ARAMusicalContextHostRef hostRef)
    : ARA::PlugIn::MusicalContext (document, hostRef)
{}

//==============================================================================

ARARegionSequence::ARARegionSequence (ARADocument* document, ARA::ARARegionSequenceHostRef hostRef)
    : ARA::PlugIn::RegionSequence (document, hostRef)
{}

void ARARegionSequence::getTimeRange (double& startTime, double& endTime, bool includeHeadAndTail) const
{
    if (getPlaybackRegions().empty())
    {
        startTime = 0.0;
        endTime = 0.0;
        return;
    }

    startTime = std::numeric_limits<double>::max();
    endTime = std::numeric_limits<double>::lowest();
    for (auto playbackRegion : getPlaybackRegions())
    {
        double regionStartTime = playbackRegion->getStartInPlaybackTime();
        double regionEndTime = playbackRegion->getEndInPlaybackTime();

        if (includeHeadAndTail)
        {
            auto region = static_cast<ARAPlaybackRegion*> (playbackRegion);
            regionStartTime -= region->getHeadTime();
            regionEndTime += region->getTailTime();
        }

        startTime = jmin (startTime, regionStartTime);
        endTime = jmax (endTime, regionEndTime);
    }
}

double ARARegionSequence::getCommonSampleRate() const
{
    double commonSampleRate = 0.0;
    for (auto playbackRegion : getPlaybackRegions())
    {
        double sampleRate = playbackRegion->getAudioModification()->getAudioSource()->getSampleRate();
        if (commonSampleRate == 0.0)
            commonSampleRate = sampleRate;
        if (commonSampleRate != sampleRate)
            return 0.0;
    }
    return commonSampleRate;
}

//==============================================================================

ARAAudioSource::ARAAudioSource (ARADocument* document, ARA::ARAAudioSourceHostRef hostRef)
    : ARA::PlugIn::AudioSource(document, hostRef)
{}

void ARAAudioSource::notifyContentChanged (ARAContentUpdateScopes scopeFlags, bool notifyAllAudioModificationsAndPlaybackRegions)
{
    static_cast<ARADocumentController*> (getDocument()->getDocumentController())->
                                notifyAudioSourceContentChanged (this, scopeFlags, notifyAllAudioModificationsAndPlaybackRegions);
}

//==============================================================================

ARAAudioModification::ARAAudioModification (ARAAudioSource* audioSource, ARA::ARAAudioModificationHostRef hostRef)
    : ARA::PlugIn::AudioModification (audioSource, hostRef)
{}

void ARAAudioModification::notifyContentChanged (ARAContentUpdateScopes scopeFlags, bool notifyAllPlaybackRegions)
{
    static_cast<ARADocumentController*> (getAudioSource()->getDocument()->getDocumentController())->
                                notifyAudioModificationContentChanged (this, scopeFlags, notifyAllPlaybackRegions);
}

//==============================================================================

ARAPlaybackRegion::ARAPlaybackRegion (ARAAudioModification* audioModification, ARA::ARAPlaybackRegionHostRef hostRef)
    : ARA::PlugIn::PlaybackRegion (audioModification, hostRef)
{}

void ARAPlaybackRegion::setHeadTime (double newHeadTime)
{
    headTime = newHeadTime;
    notifyContentChanged (ARAContentUpdateScopes::samplesAreAffected());
}

void ARAPlaybackRegion::setTailTime (double newTailTime)
{
    tailTime = newTailTime;
    notifyContentChanged (ARAContentUpdateScopes::samplesAreAffected());
}

void ARAPlaybackRegion::setHeadAndTailTime (double newHeadTime, double newTailTime)
{
    headTime = newHeadTime;
    tailTime = newTailTime;
    notifyContentChanged (ARAContentUpdateScopes::samplesAreAffected());
}

void ARAPlaybackRegion::notifyContentChanged (ARAContentUpdateScopes scopeFlags)
{
    static_cast<ARADocumentController*> (getAudioModification()->getAudioSource()->getDocument()->getDocumentController())->
                            notifyPlaybackRegionContentChanged (this, scopeFlags);
}

} // namespace juce
