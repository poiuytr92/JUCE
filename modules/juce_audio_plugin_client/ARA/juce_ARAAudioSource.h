#pragma once

#include "juce_ARA_audio_plugin.h"

namespace juce
{

class ARAAudioSource : public ARA::PlugIn::AudioSource
{
public:
    ARAAudioSource (ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef);

    void willUpdateAudioSourceProperties (ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties> newProperties) noexcept;
    void didUpdateAudioSourceProperties () noexcept;
    void doUpdateAudioSourceContent (const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept;
    void willEnableAudioSourceSamplesAccess (bool enable) noexcept;
    void didEnableAudioSourceSamplesAccess (bool enable) noexcept;
    void doDeactivateAudioSourceForUndoHistory (bool deactivate) noexcept;
    void willDestroyAudioSource () noexcept;

    class Listener
    {
    public:
        ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        virtual ~Listener()  {}

        virtual void willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties> newProperties) noexcept {}
        virtual void didUpdateAudioSourceProperties (ARAAudioSource* audioSource) noexcept {}
        virtual void doUpdateAudioSourceContent (ARAAudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept {}
        virtual void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept {}
        virtual void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) noexcept {}
        virtual void doDeactivateAudioSourceForUndoHistory (ARAAudioSource* audioSource, bool deactivate) noexcept {}
        virtual void willDestroyAudioSource (ARAAudioSource* audioSource) noexcept {}

        ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    void addListener (Listener* l);
    void removeListener (Listener* l);

private:
    ListenerList<Listener> listeners;
};

}
