#pragma once

#include "JuceHeader.h"
#include "RegionSequenceView.h"

class PlaybackRegionView: public Component, 
                          public juce::ChangeListener,
                          public ARADocument::Listener,
                          public ARAAudioSource::Listener,
                          public ARAPlaybackRegion::Listener
{
public:
    PlaybackRegionView (ARASampleProjectAudioProcessorEditor* editor, ARAPlaybackRegion* region);
    ~PlaybackRegionView();

    void paint (Graphics&) override;
    void changeListenerCallback (ChangeBroadcaster*) override;

    // ARAAudioSource::Listener overrides
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) override;

    // ARAPlaybackRegion::Listener overrides
    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) override;

    ARAPlaybackRegion* getPlaybackRegion() const { return playbackRegion; }

    void setIsSelected (bool value);
    bool getIsSelected () const { return isSelected; }

    double getStartInSeconds() const;
    double getLengthInSeconds() const;
    double getEndInSeconds() const;

    // use this to check if our reader's been invalidated
    void doEndEditing (ARADocument* document) override;

private:
    void recreatePlaybackRegionReader();

private:
    ARASampleProjectAudioProcessorEditor* editorComponent;
    ARAPlaybackRegion* playbackRegion;
    ARAPlaybackRegionReader* playbackRegionReader;  // careful: "weak" pointer, actual pointer is owned by our audioThumb
    bool isSelected;
    bool isSampleAccessEnabled;

    juce::AudioFormatManager audioFormatManger;
    juce::AudioThumbnailCache audioThumbCache;
    juce::AudioThumbnail audioThumb;
};
