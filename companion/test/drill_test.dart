import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:provider/provider.dart';
import 'package:gridlock_companion/models/drill_state.dart';
import 'package:gridlock_companion/models/parameter.dart';
import 'package:gridlock_companion/screens/drill_screen.dart';
import 'package:gridlock_companion/widgets/status_bar.dart' as app;
import 'package:gridlock_companion/services/connection_service.dart';

class DrillConnection extends ConnectionService {
  final commands = <String>[];
  final sentSettings = <Map<String, Object>>[];
  void publish(DrillState value) {
    drill = value;
    notifyListeners();
  }

  @override
  bool get isConnected => true;
  @override
  void drillCommand(String action, {Map<String, Object> settings = const {}}) {
    commands.add(action);
    sentSettings.add(settings);
  }
}

void main() {
  test(
    'Server snapshots restore an existing drill and commands preserve its session',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final received = <Map<String, dynamic>>[];
      final snapshots = Completer<void>();
      WebSocket? peer;
      server.listen((request) async {
        peer = await WebSocketTransformer.upgrade(request);
        peer!.listen(
          (data) =>
              received.add(jsonDecode(data as String) as Map<String, dynamic>),
        );
        peer!.add(
          jsonEncode({
            'type': 'drill',
            'state': 'playing',
            'pattern': 'RLK',
            'bpm': 84,
            'best': 81,
            'passes': 1,
            'progress': 0.5,
            'automatic': false,
            'toleranceOverride': 32,
            'tolerance': 28,
            'sequenceDetected': true,
            'sequenceSeen': true,
          }),
        );
      });
      final connection = ConnectionService();
      connection.addListener(() {
        if (connection.drill.bpm == 84 && !snapshots.isCompleted) {
          snapshots.complete();
        }
      });
      try {
        expect(await connection.connect('127.0.0.1', server.port), isTrue);
        await snapshots.future.timeout(const Duration(seconds: 5));
        expect(connection.drill.active, isTrue);
        expect(connection.drill.best, 81);
        expect(connection.drill.toleranceOverride, 32);
        expect(connection.drill.tolerance, 28);
        expect(connection.drill.sequenceDetected, isTrue);
        expect(connection.drill.automatic, isFalse);
        connection.setParameter('internal_bpm', 150);
        connection.drillCommand('hold');
        await Future<void>.delayed(const Duration(milliseconds: 100));
        expect(received, [
          {'type': 'drill_command', 'action': 'hold'},
        ]);
      } finally {
        connection.dispose();
        await peer?.close();
        await server.close(force: true);
      }
    },
  );

  testWidgets('Setup validates notation and fits a small phone', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(320, 640);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final connection = DrillConnection();
    await tester.pumpWidget(
      ChangeNotifierProvider<ConnectionService>.value(
        value: connection,
        child: const MaterialApp(home: DrillScreen()),
      ),
    );
    await tester.enterText(find.byType(TextFormField).first, 'RXK');
    await tester.ensureVisible(find.text('Start'));
    await tester.tap(find.text('Start'));
    await tester.pump();
    expect(find.text('Enter 1–64 R, L or K letters'), findsOneWidget);
    expect(connection.commands, isEmpty);
    await tester.enterText(find.byType(TextFormField).first, 'r l k');
    await tester.ensureVisible(find.text('Start'));
    await tester.tap(find.text('Start'));
    await tester.pump();
    expect(connection.commands, ['start']);
    expect(tester.takeException(), isNull);
  });

  testWidgets(
    'Live state shows confirmed tempo and resumes a silent paused drill',
    (tester) async {
      final connection = DrillConnection();
      connection.drill = const DrillState(
        state: 'paused',
        pattern: 'RLRLKK',
        bpm: 84,
        best: 81,
        attempted: 84,
        noHits: true,
      );
      await tester.pumpWidget(
        ChangeNotifierProvider<ConnectionService>.value(
          value: connection,
          child: const MaterialApp(home: DrillScreen()),
        ),
      );
      expect(find.text('Highest confirmed: 81 BPM'), findsOneWidget);
      expect(find.text('No hits detected — paused'), findsOneWidget);
      expect(find.text('Start'), findsNothing);
      await tester.ensureVisible(find.text('Resume'));
      await tester.tap(find.text('Resume'));
      expect(connection.commands, ['resume']);
      expect(tester.takeException(), isNull);
    },
  );
  testWidgets(
    'Waiting drill has a static pattern reference and no moving helper',
    (tester) async {
      final connection = DrillConnection();
      connection.drill = const DrillState(
        state: 'waiting',
        pattern: 'RLK',
        bpm: 60,
      );
      await tester.pumpWidget(
        ChangeNotifierProvider<ConnectionService>.value(
          value: connection,
          child: const MaterialApp(home: DrillScreen()),
        ),
      );
      expect(find.text('Play when ready — any subdivision'), findsOneWidget);
      expect(find.text('Pattern: RLK'), findsOneWidget);
      expect(find.byType(Chip), findsNothing);
      expect(find.text('Resume'), findsNothing);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Live tolerance is initialized from main and sends only drill commands',
    (tester) async {
      final connection = DrillConnection();
      connection.parameters['tolerance_ms'] = RemoteParameter.fromJson(
        'tolerance_ms',
        {'value': 12},
      );
      await tester.pumpWidget(
        ChangeNotifierProvider<ConnectionService>.value(
          value: connection,
          child: const MaterialApp(home: DrillScreen()),
        ),
      );
      await tester.ensureVisible(find.text('Start'));
      await tester.tap(find.text('Start'));
      expect(connection.sentSettings.last['tolerance'], 12);
      connection.publish(
        const DrillState(
          state: 'playing',
          pattern: 'RLK',
          toleranceOverride: 12,
          tolerance: 12,
          sequenceDetected: true,
          sequenceSeen: true,
        ),
      );
      await tester.pump();
      expect(find.text('● Sequence detected'), findsOneWidget);
      await tester.ensureVisible(find.byType(Slider));
      await tester.tap(find.byType(Slider));
      await tester.pump();
      expect(connection.commands.last, 'tolerance');
      expect(connection.sentSettings.last['tolerance'], isNot(12));
      expect(connection.parameters['tolerance_ms']!.value, 12);
      connection.publish(
        const DrillState(
          state: 'playing',
          pattern: 'RLK',
          toleranceOverride: 30,
          tolerance: 25,
          sequenceSeen: true,
        ),
      );
      await tester.pump();
      expect(find.text('○ Sequence lost — listening'), findsOneWidget);
      expect(find.text('Drill tolerance: ±30.0 ms'), findsOneWidget);
      expect(find.text('Effective window: ±25.0 ms'), findsOneWidget);
      connection.publish(const DrillState(state: 'finished', pattern: 'RLK'));
      await tester.pump();
      await tester.ensureVisible(find.text('Start'));
      await tester.tap(find.text('Start'));
      expect(connection.sentSettings.last['tolerance'], 12);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets('Grouping drill opens from the upper bar on a narrow phone', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(320, 640);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final connection = DrillConnection()..drillSupported = true;
    await tester.pumpWidget(
      ChangeNotifierProvider<ConnectionService>.value(
        value: connection,
        child: MaterialApp(
          home: Builder(
            builder: (context) => Scaffold(
              body: Align(
                alignment: Alignment.topCenter,
                child: app.StatusBar(
                  connection: connection,
                  onClearGrid: () {},
                  onOptions: () {},
                  onRefresh: () {},
                  onDrill: () => Navigator.of(context).push(
                    MaterialPageRoute<void>(
                      builder: (_) => const DrillScreen(),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
    final button = find.byTooltip('Grouping Drill');
    expect(button, findsOneWidget);
    expect(
      find.ancestor(of: button, matching: find.byType(app.StatusBar)),
      findsOneWidget,
    );
    expect(tester.getTopLeft(button).dy, lessThan(80));
    await tester.tap(button);
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 400));
    expect(find.byType(DrillScreen), findsOneWidget);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
}
