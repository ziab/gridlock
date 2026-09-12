#pragma once

#include "Constants.h"
#include "DrumMap.h"
#include "Timing.h"

#include <array>
#include <cmath>
#include <limits>

// Audio-thread owned. Fixed storage; no allocation, locks, or UI callbacks.
class DrillEngine {
public:
  enum class State { Idle, Waiting, Playing, Paused, Finished };
  enum class Action { Start, Hold, TooFast, Pause, Resume, Finish, Retry, Disconnect, Tolerance, PassThreshold };
  struct Config {
    std::array<char, constants::drill::maxPattern + 1> pattern{};
    int length{0}, kick{DrumMap::Kick}, beats{4}, minVelocity{5};
    double interval{constants::musical::ppq_1_16}, bpm{constants::drill::startBpm};
    float tolerance{constants::params::toleranceDefault};
    double passThreshold{constants::drill::passThresholdDefault};
    double latencyMs{0};
    int clickSubdivisionIndex () const {
      return interval == constants::musical::ppq_1_8 ? 2 : interval == constants::musical::ppq_1_8T ? 4 : 3;
    }
  };
  struct Snapshot {
    Config config;
    State state{State::Idle};
    double bpm{constants::drill::startBpm}, best{0}, attempted{0}, nextBpm{0};
    double progress{0}, accuracy{0}, toleranceMs{20};
    int passes{0}, blocks{0}, activeSlot{0}, beatsRemaining{0};
    int correct{0}, missing{0}, wrong{0}, late{0}, extras{0}, expected{0};
    bool sequenceDetected{false}, sequenceSeen{false};
    bool automatic{true}, noHits{false}, limitReached{false};
    bool hasBlock{false}, failAccuracy{false}, failExtras{false}, failSequence{false};
  };

  Snapshot view;
  bool active () const {
    return view.state != State::Idle && view.state != State::Finished;
  }
  bool running () const {
    return active () && view.state != State::Paused;
  }
  double nextBoundary () const {
    return pendingAt;
  }

  void command (Action action, const Config &config, double ppq) {
    if (action == Action::Start) {
      view = {};
      view.config = config;
      view.bpm = config.bpm;
      view.attempted = config.bpm;
      waitForFirstHit (ppq);
      return;
    }
    if (!active ()) {
      return;
    }
    if (action == Action::Tolerance && config.tolerance != view.config.tolerance) {
      view.config.tolerance = config.tolerance;
      view.best = 0;
      view.accuracy = 0;
      view.correct = view.missing = view.wrong = view.late = view.extras = 0;
      view.hasBlock = view.failAccuracy = view.failExtras = view.failSequence = false;
      pendingAt = infinity;
      view.nextBpm = 0;
      // Do not mix results measured with different tolerances. Keep the clock and phase.
      beginStage (std::ceil (ppq / interval ()) * interval ());
    }
    if (action == Action::PassThreshold && config.passThreshold != view.config.passThreshold) {
      view.config.passThreshold = config.passThreshold;
      view.best = 0;
      view.accuracy = 0;
      view.correct = view.missing = view.wrong = view.late = view.extras = 0;
      view.hasBlock = view.failAccuracy = view.failExtras = view.failSequence = false;
      pendingAt = infinity;
      view.nextBpm = 0;
      // A different pass bar makes old confirmations incomparable. Keep the clock and phase.
      beginStage (std::ceil (ppq / interval ()) * interval ());
    }
    if (action == Action::Finish) {
      view.state = State::Finished;
      view.nextBpm = 0;
      pendingAt = infinity;
    }
    if (action == Action::Pause) {
      view.sequenceDetected = false;
      view.state = State::Paused;
      view.nextBpm = 0;
      pendingAt = infinity;
    }
    if (action == Action::Hold || action == Action::Disconnect) {
      view.automatic = false;
      pendingAt = infinity;
      view.nextBpm = 0;
    }
    if (action == Action::TooFast) {
      view.bpm = fallbackBpm ();
      view.automatic = false;
      view.limitReached = true;
      waitForFirstHit (ppq);
    }
    if (action == Action::Resume || action == Action::Retry) {
      if (action == Action::Retry) {
        view.automatic = true;
        view.limitReached = false;
      }
      waitForFirstHit (ppq);
    }
  }

  void advance (double physicalPpq, double compensatedPpq) {
    if (!running ()) {
      return;
    }
    if (view.sequenceDetected && compensatedPpq > (lastSequenceTick + 1.5) * interval ()) {
      loseSequence ();
    }
    if (physicalPpq + epsilon >= pendingAt) {
      const double boundary = pendingAt;
      view.bpm = view.nextBpm;
      view.attempted = std::max (view.attempted, view.bpm);
      pendingAt = infinity;
      view.nextBpm = 0;
      // One whole repetition at the new tempo is deliberately unscored.
      beginStage (boundary + cycle ());
    }
    if (view.state == State::Waiting) {
      return;
    }
    if (physicalPpq - lastHit >= constants::drill::silenceBars * view.config.beats) {
      waitForFirstHit (physicalPpq);
      view.noHits = true;
      return;
    }
    while (pendingAt == infinity && compensatedPpq > origin + (finalized + 0.5) * interval ()) {
      finalizeSlot ();
      if (finalized % blockSize == 0) {
        assess (physicalPpq);
      }
    }
    view.progress = static_cast<double> (finalized % blockSize) / blockSize;
  }

  void hit (double compensatedPpq, int note) {
    if (!running () || note == DrumMap::PedalHiHat) {
      return;
    }
    if (view.state == State::Waiting) {
      const bool firstIsKick = view.config.pattern[0] == 'K';
      if (firstIsKick != (note == view.config.kick)) {
        return;
      }
      // Capture grouping phase, but retain the metronome's grid for timing accuracy.
      beginStage (std::round (compensatedPpq / interval ()) * interval ());
      view.state = State::Playing;
      view.noHits = false;
    }
    const auto tick = static_cast<int64_t> (std::floor (compensatedPpq / interval () + 0.5));
    const bool primaryHit = tick > lastSequenceTick;
    if (primaryHit) {
      trackSequence (tick, note == view.config.kick);
    }
    const bool wantsKick = view.config.pattern[static_cast<size_t> (nextStroke)] == 'K';
    if (primaryHit) {
      view.activeSlot = nextStroke;
      nextStroke = (nextStroke + 1) % view.config.length;
      lastSequenceTick = tick;
    }
    const auto slot = static_cast<int64_t> (std::floor ((compensatedPpq - origin) / interval () + 0.5));
    lastHit = compensatedPpq;
    if (slot < finalized || slot < 0 || pendingAt != infinity) {
      return;
    }
    auto &entry = slots[static_cast<size_t> (slot) % slots.size ()];
    if (entry.index != slot) {
      entry = {slot, 0, false, false};
    }
    ++entry.hits;
    if (entry.hits != 1) {
      return;
    }
    entry.rightClass = wantsKick == (note == view.config.kick);
    const auto result =
        Timing::compute (compensatedPpq - origin, interval (), view.bpm, static_cast<float> (view.toleranceMs));
    entry.onTime = result.state == TimingState::OnGrid;
  }

private:
  struct Slot {
    int64_t index{-1};
    int hits{0};
    bool rightClass{false}, onTime{false};
  };
  static constexpr double infinity = std::numeric_limits<double>::infinity ();
  static constexpr double epsilon = 1e-9;
  std::array<Slot, 4096> slots{};
  double origin{0}, lastHit{0}, pendingAt{infinity};
  int64_t finalized{0}, lastSequenceTick{std::numeric_limits<int64_t>::min ()};
  int nextStroke{0};
  std::array<int, constants::drill::maxPattern> matches{};
  int blockSize{32}, good{0}, missing{0}, wrong{0}, late{0}, extras{0}, struggles{0};
  double interval () const {
    return view.config.interval;
  }
  double cycle () const {
    return interval () * view.config.length;
  }
  double fallbackBpm () const {
    const double raw = view.best > 0 ? view.best : std::floor (view.bpm * constants::drill::resumeRatio);
    return std::clamp (raw, constants::drill::minBpm, static_cast<double> (constants::params::bpmMax));
  }
  void loseSequence () {
    if (view.sequenceDetected) {
      pendingAt = infinity;
      view.nextBpm = 0;
      view.passes = 0;
    }
    view.sequenceDetected = false;
  }
  void trackSequence (int64_t tick, bool kick) {
    // Keep every possible cyclic phase: R and L are observationally identical.
    if (lastSequenceTick != std::numeric_limits<int64_t>::min () && tick != lastSequenceTick + 1) {
      matches.fill (0);
      loseSequence ();
    }
    const auto previous = matches;
    int bestPhase = -1;
    for (int phase = 0; phase < view.config.length; ++phase) {
      const auto index = static_cast<size_t> (phase);
      const int preceding = (phase + view.config.length - 1) % view.config.length;
      matches[index] = (view.config.pattern[index] == 'K') == kick
                           ? std::min (view.config.length, previous[static_cast<size_t> (preceding)] + 1)
                           : 0;
      if (matches[index] >= view.config.length && (bestPhase < 0 || phase == nextStroke)) {
        bestPhase = phase;
      }
    }
    if (bestPhase >= 0) {
      nextStroke = bestPhase;
      view.sequenceDetected = view.sequenceSeen = true;
    } else {
      loseSequence ();
    }
  }
  void waitForFirstHit (double ppq) {
    pendingAt = infinity;
    view.nextBpm = 0;
    view.noHits = false;
    loseSequence ();
    matches.fill (0);
    nextStroke = 0;
    lastSequenceTick = std::numeric_limits<int64_t>::min ();
    beginStage (ppq);
    view.activeSlot = 0;
    view.state = State::Waiting;
  }
  void beginStage (double start) {
    origin = start;
    lastHit = start;
    finalized = 0;
    slots.fill ({});
    good = missing = wrong = late = extras = struggles = 0;
    view.passes = view.blocks = 0;
    view.progress = 0;
    const double secondsPerStroke = interval () * 60.0 / view.bpm;
    const double needed =
        std::max<double> (constants::drill::minStrokes, constants::drill::minSeconds / secondsPerStroke);
    blockSize = static_cast<int> (std::ceil (needed / view.config.length)) * view.config.length;
    view.expected = blockSize;
    view.toleranceMs =
        std::min<double> (view.config.tolerance, secondsPerStroke * 1000 * constants::drill::toleranceFraction);
  }
  void finalizeSlot () {
    auto &slot = slots[static_cast<size_t> (finalized) % slots.size ()];
    if (slot.index != finalized || slot.hits == 0) {
      ++missing;
      loseSequence ();
    } else {
      extras += slot.hits - 1;
      if (!slot.rightClass) {
        ++wrong;
      } else if (!slot.onTime) {
        ++late;
      } else {
        ++good;
      }
    }
    ++finalized;
  }
  void assess (double physicalPpq) {
    view.correct = good;
    view.missing = missing;
    view.wrong = wrong;
    view.late = late;
    view.extras = extras;
    view.accuracy = static_cast<double> (good) / blockSize;
    const double extraRate = static_cast<double> (extras) / blockSize;
    const bool okSequence = view.sequenceDetected;
    const bool okAccuracy = view.accuracy >= view.config.passThreshold;
    const bool okExtras = extraRate <= constants::drill::passExtras;
    const bool pass = okSequence && okAccuracy && okExtras;
    view.hasBlock = true;
    view.failSequence = !okSequence;
    view.failAccuracy = !okAccuracy;
    view.failExtras = !okExtras;
    const bool struggle = view.accuracy < constants::drill::holdAccuracy || extraRate > constants::drill::holdExtras;
    view.passes = pass ? std::min (constants::drill::requiredPasses, view.passes + 1) : 0;
    struggles = struggle ? struggles + 1 : 0;
    ++view.blocks;
    good = missing = wrong = late = extras = 0;
    if (view.passes >= constants::drill::requiredPasses) {
      view.best = std::max (view.best, view.bpm);
      if (view.automatic && view.bpm < constants::params::bpmMax) {
        schedule (std::min<double> (constants::params::bpmMax, view.bpm + constants::drill::bpmStep), physicalPpq);
      } else {
        view.automatic = false;
      }
    } else if (view.automatic &&
               (struggles >= constants::drill::requiredStruggles || view.blocks >= constants::drill::maxBlocks)) {
      const double confirmed =
          std::clamp (view.best, constants::drill::minBpm, static_cast<double> (constants::params::bpmMax));
      if (view.best > 0 && confirmed < view.bpm - 1e-9) {
        // Climbed above a proven tempo then struggled: step back to the confirmed
        // tempo but stay automatic so clean playing climbs again. Slower is not
        // easier, so never drop below what was already proven.
        schedule (confirmed, physicalPpq);
        view.limitReached = false;
      } else {
        // Nothing confirmed yet: hold the starting tempo and keep listening.
        // Auto-dropping to ~48 BPM traps players at a harder-to-play tempo
        // with increases locked out.
        struggles = 0;
        view.blocks = 0;
        view.limitReached = false;
      }
    }
  }
  void schedule (double bpm, double ppq) {
    view.nextBpm = bpm;
    // Leave a full repeat for the announcement and compensated late MIDI to arrive.
    const auto untilRepeat = (view.config.length - nextStroke) % view.config.length;
    pendingAt = (lastSequenceTick + 1 + untilRepeat) * interval ();
    const double earliest = ppq + cycle ();
    if (pendingAt < earliest) {
      pendingAt += std::ceil ((earliest - pendingAt) / cycle ()) * cycle ();
    }
  }
};
