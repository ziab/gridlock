import 'dart:async';
import 'package:flutter/foundation.dart';

class PracticeTimerService extends ChangeNotifier {
  Timer? _timer;
  bool _isPracticing = false;

  int _totalDurationSeconds = 300; // default 5 minutes
  int _remainingSeconds = 300;

  bool _hasEndBpm = false;
  double _startBpm = 120.0;
  double _endBpm = 160.0;
  double _currentBpm = 120.0;

  void Function(double bpm)? _onBpmChanged;

  bool get isPracticing => _isPracticing;
  int get totalDurationSeconds => _totalDurationSeconds;
  int get remainingSeconds => _remainingSeconds;
  bool get hasEndBpm => _hasEndBpm;
  double get startBpm => _startBpm;
  double get endBpm => _endBpm;
  double get currentBpm => _currentBpm;

  String get formattedRemainingTime {
    final minutes = (_remainingSeconds / 60).floor();
    final seconds = _remainingSeconds % 60;
    return '${minutes.toString().padLeft(2, '0')}:${seconds.toString().padLeft(2, '0')}';
  }

  double get progress {
    if (_totalDurationSeconds == 0) return 0.0;
    return (_totalDurationSeconds - _remainingSeconds) / _totalDurationSeconds;
  }

  void configure({
    required int durationSeconds,
    required bool hasEndBpm,
    required double startBpm,
    double? endBpm,
    void Function(double bpm)? onBpmChanged,
  }) {
    if (_isPracticing) return;
    _totalDurationSeconds = durationSeconds;
    _remainingSeconds = durationSeconds;
    _hasEndBpm = hasEndBpm;
    _startBpm = startBpm;
    _endBpm = endBpm ?? startBpm;
    _currentBpm = startBpm;
    _onBpmChanged = onBpmChanged;
    notifyListeners();
  }

  void startPractice() {
    if (_isPracticing) return;
    _isPracticing = true;
    _timer?.cancel();

    _timer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (_remainingSeconds > 0) {
        _remainingSeconds--;

        if (_hasEndBpm && _totalDurationSeconds > 0) {
          final ratio = (_totalDurationSeconds - _remainingSeconds) / _totalDurationSeconds;
          _currentBpm = (_startBpm + (_endBpm - _startBpm) * ratio).roundToDouble();
          _onBpmChanged?.call(_currentBpm);
        }

        notifyListeners();
      } else {
        stopPractice();
      }
    });

    notifyListeners();
  }

  void stopPractice() {
    _isPracticing = false;
    _timer?.cancel();
    _timer = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }
}
