import 'package:flutter_test/flutter_test.dart';
import 'package:gridlock_companion/services/practice_timer_service.dart';

void main() {
  group('PracticeTimerService', () {
    late PracticeTimerService timerService;

    setUp(() {
      timerService = PracticeTimerService();
    });

    tearDown(() {
      timerService.dispose();
    });

    test('initial state is non-practicing and default 5-min timer', () {
      expect(timerService.isPracticing, isFalse);
      expect(timerService.totalDurationSeconds, equals(300));
      expect(timerService.remainingSeconds, equals(300));
      expect(timerService.formattedRemainingTime, equals('05:00'));
      expect(timerService.progress, equals(0.0));
      expect(timerService.hasEndBpm, isFalse);
    });

    test('configure updates duration and BPM configuration', () {
      timerService.configure(
        durationSeconds: 600, // 10 mins
        hasEndBpm: true,
        startBpm: 100.0,
        endBpm: 160.0,
      );

      expect(timerService.totalDurationSeconds, equals(600));
      expect(timerService.remainingSeconds, equals(600));
      expect(timerService.formattedRemainingTime, equals('10:00'));
      expect(timerService.hasEndBpm, isTrue);
      expect(timerService.startBpm, equals(100.0));
      expect(timerService.endBpm, equals(160.0));
      expect(timerService.currentBpm, equals(100.0));
    });

    test('formattedRemainingTime formats minutes and seconds correctly', () {
      timerService.configure(
        durationSeconds: 125, // 2m 05s
        hasEndBpm: false,
        startBpm: 120.0,
      );

      expect(timerService.formattedRemainingTime, equals('02:05'));
    });

    test('startPractice sets isPracticing to true and stopPractice stops it', () {
      timerService.configure(
        durationSeconds: 60,
        hasEndBpm: false,
        startBpm: 120.0,
      );

      timerService.startPractice();
      expect(timerService.isPracticing, isTrue);

      timerService.stopPractice();
      expect(timerService.isPracticing, isFalse);
    });
  });
}
