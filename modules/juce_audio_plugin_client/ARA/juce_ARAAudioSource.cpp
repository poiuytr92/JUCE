#include "juce_ARAAudioSource.h"

namespace juce
{

ARAAudioSource::ARAAudioSource (ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef)
: ARA::PlugIn::AudioSource(document, hostRef)
{}

void ARAAudioSource::willUpdateAudioSourceProperties (PropertiesPtr newProperties)
{
    listeners.call ([this, &newProperties] (Listener& l) { l.willUpdateAudioSourceProperties (this, newProperties); });
}

void ARAAudioSource::didUpdateAudioSourceProperties()
{
    listeners.call ([this] (Listener& l) { l.didUpdateAudioSourceProperties (this); });
}

void ARAAudioSource::doUpdateAudioSourceContent (const ARA::ARAContentTimeRange* range, ARAContentUpdateScopes scopeFlags)
{
    listeners.call ([this, range, scopeFlags] (Listener& l) { l.doUpdateAudioSourceContent (this, range, scopeFlags); });
}

void ARAAudioSource::willEnableAudioSourceSamplesAccess (bool enable)
{
    listeners.call ([this, enable] (Listener& l) { l.willEnableAudioSourceSamplesAccess (this, enable); });
}

void ARAAudioSource::didEnableAudioSourceSamplesAccess (bool enable)
{
    listeners.call ([this, enable] (Listener& l) { l.didEnableAudioSourceSamplesAccess (this, enable); });
}

void ARAAudioSource::doDeactivateAudioSourceForUndoHistory (bool deactivate)
{
    listeners.call ([this, deactivate] (Listener& l) { l.doDeactivateAudioSourceForUndoHistory (this, deactivate); });
}

void ARAAudioSource::willDestroyAudioSource()
{
    // TODO JUCE_ARA
    // like in ARAPlaybackRegion, we have to avoid listeners removing themselves while
    // this function is called. Two ideas:
    // a) make some scoped vector of listeners to remove that we flush at the end of each call
    // b) make these functions const - these functions are called within an edit cycle, so it
    //    seems like a good idea to encourage folks to postpone responses until the end of an edit cycle
    auto listenersCopy (listeners.getListeners());
    for (auto listener : listenersCopy)
    {
        if (listeners.contains (listener))
            listener->willDestroyAudioSource (this);
    }
}

void ARAAudioSource::addListener (Listener * l)
{
    listeners.add (l);
}

void ARAAudioSource::removeListener (Listener * l)
{
    listeners.remove (l);
}

} // namespace juce
