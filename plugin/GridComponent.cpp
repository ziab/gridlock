#include "GridComponent.h"
#include <cmath>
#include <algorithm>

GridComponent::GridComponent()
{
    drumLanes = DrumMap::getStandardDrumLanes();
}

int GridComponent::getLaneIndexForNote(uint8_t note) const noexcept
{
    for (size_t i = 0; i < drumLanes.size(); ++i)
    {
        for (uint8_t laneNote : drumLanes[i].notes)
        {
            if (laneNote == note)
                return static_cast<int>(i);
        }
    }
    return 5; // Default "OTHER" lane
}

juce::Colour GridComponent::getContinuousHitColor(float deltaMs, float toleranceMs, float maxErrorMs) noexcept
{
    const float absDelta = std::abs(deltaMs);

    // Everything inside tolerance threshold is 100% Emerald Green
    if (absDelta <= toleranceMs)
    {
        return juce::Colour(0xff00ff88); // Pure Emerald Green
    }

    // Beyond tolerance: Instantly pops to Yellow (Rush) or Cyan (Drag) and deepens to Red/Purple
    const float denom = std::max(0.001f, maxErrorMs - toleranceMs);
    const float t = std::clamp((absDelta - toleranceMs) / denom, 0.0f, 1.0f);

    if (deltaMs < 0.0f) // Rush / Early -> Yellow (#FFEA00) -> Orange (#FF9100) -> Electric Red (#FF1744)
    {
        if (t <= 0.5f) {
            const float k = t / 0.5f;
            return juce::Colour::fromRGB(
                255,
                static_cast<juce::uint8>(juce::jmap(k, 234.0f, 145.0f)),
                0
            );
        } else {
            const float k = (t - 0.5f) / 0.5f;
            return juce::Colour::fromRGB(
                255,
                static_cast<juce::uint8>(juce::jmap(k, 145.0f, 23.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 0.0f, 68.0f))
            );
        }
    }
    else // Drag / Late -> Cyan (#00E5FF) -> Deep Blue (#2979FF) -> Vivid Purple (#D500F9)
    {
        if (t <= 0.5f) {
            const float k = t / 0.5f;
            return juce::Colour::fromRGB(
                static_cast<juce::uint8>(juce::jmap(k, 0.0f, 41.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 229.0f, 121.0f)),
                255
            );
        } else {
            const float k = (t - 0.5f) / 0.5f;
            return juce::Colour::fromRGB(
                static_cast<juce::uint8>(juce::jmap(k, 41.0f, 213.0f)),
                static_cast<juce::uint8>(juce::jmap(k, 121.0f, 0.0f)),
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
                                 bool showVelocityLabels,
                                 bool showNoteNumbers,
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
    displayVelLabels = showVelocityLabels;
    displayNoteNumLabels = showNoteNumbers;
    toleranceMsVal = (toleranceMs >= 0.0f) ? toleranceMs : 20.0f;
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
    const float footerHeight = 26.0f;
    const float boundsWidth = static_cast<float>(getWidth());
    const float boundsHeight = static_cast<float>(getHeight());

    const float canvasLeft = labelWidth;
    const float canvasWidth = boundsWidth - labelWidth;

    if (canvasWidth <= 10.0f || boundsHeight <= rulerHeight + footerHeight + 10.0f)
        return;

    const float laneAreaTop = rulerHeight;
    const float laneAreaHeight = boundsHeight - rulerHeight - footerHeight;

    const int numLanes = static_cast<int>(drumLanes.size());
    const float laneHeight = laneAreaHeight / static_cast<float>(numLanes);

    // Calculate Dynamic Progressive Scaling Factors based on Window Size and Bars Window Zoom
    float zoomScale = 1.0f;
    if (barsWindow == 2) zoomScale = 1.4f;
    else if (barsWindow == 1) zoomScale = 2.0f;

    const float windowWidthScale = std::clamp(canvasWidth / 1200.0f, 0.75f, 2.5f);
    const float laneHeightScale = std::clamp(laneHeight / 80.0f, 0.75f, 2.5f);
    const float dynamicScale = std::clamp(zoomScale * windowWidthScale, 0.8f, 3.0f);

    const float nodeRadius = 11.0f * dynamicScale; // Up to 26.4px radius (52.8px diameter circle node!)
    const float strokeWidth = std::clamp(2.5f * dynamicScale, 2.0f, 6.0f);

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

        // Bold High-Contrast Text for Drum Lanes (Scaled dynamically for throne viewing)
        const float laneFontHeight = std::clamp(16.0f * laneHeightScale, 14.0f, 32.0f);
        g.setColour(juce::Colour(0xfff0f2f8));
        g.setFont(juce::Font(laneFontHeight, juce::Font::bold));
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
                g.drawVerticalLine(static_cast<int>(x), 0.0f, boundsHeight - footerHeight);

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
                g.drawVerticalLine(static_cast<int>(x), rulerHeight, boundsHeight - footerHeight);

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
                g.drawVerticalLine(static_cast<int>(x), rulerHeight, boundsHeight - footerHeight);
            }
        }
    }

    // Track Visible Hits for Rolling Window Accuracy Score Calculation
    int totalVisibleHits = 0;
    int greenVisibleHits = 0;

    // Render Hit Events with Live Real-Time Latency & Tolerance Recalculation
    const double userLatencyPpq = (static_cast<double>(latencyOffsetMsVal) / 1000.0) * (static_cast<double>(bpmVal) / 60.0);
    const float maxErrorMs = static_cast<float>((subdivisionPpq / 2.0) * (60.0 / static_cast<double>(bpmVal)) * 1000.0);

    for (const auto& event : activeEvents)
    {
        const double rawPpq = (event.rawHitPpqPosition > 0.0) ? event.rawHitPpqPosition : event.hitPpqPosition;
        const double compPpq = rawPpq - userLatencyPpq;

        if (compPpq < minPpq || compPpq > maxPpq)
            continue;

        const int laneIndex = getLaneIndexForNote(event.noteNumber);
        if (laneIndex < 0 || laneIndex >= numLanes) continue;

        // Recalculate Delta MS live using current user latency & subdivision settings
        const double nearestGridPpq = std::round(compPpq / subdivisionPpq) * subdivisionPpq;
        const double deltaPpq = compPpq - nearestGridPpq;
        const double liveDeltaMs = (deltaPpq / (static_cast<double>(bpmVal) / 60.0)) * 1000.0;
        const float absDelta = static_cast<float>(std::abs(liveDeltaMs));

        totalVisibleHits++;
        if (absDelta <= toleranceMsVal)
        {
            greenVisibleHits++;
        }

        const float normalizedX = static_cast<float>((compPpq - minPpq) / totalPpqWindow);
        const float hitX = canvasLeft + (normalizedX * canvasWidth);
        const float hitY = laneAreaTop + (laneIndex * laneHeight) + (laneHeight * 0.5f);

        // Instant Yellow/Cyan transition at tolerance boundary, deepening into Red/Purple at maxErrorMs
        const juce::Colour fillColour = getContinuousHitColor(static_cast<float>(liveDeltaMs), toleranceMsVal, maxErrorMs);

        const auto hitRect = juce::Rectangle<float>(hitX - nodeRadius, hitY - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);

        // Outer contrast ring
        g.setColour(juce::Colour(0xff0a0c10));
        g.fillEllipse(hitRect.expanded(2.0f * dynamicScale));

        // Filled color node
        g.setColour(fillColour);
        g.fillEllipse(hitRect);

        // Render Vector Symbol inside node circle
        if (displayVelLabels)
        {
            const float velFontHeight = std::clamp(10.0f * dynamicScale, 9.0f, 22.0f);
            g.setColour (juce::Colour (0xff000000));
            g.setFont (juce::Font (velFontHeight, juce::Font::bold));
            g.drawText (juce::String (event.velocity), hitRect, juce::Justification::centred, false);
        }
        else if (absDelta <= toleranceMsVal)
        {
            // Vector Checkmark '✓' inside green node
            juce::Path checkPath;
            const float r = nodeRadius * 0.55f;
            checkPath.startNewSubPath (hitX - r * 0.55f, hitY + r * 0.05f);
            checkPath.lineTo (hitX - r * 0.10f, hitY + r * 0.55f);
            checkPath.lineTo (hitX + r * 0.65f, hitY - r * 0.45f);

            g.setColour (juce::Colour (0xff0a0c10));
            g.strokePath (checkPath, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (liveDeltaMs < 0.0f)
        {
            // Vector Right Arrow '>' for Rush (tells drummer to play LATER)
            juce::Path arrowPath;
            const float r = nodeRadius * 0.45f;
            arrowPath.startNewSubPath (hitX - r * 0.45f, hitY - r * 0.65f);
            arrowPath.lineTo (hitX + r * 0.45f, hitY);
            arrowPath.lineTo (hitX - r * 0.45f, hitY + r * 0.65f);

            g.setColour (juce::Colour (0xff0a0c10));
            g.strokePath (arrowPath, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            // Vector Left Arrow '<' for Drag (tells drummer to play EARLIER)
            juce::Path arrowPath;
            const float r = nodeRadius * 0.45f;
            arrowPath.startNewSubPath (hitX + r * 0.45f, hitY - r * 0.65f);
            arrowPath.lineTo (hitX - r * 0.45f, hitY);
            arrowPath.lineTo (hitX + r * 0.45f, hitY + r * 0.65f);

            g.setColour (juce::Colour (0xff0a0c10));
            g.strokePath (arrowPath, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Display NOTE # Label above note circle if enabled
        if (displayNoteNumLabels)
        {
            juce::String noteText = "#" + juce::String(event.noteNumber);

            const float noteLabelW = 38.0f * dynamicScale;
            const float noteLabelH = 13.0f * dynamicScale;
            const auto noteLabelRect = juce::Rectangle<float>(hitX - (noteLabelW * 0.5f), hitY - nodeRadius - noteLabelH - 2.0f, noteLabelW, noteLabelH);

            // Dark pill for high contrast note number debugging
            g.setColour(juce::Colour(0xd01e293b));
            g.fillRoundedRectangle(noteLabelRect, 3.0f * dynamicScale);

            const float noteFontHeight = std::clamp(9.5f * dynamicScale, 8.5f, 20.0f);
            g.setColour(juce::Colour(0xff38bdf8)); // Bright Sky Blue Note Text
            g.setFont(juce::Font(noteFontHeight, juce::Font::bold));
            g.drawText(noteText, noteLabelRect, juce::Justification::centred, false);
        }

        // Display MS Offset Label underneath note if enabled
        if (displayMsLabels)
        {
            juce::String msText = juce::String(static_cast<int>(std::round(liveDeltaMs))) + "ms";
            if (liveDeltaMs > 0.0) msText = "+" + msText;

            const float labelW = 44.0f * dynamicScale;
            const float labelH = 14.0f * dynamicScale;
            const auto labelRect = juce::Rectangle<float>(hitX - (labelW * 0.5f), hitY + nodeRadius + 2.0f, labelW, labelH);

            // Dark background pill for throne-distance contrast
            g.setColour(juce::Colour(0xd00a0c10));
            g.fillRoundedRectangle(labelRect, 3.0f * dynamicScale);

            const float msFontHeight = std::clamp(10.0f * dynamicScale, 9.0f, 22.0f);
            g.setColour(fillColour);
            g.setFont(juce::Font(msFontHeight, juce::Font::bold));
            g.drawText(msText, labelRect, juce::Justification::centred, false);
        }
    }

    // Draw Current Playhead Line (Bright Cyan Vertical Bar)
    g.setColour(juce::Colour(0xff00e5ff));
    g.drawVerticalLine(static_cast<int>(boundsWidth - 2.0f), 0.0f, boundsHeight - footerHeight);

    // Draw Bottom Accuracy Score Footer Bar
    const float footerY = boundsHeight - footerHeight;
    g.setColour(juce::Colour(0xff181b24));
    g.fillRect(juce::Rectangle<float>(0.0f, footerY, boundsWidth, footerHeight));

    g.setColour(juce::Colour(0xff2d3245));
    g.drawHorizontalLine(static_cast<int>(footerY), 0.0f, boundsWidth);
    g.drawVerticalLine(static_cast<int>(labelWidth), footerY, boundsHeight);

    const int accuracyPct = (totalVisibleHits > 0)
        ? static_cast<int>(std::round((static_cast<double>(greenVisibleHits) / static_cast<double>(totalVisibleHits)) * 100.0))
        : 100;

    // Sidebar Score Title
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("ACCURACY", juce::Rectangle<float>(12.0f, footerY, labelWidth - 16.0f, footerHeight),
               juce::Justification::centredLeft, true);

    // Progress Bar Track
    const float barTrackX = canvasLeft + 12.0f;
    const float barTrackW = std::max(100.0f, canvasWidth - 320.0f);
    const float barTrackH = 10.0f;
    const float barTrackY = footerY + (footerHeight - barTrackH) * 0.5f;

    g.setColour(juce::Colour(0xff111827));
    g.fillRoundedRectangle(juce::Rectangle<float>(barTrackX, barTrackY, barTrackW, barTrackH), 3.0f);

    // Emerald Green Progress Fill
    const float fillW = barTrackW * (static_cast<float>(accuracyPct) / 100.0f);
    if (fillW > 0.0f)
    {
        g.setColour(juce::Colour(0xff00ff88)); // Pure Emerald Green
        g.fillRoundedRectangle(juce::Rectangle<float>(barTrackX, barTrackY, fillW, barTrackH), 3.0f);
    }

    // Progress Bar Border
    g.setColour(juce::Colour(0xff374151));
    g.drawRoundedRectangle(juce::Rectangle<float>(barTrackX, barTrackY, barTrackW, barTrackH), 3.0f, 1.0f);

    // Accuracy Score Text Readout
    const float textX = barTrackX + barTrackW + 16.0f;
    const juce::String scoreText = juce::String(accuracyPct) + "%  (" + juce::String(greenVisibleHits) + " / " + juce::String(totalVisibleHits) + " On-Grid)";

    g.setColour(juce::Colour(0xff00ff88));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(scoreText, juce::Rectangle<float>(textX, footerY, boundsWidth - textX - 12.0f, footerHeight),
               juce::Justification::centredLeft, true);
}
