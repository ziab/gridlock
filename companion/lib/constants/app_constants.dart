/// App-wide tunable constants — mirrors `plugin/Constants.h` network/musical groups.
/// Keep Dart and C++ ports in sync (see `constants::network::wsPort` etc.).
abstract final class AppConstants {
  // ── Network ──
  static const udpPort = 9877;
  static const wsPort = 9876;
  static const wsDefaultHost = '127.0.0.1';
  static const probeMessage = 'GRIDLOCK_DISCOVER';
  static const replyPrefix = 'GRIDLOCK_HERE:';
  static const probeIntervalMs = 500;
  static const discoveryTimeoutSec = 8; // DiscoveryService default; UI uses 10s for user-visible scan
  static const discoveryUiTimeoutSec = 10;
  static const wsMaxPayload = 65536;

  // ── BPM ──
  static const bpmMin = 40.0;
  static const bpmMax = 300.0;
  static const bpmDefault = 120.0;
  static const bpmRulerMin = 30.0;
  static const bpmRulerMax = 300.0;
  static const pixelsPerBpm = 12.0;

  // ── Practice timer ──
  // Adaptive drill defaults; mirrors constants::drill and DrumMap::Kick.
  static const drillStartBpm = 60.0;
  static const drillMinBpm = 60.0;
  static const drillEighthStartBpm = 90;
  static const drillKickNote = 36;
  static const double drillToleranceMs = 20;
  static const drillResumeRatio = 0.8;
  static const drillPassDefault = 0.95;
  static const drillPassMin = 0.70;
  static const drillPassMax = 1.0;
  static const drillStepDefault = 3.0;
  static const drillStepMin = 1.0;
  static const drillStepMax = 10.0;

  static const practiceDefaultSec = 300; // 5 min
  static const practiceStartBpmDefault = 120.0;
  static const practiceEndBpmDefault = 160.0;

  // ── Tolerance ──
  static const toleranceMin = 5.0;
  static const toleranceMax = 40.0;

  // ── UI ──
  static const historyOptions = ['1 Bar', '2 Bars', '4 Bars', '8 Bars'];
  static const subdivisionOptions = ['Off', '1/4', '1/8', '1/16', 'Trip', 'Sixt'];
  static const timeSignatures = [
    (num: 2, label: '2/4'),
    (num: 3, label: '3/4'),
    (num: 4, label: '4/4'),
    (num: 5, label: '5/4'),
    (num: 6, label: '6/8'),
    (num: 7, label: '7/8'),
  ];
}
