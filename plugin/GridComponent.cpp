#include "GridComponent.h"

#include "Theme.h"
#include "Timing.h"

#include <algorithm>
#include <cmath>

GridComponent::GridComponent ()
{
    drumLanes = DrumMap::getStandardDrumLanes ();
}

int GridComponent::getLaneIndexForNote (uint8_t note) const noexcept
{
    for (size_t i = 0; i < drumLanes.size (); ++i)
        for (uint8_t laneNote : drumLanes[i].notes)
            if (laneNote == note)
                return static_cast<int> (i);
    return 5;
}

bool GridComponent::isNearMultiple (double value, double period, double eps) noexcept
{
    const double r = std::abs (std::fmod (value, period));
    return r < eps || r > period - eps;
}

juce::Colour GridComponent::getContinuousHitColor (float deltaMs, float toleranceMs, float maxErrorMs) noexcept
{
    const float absDelta = std::abs (deltaMs);
    if (absDelta <= toleranceMs)
        return Theme::col (Theme::emerald);

    const float denom = std::max (0.001f, maxErrorMs - toleranceMs);
    const float t = std::clamp ((absDelta - toleranceMs) / denom, 0.0f, 1.0f);

    if (deltaMs < 0.0f) // Rush
    {
        if (t <= 0.5f)
        {
            const float k = t / 0.5f;
            return juce::Colour::fromRGB (255, static_cast<juce::uint8> (lerpChannel (k, 234.0f, 145.0f)), 0);
        }
        const float k = (t - 0.5f) / 0.5f;
        return juce::Colour::fromRGB (255, static_cast<juce::uint8> (lerpChannel (k, 145.0f, 23.0f)),
                                      static_cast<juce::uint8> (lerpChannel (k, 0.0f, 68.0f)));
    }

    // Drag
    if (t <= 0.5f)
    {
        const float k = t / 0.5f;
        return juce::Colour::fromRGB (static_cast<juce::uint8> (lerpChannel (k, 0.0f, 41.0f)),
                                      static_cast<juce::uint8> (lerpChannel (k, 229.0f, 121.0f)), 255);
    }
    const float k = (t - 0.5f) / 0.5f;
    return juce::Colour::fromRGB (static_cast<juce::uint8> (lerpChannel (k, 41.0f, 213.0f)),
                                  static_cast<juce::uint8> (lerpChannel (k, 121.0f, 0.0f)),
                                  static_cast<juce::uint8> (lerpChannel (k, 255.0f, 249.0f)));
}

void GridComponent::update (const GridViewState &state, const std::vector<HitEvent> &events)
{
    activeEvents = events;
    view = state;
    if (view.numBars <= 0) view.numBars = 4;
    if (view.gridSubdivisionPpq <= 0.0) view.gridSubdivisionPpq = 0.25;
    if (view.timeSigNum <= 0) view.timeSigNum = 4;
    if (view.toleranceMs < 0.0f) view.toleranceMs = 20.0f;
    if (view.bpm <= 0.0f) view.bpm = 120.0f;
    repaint ();
}

void GridComponent::updateEvents (const std::vector<HitEvent> &events, double currentPpq, int numBars,
                                  double gridSubdivisionPpq, int timeSigNum, bool showMsLabels,
                                  bool showVelocityLabels, bool showNoteNumbers, float toleranceMs,
                                  float latencyOffsetMs, float bpm)
{
    GridViewState s;
    s.currentPpq = currentPpq;
    s.numBars = numBars;
    s.gridSubdivisionPpq = gridSubdivisionPpq;
    s.timeSigNum = timeSigNum;
    s.showMsLabels = showMsLabels;
    s.showVelocityLabels = showVelocityLabels;
    s.showNoteNumbers = showNoteNumbers;
    s.toleranceMs = toleranceMs;
    s.latencyOffsetMs = latencyOffsetMs;
    s.bpm = bpm;
    update (s, events);
}

void GridComponent::clearEvents ()
{
    activeEvents.clear ();
    repaint ();
}

void GridComponent::resized () {}

// ── Layout ──
GridComponent::Layout GridComponent::computeLayout () const
{
    Layout l;
    l.boundsW = static_cast<float> (getWidth ());
    l.boundsH = static_cast<float> (getHeight ());
    l.canvasLeft = l.labelWidth;
    l.canvasW = l.boundsW - l.labelWidth;
    l.laneAreaTop = l.rulerHeight;
    l.laneAreaH = l.boundsH - l.rulerHeight - l.footerHeight;
    l.numLanes = static_cast<int> (drumLanes.size ());
    l.laneH = l.numLanes > 0 ? l.laneAreaH / static_cast<float> (l.numLanes) : 0.0f;

    float zoomScale = 1.0f;
    if (view.numBars == 2) zoomScale = 1.4f;
    else if (view.numBars == 1) zoomScale = 2.0f;

    const float windowWidthScale = std::clamp (l.canvasW / 1200.0f, 0.75f, 2.5f);
    l.dynamicScale = std::clamp (zoomScale * windowWidthScale, 0.8f, 3.0f);
    l.nodeRadius = 11.0f * l.dynamicScale;
    l.strokeW = std::clamp (2.5f * l.dynamicScale, 2.0f, 6.0f);
    return l;
}

// ── Draw helpers ──
void GridComponent::drawRuler (juce::Graphics &g, const Layout &l) const
{
    g.setColour (Theme::col (Theme::bgHeader));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, l.boundsW, l.rulerHeight));
    g.setColour (Theme::col (Theme::border));
    g.drawHorizontalLine (static_cast<int> (l.rulerHeight), 0.0f, l.boundsW);
    g.drawVerticalLine (static_cast<int> (l.labelWidth), 0.0f, l.boundsH);

    g.setColour (Theme::col (Theme::textMuted));
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("GRID RULER", juce::Rectangle<float> (12.0f, 0.0f, l.labelWidth - 16.0f, l.rulerHeight),
                juce::Justification::centredLeft, true);
}

void GridComponent::drawLanes (juce::Graphics &g, const Layout &l) const
{
    const float laneHeightScale = std::clamp (l.laneH / 80.0f, 0.75f, 2.5f);

    for (int i = 0; i < l.numLanes; ++i)
    {
        const float y = l.laneAreaTop + (i * l.laneH);
        const auto laneRect = juce::Rectangle<float> (0.0f, y, l.boundsW, l.laneH);

        g.setColour (Theme::col (i % 2 == 0 ? Theme::bgLaneEven : Theme::bgLaneOdd));
        g.fillRect (laneRect);

        g.setColour (Theme::col (Theme::border));
        g.drawHorizontalLine (static_cast<int> (y), 0.0f, l.boundsW);

        g.setColour (Theme::col (Theme::bgGrid));
        g.fillRect (juce::Rectangle<float> (0.0f, y, l.labelWidth, l.laneH));
        g.setColour (Theme::col (Theme::border));
        g.drawVerticalLine (static_cast<int> (l.labelWidth), y, y + l.laneH);

        const float laneFontHeight = std::clamp (16.0f * laneHeightScale, 14.0f, 32.0f);
        g.setColour (Theme::col (Theme::textPrimary));
        g.setFont (juce::Font (laneFontHeight, juce::Font::bold));
        g.drawText (drumLanes[static_cast<size_t> (i)].label,
                    juce::Rectangle<float> (12.0f, y, l.labelWidth - 16.0f, l.laneH),
                    juce::Justification::centredLeft, true);
    }
}

void GridComponent::drawGridLines (juce::Graphics &g, const Layout &l, double minPpq, double maxPpq,
                                   double totalPpqWindow) const
{
    if (view.gridSubdivisionPpq <= 0.0)
        return;

    const double barPpqInterval = static_cast<double> (view.timeSigNum);
    const double firstTick = std::floor (minPpq / view.gridSubdivisionPpq) * view.gridSubdivisionPpq;

    for (double tick = firstTick; tick <= maxPpq + 0.0001; tick += view.gridSubdivisionPpq)
    {
        if (tick < minPpq)
            continue;

        const float normalizedX = static_cast<float> ((tick - minPpq) / totalPpqWindow);
        const float x = l.canvasLeft + (normalizedX * l.canvasW);

        const bool isBarBoundary = isNearMultiple (tick, barPpqInterval);
        const bool isBeatBoundary = isNearMultiple (tick, 1.0);

        if (isBarBoundary)
        {
            g.setColour (Theme::col (Theme::skyBlue));
            g.drawVerticalLine (static_cast<int> (x), 0.0f, l.boundsH - l.footerHeight);

            const int rawBarIdx = static_cast<int> (std::floor (tick / barPpqInterval));
            int wrappedBarIdx = rawBarIdx % view.numBars;
            if (wrappedBarIdx < 0) wrappedBarIdx += view.numBars;
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("Bar " + juce::String (wrappedBarIdx + 1),
                        juce::Rectangle<float> (x + 4.0f, 0.0f, 60.0f, l.rulerHeight),
                        juce::Justification::centredLeft, false);
        }
        else if (isBeatBoundary)
        {
            g.setColour (juce::Colour (0xff475569));
            g.drawVerticalLine (static_cast<int> (x), l.rulerHeight, l.boundsH - l.footerHeight);

            const int beatNumber = static_cast<int> (std::floor (std::fmod (tick, barPpqInterval))) + 1;
            g.setColour (Theme::col (Theme::textMuted));
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (juce::String (beatNumber),
                        juce::Rectangle<float> (x + 3.0f, 0.0f, 30.0f, l.rulerHeight),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (Theme::col (Theme::borderFaint));
            g.drawVerticalLine (static_cast<int> (x), l.rulerHeight, l.boundsH - l.footerHeight);
        }
    }
}

void GridComponent::drawHitSymbol (juce::Graphics &g, float cx, float cy, float radius, float strokeW, double deltaMs,
                                   float absDelta, float tolerance, bool showVel, uint8_t velocity) const
{
    if (showVel)
    {
        const float h = std::clamp (10.0f * (radius / 11.0f), 9.0f, 22.0f);
        g.setColour (juce::Colour (0xff000000));
        g.setFont (juce::Font (h, juce::Font::bold));
        g.drawText (juce::String (velocity),
                    juce::Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f),
                    juce::Justification::centred, false);
        return;
    }

    if (absDelta <= tolerance)
    {
        juce::Path p;
        const float r = radius * 0.55f;
        p.startNewSubPath (cx - r * 0.55f, cy + r * 0.05f);
        p.lineTo (cx - r * 0.10f, cy + r * 0.55f);
        p.lineTo (cx + r * 0.65f, cy - r * 0.45f);
        g.setColour (Theme::col (Theme::bgMain));
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    juce::Path p;
    const float r = radius * 0.45f;
    if (deltaMs < 0.0f) // Rush -> >
    {
        p.startNewSubPath (cx - r * 0.45f, cy - r * 0.65f);
        p.lineTo (cx + r * 0.45f, cy);
        p.lineTo (cx - r * 0.45f, cy + r * 0.65f);
    }
    else // Drag -> <
    {
        p.startNewSubPath (cx + r * 0.45f, cy - r * 0.65f);
        p.lineTo (cx - r * 0.45f, cy);
        p.lineTo (cx + r * 0.45f, cy + r * 0.65f);
    }
    g.setColour (Theme::col (Theme::bgMain));
    g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

std::pair<int, int> GridComponent::drawHits (juce::Graphics &g, const Layout &l, double minPpq, double maxPpq,
                                             double totalPpqWindow, double userLatencyPpq, float maxErrorMs) const
{
    int total = 0, green = 0;

    for (const auto &event : activeEvents)
    {
        const double rawPpq = (event.rawHitPpqPosition > 0.0) ? event.rawHitPpqPosition : event.hitPpqPosition;
        const double compPpq = rawPpq - userLatencyPpq;
        if (compPpq < minPpq || compPpq > maxPpq)
            continue;

        const int laneIndex = getLaneIndexForNote (event.noteNumber);
        if (laneIndex < 0 || laneIndex >= l.numLanes)
            continue;

        const auto timing = Timing::compute (compPpq, view.gridSubdivisionPpq, view.bpm, view.toleranceMs);
        const double liveDeltaMs = timing.deltaMs;
        const float absDelta = static_cast<float> (std::abs (liveDeltaMs));

        ++total;
        if (absDelta <= view.toleranceMs)
            ++green;

        const float normalizedX = static_cast<float> ((compPpq - minPpq) / totalPpqWindow);
        const float hitX = l.canvasLeft + (normalizedX * l.canvasW);
        const float hitY = l.laneAreaTop + (laneIndex * l.laneH) + (l.laneH * 0.5f);

        const juce::Colour fillColour = getContinuousHitColor (static_cast<float> (liveDeltaMs), view.toleranceMs, maxErrorMs);
        const auto hitRect = juce::Rectangle<float> (hitX - l.nodeRadius, hitY - l.nodeRadius, l.nodeRadius * 2.0f, l.nodeRadius * 2.0f);

        g.setColour (Theme::col (Theme::bgMain));
        g.fillEllipse (hitRect.expanded (2.0f * l.dynamicScale));
        g.setColour (fillColour);
        g.fillEllipse (hitRect);

        drawHitSymbol (g, hitX, hitY, l.nodeRadius, l.strokeW, liveDeltaMs, absDelta, view.toleranceMs,
                       view.showVelocityLabels, event.velocity);

        if (view.showNoteNumbers)
        {
            const juce::String noteText = "#" + juce::String (event.noteNumber);
            const float w = 38.0f * l.dynamicScale, h = 13.0f * l.dynamicScale;
            const auto r = juce::Rectangle<float> (hitX - w * 0.5f, hitY - l.nodeRadius - h - 2.0f, w, h);
            g.setColour (juce::Colour (0xd01e293b));
            g.fillRoundedRectangle (r, 3.0f * l.dynamicScale);
            g.setColour (Theme::col (Theme::skyBlue));
            g.setFont (juce::Font (std::clamp (9.5f * l.dynamicScale, 8.5f, 20.0f), juce::Font::bold));
            g.drawText (noteText, r, juce::Justification::centred, false);
        }

        if (view.showMsLabels)
        {
            juce::String msText = juce::String (static_cast<int> (std::round (liveDeltaMs))) + "ms";
            if (liveDeltaMs > 0.0) msText = "+" + msText;
            const float w = 44.0f * l.dynamicScale, h = 14.0f * l.dynamicScale;
            const auto r = juce::Rectangle<float> (hitX - w * 0.5f, hitY + l.nodeRadius + 2.0f, w, h);
            g.setColour (juce::Colour (0xd00a0c10));
            g.fillRoundedRectangle (r, 3.0f * l.dynamicScale);
            g.setColour (fillColour);
            g.setFont (juce::Font (std::clamp (10.0f * l.dynamicScale, 9.0f, 22.0f), juce::Font::bold));
            g.drawText (msText, r, juce::Justification::centred, false);
        }
    }

    return {total, green};
}

void GridComponent::drawPlayhead (juce::Graphics &g, const Layout &l) const
{
    g.setColour (Theme::col (Theme::cyan));
    g.drawVerticalLine (static_cast<int> (l.boundsW - 2.0f), 0.0f, l.boundsH - l.footerHeight);
}

void GridComponent::drawFooter (juce::Graphics &g, const Layout &l, int totalHits, int greenHits) const
{
    const float footerY = l.boundsH - l.footerHeight;
    g.setColour (Theme::col (Theme::bgHeader));
    g.fillRect (juce::Rectangle<float> (0.0f, footerY, l.boundsW, l.footerHeight));
    g.setColour (Theme::col (Theme::border));
    g.drawHorizontalLine (static_cast<int> (footerY), 0.0f, l.boundsW);
    g.drawVerticalLine (static_cast<int> (l.labelWidth), footerY, l.boundsH);

    const int accuracyPct = totalHits > 0
                            ? static_cast<int> (std::round (static_cast<double> (greenHits) / totalHits * 100.0))
                            : 0;

    g.setColour (Theme::col (Theme::textMuted));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("ACCURACY", juce::Rectangle<float> (12.0f, footerY, l.labelWidth - 16.0f, l.footerHeight),
                juce::Justification::centredLeft, true);

    const float barTrackX = l.canvasLeft + 12.0f;
    const float barTrackW = std::max (100.0f, l.canvasW - 320.0f);
    const float barTrackH = 10.0f;
    const float barTrackY = footerY + (l.footerHeight - barTrackH) * 0.5f;

    g.setColour (Theme::col (Theme::bgProgressTrack));
    g.fillRoundedRectangle (juce::Rectangle<float> (barTrackX, barTrackY, barTrackW, barTrackH), 3.0f);

    const float fillW = barTrackW * (accuracyPct / 100.0f);
    if (fillW > 0.0f)
    {
        g.setColour (Theme::col (Theme::emerald));
        g.fillRoundedRectangle (juce::Rectangle<float> (barTrackX, barTrackY, fillW, barTrackH), 3.0f);
    }

    g.setColour (Theme::col (Theme::borderTrack));
    g.drawRoundedRectangle (juce::Rectangle<float> (barTrackX, barTrackY, barTrackW, barTrackH), 3.0f, 1.0f);

    const float textX = barTrackX + barTrackW + 16.0f;
    const juce::String scoreText = juce::String (accuracyPct) + "%  (" + juce::String (greenHits) + " / "
                                 + juce::String (totalHits) + " On-Grid)";
    g.setColour (Theme::col (Theme::emerald));
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText (scoreText, juce::Rectangle<float> (textX, footerY, l.boundsW - textX - 12.0f, l.footerHeight),
                juce::Justification::centredLeft, true);
}

void GridComponent::paint (juce::Graphics &g)
{
    g.fillAll (Theme::col (Theme::bgGrid));

    const Layout l = computeLayout ();
    if (l.canvasW <= 10.0f || l.boundsH <= l.rulerHeight + l.footerHeight + 10.0f)
        return;

    drawRuler (g, l);
    drawLanes (g, l);

    const double totalPpqWindow = static_cast<double> (view.numBars) * static_cast<double> (view.timeSigNum);
    const double maxPpq = view.currentPpq;
    const double minPpq = maxPpq - totalPpqWindow;

    drawGridLines (g, l, minPpq, maxPpq, totalPpqWindow);

    const double userLatencyPpq = (static_cast<double> (view.latencyOffsetMs) / 1000.0) * (view.bpm / 60.0);
    const float maxErrorMs = static_cast<float> ((view.gridSubdivisionPpq / 2.0) * (60.0 / view.bpm) * 1000.0);

    const auto [totalHits, greenHits] = drawHits (g, l, minPpq, maxPpq, totalPpqWindow, userLatencyPpq, maxErrorMs);

    drawPlayhead (g, l);
    drawFooter (g, l, totalHits, greenHits);
}
