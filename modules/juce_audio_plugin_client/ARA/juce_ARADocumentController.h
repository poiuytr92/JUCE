#pragma once

#include "JuceHeader.h"
#include "juce_ARAModelObjects.h"

namespace juce
{

class ARAAudioSourceReader;
class ARAPlaybackRegionReader;
class ARARegionSequenceReader;

class ARADocumentController  : public ARA::PlugIn::DocumentController
{
public:
    using ARA::PlugIn::DocumentController::DocumentController;

    //==============================================================================
    // notify the host and any potential internal reader about content changes
    // Note that while the ARA API allows for specifying update ranges, this feature is not yet
    // in our current plug-in implementation (many hosts do not evaluate it anyways)

    /** notify the host and any listeners of \p audioSource about updates to \p audioSource's content.
        @param audioSource The ARAAudioSource with changed content
        @param notifyAllAudioModificationsAndPlaybackRegions Whether or not to notify \p audioSource's underlying ARA audio modifications and playback regions. 
        @param scopeFlags The scope of the changed content
    
        This must be called by the plug-in model management code on the message thread whenever changing relevant parts of the internal model graph.
        A notification to the host will be enqueued, and send out the next time it polls this document controller for model updates.
        Listeners of \p audioSource however will be notified immediately, even if the call is made outside of a host edit cycle.
        Accordingly, listeners must be either robust regarding this, or the calling code must set up the appropriate internal states.
    */
    void notifyAudioSourceContentChanged (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags, bool notifyAllAudioModificationsAndPlaybackRegions = false);

    /** notify the host and any listeners of \p audioModification about updates to \p audioModification's content.
        @param audioModification The ARAAudioModification with changed content
        @param notifyAllPlaybackRegions Whether or not to notify \p audioModification's underlying ARA playback regions.
        @param scopeFlags The scope of the changed content

        This must be called by the plug-in model management code on the message thread whenever changing relevant parts of the internal model graph.
        A notification to the host will be enqueued, and send out the next time it polls this document controller for model updates.
        Listeners of \p audioSource however will be notified immediately, even if the call is made outside of a host edit cycle.
        Accordingly, listeners must be either robust regarding this, or the calling code must set up the appropriate internal states.
    */
    void notifyAudioModificationContentChanged (ARAAudioModification* audioModification, ARAContentUpdateScopes scopeFlags, bool notifyAllPlaybackRegions = false);

    /** notify the host and any listeners of \p playbackRegion about updates to \p playbackRegion's content.
        @param playbackRegion The ARAPlaybackRegion whose content is changing
        @param scopeFlags The scope of the changed content

        This must be called by the plug-in model management code on the message thread whenever changing relevant parts of the internal model graph.
        A notification to the host will be enqueued, and send out the next time it polls this document controller for model updates.
        Listeners of \p audioSource however will be notified immediately, even if the call is made outside of a host edit cycle.
        Accordingly, listeners must be either robust regarding this, or the calling code must set up the appropriate internal states.
    */
    void notifyPlaybackRegionContentChanged (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags);

    //==============================================================================
    // Override document controller methods here
    // If you are subclassing ARADocumentController, make sure to call the base class
    // implementations of any overridden function, except for any doCreate...()
    // or where specified.

private:
    // some helper macros to ease repeated declaration & implementation of notification functions below:

    // no notification arguments
   #define OVERRIDE_TO_NOTIFY_1(function, ModelObjectPtrType, modelObject) \
    void function (ARA::PlugIn::ModelObjectPtrType modelObject) noexcept override \
    { \
        auto object = static_cast<ARA##ModelObjectPtrType> (modelObject); \
        object->notifyListeners ([&] (std::remove_pointer<ARA##ModelObjectPtrType>::type::Listener& l) { l.function (object); }); \
    }

    // single notification argument, model object version
   #define OVERRIDE_TO_NOTIFY_2(function, ModelObjectPtrType, modelObject, ArgumentType, argument) \
    void function (ARA::PlugIn::ModelObjectPtrType modelObject, ARA::PlugIn::ArgumentType argument) noexcept override \
    { \
        auto object = static_cast<ARA##ModelObjectPtrType> (modelObject); \
        object->notifyListeners ([&] (std::remove_pointer<ARA##ModelObjectPtrType>::type::Listener& l) { l.function (object, static_cast<ARA##ArgumentType> (argument)); }); \
    }

    // single notification argument, non-model object version
   #define OVERRIDE_TO_NOTIFY_3(function, ModelObjectPtrType, modelObject, ArgumentType, argument) \
    void function (ARA::PlugIn::ModelObjectPtrType modelObject, ArgumentType argument) noexcept override \
    { \
        auto object = static_cast<ARA##ModelObjectPtrType> (modelObject); \
        object->notifyListeners ([&] (std::remove_pointer<ARA##ModelObjectPtrType>::type::Listener& l) { l.function (object, argument); }); \
    }

    // single notification argument, version for content updates which drops the currently unsupported range parameter
   #define OVERRIDE_TO_NOTIFY_4(function, ModelObjectPtrType, modelObject, ArgumentType1, argument1, ArgumentType2, argument2) \
    void function (ARA::PlugIn::ModelObjectPtrType modelObject, const ARA::ARA##ArgumentType1 /*argument1*/, ARA::ArgumentType2 argument2) noexcept override \
    { \
        auto object = static_cast<ARA##ModelObjectPtrType> (modelObject); \
        object->notifyListeners ([&] (std::remove_pointer<ARA##ModelObjectPtrType>::type::Listener& l) { l.function (object, argument2); }); \
    }

protected:
    // Model Update Management
    void willBeginEditing() noexcept override;
    void didEndEditing() noexcept override;
    void doNotifyModelUpdates() noexcept override;

    /** Read an ARADocument archive from a juce::InputStream.
    @param input Data stream containing previously persisted data to be used when restoring the ARADocument
    @param filter An optional filter to be applied to the stream; nullptr if no filtering is required

    The optional \p filter parameter can be used to restore a subsection of the document, in which case
    it will not be nullptr. 

    Overriding this method is the preferred way of handling ARA document persistence, but you can also
    override ARA::PlugIn::DocumentController::doRestoreObjectsFromArchive to deal with an ARA archive 
    ARA::PlugIn::HostArchiveReader directly. 
    */
    virtual bool doRestoreObjectsFromStream (InputStream& input, ARA::PlugIn::RestoreObjectsFilter* filter) noexcept;

    /** Write an ARADocument archive to a juce::OutputStream.
    @param output Data stream that should be used to write the persistent ARADocument data
    @param filter An optional filter to be applied to the stream; nullptr if no filtering is required

    The optional \p filter parameter can be used to store a subsection of the document, in which case
    it will not be nullptr. 

    Overriding this method is the preferred way of handling ARA document persistence, but you can also
    override ARA::PlugIn::DocumentController::doStoreObjectsToArchive to deal with an ARA archive 
    ARA::PlugIn::HostArchiveWriter directly. 
    */
    virtual bool doStoreObjectsToStream (OutputStream& output, ARA::PlugIn::StoreObjectsFilter* filter) noexcept;

    bool doRestoreObjectsFromArchive (ARA::PlugIn::HostArchiveReader* archiveReader, ARA::PlugIn::RestoreObjectsFilter* filter) noexcept override;
    bool doStoreObjectsToArchive (ARA::PlugIn::HostArchiveWriter* archiveWriter, ARA::PlugIn::StoreObjectsFilter* filter) noexcept override;

    // Document callbacks
    ARA::PlugIn::Document* doCreateDocument (ARA::PlugIn::DocumentController* documentController) noexcept override;
    OVERRIDE_TO_NOTIFY_3 (willUpdateDocumentProperties, Document*, document, ARADocument::PropertiesPtr, newProperties);
    OVERRIDE_TO_NOTIFY_1 (didUpdateDocumentProperties, Document*, document);
    OVERRIDE_TO_NOTIFY_2 (didAddMusicalContextToDocument, Document*, document, MusicalContext*, musicalContext);
    OVERRIDE_TO_NOTIFY_2 (willRemoveMusicalContextFromDocument, Document*, document, MusicalContext*, musicalContext);
    OVERRIDE_TO_NOTIFY_1 (didReorderMusicalContextsInDocument, Document*, document);
    OVERRIDE_TO_NOTIFY_2 (didAddRegionSequenceToDocument, Document*, document, RegionSequence*, regionSequence);
    OVERRIDE_TO_NOTIFY_2 (willRemoveRegionSequenceFromDocument, Document*, document, RegionSequence*, regionSequence);
    OVERRIDE_TO_NOTIFY_1 (didReorderRegionSequencesInDocument, Document*, document);
    OVERRIDE_TO_NOTIFY_2 (didAddAudioSourceToDocument, Document*, document, AudioSource*, audioSource);
    OVERRIDE_TO_NOTIFY_2 (willRemoveAudioSourceFromDocument, Document*, document, AudioSource*, audioSource);
    OVERRIDE_TO_NOTIFY_1 (willDestroyDocument, Document*, document);

    // MusicalContext callbacks
    ARA::PlugIn::MusicalContext* doCreateMusicalContext (ARA::PlugIn::Document* document, ARA::ARAMusicalContextHostRef hostRef) noexcept override;
    OVERRIDE_TO_NOTIFY_3 (willUpdateMusicalContextProperties, MusicalContext*, musicalContext, ARAMusicalContext::PropertiesPtr, newProperties);
    OVERRIDE_TO_NOTIFY_1 (didUpdateMusicalContextProperties, MusicalContext*, musicalContext);
    OVERRIDE_TO_NOTIFY_4 (doUpdateMusicalContextContent, MusicalContext*, musicalContext, ContentTimeRange*, range, ContentUpdateScopes, scopeFlags);
    OVERRIDE_TO_NOTIFY_1 (willDestroyMusicalContext, MusicalContext*, musicalContext);

    // RegionSequence callbacks
    ARA::PlugIn::RegionSequence* doCreateRegionSequence (ARA::PlugIn::Document* document, ARA::ARARegionSequenceHostRef hostRef) noexcept override;
    OVERRIDE_TO_NOTIFY_3 (willUpdateRegionSequenceProperties, RegionSequence*, regionSequence, ARARegionSequence::PropertiesPtr, newProperties);
    OVERRIDE_TO_NOTIFY_1 (didUpdateRegionSequenceProperties, RegionSequence*, regionSequence);
    OVERRIDE_TO_NOTIFY_2 (didAddPlaybackRegionToRegionSequence, RegionSequence*, regionSequence, PlaybackRegion*, playbackRegion);
    OVERRIDE_TO_NOTIFY_2 (willRemovePlaybackRegionFromRegionSequence, RegionSequence*, regionSequence, PlaybackRegion*, playbackRegion);
    OVERRIDE_TO_NOTIFY_1 (willDestroyRegionSequence, RegionSequence*, regionSequence);

    // AudioSource callbacks
    ARA::PlugIn::AudioSource* doCreateAudioSource (ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef) noexcept override;
    OVERRIDE_TO_NOTIFY_3 (willUpdateAudioSourceProperties, AudioSource*, audioSource, ARAAudioSource::PropertiesPtr, newProperties);
    OVERRIDE_TO_NOTIFY_1 (didUpdateAudioSourceProperties, AudioSource*, audioSource);
    OVERRIDE_TO_NOTIFY_4 (doUpdateAudioSourceContent, AudioSource*, audioSource, ContentTimeRange*, range, ContentUpdateScopes, scopeFlags);
    OVERRIDE_TO_NOTIFY_3 (willEnableAudioSourceSamplesAccess, AudioSource*, audioSource, bool, enable);
    OVERRIDE_TO_NOTIFY_3 (didEnableAudioSourceSamplesAccess, AudioSource*, audioSource, bool, enable);
    OVERRIDE_TO_NOTIFY_2 (didAddAudioModificationToAudioSource, AudioSource*, audioSource, AudioModification*, audioModification);
    OVERRIDE_TO_NOTIFY_2 (willRemoveAudioModificationFromAudioSource, AudioSource*, audioSource, AudioModification*, audioModification);
    OVERRIDE_TO_NOTIFY_3 (doDeactivateAudioSourceForUndoHistory, AudioSource*, audioSource, bool, deactivate);
    OVERRIDE_TO_NOTIFY_1 (willDestroyAudioSource, AudioSource*, audioSource);

    // AudioModification callbacks
    ARA::PlugIn::AudioModification* doCreateAudioModification (ARA::PlugIn::AudioSource* audioSource, ARA::ARAAudioModificationHostRef hostRef, ARA::PlugIn::AudioModification* optionalModificationToClone) noexcept override;
    OVERRIDE_TO_NOTIFY_3 (willUpdateAudioModificationProperties, AudioModification*, audioModification, ARAAudioModification::PropertiesPtr, newProperties);
    OVERRIDE_TO_NOTIFY_1 (didUpdateAudioModificationProperties, AudioModification*, audioModification);
    OVERRIDE_TO_NOTIFY_2 (didAddPlaybackRegionToAudioModification, AudioModification*, audioModification, PlaybackRegion*, playbackRegion);
    OVERRIDE_TO_NOTIFY_2 (willRemovePlaybackRegionFromAudioModification, AudioModification*, audioModification, PlaybackRegion*, playbackRegion);
    OVERRIDE_TO_NOTIFY_3 (doDeactivateAudioModificationForUndoHistory, AudioModification*, audioModification, bool, deactivate);
    OVERRIDE_TO_NOTIFY_1 (willDestroyAudioModification, AudioModification*, audioModification);

    // PlaybackRegion callbacks
    ARA::PlugIn::PlaybackRegion* doCreatePlaybackRegion (ARA::PlugIn::AudioModification* modification, ARA::ARAPlaybackRegionHostRef hostRef) noexcept override;
    /*OVERRIDE_TO_NOTIFY_2*/ void willUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) noexcept override;
    /*OVERRIDE_TO_NOTIFY_1*/ void didUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;
    void doGetPlaybackRegionHeadAndTailTime (ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARATimeDuration* headTime, ARA::ARATimeDuration* tailTime) noexcept override;
    OVERRIDE_TO_NOTIFY_1 (willDestroyPlaybackRegion, PlaybackRegion*, playbackRegion);

    // PlugIn instance role creation
    // these can be overridden with custom types so long as
    // they inherit from our ARA instance role classes
    ARA::PlugIn::PlaybackRenderer* doCreatePlaybackRenderer() noexcept override;
    ARA::PlugIn::EditorRenderer* doCreateEditorRenderer() noexcept override;
    ARA::PlugIn::EditorView* doCreateEditorView() noexcept override;

protected:

    //==============================================================================
    class ArchiveReader     : public InputStream
    {
    public:
        ArchiveReader (ARA::PlugIn::HostArchiveReader*);

        int64 getPosition() override { return (int64) position; }
        int64 getTotalLength() override { return (int64) size; }

        bool setPosition (int64) override;
        bool isExhausted() override;
        int read (void*, int) override;

    private:
        ARA::PlugIn::HostArchiveReader* archiveReader;
        size_t position, size;
    };

    //==============================================================================
    class ArchiveWriter     : public OutputStream
    {
    public:
        ArchiveWriter (ARA::PlugIn::HostArchiveWriter*);

        int64 getPosition() override { return (int64) position; }
        void flush() override {}

        bool setPosition (int64) override;
        bool write (const void*, size_t) override;

    private:
        ARA::PlugIn::HostArchiveWriter* archiveWriter;
        size_t position;
    };

private:
   #undef OVERRIDE_TO_NOTIFY_1
   #undef OVERRIDE_TO_NOTIFY_2
   #undef OVERRIDE_TO_NOTIFY_3
   #undef OVERRIDE_TO_NOTIFY_4

private:
    // this flag is used automatically trigger content update if a property change implies this
    bool _currentPropertyUpdateAffectsContent = false;

    std::map<ARAAudioSource*, ARAContentUpdateScopes> audioSourceUpdates;
    std::map<ARAAudioModification*, ARAContentUpdateScopes> audioModificationUpdates;
    std::map<ARAPlaybackRegion*, ARAContentUpdateScopes> playbackRegionUpdates;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocumentController)
};

} // namespace juce
