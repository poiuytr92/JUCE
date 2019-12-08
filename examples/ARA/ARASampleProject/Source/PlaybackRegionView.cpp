#include "PlaybackRegionView.h"
#include "DocumentView.h"
#include "ARASampleProjectAudioProcessor.h"

//==============================================================================
PlaybackRegionView::PlaybackRegionView (DocumentView& docView, ARAPlaybackRegion* region)
    : documentView (docView),
      playbackRegion (region),
      audioThumb (128, docView.getAudioFormatManger(), *sharedAudioThumbnailCache)
{
    audioThumb.addChangeListener (this);

    documentView.getARAEditorView()->addListener (this);
    onNewSelection (documentView.getARAEditorView()->getViewSelection());

    playbackRegion->getRegionSequence()->getDocument<ARADocument>()->addListener (this);
    playbackRegion->getAudioModification<ARAAudioModification>()->addListener (this);
    playbackRegion->getAudioModification()->getAudioSource<ARAAudioSource>()->addListener (this);
    playbackRegion->addListener (this);

    recreatePlaybackRegionReader();
}

PlaybackRegionView::~PlaybackRegionView()
{
    documentView.getARAEditorView()->removeListener (this);

    playbackRegion->removeListener (this);
    playbackRegion->getAudioModification<ARAAudioModification>()->removeListener (this);
    playbackRegion->getAudioModification()->getAudioSource<ARAAudioSource>()->removeListener (this);
    playbackRegion->getRegionSequence()->getDocument<ARADocument>()->removeListener (this);

    destroyPlaybackRegionReader();
    audioThumb.removeChangeListener (this);
}

//==============================================================================
void PlaybackRegionView::paint (Graphics& g)
{
    auto rect = getLocalBounds();
    g.setColour (isSelected ? Colours::yellow : Colours::black);
    g.drawRect (rect);
    rect.reduce (1, 1);

    const Colour regionColour = convertOptionalARAColour (playbackRegion->getEffectiveColor());
    g.setColour (regionColour);
    g.fillRect (rect);

    if (playbackRegion->getAudioModification()->getAudioSource()->isSampleAccessEnabled())
    {
        auto clipBounds = g.getClipBounds();
        if (clipBounds.getWidth() > 0)
        {
            const auto convertedBounds = clipBounds + getBoundsInParent().getPosition();
            const double startTime = documentView.getPlaybackRegionsViewsTimeForX (convertedBounds.getX());
            const double endTime = documentView.getPlaybackRegionsViewsTimeForX (convertedBounds.getRight());

            const auto regionTimeRange = getTimeRange();

            auto drawBounds = getBounds() - getPosition();
            drawBounds.setHorizontalRange (clipBounds.getHorizontalRange());
            g.setColour (regionColour.contrasting (0.7f));
            audioThumb.drawChannels (g, drawBounds, startTime - regionTimeRange.getStart(), endTime - regionTimeRange.getStart(), 1.0f);
        }
    }
    else
    {
        g.setColour (regionColour.contrasting (1.0f));
        g.setFont (Font (12.0f));
        g.drawText ("Access Disabled", getBounds(), Justification::centred);
    }

    g.setColour (regionColour.contrasting (1.0f));
    g.setFont (Font (12.0f));
    g.drawText (convertOptionalARAString (playbackRegion->getEffectiveName()), rect, Justification::topLeft);
}

//==============================================================================
void PlaybackRegionView::changeListenerCallback (ChangeBroadcaster* /*broadcaster*/)
{
    // our thumb nail has changed
    repaint();
}

void PlaybackRegionView::onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection)
{
    bool selected = ARA::contains (viewSelection.getPlaybackRegions(), playbackRegion);
    if (selected != isSelected)
    {
        isSelected = selected;
        repaint();
    }
}

void PlaybackRegionView::didEndEditing (ARADocument* /*document*/)
{
    // our reader will pick up any changes in audio samples or region time range
    if (playbackRegionReader ==  nullptr || ! playbackRegionReader->isValid())
    {
        recreatePlaybackRegionReader();
        documentView.invalidateTimeRange();
        repaint();
    }
}

void PlaybackRegionView::willEnableAudioSourceSamplesAccess (ARAAudioSource* /*audioSource*/, bool enable)
{
    // AudioThumbnail does not handle "pausing" access, so we clear it if any data is still pending, and recreate it when access is reenabled
    if (! enable && ! audioThumb.isFullyLoaded())
        destroyPlaybackRegionReader();
}

void PlaybackRegionView::didEnableAudioSourceSamplesAccess (ARAAudioSource* /*audioSource*/, bool enable)
{
    // check whether we need to recreate the thumb data because it hadn't been loaded completely when access was disabled
    // (if we're inside a host edit cycle, we'll wait until it has completed to catch all changes in one update)
    if (enable && playbackRegionReader == nullptr && ! playbackRegion->getDocumentController()->isHostEditingDocument())
        recreatePlaybackRegionReader();

    repaint();
}

void PlaybackRegionView::willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties)
{
    if (playbackRegion->getName() == nullptr && playbackRegion->getAudioModification()->getName() == nullptr && newProperties->name != audioSource->getName())
        repaint();
}

void PlaybackRegionView::willUpdateAudioModificationProperties (ARAAudioModification* audioModification, ARAAudioModification::PropertiesPtr newProperties)
{
    if (playbackRegion->getName() == nullptr && newProperties->name != audioModification->getName())
        repaint();
}

void PlaybackRegionView::willUpdatePlaybackRegionProperties (ARAPlaybackRegion* /*playbackRegion*/, ARAPlaybackRegion::PropertiesPtr newProperties)
{
    if (playbackRegion->getName() != newProperties->name || playbackRegion->getColor() != newProperties->color)
        repaint();
}

void PlaybackRegionView::didUpdatePlaybackRegionContent (ARAPlaybackRegion* /*playbackRegion*/, ARAContentUpdateScopes scopeFlags)
{
    // Our reader catches this too, but we only check for its validity after host edits.
    // If the update is triggered inside the plug-in, we need to update the view from this call
    // (unless we're within a host edit already).
    if (scopeFlags.affectSamples() && ! playbackRegion->getDocumentController()->isHostEditingDocument())
    {
        recreatePlaybackRegionReader();
        repaint();
    }
}

//==============================================================================
void PlaybackRegionView::destroyPlaybackRegionReader()
{
    if (playbackRegionReader == nullptr)
        return;

    sharedAudioThumbnailCache->removeThumb (reinterpret_cast<intptr_t> (playbackRegionReader));
    audioThumb.clear();
    playbackRegionReader = nullptr;
}

void PlaybackRegionView::recreatePlaybackRegionReader()
{
    destroyPlaybackRegionReader();

    // Create an audio processor for rendering our region
    // We're disabling buffered audio source reading because the thumb nail cache will do buffering.
    auto audioProcessor = std::make_unique<ARASampleProjectAudioProcessor> (false);
    const auto sampleRate = playbackRegion->getAudioModification()->getAudioSource()->getSampleRate();
    const auto numChannels = playbackRegion->getAudioModification()->getAudioSource()->getChannelCount();
    const auto channelSet = AudioChannelSet::canonicalChannelSet (numChannels);
    for (int i = 0; i < audioProcessor->getBusCount (false); i++)
        audioProcessor->setChannelLayoutOfBus (false, i, channelSet);
    audioProcessor->setProcessingPrecision (AudioProcessor::singlePrecision);
    audioProcessor->setRateAndBufferSizeDetails (sampleRate, 4*1024);
    audioProcessor->setNonRealtime (true);

    // Create a playback region reader using this processor for our audio thumb
    playbackRegionReader = new ARAPlaybackRegionReader (std::move (audioProcessor), { playbackRegion });
    audioThumb.setReader (playbackRegionReader, reinterpret_cast<intptr_t> (playbackRegionReader));

    // TODO JUCE_ARA see juce_AudioThumbnail.cpp, line 122: AudioThumbnail handles zero-length sources
    // by deleting the reader, therefore we must clear our "weak" pointer to the reader in this case.
    if (playbackRegionReader->lengthInSamples <= 0)
        playbackRegionReader = nullptr;
}
