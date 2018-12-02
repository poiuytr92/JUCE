#pragma once

#include "juce_ARA_audio_plugin.h"

namespace juce
{
class ARAPlaybackRegion;

class ARAAudioModification : public ARA::PlugIn::AudioModification
{
public:
    ARAAudioModification (ARAAudioSource* audioSource, ARA::ARAAudioModificationHostRef hostRef);

    class Listener
    {
    public:
        virtual ~Listener() {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
        virtual void willUpdateAudioModificationProperties (ARAAudioModification* audioModification, PropertiesPtr newProperties) {}
        virtual void didUpdateAudioModificationProperties (ARAAudioModification* audioModification) {}
        virtual void didUpdateAudioModificationContent (ARAAudioModification* audioModification, ARAContentUpdateScopes scopeFlags) {}
        virtual void doDeactivateAudioModificationForUndoHistory (ARAAudioModification* audioModification, bool deactivate) {}
        virtual void didAddPlaybackRegion (ARAAudioModification* audioModification, ARAPlaybackRegion* playbackRegion) {}
        virtual void willDestroyAudioModification (ARAAudioModification* audioModification) {}
       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    void addListener (Listener* l);
    void removeListener (Listener* l);

public:         // to be called by ARADocumentController only
    void willUpdateAudioModificationProperties (PropertiesPtr newProperties);
    void didUpdateAudioModificationProperties();
    void didUpdateAudioModificationContent (ARAContentUpdateScopes scopeFlags);
    void doDeactivateAudioModificationForUndoHistory (bool deactivate);
    void didAddPlaybackRegion (ARAPlaybackRegion* playbackRegion);
    void willDestroyAudioModification();

private:
    ListenerList<Listener> listeners;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAAudioModification)
};

} // namespace juce
