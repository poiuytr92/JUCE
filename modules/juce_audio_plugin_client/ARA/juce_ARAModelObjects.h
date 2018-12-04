#pragma once

#include "juce_ARA_audio_plugin.h"

namespace juce
{

class ARADocumentController;
class ARADocument;
class ARAMusicalContext;
class ARARegionSequence;
class ARAAudioSource;
class ARAAudioModification;
class ARAPlaybackRegion;

template<typename Listener>
class ARAModelClass
{
public:
    ARAModelClass() = default;
    virtual ~ARAModelClass() = default;

    inline void addListener (Listener* l) { listeners.add (l); }
    inline void removeListener (Listener* l) { listeners.remove (l); }

    template<typename Callback>
    inline void notifyListeners (Callback&& callback) { listeners.callExpectingUnregistration (callback); } \

private:
    ListenerList<Listener> listeners;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAModelClass)
};

// TODO JUCE_ARA
// Do we want to use this Listener pattern?
// @jj yes please :-)
template<class ModelClassType>
class ARAListenableModelClass
{
public:
    class Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;
    };

    ARAListenableModelClass() = default;
    virtual ~ARAListenableModelClass() = default;

    inline void addListener (Listener* l) { listeners.add (l); }
    inline void removeListener (Listener* l) { listeners.remove (l); }

    template<typename Callback>
    inline void notifyListeners (Callback&& callback)
    {
        reinterpret_cast<ListenerList<typename ModelClassType::Listener>*> (&listeners)->callExpectingUnregistration (callback);
    }

private:
    ListenerList<Listener> listeners;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAListenableModelClass)
};

//==============================================================================
class ARADocument: public ARA::PlugIn::Document,
                   public ARAListenableModelClass<ARADocument>
{
public:
    ARADocument (ARADocumentController* documentController);

    class Listener  : public ARAListenableModelClass<ARADocument>::Listener
    {
    public:
       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
        virtual void willBeginEditing (ARADocument* document) {}
        virtual void didEndEditing (ARADocument* document) {}
        virtual void willUpdateDocumentProperties (ARADocument* document, ARADocument::PropertiesPtr newProperties) {}
        virtual void didUpdateDocumentProperties (ARADocument* document) {}
        virtual void didReorderRegionSequencesInDocument (ARADocument* document) {}
        virtual void didAddMusicalContextToDocument (ARADocument* document, ARAMusicalContext* musicalContext) {}
        virtual void willRemoveMusicalContextFromDocument (ARADocument* document, ARAMusicalContext* musicalContext) {}
        virtual void didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* regionSequence) {}
        virtual void willRemoveRegionSequenceFromDocument (ARADocument* document, ARARegionSequence* regionSequence) {}
        virtual void didAddAudioSourceToDocument (ARADocument* document, ARAAudioSource* audioSource) {}
        virtual void willRemoveAudioSourceFromDocument (ARADocument* document, ARAAudioSource* audioSource) {}
        virtual void willDestroyDocument (ARADocument* document) {}
       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };
};

//==============================================================================
class _ARAMusicalContextListener;
class ARAMusicalContext  : public ARA::PlugIn::MusicalContext,
                           public ARAModelClass<_ARAMusicalContextListener>
{
public:
    ARAMusicalContext (ARADocument* document, ARA::ARAMusicalContextHostRef hostRef);

    using Listener = _ARAMusicalContextListener;
};

class _ARAMusicalContextListener
{
public:
    _ARAMusicalContextListener() = default;
    virtual ~_ARAMusicalContextListener() = default;

   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
    virtual void willUpdateMusicalContextProperties (ARAMusicalContext* musicalContext, ARAMusicalContext::PropertiesPtr newProperties) {}
    virtual void didUpdateMusicalContextProperties (ARAMusicalContext* musicalContext) {}
    virtual void doUpdateMusicalContextContent (ARAMusicalContext* musicalContext, ARAContentUpdateScopes scopeFlags) {}
    virtual void willDestroyMusicalContext (ARAMusicalContext* musicalContext) {}
   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
};

//==============================================================================
class _ARARegionSequenceListener;
class ARARegionSequence  : public ARA::PlugIn::RegionSequence,
                           public ARAModelClass<_ARARegionSequenceListener>
{
public:
    ARARegionSequence (ARADocument* document, ARA::ARARegionSequenceHostRef hostRef);

    using Listener = _ARARegionSequenceListener;

    // If all audio sources used by the playback regions in this region sequence have the
    // same sample rate, this rate is returned here, otherwise 0.0 is returned.
    // If the region sequence has no playback regions, this also returns 0.0.
    double getCommonSampleRate();
};

class _ARARegionSequenceListener
{
public:
    _ARARegionSequenceListener() = default;
    virtual ~_ARARegionSequenceListener() = default;

   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
    virtual void willUpdateRegionSequenceProperties (ARARegionSequence* regionSequence, ARARegionSequence::PropertiesPtr newProperties) {}
    virtual void didUpdateRegionSequenceProperties (ARARegionSequence* regionSequence) {}
    virtual void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) {}
    virtual void didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) {}
    virtual void willDestroyRegionSequence (ARARegionSequence* regionSequence) {}
   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
};

class _ARAAudioSourceListener;
class ARAAudioSource  : public ARA::PlugIn::AudioSource,
                        public ARAModelClass<_ARAAudioSourceListener>
{
public:
    ARAAudioSource (ARADocument* document, ARA::ARAAudioSourceHostRef hostRef);

    using Listener = _ARAAudioSourceListener;

    void notifyContentChanged (ARAContentUpdateScopes scopeFlags, bool notifyAllAudioModificationsAndPlaybackRegions = false);
};

class _ARAAudioSourceListener
{
public:
    _ARAAudioSourceListener() = default;
    virtual ~_ARAAudioSourceListener() = default;

   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
    virtual void willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties) {}
    virtual void didUpdateAudioSourceProperties (ARAAudioSource* audioSource) {}
    virtual void doUpdateAudioSourceContent (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags) {}
    virtual void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) {}
    virtual void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) {}
    virtual void doDeactivateAudioSourceForUndoHistory (ARAAudioSource* audioSource, bool deactivate) {}
    virtual void didAddAudioModificationToAudioSource (ARAAudioSource* audioSource, ARAAudioModification* audioModification) {}
    virtual void willRemoveAudioModificationFromAudioSource (ARAAudioSource* audioSource, ARAAudioModification* audioModification) {}
    virtual void willDestroyAudioSource (ARAAudioSource* audioSource) {}
   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
};

//==============================================================================
class _ARAAudioModificationListener;
class ARAAudioModification  : public ARA::PlugIn::AudioModification,
                              public ARAModelClass<_ARAAudioModificationListener>
{
public:
    ARAAudioModification (ARAAudioSource* audioSource, ARA::ARAAudioModificationHostRef hostRef);

    using Listener = _ARAAudioModificationListener;

    void notifyContentChanged (ARAContentUpdateScopes scopeFlags, bool notifyAllPlaybackRegions = false);
};

class _ARAAudioModificationListener
{
public:
    _ARAAudioModificationListener() = default;
    virtual ~_ARAAudioModificationListener() = default;

   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
    virtual void willUpdateAudioModificationProperties (ARAAudioModification* audioModification, ARAAudioModification::PropertiesPtr newProperties) {}
    virtual void didUpdateAudioModificationProperties (ARAAudioModification* audioModification) {}
    virtual void doUpdateAudioModificationContent (ARAAudioModification* audioModification, ARAContentUpdateScopes scopeFlags) {}
    virtual void doDeactivateAudioModificationForUndoHistory (ARAAudioModification* audioModification, bool deactivate) {}
    virtual void didAddPlaybackRegionToAudioModification (ARAAudioModification* audioModification, ARAPlaybackRegion* playbackRegion) {}
    virtual void willRemovePlaybackRegionFromAudioModification (ARAAudioModification* audioModification, ARAPlaybackRegion* playbackRegion) {}
    virtual void willDestroyAudioModification (ARAAudioModification* audioModification) {}
   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
};

//==============================================================================
class _ARAPlaybackRegionListener;
class ARAPlaybackRegion  : public ARA::PlugIn::PlaybackRegion,
                           public ARAModelClass<_ARAPlaybackRegionListener>
{
public:
    ARAPlaybackRegion (ARAAudioModification* audioModification, ARA::ARAPlaybackRegionHostRef hostRef);

    using Listener = _ARAPlaybackRegionListener;

    double getHeadTime() const { return headTime; }
    double getTailTime() const { return tailTime; }
    void setHeadTime (double newHeadTime);
    void setTailTime (double newTailTime);
    void setHeadAndTailTime (double newHeadTime, double newTailTime);

    void notifyContentChanged (ARAContentUpdateScopes scopeFlags);

private:
    double headTime = 0.0;
    double tailTime = 0.0;
};

class _ARAPlaybackRegionListener
{
public:
    _ARAPlaybackRegionListener() = default;
    virtual ~_ARAPlaybackRegionListener() = default;

   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN
    virtual void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) {}
    virtual void didUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion) {}
    virtual void didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags) {}
    virtual void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) {}
   ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
};

} // namespace juce
