#include "GridComponent.h"
#include <cmath>
#include <algorithm>

GridComponent::GridComponent()
{
    drumLanes = {
        { "KICK",    { 36 } },
        { "SNARE",   { 38, 40 } },
        { "HI-HAT",  { 42, 44, 46 } },
        { "TOMS",    { 48, 45, 43 } },
        { "CYMBALS", { 49, 51, 53 } },
        { "OTHER",   {} }
    };
}

int GridComponent::getLaneIndexForNote(uint8_t note) const noexcept
{
    if (note == 36) return 0; // Kick
    if (note == 38 || note == 40) return 1; // Snare Head / Rim
    if (note == 42 || note == 44 || note == 46) return 2; // Hi-Hat Closed/Pedal/Open
    if (note == 48 || note == 45 || note == 43) return 3; // Toms
    if (note == 49 || note == 51 || note == 53) return 4; // Cymbals
    return 5; // Other
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

void GridComponent::updateEvents(const std::vector<HitEvent>& events, double currentPpq, int numBars, double gridSubdivisionPpq)
{
    activeEvents = events;
    currentPpqPos = currentPpq;
    barsWindow = (numBars > 0) ? numBars : 4;
    subdivisionPpq = (gridSubdivisionPpq > 0.0) ? gridSubdivisionPpq : 0.25;
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
    const float boundsWidth = static_cast<float>(getWidth());
    const float boundsHeight = static_cast<float>(getHeight());

    const float canvasLeft = labelWidth;
    const float canvasWidth = boundsWidth - labelWidth;

    if (canvasWidth <= 10.0f || boundsHeight <= 10.0f)
        return;

    const int numLanes = static_cast<int>(drumLanes.size());
    const float laneHeight = boundsHeight / static_cast<float>(numLanes);

    // Draw Lane Backgrounds & Labels
    for (int i = 0; i < numLanes; ++i)
    {
        const float y = i * laneHeight;
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
    const double totalPpqWindow = static_cast<double>(barsWindow) * 4.0;
    const double maxPpq = currentPpqPos;
    const double minPpq = maxPpq - totalPpqWindow;

    // Draw Grid Lines (Subdivisions and Bar Boundaries)
    if (subdivisionPpq > 0.0)
    {
        const double firstTick = std::floor(minPpq / subdivisionPpq) * subdivisionPpq;

        for (double tick = firstTick; tick <= maxPpq; tick += subdivisionPpq)
        {
            if (tick < minPpq) continue;

            const float normalizedX = static_cast<float>((tick - minPpq) / totalPpqWindow);
            const float x = canvasLeft + (normalizedX * canvasWidth);

            const double barCheck = std::abs(std::fmod(tick, 4.0));
            const bool isBarLine = (barCheck < 0.001 || barCheck > 3.999);

            if (isBarLine)
            {
                g.setColour(juce::Colour(0xff4a526b)); // Strong bar line
                g.drawVerticalLine(static_cast<int>(x), 0.0f, boundsHeight);
            }
            else
            {
                g.setColour(juce::Colour(0xff242938)); // Subtle subdivision line
                g.drawVerticalLine(static_cast<int>(x), 0.0f, boundsHeight);
            }
        }
    }

    // Render Hit Events with Continuous Dynamic Color Gradient
    const float nodeRadius = 11.0f; // High visibility hit nodes

    for (const auto& event : activeEvents)
    {
        if (event.hitPpqPosition < minPpq || event.hitPpqPosition > maxPpq)
            continue;

        const int laneIndex = getLaneIndexForNote(event.noteNumber);
        if (laneIndex < 0 || laneIndex >= numLanes) continue;

        const float normalizedX = static_cast<float>((event.hitPpqPosition - minPpq) / totalPpqWindow);
        const float hitX = canvasLeft + (normalizedX * canvasWidth);
        const float hitY = (laneIndex * laneHeight) + (laneHeight * 0.5f);

        // Continuous Dynamic Color Interpolation
        const juce::Colour fillColour = getContinuousHitColor(event.normalizedDeviation);

        const auto hitRect = juce::Rectangle<float>(hitX - nodeRadius, hitY - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);

        // Outer contrast ring
        g.setColour(juce::Colour(0xff0a0c10));
        g.fillEllipse(hitRect.expanded(2.0f));

        // Filled color node
        g.setColour(fillColour);
        g.fillEllipse(hitRect);

        // Draw Delta ms text inside node
        g.setColour(juce::Colour(0xff000000));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        juce::String deltaText = juce::String(static_cast<int>(std::round(event.deltaMs)));
        if (event.deltaMs > 0) deltaText = "+" + deltaText;
        g.drawText(deltaText, hitRect, juce::Justification::centred, false);
    }

    // Draw Current Playhead Line (Bright Cyan Vertical Bar)
    g.setColour(juce::Colour(0xff00e5ff));
    g.drawVerticalLine(static_cast<int>(boundsWidth - 2.0f), 0.0f, boundsHeight);
}
