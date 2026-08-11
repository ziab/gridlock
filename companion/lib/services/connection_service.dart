import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../models/parameter.dart';
import '../utils/net_utils.dart';

/// Manages the WebSocket connection to the Gridlock JUCE standalone app.
///
/// Maintains a live [parameters] map synchronized with the server,
/// and exposes [setParameter] to push changes back.
class ConnectionService extends ChangeNotifier {
  WebSocketChannel? _channel;
  StreamSubscription? _subscription;

  bool _connected = false;
  String? _serverIp;
  int? _serverPort;

  /// Current connection state.
  bool get isConnected => _connected;
  String get serverAddress =>
      _serverIp != null ? '$_serverIp:$_serverPort' : 'Not connected';

  /// All parameters received from the server.
  final Map<String, RemoteParameter> parameters = {};

  /// Connect to the Gridlock server at the given address.
  Future<bool> connect(String ip, int port) async {
    disconnect();

    try {
      final cleaned = cleanIp(ip);
      final uri = Uri.parse('ws://$cleaned:$port');
      _channel = WebSocketChannel.connect(uri);
      await _channel!.ready;

      _serverIp = cleaned;
      _serverPort = port;
      _connected = true;

      _subscription = _channel!.stream.listen(
        _onMessage,
        onError: (error) {
          debugPrint('WebSocket error: $error');
          _handleDisconnect();
        },
        onDone: () {
          debugPrint('WebSocket closed');
          _handleDisconnect();
        },
      );

      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('WebSocket connect failed: $e');
      _handleDisconnect();
      return false;
    }
  }

  /// Disconnect from the server.
  void disconnect() {
    _subscription?.cancel();
    _subscription = null;
    _channel?.sink.close();
    _channel = null;
    _handleDisconnect();
  }

  /// Update parameter value in local state for smooth visual feedback without sending network frames over WebSocket.
  void updateLocalParameterValue(String id, double value) {
    if (parameters.containsKey(id)) {
      parameters[id]!.value = value;
      notifyListeners();
    }
  }

  /// Send a parameter update command to the server.
  void setParameter(String id, double value) {
    if (!_connected || _channel == null) return;

    final msg = jsonEncode({'type': 'set', 'id': id, 'value': value});
    _channel!.sink.add(msg);

    // Optimistic update
    if (parameters.containsKey(id)) {
      parameters[id]!.value = value;
      notifyListeners();
    }
  }

  /// Toggle a boolean parameter.
  void toggleParameter(String id) {
    final param = parameters[id];
    if (param == null || !param.isBool) return;
    setParameter(id, param.boolValue ? 0.0 : 1.0);
  }

  /// Request clearing the hit grid.
  void clearGrid() {
    if (!_connected || _channel == null) return;
    _channel!.sink.add(jsonEncode({'type': 'clear_grid'}));
  }

  /// Calibration controls (init'able from companion)
  void startCalibration() {
    if (!_connected || _channel == null) return;
    _channel!.sink.add(jsonEncode({'type': 'calibrate'}));
  }

  void applyCalibration({bool addToExisting = false}) {
    if (!_connected || _channel == null) return;
    _channel!.sink.add(jsonEncode({'type': 'calibration_apply', 'add': addToExisting}));
  }

  void cancelCalibration() {
    if (!_connected || _channel == null) return;
    _channel!.sink.add(jsonEncode({'type': 'calibration_cancel'}));
  }

  /// Last calibration state received from server
  String calibrationState = 'idle'; // idle|countin|recording|done
  double calibrationProgress = 0.0;
  int calibrationBeatsRemaining = 0;
  double calibrationMeanMs = 0.0;
  double calibrationMedianMs = 0.0;
  double calibrationSdMs = 0.0;
  int calibrationHitCount = 0;
  int calibrationExpectedHits = 0;
  bool calibrationHasResult = false;
  String calibrationReason = ''; // ""|noHits|tooFew|jitter
  double calibrationBpm = 120.0;
  int calibrationTimeSigNum = 4;
  String calibrationGridInterval = '';
  Timer? _calibrationAutoResetTimer;

  /// Request a full state refresh.
  void requestFullState() {
    if (!_connected || _channel == null) return;
    _channel!.sink.add(jsonEncode({'type': 'get_state'}));
  }

  void _onMessage(dynamic data) {
    try {
      final json = jsonDecode(data as String) as Map<String, dynamic>;
      final type = json['type'] as String?;

      switch (type) {
        case 'state':
          _handleFullState(json);
          break;
        case 'changed':
          _handleParamChanged(json);
          break;
        case 'calibration':
          _handleCalibration(json);
          break;
        case 'ping':
          // Heartbeat — no action needed
          break;
      }
    } catch (e) {
      debugPrint('Failed to parse message: $e');
    }
  }

  void _handleFullState(Map<String, dynamic> json) {
    final params = json['params'] as Map<String, dynamic>?;
    if (params == null) return;

    parameters.clear();
    for (final entry in params.entries) {
      final paramData = entry.value as Map<String, dynamic>;
      parameters[entry.key] = RemoteParameter.fromJson(entry.key, paramData);
    }
    notifyListeners();
  }

  void _handleParamChanged(Map<String, dynamic> json) {
    final id = json['id'] as String?;
    final value = (json['value'] as num?)?.toDouble();
    final norm = (json['norm'] as num?)?.toDouble();

    if (id == null || value == null) return;

    if (parameters.containsKey(id)) {
      parameters[id]!.value = value;
      if (norm != null) parameters[id]!.norm = norm;
      notifyListeners();
    }
  }

  void _handleCalibration(Map<String, dynamic> json) {
    final prev = calibrationState;
    calibrationState = (json['state'] as String?) ?? 'idle';
    calibrationProgress = (json['progress'] as num?)?.toDouble() ?? 0.0;
    calibrationBeatsRemaining = (json['beatsRemaining'] as num?)?.toInt() ?? 0;
    calibrationMeanMs = (json['meanMs'] as num?)?.toDouble() ?? 0.0;
    calibrationMedianMs = (json['medianMs'] as num?)?.toDouble() ?? 0.0;
    calibrationSdMs = (json['sdMs'] as num?)?.toDouble() ?? 0.0;
    calibrationHitCount = (json['hitCount'] as num?)?.toInt() ?? 0;
    calibrationExpectedHits = (json['expectedHits'] as num?)?.toInt() ?? 0;
    calibrationHasResult = (json['hasResult'] as bool?) ?? false;
    calibrationReason = (json['reason'] as String?) ?? '';
    // Fallback inference for old servers without reason
    if (!calibrationHasResult && calibrationReason.isEmpty) {
      if (calibrationHitCount == 0) {
        calibrationReason = 'noHits';
      } else if (calibrationHitCount < calibrationExpectedHits * 0.5) {
        calibrationReason = 'tooFew';
      } else if (calibrationSdMs > 20.0) {
        calibrationReason = 'jitter';
      }
    }
    calibrationBpm = (json['bpm'] as num?)?.toDouble() ?? 120.0;
    calibrationTimeSigNum = (json['timeSigNum'] as num?)?.toInt() ?? 4;
    // Auto-reset DONE -> idle after 3s only when no valid result (no hits / jitter)
    // Valid result (hasResult true) keeps APPLY dialog until user acts — only the "DONE" banner disappears
    _calibrationAutoResetTimer?.cancel();
    if (calibrationState == 'done' && prev != 'done' && !calibrationHasResult) {
      _calibrationAutoResetTimer = Timer(const Duration(seconds: 3), () {
        if (calibrationState == 'done' && !calibrationHasResult) {
          cancelCalibration();
        }
      });
    } else if (calibrationState == 'idle') {
      _calibrationAutoResetTimer?.cancel();
    }
    notifyListeners();
  }

  void _handleDisconnect() {
    _connected = false;
    _serverIp = null;
    _serverPort = null;
    notifyListeners();
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}
