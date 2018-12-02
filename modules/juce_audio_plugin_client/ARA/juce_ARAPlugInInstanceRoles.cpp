#include "juce_ARAPlugInInstanceRoles.h"

namespace juce
{

ARAPlaybackRenderer::ARAPlaybackRenderer (ARADocumentController* documentController)
: ARA::PlugIn::PlaybackRenderer (documentController),
  sampleRate (44100),
  numChannels (1),
  maxSamplesPerBlock (1024),
  prepared (false)
{}

void ARAPlaybackRenderer::prepareToPlay (double newSampleRate, int newNumChannels, int newMaxSamplesPerBlock)
{
    sampleRate = newSampleRate;
    numChannels = newNumChannels;
    maxSamplesPerBlock = newMaxSamplesPerBlock;

    setRendering(true);
    prepared = true;
}

void ARAPlaybackRenderer::releaseResources()
{
    prepared = false;
    setRendering(false);
}

bool ARAPlaybackRenderer::processBlock (AudioBuffer<float>& buffer, int64 /*timeInSamples*/, bool /*isPlayingBack*/)
{
    jassert (buffer.getNumSamples() <= getMaxSamplesPerBlock());
    for (int c = 0; c < buffer.getNumChannels(); c++)
        FloatVectorOperations::clear (buffer.getArrayOfWritePointers()[c], buffer.getNumSamples());
    return true;
}

void ARAPlaybackRenderer::addPlaybackRegion (ARAPlaybackRegion* playbackRegion) noexcept
{
    ARA::PlugIn::PlaybackRenderer::addPlaybackRegion (ARA::PlugIn::toRef (playbackRegion));
}
void ARAPlaybackRenderer::removePlaybackRegion (ARAPlaybackRegion* playbackRegion) noexcept
{
    ARA::PlugIn::PlaybackRenderer::removePlaybackRegion (ARA::PlugIn::toRef (playbackRegion));
}

//==============================================================================

ARAEditorRenderer::ARAEditorRenderer (ARADocumentController* documentController)
: ARA::PlugIn::EditorRenderer (documentController)
{}

//==============================================================================

ARAEditorView::ARAEditorView (ARA::PlugIn::DocumentController* documentController) noexcept
: ARA::PlugIn::EditorView (documentController)
{}

void ARAEditorView::doNotifySelection (const ARA::PlugIn::ViewSelection* currentSelection) noexcept
{
    for (Listener* l : listeners)
        l->onNewSelection (*currentSelection);
}

void ARAEditorView::doNotifyHideRegionSequences (std::vector<ARA::PlugIn::RegionSequence*> const& regionSequences) noexcept 
{
    for (Listener* l : listeners)
        l->onHideRegionSequences (reinterpret_cast<std::vector<ARARegionSequence*> const&> (regionSequences));
}

void ARAEditorView::addSelectionListener (Listener* l) 
{ 
    listeners.push_back (l); 
}

void ARAEditorView::removeSelectionListener (Listener* l) 
{ 
    ARA::find_erase (listeners, l); 
}

} // namespace juce
