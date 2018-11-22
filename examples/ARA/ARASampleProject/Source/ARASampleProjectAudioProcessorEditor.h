#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "ARASampleProjectAudioProcessor.h"
#include "ARASampleProjectDocumentController.h"
#include "RegionSequenceView.h"

#if ! JucePlugin_Enable_ARA
    #error "bad project configuration, JucePlugin_Enable_ARA is required for compiling this class"
#endif

//==============================================================================
/** 
    Editor class for ARA sample project
    This class manages the UI we use to display region sequences in the 
    ARA document as well as their current selection state
*/
class ARASampleProjectAudioProcessorEditor: public AudioProcessorEditor,
                                            public AudioProcessorEditorARAExtension,    // Provides access to the ARA EditorView instance
                                            public ARAEditorView::Listener,             // Receives ARA selection notifications
                                            public ARARegionSequence::Listener,         // Receives ARA region sequence update notifications
                                            public ARADocument::Listener                // Receives ARA document controller update notifications
{
public:
    ARASampleProjectAudioProcessorEditor (ARASampleProjectAudioProcessor&);
    ~ARASampleProjectAudioProcessorEditor();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& currentSelection) override;

    // ARADocument::Listener overrides
    void doEndEditing (ARADocument* document) override;

    // ARARegionSequence::Listener overrides
    void didUpdateRegionSequenceProperties (ARARegionSequence* regionSequence) override;
    void willDestroyRegionSequence (ARARegionSequence* regionSequence) override;

    // function to flag that our view needs to be rebuilt
    void setDirty() { isViewDirty = true; }

private:
    void rebuildView();

private:

    // we'll be displaying all region sequences in the document in a scrollable view
    Viewport regionSequenceViewPort;
    Component regionSequenceListView;

    juce::OwnedArray <RegionSequenceView> regionSequenceViews;

    bool isViewDirty;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARASampleProjectAudioProcessorEditor)
};
