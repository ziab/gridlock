// ignore_for_file: avoid_print, unused_local_variable
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:web_socket_channel/web_socket_channel.dart';

// End-to-end calibration test: spawns no UI, talks directly to the
// Gridlock standalone via UDP discovery + WebSocket.
// Verifies the bug: companion shows 0 hits / no progress during recording.
Future<void> main() async {
  print('=== Gridlock E2E calibration test (headless) ===');
  final server = await _discover();
  if (server == null) {
    print('FAIL: could not discover Gridlock (is the standalone running?)');
    exit(1);
  }
  print('Discovered ${server.ip}:${server.port}');

  final channel = WebSocketChannel.connect(Uri.parse('ws://${server.ip}:${server.port}'));
  await channel.ready;
  print('WS connected');

  final completer = Completer<void>();
  int seenCountIn = 0;
  int seenRecording = 0;
  int maxHitCount = 0;
  int expectedHits = 0;
  bool sawProgress = false;
  String lastState = 'idle';

  late StreamSubscription sub;
  sub = channel.stream.listen((data) {
    try {
      final json = jsonDecode(data as String) as Map<String, dynamic>;
      final type = json['type'] as String?;
      if (type == 'state') {
        print('-> state snapshot received');
      } else if (type == 'calibration') {
        final state = json['state'] as String? ?? 'idle';
        final progress = (json['progress'] as num?)?.toDouble() ?? 0.0;
        final hitCount = (json['hitCount'] as num?)?.toInt() ?? 0;
        final exp = (json['expectedHits'] as num?)?.toInt() ?? 0;
        lastState = state;
        expectedHits = exp;
        if (hitCount > maxHitCount) maxHitCount = hitCount;
        if (state == 'countin') seenCountIn++;
        if (state == 'recording') {
          seenRecording++;
          if (hitCount > 0) sawProgress = true;
          print('  calibration $state progress=${(progress * 100).toStringAsFixed(0)}% hits $hitCount/$exp');
        }
        if (state == 'done') {
          print('  calibration done hits $hitCount/$exp mean ${json['meanMs']} sd ${json['sdMs']} hasResult ${json['hasResult']}');
          if (hitCount == 0 && exp > 0) {
            print('FAIL: done with 0 hits (bug: early hits dropped or not broadcast)');
          } else if (!sawProgress && exp > 0) {
            print('FAIL: never saw progress hitCount>0 during recording (live broadcast bug)');
          } else {
            print('PASS: calibration live progress and hitCount OK');
          }
          if (!completer.isCompleted) completer.complete();
        }
      }
    } catch (e) {
      print('parse error $e');
    }
  }, onError: (e) {
    print('WS error $e');
    if (!completer.isCompleted) completer.completeError(e);
  }, onDone: () {
    print('WS done');
    if (!completer.isCompleted) completer.complete();
  });

  // Configure: 120 BPM 4/4 1/8, enable test_mode so hits are generated
  void send(Map<String, dynamic> m) => channel.sink.add(jsonEncode(m));
  await Future.delayed(Duration(milliseconds: 300));
  send({'type': 'set', 'id': 'internal_bpm', 'value': 120.0});
  await Future.delayed(Duration(milliseconds: 100));
  send({'type': 'set', 'id': 'time_sig_num', 'value': 4.0});
  await Future.delayed(Duration(milliseconds: 100));
  send({'type': 'set', 'id': 'subdivision', 'value': 0.0}); // 1/8
  await Future.delayed(Duration(milliseconds: 100));
  send({'type': 'set', 'id': 'click_enabled', 'value': 0.0}); // disable click so effective == display (32 hits) — otherwise default 1/4 vs 1/8 gives 250ms jitter -> hasResult false
  await Future.delayed(Duration(milliseconds: 100));
  send({'type': 'set', 'id': 'click_subdivision', 'value': 0.0}); // Off
  await Future.delayed(Duration(milliseconds: 100));
  send({'type': 'set', 'id': 'test_mode', 'value': 1.0});
  await Future.delayed(Duration(milliseconds: 200));
  // Ensure clean start
  send({'type': 'calibration_cancel'});
  await Future.delayed(Duration(milliseconds: 200));

  print('Starting calibration (should be grid-synced to next bar)...');
  send({'type': 'calibrate'});

  // Wait up to 12s for done (count-in 1 bar + 4 bars rec at 120bpm = 10s + margin)
  try {
    await completer.future.timeout(Duration(seconds: 12));
  } catch (e) {
    print('TIMEOUT waiting for calibration done (lastState=$lastState seenRecording=$seenRecording maxHit=$maxHitCount/$expectedHits)');
    await sub.cancel();
    channel.sink.close();
    exit(1);
  }

  await Future.delayed(Duration(milliseconds: 500));
  await sub.cancel();
  channel.sink.close();

  if (maxHitCount == 0) {
    print('FAIL: companion never saw hitCount>0 during recording (live broadcast bug)');
    exit(1);
  }
  if (seenRecording == 0) {
    print('FAIL: never saw recording state');
    exit(1);
  }
  print('E2E PASS');
}

class _Server {
  final String ip;
  final int port;
  _Server(this.ip, this.port);
}

Future<_Server?> _discover({Duration timeout = const Duration(seconds: 3)}) async {
  final socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
  socket.broadcastEnabled = true;
  final completer = Completer<_Server?>();
  late StreamSubscription sub;
  sub = socket.listen((event) {
    if (event == RawSocketEvent.read) {
      final dg = socket.receive();
      if (dg == null) return;
      final msg = String.fromCharCodes(dg.data).trim();
      if (msg.startsWith('GRIDLOCK_HERE:')) {
        final parts = msg.split(':');
        final port = parts.length > 1 ? int.tryParse(parts[1]) ?? 9876 : 9876;
        final ip = dg.address.address;
        if (!completer.isCompleted) completer.complete(_Server(ip, port));
      }
    }
  });
  // Send probe every 300ms
  Timer? timer;
  timer = Timer.periodic(Duration(milliseconds: 300), (_) {
    socket.send(utf8.encode('GRIDLOCK_DISCOVER'), InternetAddress('255.255.255.255'), 9877);
  });
  // Also try localhost
  socket.send(utf8.encode('GRIDLOCK_DISCOVER'), InternetAddress.loopbackIPv4, 9877);
  final result = await completer.future.timeout(timeout, onTimeout: () => null);
  await sub.cancel();
  timer.cancel();
  socket.close();
  return result;
}
