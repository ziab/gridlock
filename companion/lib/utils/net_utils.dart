import '../constants/app_constants.dart';

/// Parses `host:port` or full URI into (ip, port).
/// Mirrors `ConnectionService._cleanIp` + manual split in `ControlScreen`.
({String ip, int port}) parseHostPort(String input, {int fallbackPort = AppConstants.wsPort}) {
  final cleaned = input
      .replaceAll(RegExp(r'^https?://|^ws://'), '')
      .split('/')[0]
      .split(':')[0];
  final parts = input.split(':');
  // input may be "192.168.1.10:9876" or just "192.168.1.10"
  final ip = input.contains('://')
      ? cleaned
      : parts[0].replaceAll(RegExp(r'^https?://|^ws://'), '').split('/')[0];
  final port = parts.length > 1 ? int.tryParse(parts.last.split('/')[0]) ?? fallbackPort : fallbackPort;
  // For plain "127.0.0.1:9876", cleaned already correct; handle fallback above
  final finalIp = ip.isEmpty ? input.split(':')[0] : ip;
  return (ip: finalIp, port: port);
}

String cleanIp(String ip) {
  return ip.replaceAll(RegExp(r'^https?://|^ws://'), '').split('/')[0].split(':')[0];
}
