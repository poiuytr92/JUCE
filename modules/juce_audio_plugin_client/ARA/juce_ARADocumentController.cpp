#include "juce_ARADocumentController.h"
#include "juce_ARARegionSequence.h"
#include "juce_ARAAudioSource.h"
#include "juce_ARAPlaybackRegion.h"
#include "juce_ARAAudioReaders.h"
#include "juce_ARAPlugInInstanceRoles.h"

const ARA::ARAFactory* ARA::PlugIn::DocumentController::getARAFactory() noexcept
{
    using namespace ARA;

    static ARAFactory* factory = nullptr;
    if (factory == nullptr)
    {
        factory = new SizedStruct<ARA_MEMBER_PTR_ARGS (ARAFactory, supportedPlaybackTransformationFlags)>(
                                                        // Supported API generations
                                                        kARAAPIGeneration_2_0_Draft, kARAAPIGeneration_2_0_Final,
                                                        // Factory ID
                                                        JucePlugin_ARAFactoryID,
                                                        // ARA lifetime management functions
                                                        ARAInitialize, ARAUninitialize,
                                                        // Strings for user dialogs
                                                        JucePlugin_Name, JucePlugin_Manufacturer,
                                                        JucePlugin_ManufacturerWebsite, JucePlugin_VersionString,
                                                        // DocumentController factory function
                                                        ARACreateDocumentControllerWithDocumentInstance,
                                                        // Document archive IDs
                                                        // TODO JUCE_ARA add a way to update compatible archive IDs and count if needed!
                                                        JucePlugin_ARADocumentArchiveID, 0U, nullptr,
                                                        // Analyzeable content types - will be updated below
                                                        0U, nullptr,
                                                        // Playback transformation flags - will be updated below
                                                        0);

        // Update analyzeable content types
        static std::vector<ARAContentType> contentTypes;
        static ARAContentType araContentVars[]{
            kARAContentTypeNotes,
            kARAContentTypeTempoEntries,
            kARAContentTypeBarSignatures,
            kARAContentTypeSignatures,
            kARAContentTypeStaticTuning,
            kARAContentTypeDynamicTuningOffsets,
            kARAContentTypeKeySignatures,
            kARAContentTypeSheetChords
        };
        for (size_t i = 0; i < sizeof (araContentVars) / sizeof (ARAContentType); i++)
        {
            if (JucePlugin_ARAContentTypes & (1 << i))
                contentTypes.push_back (araContentVars[i]);
        }

        factory->analyzeableContentTypesCount = (ARASize) contentTypes.size();
        factory->analyzeableContentTypes = contentTypes.data();

        // Update playback transformation flags
        static ARAPlaybackTransformationFlags araPlaybackTransformations[]{
            kARAPlaybackTransformationTimestretch,
            kARAPlaybackTransformationTimestretchReflectingTempo,
            kARAPlaybackTransformationContentBasedFadeAtTail,
            kARAPlaybackTransformationContentBasedFadeAtHead
        };

        factory->supportedPlaybackTransformationFlags = 0;
        for (size_t i = 0; i < sizeof (araPlaybackTransformations) / sizeof (ARAPlaybackTransformationFlags); i++)
        {
            if (JucePlugin_ARATransformationFlags & (1 << i))
                factory->supportedPlaybackTransformationFlags |= araPlaybackTransformations[i];
        }

        // TODO JUCE_ARA
        // Any other factory fields? Algorithm selection?
    }

    return factory;
}

namespace juce
{

//==============================================================================

ARA::PlugIn::MusicalContext* ARADocumentController::doCreateMusicalContext (ARA::PlugIn::Document* document, ARA::ARAMusicalContextHostRef hostRef) noexcept
{
    return new ARAMusicalContext (document, hostRef);
}

void ARADocumentController::willUpdateMusicalContextProperties (ARA::PlugIn::MusicalContext* musicalContext, ARA::PlugIn::PropertiesPtr<ARA::ARAMusicalContextProperties> newProperties) noexcept
{
    static_cast<ARAMusicalContext*> (musicalContext)->willUpdateMusicalContextProperties (newProperties);
}

void ARADocumentController::didUpdateMusicalContextProperties (ARA::PlugIn::MusicalContext* musicalContext) noexcept
{
    static_cast<ARAMusicalContext*> (musicalContext)->didUpdateMusicalContextProperties ();
}

void ARADocumentController::doUpdateMusicalContextContent (ARA::PlugIn::MusicalContext* musicalContext, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
    static_cast<ARAMusicalContext*> (musicalContext)->doUpdateMusicalContextContent (range, scopeFlags);
}

void ARADocumentController::willDestroyMusicalContext (ARA::PlugIn::MusicalContext* musicalContext) noexcept
{
    static_cast<ARAMusicalContext*> (musicalContext)->willDestroyMusicalContext ();
}

//==============================================================================

ARA::PlugIn::RegionSequence* ARADocumentController::doCreateRegionSequence (ARA::PlugIn::Document* document, ARA::ARARegionSequenceHostRef hostRef) noexcept
{
    return new ARARegionSequence (document, hostRef);
}

void ARADocumentController::willUpdateRegionSequenceProperties (ARA::PlugIn::RegionSequence* regionSequence, ARA::PlugIn::PropertiesPtr<ARA::ARARegionSequenceProperties> newProperties) noexcept
{
    static_cast<ARARegionSequence*> (regionSequence)->willUpdateRegionSequenceProperties (newProperties);
}

void ARADocumentController::didUpdateRegionSequenceProperties (ARA::PlugIn::RegionSequence* regionSequence) noexcept
{
    static_cast<ARARegionSequence*> (regionSequence)->didUpdateRegionSequenceProperties ();
}

void ARADocumentController::willDestroyRegionSequence (ARA::PlugIn::RegionSequence* regionSequence) noexcept
{
    static_cast<ARARegionSequence*> (regionSequence)->willDestroyRegionSequence ();
}

//==============================================================================

ARA::PlugIn::AudioSource* ARADocumentController::doCreateAudioSource (ARA::PlugIn::Document *document, ARA::ARAAudioSourceHostRef hostRef) noexcept
{
    return new ARAAudioSource (document, hostRef);
}

void ARADocumentController::willUpdateAudioSourceProperties (
    ARA::PlugIn::AudioSource* audioSource,
    ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties> newProperties) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->willUpdateAudioSourceProperties (newProperties);
}

void ARADocumentController::didUpdateAudioSourceProperties (ARA::PlugIn::AudioSource* audioSource) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->didUpdateAudioSourceProperties ();
}

void ARADocumentController::doUpdateAudioSourceContent (ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->doUpdateAudioSourceContent (range, scopeFlags);
}

void ARADocumentController::willEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->willEnableAudioSourceSamplesAccess (enable);
}

void ARADocumentController::didEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->didEnableAudioSourceSamplesAccess (enable);
}

void ARADocumentController::doDeactivateAudioSourceForUndoHistory (ARA::PlugIn::AudioSource* audioSource, bool deactivate) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->doDeactivateAudioSourceForUndoHistory (deactivate);
}

void ARADocumentController::willDestroyAudioSource (ARA::PlugIn::AudioSource* audioSource) noexcept
{
    static_cast<ARAAudioSource*> (audioSource)->willDestroyAudioSource ();
}

AudioFormatReader* ARADocumentController::createAudioSourceReader (ARAAudioSource* audioSource)
{
    return new ARAAudioSourceReader (audioSource);
}

BufferingAudioSource* ARADocumentController::createBufferingAudioSourceReader (ARAAudioSource* audioSource, TimeSliceThread& thread, int bufferSize)
{
    return new BufferingAudioSource (new AudioFormatReaderSource (createAudioSourceReader (audioSource), true), thread, true, bufferSize, audioSource->getChannelCount ());
}

//==============================================================================

ARA::PlugIn::AudioModification* ARADocumentController::doCreateAudioModification (ARA::PlugIn::AudioSource* audioSource, ARA::ARAAudioModificationHostRef hostRef) noexcept
{
    return new ARAAudioModification (audioSource, hostRef);
}

void ARADocumentController::willUpdateAudioModificationProperties (ARA::PlugIn::AudioModification* audioModification, ARA::PlugIn::PropertiesPtr<ARA::ARAAudioModificationProperties> newProperties) noexcept
{
    static_cast<ARAAudioModification*> (audioModification)->willUpdateAudioModificationProperties (newProperties);
}

void ARADocumentController::didUpdateAudioModificationProperties (ARA::PlugIn::AudioModification* audioModification) noexcept
{
    static_cast<ARAAudioModification*> (audioModification)->didUpdateAudioModificationProperties ();
}

void ARADocumentController::doDeactivateAudioModificationForUndoHistory (ARA::PlugIn::AudioModification* audioModification, bool deactivate) noexcept
{
    static_cast<ARAAudioModification*> (audioModification)->doDeactivateAudioModificationForUndoHistory (deactivate);
}

void ARADocumentController::willDestroyAudioModification (ARA::PlugIn::AudioModification* audioModification) noexcept
{
    static_cast<ARAAudioModification*> (audioModification)->willDestroyAudioModification ();
}

//==============================================================================

ARA::PlugIn::PlaybackRegion* ARADocumentController::doCreatePlaybackRegion (ARA::PlugIn::AudioModification* modification, ARA::ARAPlaybackRegionHostRef hostRef) noexcept
{
    return new ARAPlaybackRegion (modification, hostRef);
}

void ARADocumentController::willUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties> newProperties) noexcept
{
    jassert (dynamic_cast<ARAPlaybackRegion*> (playbackRegion) != nullptr);
    auto araPlaybackRegion = static_cast<ARAPlaybackRegion*> (playbackRegion);
    araPlaybackRegion->willUpdatePlaybackRegionProperties (newProperties);

    // if the region sequence is changing, call the corresponding ARARegionSequence function
    auto curSequence = static_cast<ARARegionSequence*> (araPlaybackRegion->getRegionSequence ());
    auto newSequence = static_cast<ARARegionSequence*> (ARA::PlugIn::fromRef (newProperties->regionSequenceRef));
    if (curSequence != newSequence)
    {
        if (curSequence)
            curSequence->willRemovePlaybackRegion (araPlaybackRegion);
        if (newSequence)
            newSequence->willAddPlaybackRegion (araPlaybackRegion);

        jassert (previousSequencesForUpdatingRegions.count (araPlaybackRegion) == 0);
        previousSequencesForUpdatingRegions[araPlaybackRegion] = curSequence;
    }
}

void ARADocumentController::didUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    auto araPlaybackRegion = static_cast<ARAPlaybackRegion*> (playbackRegion);
    araPlaybackRegion->didUpdatePlaybackRegionProperties ();

    // if the region sequence is changing, call the corresponding ARARegionSequence function
    auto curSequence = static_cast<ARARegionSequence*> (araPlaybackRegion->getRegionSequence ());
    auto itPrevSequence = previousSequencesForUpdatingRegions.find (araPlaybackRegion);
    if (itPrevSequence != previousSequencesForUpdatingRegions.end ())
    {
        auto prevSequence = itPrevSequence->second;
        if (curSequence)
            curSequence->didAddPlaybackRegion (araPlaybackRegion);
        if (prevSequence)
            prevSequence->didRemovePlaybackRegion (araPlaybackRegion);

        previousSequencesForUpdatingRegions.erase (itPrevSequence);
    }
}

void ARADocumentController::willDestroyPlaybackRegion (ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    static_cast<ARAPlaybackRegion*> (playbackRegion)->willDestroyPlaybackRegion ();
}

//==============================================================================

ARA::PlugIn::PlaybackRenderer* ARADocumentController::doCreatePlaybackRenderer () noexcept
{
    return new ARAPlaybackRenderer (this);
}

ARA::PlugIn::EditorRenderer* ARADocumentController::doCreateEditorRenderer () noexcept
{
    return new ARAEditorRenderer (this);
}

ARA::PlugIn::EditorView* ARADocumentController::doCreateEditorView () noexcept
{
    return new ARAEditorView (this);
}

} // namespace juce
