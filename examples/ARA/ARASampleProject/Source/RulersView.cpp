#include "RulersView.h"

#include "ARA_Library/Utilities/ARAPitchInterpretation.h"
#include "ARA_Library/Utilities/ARATimelineConversion.h"

//==============================================================================
RulersView::RulersView (DocumentView& documentView)
    : documentView (documentView),
      document (nullptr),
      musicalContext (nullptr)
{
    document = documentView.getARADocumentController()->getDocument<ARADocument>();
    document->addListener (this);
    findMusicalContext();
    lastPaintedPosition.resetToDefault();
    startTimerHz (10);
}

RulersView::~RulersView()
{
    detachFromMusicalContext();
    detachFromDocument();
}

void RulersView::detachFromDocument()
{
    if (document == nullptr)
        return;

    document->removeListener (this);

    document = nullptr;
}

void RulersView::detachFromMusicalContext()
{
    if (musicalContext == nullptr)
        return;

    musicalContext->removeListener (this);

    musicalContext = nullptr;
}

void RulersView::findMusicalContext()
{
    // evaluate selection
    ARAMusicalContext* newMusicalContext = nullptr;
    auto viewSelection = documentView.getARAEditorView()->getViewSelection();
    if (! viewSelection.getRegionSequences().empty())
        newMusicalContext = viewSelection.getRegionSequences().front()->getMusicalContext<ARAMusicalContext>();
    else if (! viewSelection.getPlaybackRegions().empty())
        newMusicalContext = viewSelection.getPlaybackRegions().front()->getRegionSequence()->getMusicalContext<ARAMusicalContext>();

    // if no context used yet and selection does not yield a new one, use the first musical context in the docment
    if (musicalContext == nullptr && newMusicalContext == nullptr && ! document->getMusicalContexts().empty())
        newMusicalContext = document->getMusicalContexts<ARAMusicalContext>().front();

    if (newMusicalContext != musicalContext)
    {
        detachFromMusicalContext();

        musicalContext = newMusicalContext;
        musicalContext->addListener (this);

        repaint();
    }
}

void RulersView::timerCallback()
{
    auto positionInfo = documentView.getPlayHeadPositionInfo();
    if (lastPaintedPosition.ppqLoopStart != positionInfo.ppqLoopStart ||
        lastPaintedPosition.ppqLoopEnd != positionInfo.ppqLoopEnd ||
        lastPaintedPosition.isLooping  != positionInfo.isLooping)
    {
        repaint();
    }
}

//==============================================================================
void RulersView::paint (juce::Graphics& g)
{
    const auto bounds = g.getClipBounds();

    g.setColour (Colours::lightslategrey);

    if (musicalContext == nullptr)
    {
        g.setFont (Font (12.0f));
        g.drawText ("No musical context found in ARA document!", bounds, Justification::centred);
        return;
    }

    const auto visibleRange = documentView.getVisibleTimeRange();
    const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader (musicalContext);
    const ARA::TempoConverter<decltype (tempoReader)> tempoConverter (tempoReader);

    // we'll draw three rulers: seconds, beats, and chords
    constexpr int lightLineWidth = 1;
    constexpr int heavyLineWidth = 3;
    const int chordRulerY = 0;
    const int chordRulerHeight = getBounds().getHeight() / 3;
    const int beatsRulerY = chordRulerY + chordRulerHeight;
    const int beatsRulerHeight = (getBounds().getHeight() - chordRulerHeight) / 2;
    const int secondsRulerY = beatsRulerY + beatsRulerHeight;
    const int secondsRulerHeight = getBounds().getHeight() - chordRulerHeight - beatsRulerHeight;

    // seconds ruler: one tick for each second
    if (true)
    {
        RectangleList<int> rects;
        const int endTime = roundToInt (floor (visibleRange.getEnd()));
        for (int time = roundToInt (ceil (visibleRange.getStart())); time <= endTime; ++time)
        {
            const int lineWidth = (time % 60 == 0) ? heavyLineWidth : lightLineWidth;
            const int lineHeight = (time % 10 == 0) ? secondsRulerHeight : secondsRulerHeight / 2;
            const int x = documentView.getPlaybackRegionsViewsXForTime (time);
            rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, secondsRulerY + secondsRulerHeight - lineHeight, lineWidth, lineHeight));
        }
        g.fillRectList (rects);
    }
    g.drawText ("seconds", bounds.withTrimmedRight (2), Justification::bottomRight);

    // beat ruler: evaluates tempo and bar signatures to draw a line for each beat
    if (tempoReader)
    {
        const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> barSignaturesReader (musicalContext);
        const ARA::BarSignaturesConverter<decltype (barSignaturesReader)> barSignaturesConverter (barSignaturesReader);
        if (barSignaturesReader)
        {
            RectangleList<int> rects;
            const double beatStart = barSignaturesConverter.getBeatForQuarter (tempoConverter.getQuarterForTime (visibleRange.getStart()));
            const double beatEnd = barSignaturesConverter.getBeatForQuarter (tempoConverter.getQuarterForTime (visibleRange.getEnd()));
            const int endBeat = roundToInt (floor (beatEnd));
            for (int beat = roundToInt (ceil (beatStart)); beat <= endBeat; ++beat)
            {
                const auto quarterPos = barSignaturesConverter.getQuarterForBeat (beat);
                const int x = documentView.getPlaybackRegionsViewsXForTime (tempoConverter.getTimeForQuarter (quarterPos));
                const auto barSignature = barSignaturesConverter.getBarSignatureForQuarter (quarterPos);
                const int lineWidth = (quarterPos == barSignature.position) ? heavyLineWidth : lightLineWidth;
                const int beatsSinceBarStart = roundToInt( barSignaturesConverter.getBeatDistanceFromBarStartForQuarter (quarterPos));
                const int lineHeight = (beatsSinceBarStart == 0) ? beatsRulerHeight : beatsRulerHeight / 2;
                rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, beatsRulerY + beatsRulerHeight - lineHeight, lineWidth, lineHeight));
            }
            g.fillRectList (rects);
        }
    }
    g.drawText ("beats", bounds.withTrimmedRight (2).withTrimmedBottom (secondsRulerHeight), Justification::bottomRight);

    // chord ruler: one rect per chord, skipping empty "no chords"
    if (tempoReader)
    {
        RectangleList<int> rects;
        const ARA::ChordInterpreter interpreter;
        const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords> chordsReader (musicalContext);
        for (auto itChord = chordsReader.begin(); itChord != chordsReader.end(); ++itChord)
        {
            if (interpreter.isNoChord (*itChord))
                continue;

            Rectangle<int> chordRect = bounds;
            chordRect.setVerticalRange (Range<int> (chordRulerY, chordRulerY + chordRulerHeight));
            
            // find the starting position of the chord in pixels
            const auto chordStartTime = (itChord == chordsReader.begin()) ?
                                            documentView.getTimeRange().getStart() : tempoConverter.getTimeForQuarter (itChord->position);
            if (chordStartTime >= visibleRange.getEnd())
                break;
            chordRect.setLeft (documentView.getPlaybackRegionsViewsXForTime (chordStartTime));

            // if we have a chord after this one, use its starting position to end our rect
            if (std::next(itChord) != chordsReader.end())
            {
                const auto nextChordStartTime = tempoConverter.getTimeForQuarter (std::next (itChord)->position);
                if (nextChordStartTime < visibleRange.getStart())
                    continue;
                chordRect.setRight (documentView.getPlaybackRegionsViewsXForTime (nextChordStartTime));
            }

            // draw chord rect and name
            g.drawRect (chordRect);
            g.drawText (convertARAString (interpreter.getNameForChord (*itChord).c_str()), chordRect.withTrimmedLeft (2), Justification::centredLeft);
        }
    }
    g.drawText ("chords", bounds.withTrimmedRight (2).withTrimmedBottom (beatsRulerHeight + secondsRulerHeight), Justification::bottomRight);

    // locators
    {
        lastPaintedPosition = documentView.getPlayHeadPositionInfo();
        const auto startInSeconds = tempoConverter.getTimeForQuarter (lastPaintedPosition.ppqLoopStart);
        const auto endInSeconds = tempoConverter.getTimeForQuarter (lastPaintedPosition.ppqLoopEnd);
        const int startX = documentView.getPlaybackRegionsViewsXForTime (startInSeconds);
        const int endX = documentView.getPlaybackRegionsViewsXForTime (endInSeconds);
        g.setColour (lastPaintedPosition.isLooping ? Colours::skyblue.withAlpha (0.3f) : Colours::white.withAlpha (0.3f));
        g.fillRect (startX, bounds.getY(), endX - startX, bounds.getHeight());
    }

    // borders
    {
        g.setColour (Colours::darkgrey);
        g.drawLine ((float) bounds.getX(), (float) beatsRulerY, (float) bounds.getRight(), (float) beatsRulerY);
        g.drawLine ((float) bounds.getX(), (float) secondsRulerY, (float) bounds.getRight(), (float) secondsRulerY);
        g.drawRect (bounds);
    }
}

//==============================================================================

void RulersView::mouseDown (const MouseEvent& event)
{
    // use mouse click to set the playhead position in the host (if they provide a playback controller interface)
    auto playbackController = musicalContext->getDocument()->getDocumentController()->getHostInstance()->getPlaybackController();
    if (playbackController != nullptr)
        playbackController->requestSetPlaybackPosition (documentView.getPlaybackRegionsViewsTimeForX (roundToInt (event.position.x)));
}

void RulersView::mouseDoubleClick (const MouseEvent& /*event*/)
{
    // use mouse double click to start host playback (if they provide a playback controller interface)
    auto playbackController = musicalContext->getDocument()->getDocumentController()->getHostInstance()->getPlaybackController();
    if (playbackController != nullptr)
        playbackController->requestStartPlayback();
}

//==============================================================================

void RulersView::onNewSelection (const ARA::PlugIn::ViewSelection& /*viewSelection*/)
{
    findMusicalContext();
}

void RulersView::didEndEditing (ARADocument* /*doc*/)
{
    if (musicalContext == nullptr)
        findMusicalContext();
}

void RulersView::willRemoveMusicalContextFromDocument (ARADocument* doc, ARAMusicalContext* context)
{
    jassert (document == doc);

    if (musicalContext == context)
        detachFromMusicalContext();     // will restore in didEndEditing()
}

void RulersView::didReorderMusicalContextsInDocument (ARADocument* doc)
{
    jassert (document == doc);

    if (musicalContext != document->getMusicalContexts().front())
        detachFromMusicalContext();     // will restore in didEndEditing()
}
 void RulersView::willDestroyDocument (ARADocument* doc)
{
    jassert (document == doc);

    detachFromDocument();
}

void RulersView::doUpdateMusicalContextContent (ARAMusicalContext* context, ARAContentUpdateScopes /*scopeFlags*/)
{
    jassert (musicalContext == context);

    repaint();
}
