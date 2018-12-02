#pragma once

#include "juce_ARA_audio_plugin.h"
#include "juce_ARADocumentController.h"

namespace juce
{

class ARAPlaybackRegion : public ARA::PlugIn::PlaybackRegion
{
public:
    ARAPlaybackRegion (ARAAudioModification* audioModification, ARA::ARAPlaybackRegionHostRef hostRef);

    class Listener
    {
    public:
        virtual ~Listener()  {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
        virtual void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) {}
        virtual void didUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion) {}
        virtual void didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags) {}
        virtual void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) {}
       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    void addListener (Listener* l);
    void removeListener (Listener* l);

public:         // to be called by ARADocumentController only
    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion::PropertiesPtr newProperties);
    void didUpdatePlaybackRegionProperties();
    void didUpdatePlaybackRegionContent (ARAContentUpdateScopes scopeFlags);
    void willDestroyPlaybackRegion();

private:
    ListenerList<Listener> listeners;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAPlaybackRegion)
};

} // namespace juce
