/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for an ARA document controller implementation. 

  ==============================================================================
*/

#include "PluginARADocumentController.h"
#include "PluginARAPlaybackRenderer.h"
#include "PluginARAEditorView.h"

ARASampleProjectDocumentController::ARASampleProjectDocumentController() noexcept
: juce::ARADocumentController()
{
    araAudioSourceReadingThread.reset (new TimeSliceThread (String (JucePlugin_Name) + " ARA Sample Reading Thread"));
    araAudioSourceReadingThread->startThread();
}

// return an instance of our editor view implementation
ARA::PlugIn::EditorView* ARASampleProjectDocumentController::doCreateEditorView() noexcept
{
    return new ARASampleProjectEditorView (this);
}

// return an instance of our playback renderer implementation
ARA::PlugIn::PlaybackRenderer* ARASampleProjectDocumentController::doCreatePlaybackRenderer() noexcept
{
    return new ARASampleProjectPlaybackRenderer (this, *araAudioSourceReadingThread.get(), (1 << 16));
}

void ARASampleProjectDocumentController::doEndEditing () noexcept
{
    for (ARA::PlugIn::PlaybackRenderer* playbackRenderer : getPlaybackRenderers ())
        static_cast<ARASampleProjectPlaybackRenderer*>(playbackRenderer)->ensureReadersForAllPlaybackRegions ();
}

//==============================================================================
// This creates new instances of the document controller..
ARA::PlugIn::DocumentController* ARA::PlugIn::DocumentController::doCreateDocumentController() noexcept
{
    return new ARASampleProjectDocumentController();
};


