#pragma once

#include "JuceHeader.h"
#include "RegionSequenceView.h"

//==============================================================================
/**
    RegionSequenceView
    JUCE component used to display ARA playback regions in a host document
    along with their name, color, and selection state
*/
class PlaybackRegionView    : public Component,
                              private ChangeListener,
                              private ARAEditorView::Listener,
                              private ARADocument::Listener,
                              private ARAAudioSource::Listener,
                              private ARAPlaybackRegion::Listener
{
public:
    PlaybackRegionView (ARASampleProjectAudioProcessorEditor* editor, ARAPlaybackRegion* region);
    ~PlaybackRegionView();

    ARAPlaybackRegion* getPlaybackRegion() const { return playbackRegion; }
    void getTimeRange (double& startTime, double& endTime) const;

    void paint (Graphics&) override;

    // ChangeListener overrides
    void changeListenerCallback (ChangeBroadcaster*) override;

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& currentSelection) override;

    // ARADocument::Listener overrides: used to check if our reader has been invalidated
    void didEndEditing (ARADocument* document) override;

    // ARAAudioSource::Listener overrides
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) override;

    // ARAPlaybackRegion::Listener overrides
    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) override;

private:
    void recreatePlaybackRegionReader();

private:
    ARASampleProjectAudioProcessorEditor* editorComponent;
    ARAPlaybackRegion* playbackRegion;
    ARAPlaybackRegionReader* playbackRegionReader = nullptr;  // careful: "weak" pointer, actual pointer is owned by our audioThumb
    bool isSelected = false;

    AudioFormatManager audioFormatManger;
    AudioThumbnailCache audioThumbCache;
    AudioThumbnail audioThumb;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionView)
};
