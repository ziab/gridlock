#include "GridComponent.h"
#include <cmath>
#include <algorithm>

GridComponent::GridComponent()
{
    drumLanes = {
        { "CYMBALS", { 49, 51, 53 } },
        { "HI-HAT",  { 42, 44, 46 } },
        { "KICK",    { 36 } },
        { "SNARE",   { 38, 40 } },
        { "TOMS",    { 48, 45, 43 } },
        { "OTHER",   {} }
    };
}

int GridComponent::getLaneIndexForNote(uint8_t note) const noexcept
{
    if (note == 49 || note == 51 || note == 53) return 0; // Cymbals
    if (note == 42 || note == 44 || note == 46) return 1; // Hi-Hat
    if (note == 36) return 2;                             // Kick
    if (note == 38 || note == 40) return 3;               // Snare Head / Rim
    if (note == 48 || note == 45 || note == 43) return 4; // Toms
    return 5;                                             // Other
}

juce::Colour GridComponent::getContinuousHitColor(float normalizedDeviation) noexcept
{
    const float dev = std::clamp(normalizedDeviation, -1.0f, 1.0f);

    if (dev < 0.0f)
    {
        const float t = std::abs(dev);
        if (t <= 0.5f) {
            const float k = t / 0.5f;
            return juce::Colour::fromRGB(
                static_cast<juce::uint8>(juce::jmap(k, 0.0f, 255.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 255.0f, 200.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 136.0f, 0.0f))
            );
        } else {
            const float k = (t - 0.5f) / 0.5f;
            return juce::Colour::fromRGB(
                static_cast<juce::uint8>(juce::jmap(k, 255.0f, 255.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 200.0f, 23.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 0.0f, 68.0f))
            );
        }
    }
    else
    {
        const float t = dev;
        if (t <= 0.5f) {
            const float k = t / 0.5f;
            return juce::Colour::fromRGB(
                static_cast<juce::uint8>(juce::jmap(k, 0.0f, 0.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 255.0f, 229.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 136.0f, 255.0f))
            );
        } else {
            const float k = (t - 0.5f) / 0.5f;
            return juce::Colour::fromRGB(
                static_cast<juce::uint8>(juce::jmap(k, 0.0f, 213.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 229.0f, 0.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 255.0f, 249.0f))
            );
        }
    }
}

void GridComponent::updateEvents(const std::vector<HitEvent>& events,
                                 double currentPpq,
                                 int numBars,
                                 double gridSubdivisionPpq,
                                 int timeSigNum,
                                 bool showMsLabels,
                                 float toleranceMs,
                                 float latencyOffsetMs,
                                 float bpm)
{
    activeEvents = events;
    currentPpqPos = currentPpq;
    barsWindow = (numBars > 0) ? numBars : 4;
    subdivisionPpq = (gridSubdivisionPpq > 0.0) ? gridSubdivisionPpq : 0.25;
    timeSigNumerator = (timeSigNum > 0) ? timeSigNum : 4;
    displayMsLabels = showMsLabels;
    toleranceMsVal = (toleranceMs > 0.0f) ? toleranceMs : 10.0f;
    latencyOffsetMsVal = latencyOffsetMs;
    bpmVal = (bpm > 0.0f) ? bpm : 120.0f;
    repaint();
}

void GridComponent::clearEvents()
{
    activeEvents.clear();
    repaint();
}

void GridComponent::resized()
{
}

void GridComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12141a));

    const float labelWidth = 110.0f;
    const float rulerHeight = 24.0f;
    const float boundsWidth = static_cast<float>(getWidth());
    const float boundsHeight = static_cast<float>(getHeight());

    const float canvasLeft = labelWidth;
    const float canvasWidth = boundsWidth - labelWidth;

    if (canvasWidth <= 10.0f || boundsHeight <= rulerHeight + 10.0f)
        return;

    const float laneAreaTop = rulerHeight;
    const float laneAreaHeight = boundsHeight - rulerHeight;

    const int numLanes = static_cast<int>(drumLanes.size());
    const float laneHeight = laneAreaHeight / static_cast<float>(numLanes);

    // Draw Top Ruler Header Background & Separator Line
    g.setColour(juce::Colour(0xff181b24));
    g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, boundsWidth, rulerHeight));

    g.setColour(juce::Colour(0xff2d3245));
    g.drawHorizontalLine(static_cast<int>(rulerHeight), 0.0f, boundsWidth);
    g.drawVerticalLine(static_cast<int>(labelWidth), 0.0f, boundsHeight);

    // Draw Top Left Corner Label
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("GRID RULER", juce::Rectangle<float>(12.0f, 0.0f, labelWidth - 16.0f, rulerHeight),
               juce::Justification::centredLeft, true);

    // Draw Drum Lane Backgrounds & Sidebar Labels
    for (int i = 0; i < numLanes; ++i)
    {
        const float y = laneAreaTop + (i * laneHeight);
        const auto laneRect = juce::Rectangle<float>(0.0f, y, boundsWidth, laneHeight);

        if (i % 2 == 0)
            g.setColour(juce::Colour(0xff161922));
        else
            g.setColour(juce::Colour(0xff1a1d28));

        g.fillRect(laneRect);

        // Horizontal Lane Separator
        g.setColour(juce::Colour(0xff2d3245));
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, boundsWidth);

        // Sidebar Label Area
        g.setColour(juce::Colour(0xff12141a));
        g.fillRect(juce::Rectangle<float>(0.0f, y, labelWidth, laneHeight));

        g.setColour(juce::Colour(0xff2d3245));
        g.drawVerticalLine(static_cast<int>(labelWidth), y, y + laneHeight);

        // Bold High-Contrast Text for Drum Lanes (Readable 4-6 ft away)
        g.setColour(juce::Colour(0xfff0f2f8));
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(drumLanes[static_cast<size_t>(i)].label,
                   juce::Rectangle<float>(12.0f, y, labelWidth - 16.0f, laneHeight),
                   juce::Justification::centredLeft, true);
    }

    // Calculate PPQ Window
    const double barPpqInterval = static_cast<double>(timeSigNumerator);
    const double totalPpqWindow = static_cast<double>(barsWindow) * barPpqInterval;
    const double maxPpq = currentPpqPos;
    const double minPpq = maxPpq - totalPpqWindow;

    // Render Visual Markers for Strong Beats, Bars, and Subdivisions
    if (subdivisionPpq > 0.0)
    {
        const double firstTick = std::floor(minPpq / subdivisionPpq) * subdivisionPpq;

        for (double tick = firstTick; tick <= maxPpq + 0.0001; tick += subdivisionPpq)
        {
            if (tick < minPpq) continue;

            const float normalizedX = static_cast<float>((tick - minPpq) / totalPpqWindow);
            const float x = canvasLeft + (normalizedX * canvasWidth);

            const double barRem = std::abs(std::fmod(tick, barPpqInterval));
            const bool isBarBoundary = (barRem < 0.001 || barRem > barPpqInterval - 0.001);

            const double beatRem = std::abs(std::fmod(tick, 1.0));
            const bool isBeatBoundary = (beatRem < 0.001 || beatRem > 0.999);

            if (isBarBoundary)
            {
                // Prominent Bar Boundary Line & Ruler Label
                g.setColour(juce::Colour(0xff38bdf8)); // Bright Sky Blue Bar Line
                g.drawVerticalLine(static_cast<int>(x), 0.0f, boundsHeight);

                const int barNumber = static_cast<int>(std::floor(tick / barPpqInterval)) + 1;
                g.setFont(juce::Font(12.0f, juce::Font::bold));
                g.drawText("Bar " + juce::String(barNumber),
                           juce::Rectangle<float>(x + 4.0f, 0.0f, 60.0f, rulerHeight),
                           juce::Justification::centredLeft, false);
            }
            else if (isBeatBoundary)
            {
                // Strong Beat Line & Ruler Number
                g.setColour(juce::Colour(0xff475569)); // Medium Accent Beat Line
                g.drawVerticalLine(static_cast<int>(x), rulerHeight, boundsHeight);

                const int beatNumber = static_cast<int>(std::floor(std::fmod(tick, barPpqInterval))) + 1;
                g.setColour(juce::Colour(0xff94a3b8));
                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.drawText(juce::String(beatNumber),
                           juce::Rectangle<float>(x + 3.0f, 0.0f, 30.0f, rulerHeight),
                           juce::Justification::centredLeft, false);
            }
            else
            {
                // Faint Subdivision Line
                g.setColour(juce::Colour(0xff1e293b));
                g.drawVerticalLine(static_cast<int>(x), rulerHeight, boundsHeight);
            }
        }
    }

    // Render Hit Events with Live Real-Time Latency & Tolerance Recalculation
    const double userLatencyPpq = (static_cast<double>(latencyOffsetMsVal) / 1000.0) * (static_cast<double>(bpmVal) / 60.0);
    const float nodeRadius = 11.0f; // High visibility hit nodes

    for (const auto& event : activeEvents)
    {
        const double rawPpq = (event.rawHitPpqPosition > 0.0) ? event.rawHitPpqPosition : event.hitPpqPosition;
        const double compPpq = rawPpq - userLatencyPpq;

        if (compPpq < minPpq || compPpq > maxPpq)
            continue;

        const int laneIndex = getLaneIndexForNote(event.noteNumber);
        if (laneIndex < 0 || laneIndex >= numLanes) continue;

        // Recalculate Delta MS and Normalized Deviation live for all displayed notes
        const double nearestGridPpq = std::round(compPpq / subdivisionPpq) * subdivisionPpq;
        const double deltaPpq = compPpq - nearestGridPpq;
        const double liveDeltaMs = (deltaPpq / (static_cast<double>(bpmVal) / 60.0)) * 1000.0;
        const float liveNormDev = std::clamp(static_cast<float>(liveDeltaMs / static_cast<double>(toleranceMsVal)), -1.0f, 1.0f);

        const float normalizedX = static_cast<float>((compPpq - minPpq) / totalPpqWindow);
        const float hitX = canvasLeft + (normalizedX * canvasWidth);
        const float hitY = laneAreaTop + (laneIndex * laneHeight) + (laneHeight * 0.5f);

        // Continuous Dynamic Color Interpolation based on live normalized deviation
        const juce::Colour fillColour = getContinuousHitColor(liveNormDev);

        const auto hitRect = juce::Rectangle<float>(hitX - nodeRadius, hitY - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);

        // Outer contrast ring
        g.setColour(juce::Colour(0xff0a0c10));
        g.fillEllipse(hitRect.expanded(2.0f));

        // Filled color node
        g.setColour(fillColour);
        g.fillEllipse(hitRect);

        // Draw Velocity inside node
        g.setColour(juce::Colour(0xff000000));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(juce::String(event.velocity), hitRect, juce::Justification::centred, false);

        // Display MS Offset Label underneath note if enabled
        if (displayMsLabels)
        {
            juce::String msText = juce::String(static_cast<int>(std::round(liveDeltaMs))) + "ms";
            if (liveDeltaMs > 0.0) msText = "+" + msText;

            const float labelW = 44.0f;
            const float labelH = 14.0f;
            const auto labelRect = juce::Rectangle<float>(hitX - (labelW * 0.5f), hitY + nodeRadius + 2.0f, labelW, labelH);

            // Dark background pill for throne-distance contrast
            g.setColour(juce::Colour(0xd00a0c10));
            g.fillRoundedRectangle(labelRect, 3.0f);

            g.setColour(fillColour);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(msText, labelRect, juce::Justification::centred, false);
        }
    }

    // Draw Current Playhead Line (Bright Cyan Vertical Bar)
    g.setColour(juce::Colour(0xff00e5ff));
    g.drawVerticalLine(static_cast<int>(boundsWidth - 2.0f), 0.0f, boundsHeight);
}
