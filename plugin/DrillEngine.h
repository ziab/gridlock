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
  enum class Action { Start, Hold, TooFast, Pause, Resume, Finish, Retry, Disconnect };
  struct Config {
    std::array<char, constants::drill::maxPattern + 1> pattern{};
    int length{0}, kick{DrumMap::Kick}, beats{4}, minVelocity{5};
    double interval{constants::musical::ppq_1_16}, bpm{constants::drill::startBpm};
    float tolerance{constants::params::toleranceDefault};
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
    bool automatic{true}, noHits{false}, limitReached{false};
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
    if (action == Action::Finish) {
      view.state = State::Finished;
      view.nextBpm = 0;
      pendingAt = infinity;
    }
    if (action == Action::Pause) {
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
  int blockSize{32}, good{0}, missing{0}, wrong{0}, late{0}, extras{0}, struggles{0};
  double interval () const {
    return view.config.interval;
  }
  double cycle () const {
    return interval () * view.config.length;
  }
  double fallbackBpm () const {
    return view.best > 0
               ? view.best
               : std::max<double> (constants::params::bpmMin, std::floor (view.bpm * constants::drill::resumeRatio));
  }
  void waitForFirstHit (double ppq) {
    pendingAt = infinity;
    view.nextBpm = 0;
    view.noHits = false;
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
    const bool pass = view.accuracy >= constants::drill::passAccuracy && extraRate <= constants::drill::passExtras;
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
      view.automatic = false;
      view.limitReached = true;
      schedule (fallbackBpm (), physicalPpq);
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
