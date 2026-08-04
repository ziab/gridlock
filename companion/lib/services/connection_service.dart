import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../models/parameter.dart';

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
      final uri = Uri.parse('ws://$ip:$port');
      _channel = WebSocketChannel.connect(uri);
      await _channel!.ready;

      _serverIp = ip;
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

  /// Set a parameter value (denormalized).
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
