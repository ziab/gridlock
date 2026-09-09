import '../constants/app_constants.dart';

class DrillState {
  final String state, pattern;
  final double bpm, best, attempted, nextBpm, progress, accuracy;
  final int passes, activeSlot, beatsRemaining, missing, wrong, late, extras;
  final bool automatic, noHits, limitReached;

  const DrillState({
    this.state = 'idle',
    this.pattern = '',
    this.bpm = AppConstants.drillStartBpm,
    this.best = 0,
    this.attempted = 0,
    this.nextBpm = 0,
    this.progress = 0,
    this.accuracy = 0,
    this.passes = 0,
    this.activeSlot = 0,
    this.beatsRemaining = 0,
    this.missing = 0,
    this.wrong = 0,
    this.late = 0,
    this.extras = 0,
    this.automatic = true,
    this.noHits = false,
    this.limitReached = false,
  });

  bool get active => state != 'idle' && state != 'finished';

  factory DrillState.fromJson(Map<String, dynamic> json) {
    double number(String key) => (json[key] as num?)?.toDouble() ?? 0;
    return DrillState(
      state: json['state'] as String? ?? 'idle',
      pattern: json['pattern'] as String? ?? '',
      bpm: number('bpm'),
      best: number('best'),
      attempted: number('attempted'),
      nextBpm: number('nextBpm'),
      progress: number('progress').clamp(0, 1),
      accuracy: number('accuracy'),
      passes: number('passes').toInt(),
      activeSlot: number('activeSlot').toInt(),
      beatsRemaining: number('beatsRemaining').toInt(),
      missing: number('missing').toInt(),
      wrong: number('wrong').toInt(),
      late: number('late').toInt(),
      extras: number('extras').toInt(),
      automatic: json['automatic'] as bool? ?? false,
      noHits: json['noHits'] as bool? ?? false,
      limitReached: json['limitReached'] as bool? ?? false,
    );
  }
}
