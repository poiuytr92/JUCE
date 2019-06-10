#include "RulersView.h"

#include "ARA_Library/Utilities/ARAPitchInterpretation.h"

static constexpr int lightLineWidth = 1;
static constexpr int heavyLineWidth = 3;

//==============================================================================
RulersView::RulersView (TimelineViewport& timeline, const AudioPlayHead::CurrentPositionInfo* hostPosition)
    : timeline (timeline),
      timeMapper (static_cast<const ARASecondsPixelMapper&>(timeline.getPixelMapper())),
      optionalHostPosition (hostPosition),
      shouldShowLocators (true)
{
    setColour (rulersBackground, Colours::transparentBlack);
    lastPaintedPosition.resetToDefault();
    startTimerHz (10);
}

void RulersView::timerCallback()
{
    if (isLocatorsVisible() && optionalHostPosition != nullptr && (lastPaintedPosition.ppqLoopStart != optionalHostPosition->ppqLoopStart ||
        lastPaintedPosition.ppqLoopEnd != optionalHostPosition->ppqLoopEnd ||
        lastPaintedPosition.isLooping  != optionalHostPosition->isLooping))
    {
        repaint();
    }
}

//==============================================================================
void RulersView::paint (juce::Graphics& g)
{
    g.fillAll (findColour (rulersBackground));
    // locators
    if (isLocatorsVisible() && optionalHostPosition != nullptr)
    {
        const auto bounds = g.getClipBounds();
        lastPaintedPosition = *optionalHostPosition;
        const int startX = getRulerHeaderWidth() + timeMapper.getPixelForQuarter(lastPaintedPosition.ppqLoopStart);
        const int endX = getRulerHeaderWidth() + timeMapper.getPixelForQuarter(lastPaintedPosition.ppqLoopEnd);
        g.setColour (lastPaintedPosition.isLooping ? Colours::skyblue.withAlpha (0.3f) : Colours::white.withAlpha (0.3f));
        g.fillRect (startX, bounds.getY(), endX - startX, bounds.getHeight());
    }
}

void RulersView::resized()
{
    int y = 0;
    int totalVisibleRulers = 0;
    for (auto ruler : rulers)
    {
        totalVisibleRulers += ruler->isVisible() ? 1 : 0;
    }

    const int rulerHeight = totalVisibleRulers == 0 ? 0 : getLocalBounds().getHeight() / totalVisibleRulers;
    for (auto ruler : rulers)
    {
        ruler->setBounds (0, y, getLocalBounds().getWidth(), rulerHeight);
        y += rulerHeight;
    }
}

//==============================================================================

// TODO JUCE_ARA current rulers do not intercept or interact with UI
// so they're all set to NOT intercept mouse or keyboard.
// the position on mouse option might need to be refactored and
// be an optional setting to allow custom implementation.

void RulersView::mouseDown (const MouseEvent& event)
{
    const auto pos = event.position.x;
    if (pos < getRulerHeaderWidth())
        return;
    // use mouse click to set the playhead position in the host (if they provide a playback controller interface)
    if (auto* musicalCtx = timeMapper.getCurrentMusicalContext())
    {
        auto playbackController = musicalCtx->getDocument()->getDocumentController()->getHostPlaybackController();
        if (playbackController != nullptr)
            playbackController->requestSetPlaybackPosition (timeMapper.getPositionForPixel (roundToInt (jmax (0.0f ,pos - getRulerHeaderWidth()))));
    }
}

void RulersView::mouseDoubleClick (const MouseEvent& event)
{
    if (event.position.x < getRulerHeaderWidth())
        return;
    // use mouse double click to start host playback (if they provide a playback controller interface)
    if (auto* musicalCtx = timeMapper.getCurrentMusicalContext())
    {
        auto playbackController = musicalCtx->getDocument()->getDocumentController()->getHostPlaybackController();
        if (playbackController != nullptr)
            playbackController->requestStartPlayback();
    }
}

void RulersView::addRulerComponent (juce::Component *rulerToOwn)
{
    rulers.add (rulerToOwn);
    addAndMakeVisible (rulerToOwn);
}

void RulersView::clear()
{
    for (auto* x : rulers)
        removeChildComponent (x);
    rulers.clear();
}

void RulersView::addDefaultRulers()
{
    addRulerComponent (new ARASecondsRuler (*this));
    addRulerComponent (new ARABeatsRuler (*this));
    addRulerComponent (new ARAChordsRuler (*this));
}

//==============================================================================
// seconds ruler: one tick for each second
RulersView::ARASecondsRuler::ARASecondsRuler (const RulersView &owner) : rulersView (owner)
{
    setWantsKeyboardFocus (false);
    setInterceptsMouseClicks (false, false);
}

void RulersView::ARASecondsRuler::paint (juce::Graphics& g)
{
    const auto bounds = g.getClipBounds();
    const auto visibleRange = rulersView.timeline.getVisibleRange();
    const auto& timeMapper = rulersView.timeMapper;

    // draw ruler -
    g.setColour (Colours::lightslategrey);
    const int secondsRulerHeight = getBounds().getHeight();
    RectangleList<int> rects;
    const int endTime = roundToInt (floor (visibleRange.getEnd()));
    for (int time = roundToInt (ceil (visibleRange.getStart())); time <= endTime; ++time)
    {
        const int lineWidth = (time % 60 == 0) ? heavyLineWidth : lightLineWidth;
        const int lineHeight = (time % 10 == 0) ? secondsRulerHeight : secondsRulerHeight / 2;
        const int x = rulersView.getRulerHeaderWidth() + timeMapper.getPixelForPosition (time);
        rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, bounds.getHeight() - lineHeight, lineWidth, lineHeight));
    }
    g.fillRectList (rects);

    // draw ruler header -
    g.setColour (juce::Colours::white);
    g.drawText (" seconds", bounds, Justification::centredLeft);

    // borders
    g.setColour (juce::Colours::darkgrey);
    g.drawRect (bounds);
}

// beat ruler: evaluates tempo and bar signatures to draw a line for each beat
RulersView::ARABeatsRuler::ARABeatsRuler (const RulersView &owner) : rulersView (owner)
{
    setWantsKeyboardFocus (false);
    setInterceptsMouseClicks (false, false);
}

void RulersView::ARABeatsRuler::paint (juce::Graphics& g)
{
    const auto bounds = g.getClipBounds();
    const auto visibleRange = rulersView.timeline.getVisibleRange();
    const auto& timeMapper = rulersView.timeMapper;

    String rulerName;
    if (timeMapper.canTempoMap())
    {
        rulerName = " beats";
        g.setColour (Colours::lightslategrey);
        RectangleList<int> rects;
        const int beatsRulerHeight = bounds.getHeight();
        const double beatStart = timeMapper.getBeatForQuarter (timeMapper.getQuarterForTime (visibleRange.getStart()));
        const double beatEnd = timeMapper.getBeatForQuarter (timeMapper.getQuarterForTime (visibleRange.getEnd()));
        const int endBeat = roundToInt (floor (beatEnd));
        for (int beat = roundToInt (ceil (beatStart)); beat <= endBeat; ++beat)
        {
            const auto quarterPos = timeMapper.getQuarterForBeat (beat);
            const int x = rulersView.getRulerHeaderWidth() + timeMapper.getPixelForQuarter(quarterPos);
            const auto barSignature = timeMapper.getBarSignatureForQuarter (quarterPos);
            const int lineWidth = (quarterPos == barSignature.position) ? heavyLineWidth : lightLineWidth;
            const int beatsSinceBarStart = roundToInt( timeMapper.getBeatDistanceFromBarStartForQuarter (quarterPos));
            const int lineHeight = (beatsSinceBarStart == 0) ? beatsRulerHeight : beatsRulerHeight / 2;
            
            rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, beatsRulerHeight - lineHeight, lineWidth, lineHeight));
        }
        g.fillRectList (rects);
    }
    else
    {
        rulerName = " No musical context found in ARA document!";
    }
    // draw ruler header -
    g.setColour (juce::Colours::white);
    g.drawText (rulerName, bounds, Justification::centredLeft);
    // borders
    g.setColour (juce::Colours::darkgrey);
    g.drawRect (bounds);
}

// chord ruler: one rect per chord, skipping empty "no chords"
RulersView::ARAChordsRuler::ARAChordsRuler (const RulersView &owner) : rulersView (owner)
{
    setWantsKeyboardFocus (false);
    setInterceptsMouseClicks (false, false);
}

void RulersView::ARAChordsRuler::paint (juce::Graphics& g)
{
    const auto bounds = g.getClipBounds();
    const auto visibleRange = rulersView.timeline.getVisibleRange();
    const auto timelineRange = rulersView.timeline.getTimelineRange();
    const auto& timeMapper = rulersView.timeMapper;
    String rulerName;
    if (timeMapper.canTempoMap())
    {
        rulerName = " chords";
        RectangleList<int> rects;
        const ARA::ChordInterpreter interpreter;
        const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords> chordsReader (timeMapper.getCurrentMusicalContext());
        for (auto itChord = chordsReader.begin(); itChord != chordsReader.end(); ++itChord)
        {
            if (interpreter.isNoChord (*itChord))
                continue;

            Rectangle<int> chordRect = bounds;
            chordRect.setVerticalRange (Range<int> (0, bounds.getHeight()));

            // find the starting position of the chord in pixels
            const auto chordStartTime = (itChord == chordsReader.begin()) ?
            timelineRange.getStart() : timeMapper.getTimeForQuarter (itChord->position);
            if (chordStartTime >= visibleRange.getEnd())
                break;
            chordRect.setLeft (rulersView.getRulerHeaderWidth() + timeMapper.getPixelForPosition (chordStartTime));

            // if we have a chord after this one, use its starting position to end our rect
            if (std::next(itChord) != chordsReader.end())
            {
                const auto nextChordStartTime = timeMapper.getTimeForQuarter (std::next (itChord)->position);
                if (nextChordStartTime < visibleRange.getStart())
                    continue;
                chordRect.setRight (timeMapper.getPixelForPosition (nextChordStartTime));
            }
            // draw chord rect and name
            g.drawRect (chordRect);
            g.drawText (convertOptionalARAString (interpreter.getNameForChord (*itChord).c_str()), chordRect.withTrimmedLeft (2), Justification::centredLeft);
        }
    }
    else
    {
        rulerName = " No musical context found in ARA document!";
    }
    // draw ruler header -
    g.setColour (juce::Colours::white);
    g.drawText (rulerName, bounds, Justification::centredLeft);
    // borders
    g.setColour (juce::Colours::darkgrey);
    g.drawRect (bounds);
}
