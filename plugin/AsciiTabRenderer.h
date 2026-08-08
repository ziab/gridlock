#pragma once

#include "DrumMap.h"
#include "GridComponent.h"
#include "HitEvent.h"
#include "Timing.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace AsciiTab
{

struct RenderOptions
{
    double fixedIntervalPpq{0.0}; // 0 = Auto (detect finest needed up to 1/64)
    int maxColsPerSystem{64};     // 0 = no wrap
    int maxBarsPerSystem{2};      // 0 = unlimited (use view.numBars)
    bool showDeviationRow{true};
    bool showVelocityRow{false};
};

struct RenderResult
{
    std::string text;
    double usedIntervalPpq{0.25};
    int numSystems{1};
    int totalCols{0};
};

namespace detail
{
constexpr double kEps = 1e-9;
constexpr int kLabelWidth = 7;
constexpr int kMaxGridCols = 4096;

// ── pure helpers ──

inline double quantizePpq (const double ppq, const double interval) noexcept
{
    if (interval <= 0.0) return ppq;
    return std::round (ppq / interval) * interval;
}

inline int cellIndex (const double ppq, const double minPpq, const double interval) noexcept
{
    return static_cast<int> (std::floor ((ppq - minPpq) / interval + kEps));
}

inline double compensatedPpq (const HitEvent &e, const double latencyPpq) noexcept
{
    const double raw = (e.rawHitPpqPosition > 0.0) ? e.rawHitPpqPosition : e.hitPpqPosition;
    return raw - latencyPpq;
}

inline int laneForNote (const uint8_t note) noexcept
{
    const auto &lanes = DrumMap::getStandardDrumLanes ();
    for (size_t i = 0; i < lanes.size (); ++i)
        for (const uint8_t n : lanes[i].notes)
            if (n == note) return static_cast<int> (i);
    return static_cast<int> (lanes.size () - 1); // OTHER
}

inline bool hasCollision (const std::vector<HitEvent> &events,
                          const double minPpq,
                          const double maxPpq,
                          const double interval,
                          const double latencyPpq) noexcept
{
    if (interval <= 0.0 || events.empty()) return false;

    constexpr int numLanes = 6;
    const int numCols = static_cast<int> (std::ceil ((maxPpq - minPpq) / interval)) + 1;
    if (numCols <= 0 || numCols > kMaxGridCols) return true;

    std::vector<std::vector<int>> counts (numLanes, std::vector<int> (numCols, 0));

    for (const auto &e : events)
    {
        const double comp = compensatedPpq (e, latencyPpq);
        if (comp < minPpq - kEps || comp > maxPpq + kEps) continue;

        const int lane = std::clamp (laneForNote (e.noteNumber), 0, numLanes - 1);
        const int cell = cellIndex (comp, minPpq, interval);
        if (cell < 0 || cell >= numCols) continue;
        if (++counts[lane][cell] > 1) return true;
    }
    return false;
}

inline double selectInterval (const std::vector<HitEvent> &events,
                              const double minPpq,
                              const double maxPpq,
                              const double preferredInterval,
                              const double latencyPpq) noexcept
{
    if (preferredInterval > 0.0) return preferredInterval;

    constexpr double kCandidates[] = {0.25, 0.125, 0.0625}; // 1/16, 1/32, 1/64
    for (const double c : kCandidates)
        if (! hasCollision (events, minPpq, maxPpq, c, latencyPpq))
            return c;
    return 0.0625;
}

inline std::string formatDeviation (const double deltaMs, const float tol) noexcept
{
    if (std::abs (deltaMs) <= tol) return "  \xE2\x9C\x93 "; // ✓ with padding (utf-8)
    char buf[16]{};
    const int v = static_cast<int> (std::round (deltaMs));
    if (v > 0) std::snprintf (buf, sizeof (buf), "+%2d ", v);
    else       std::snprintf (buf, sizeof (buf), "%3d ", v);
    return std::string (buf);
}

inline std::string intervalLabel (const double ppq) noexcept
{
    if (std::abs (ppq - 0.5) < kEps) return "1/8";
    if (std::abs (ppq - 0.5 * 2.0 / 3.0) < kEps) return "1/8T";
    if (std::abs (ppq - 0.25) < kEps) return "1/16";
    if (std::abs (ppq - 0.25 * 2.0 / 3.0) < kEps) return "1/16T";
    if (std::abs (ppq - 0.125) < kEps) return "1/32";
    if (std::abs (ppq - 0.0625) < kEps) return "1/64";
    char buf[16]{};
    std::snprintf (buf, sizeof (buf), "%.4g", ppq);
    return std::string (buf);
}

inline char hitGlyph (const int lane, const size_t count) noexcept
{
    if (count == 0) return '-';
    if (count > 1) return (count < 10) ? static_cast<char> ('0' + count) : '*';
    if (lane == 3) return 'o'; // SNARE
    if (lane == 4) return 'K'; // KICK
    return 'x';
}

inline char deviationGlyph (const double deltaMs, const float tol) noexcept
{
    if (std::abs (deltaMs) <= tol) return '+';
    return (deltaMs < 0.0) ? '<' : '>';
}

// ── grid ──
struct Cell
{
    std::vector<const HitEvent*> hits;
    double avgDeltaMs{0.0};
};

using Grid = std::vector<std::vector<Cell>>;

inline Grid buildGrid (const std::vector<HitEvent> &events,
                       const double minPpq,
                       const double maxPpq,
                       const int totalCols,
                       const double interval,
                       const double latencyPpq) noexcept
{
    const auto &lanes = DrumMap::getStandardDrumLanes ();
    const int numLanes = static_cast<int> (lanes.size ());
    Grid grid (numLanes, std::vector<Cell> (totalCols));

    for (const auto &e : events)
    {
        const double comp = compensatedPpq (e, latencyPpq);
        if (comp < minPpq - kEps || comp > maxPpq + kEps) continue;

        const int lane = std::clamp (laneForNote (e.noteNumber), 0, numLanes - 1);
        const int col = std::clamp (cellIndex (comp, minPpq, interval), 0, totalCols - 1);
        grid[lane][col].hits.push_back (&e);
    }
    return grid;
}

inline void computeAverages (Grid &grid,
                             const double interval,
                             const float bpm,
                             const float tol,
                             const double latencyPpq) noexcept
{
    for (auto &lane : grid)
        for (auto &cell : lane)
        {
            if (cell.hits.empty()) continue;
            double sum = 0.0;
            for (const HitEvent *he : cell.hits)
                sum += Timing::compute (compensatedPpq (*he, latencyPpq), interval, bpm, tol).deltaMs;
            cell.avgDeltaMs = sum / static_cast<double> (cell.hits.size ());
        }
}

// ── layout ──
struct SystemLayout
{
    int colsPerSystem{0};
    int colsPerBar{0};
    int numSystems{1};
};

inline SystemLayout computeSystemLayout (const int totalCols,
                                         const int timeSigNum,
                                         const double interval,
                                         const RenderOptions &opts,
                                         const int numBars) noexcept
{
    const int colsPerBar = static_cast<int> (std::round (static_cast<double> (timeSigNum) / interval));
    const int effectiveColsPerBar = (colsPerBar > 0) ? colsPerBar : totalCols;
    const int effectiveBarsPerSystem = (opts.maxBarsPerSystem > 0) ? opts.maxBarsPerSystem : numBars;
    const int colsByBars = effectiveBarsPerSystem * effectiveColsPerBar;

    int colsPerSystem = totalCols;
    if (opts.maxColsPerSystem > 0 && opts.maxBarsPerSystem > 0)
        colsPerSystem = std::min (opts.maxColsPerSystem, colsByBars);
    else if (opts.maxBarsPerSystem > 0)
        colsPerSystem = colsByBars;
    else if (opts.maxColsPerSystem > 0)
        colsPerSystem = std::min (opts.maxColsPerSystem, totalCols);

    if (colsPerSystem <= 0) colsPerSystem = totalCols;

    const int numSystems = std::max (1, (totalCols + colsPerSystem - 1) / colsPerSystem);
    return {colsPerSystem, effectiveColsPerBar, numSystems};
}

// ── rendering helpers ──
inline std::string buildRuler (const int startCol,
                               const int sysCols,
                               const double minPpq,
                               const double interval,
                               const int timeSigNum) noexcept
{
    std::string ruler (sysCols, '-');
    for (int c = 0; c < sysCols; ++c)
    {
        const int globalCol = startCol + c;
        const double ppq = minPpq + globalCol * interval + interval * 0.5;
        double barFrac = std::fmod (ppq, static_cast<double> (timeSigNum));
        if (barFrac < 0) barFrac += timeSigNum;
        const bool isBar = (barFrac < interval * 0.6 || barFrac > timeSigNum - interval * 0.6);

        double beatFrac = std::fmod (ppq, 1.0);
        if (beatFrac < 0) beatFrac += 1.0;
        const bool isBeat = (beatFrac < interval * 0.6 || beatFrac > 1.0 - interval * 0.6);

        if (isBar) ruler[c] = '|';
        else if (isBeat) ruler[c] = ':';
    }
    return ruler;
}

inline std::string buildBeatNumbers (const int startCol,
                                     const int sysCols,
                                     const double interval,
                                     const int timeSigNum) noexcept
{
    std::string beatNums (sysCols, ' ');
    for (int c = 0; c < sysCols; ++c)
    {
        const int globalCol = startCol + c;
        const double offsetInBar = std::fmod (globalCol * interval, static_cast<double> (timeSigNum));
        if (std::abs (std::fmod (offsetInBar, 1.0)) < kEps)
        {
            const int beatNum = static_cast<int> (std::floor (offsetInBar + kEps)) + 1;
            if (beatNum >= 1 && beatNum <= timeSigNum)
                beatNums[c] = static_cast<char> ('0' + beatNum);
        }
    }
    return beatNums;
}

inline std::string buildHitRow (const Grid &grid,
                                const int lane,
                                const int startCol,
                                const int sysCols) noexcept
{
    std::string row (sysCols, '-');
    for (int c = 0; c < sysCols; ++c)
    {
        const auto &cell = grid[lane][startCol + c];
        row[c] = hitGlyph (lane, cell.hits.size ());
    }
    return row;
}

inline std::string buildDeviationIndicator (const Grid &grid,
                                            const int lane,
                                            const int startCol,
                                            const int sysCols,
                                            const float tol) noexcept
{
    std::string devRow (sysCols, ' ');
    for (int c = 0; c < sysCols; ++c)
    {
        const auto &cell = grid[lane][startCol + c];
        if (cell.hits.empty()) continue;
        devRow[c] = deviationGlyph (cell.avgDeltaMs, tol);
    }
    return devRow;
}

inline std::string buildVelocityRow (const Grid &grid,
                                     const int lane,
                                     const int startCol,
                                     const int sysCols) noexcept
{
    std::string velRow (sysCols, ' ');
    for (int c = 0; c < sysCols; ++c)
    {
        const auto &cell = grid[lane][startCol + c];
        if (cell.hits.empty()) continue;
        const int v = cell.hits[0]->velocity;
        if (v < 10) velRow[c] = static_cast<char> ('0' + v);
        else if (v < 36) velRow[c] = static_cast<char> ('A' + (v - 10));
        else velRow[c] = '*';
    }
    return velRow;
}

inline bool hasAnyHit (const std::string &row, const char empty = ' ') noexcept
{
    return std::any_of (row.begin (), row.end (), [empty] (const char ch) { return ch != empty; });
}

inline std::string padLabel (const juce::String &raw, const int width) noexcept
{
    std::string label = raw.toStdString ();
    if (static_cast<int> (label.size ()) > width) label = label.substr (0, width);
    else label += std::string (width - label.size (), ' ');
    return label;
}

inline std::pair<int,int> countAccuracy (const Grid &grid,
                                         const double interval,
                                         const float bpm,
                                         const float tol,
                                         const double latencyPpq) noexcept
{
    int total = 0, green = 0;
    for (const auto &lane : grid)
        for (const auto &cell : lane)
            for (const HitEvent *he : cell.hits)
            {
                const auto t = Timing::compute (compensatedPpq (*he, latencyPpq), interval, bpm, tol);
                ++total;
                if (std::abs (t.deltaMs) <= tol) ++green;
            }
    return {total, green};
}

inline void appendHeader (std::ostringstream &out,
                          const GridViewState &view,
                          const double interval,
                          const RenderOptions &opts,
                          const int totalCols)
{
    out << "Gridlock Tab | BPM " << static_cast<int> (std::round (view.bpm))
        << " " << view.timeSigNum << "/4"
        << " | Subdiv " << intervalLabel (interval);
    if (opts.fixedIntervalPpq <= 0.0) out << " AUTO";
    out << " tol " << static_cast<int> (view.toleranceMs) << "ms"
        << " | Bars " << view.numBars << " [" << totalCols << " cols]\n";
    out << "Legend: x/o/K/S=hit  -=empty  |=bar  2/3=stacked  -12/+8=ms rush/drag  +=on-grid\n";
}

inline void appendSystem (std::ostringstream &out,
                          const Grid &grid,
                          const GridViewState &view,
                          const RenderOptions &opts,
                          const SystemLayout &layout,
                          const double minPpq,
                          const double interval,
                          const int sysIndex)
{
    const int startCol = sysIndex * layout.colsPerSystem;
    const int endCol = std::min (startCol + layout.colsPerSystem, static_cast<int> (grid[0].size ()));
    const int sysCols = endCol - startCol;
    const int sysBars = static_cast<int> (std::ceil (static_cast<double> (sysCols) / layout.colsPerBar));
    const int barsPerSystem = (opts.maxBarsPerSystem > 0) ? opts.maxBarsPerSystem : view.numBars;
    const int startBar = sysIndex * barsPerSystem + 1;
    const int endBar = std::min (startBar + sysBars - 1, view.numBars);

    const auto &lanes = DrumMap::getStandardDrumLanes ();
    const int numLanes = static_cast<int> (lanes.size ());

    out << "\n-- System " << (sysIndex + 1) << "/" << layout.numSystems
        << " Bars " << startBar << "-" << endBar << " --\n";

    const std::string ruler = buildRuler (startCol, sysCols, minPpq, interval, view.timeSigNum);
    const std::string beatNums = buildBeatNumbers (startCol, sysCols, interval, view.timeSigNum);
    out << std::string (kLabelWidth, ' ') << " " << ruler << "\n";
    out << std::string (kLabelWidth, ' ') << " " << beatNums << "\n";

    for (int lane = 0; lane < numLanes; ++lane)
    {
        const std::string row = buildHitRow (grid, lane, startCol, sysCols);
        const std::string label = padLabel (lanes[lane].label, kLabelWidth);
        out << label << " |" << row << "|\n";

        if (opts.showDeviationRow)
        {
            const std::string devRow = buildDeviationIndicator (grid, lane, startCol, sysCols, view.toleranceMs);
            if (hasAnyHit (devRow))
            {
                out << std::string (kLabelWidth, ' ') << " " << devRow << "  (";
                bool first = true;
                for (int c = 0; c < sysCols; ++c)
                {
                    const auto &cell = grid[lane][startCol + c];
                    if (cell.hits.empty()) continue;
                    if (! first) out << " ";
                    first = false;
                    out << c << ":" << formatDeviation (cell.avgDeltaMs, view.toleranceMs);
                    if (cell.hits.size () > 1) out << "x" << cell.hits.size ();
                }
                out << ")\n";
            }
        }

        if (opts.showVelocityRow)
        {
            const std::string velRow = buildVelocityRow (grid, lane, startCol, sysCols);
            if (hasAnyHit (velRow)) out << std::string (kLabelWidth, ' ') << " " << velRow << "\n";
        }
    }
}

} // namespace detail

inline RenderResult render (const std::vector<HitEvent> &events,
                            const GridViewState &view,
                            const RenderOptions &opts = {}) noexcept
{
    RenderResult res;
    if (view.numBars <= 0 || view.bpm <= 0.0f || view.gridSubdivisionPpq <= 0.0) return res;

    const double latencyPpq = (static_cast<double> (view.latencyOffsetMs) / 1000.0) * (view.bpm / 60.0);
    const double totalPpqWindow = static_cast<double> (view.numBars) * view.timeSigNum;
    const double maxPpq = view.currentPpq;
    const double minPpq = maxPpq - totalPpqWindow;

    const double interval = detail::selectInterval (events, minPpq, maxPpq,
                                                    opts.fixedIntervalPpq, latencyPpq);
    res.usedIntervalPpq = interval;

    const int totalCols = static_cast<int> (std::round (totalPpqWindow / interval));
    res.totalCols = totalCols;
    if (totalCols <= 0) return res;

    detail::Grid grid = detail::buildGrid (events, minPpq, maxPpq, totalCols, interval, latencyPpq);
    detail::computeAverages (grid, interval, view.bpm, view.toleranceMs, latencyPpq);

    const detail::SystemLayout layout = detail::computeSystemLayout (
        totalCols, view.timeSigNum, interval, opts, view.numBars);
    res.numSystems = layout.numSystems;

    std::ostringstream out;
    detail::appendHeader (out, view, interval, opts, totalCols);

    const auto [totalHits, greenHits] = detail::countAccuracy (grid, interval, view.bpm, view.toleranceMs, latencyPpq);
    const int accPct = (totalHits > 0) ? static_cast<int> (std::round (100.0 * greenHits / totalHits)) : 0;

    for (int sys = 0; sys < layout.numSystems; ++sys)
        detail::appendSystem (out, grid, view, opts, layout, minPpq, interval, sys);

    out << "\nACCURACY " << accPct << "% (" << greenHits << "/" << totalHits
        << " on-grid) tol " << static_cast<int> (view.toleranceMs) << "ms\n";
    out << "Tip: paste this tab into an LLM with: 'Analyze timing tendencies per limb and give 2 exercises.'\n";

    res.text = out.str ();
    return res;
}

} // namespace AsciiTab
