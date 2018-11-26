#include "juce_ARARegionSequence.h"
#include "juce_ARAAudioSource.h"

namespace juce
{

ARARegionSequence::ARARegionSequence (ARA::PlugIn::Document* document, ARA::ARARegionSequenceHostRef hostRef)
: ARA::PlugIn::RegionSequence (document, hostRef)
{}

bool ARARegionSequence::isSampleAccessEnabled() const
{
    for (auto playbackRegion : getPlaybackRegions ())
        if (playbackRegion->getAudioModification ()->getAudioSource ()->isSampleAccessEnabled () == false)
            return false;
    return true;
}

void ARARegionSequence::willUpdateRegionSequenceProperties (ARA::PlugIn::PropertiesPtr<ARA::ARARegionSequenceProperties> newProperties) noexcept
{
    listeners.call ([this, &newProperties] (Listener& l) { l.willUpdateRegionSequenceProperties (this, newProperties); });
}

void ARARegionSequence::didUpdateRegionSequenceProperties () noexcept
{
    listeners.call ([this] (Listener& l) { l.didUpdateRegionSequenceProperties (this); });
}

void ARARegionSequence::willDestroyRegionSequence () noexcept
{
    // TODO JUCE_ARA 
    // same concerns involving removal as with other listeners
    auto listenersCopy (listeners.getListeners ());
    for (auto listener : listenersCopy)
    {
        if (listeners.contains (listener))
            listener->willDestroyRegionSequence (this);
    }
}

void ARARegionSequence::addListener (Listener * l)
{
    listeners.add (l);
}

void ARARegionSequence::removeListener (Listener * l)
{
    listeners.remove (l);
}

} // namespace juce
