import 'dart:async';
import 'dart:io';

/// Discovers a Gridlock standalone app on the local network via UDP broadcast.
///
/// Sends "GRIDLOCK_DISCOVER" on port 9877 and listens for
/// "GRIDLOCK_HERE:<wsPort>" replies. Returns the first respondent's IP + port.
class DiscoveryService {
  static const int _udpPort = 9877;
  static const String _probe = 'GRIDLOCK_DISCOVER';
  static const String _replyPrefix = 'GRIDLOCK_HERE:';

  /// Scan the network for a Gridlock instance.
  /// Returns (ip, wsPort) or null if nothing found within [timeout].
  static Future<({String ip, int port})?> discover({
    Duration timeout = const Duration(seconds: 8),
  }) async {
    RawDatagramSocket? socket;
    try {
      socket = await RawDatagramSocket.bind(
        InternetAddress.anyIPv4,
        0, // OS picks a free port
      );
      socket.broadcastEnabled = true;

      final completer = Completer<({String ip, int port})?>();

      // Listen for responses
      final sub = socket.listen((event) {
        if (event == RawSocketEvent.read) {
          final datagram = socket!.receive();
          if (datagram != null) {
            final reply = String.fromCharCodes(datagram.data).trim();
            if (reply.startsWith(_replyPrefix)) {
              final wsPort = int.tryParse(
                reply.substring(_replyPrefix.length),
              );
              if (wsPort != null && !completer.isCompleted) {
                completer.complete((
                  ip: datagram.address.address,
                  port: wsPort,
                ));
              }
            }
          }
        }
      });

      // Send discovery probes every 500ms
      final probeBytes = _probe.codeUnits;
      final broadcastAddr = InternetAddress('255.255.255.255');

      Timer.periodic(const Duration(milliseconds: 500), (timer) {
        if (completer.isCompleted) {
          timer.cancel();
          return;
        }
        socket?.send(probeBytes, broadcastAddr, _udpPort);
      });

      // Send first probe immediately
      socket.send(probeBytes, broadcastAddr, _udpPort);

      // Race between discovery and timeout
      final result = await completer.future.timeout(
        timeout,
        onTimeout: () => null,
      );

      sub.cancel();
      return result;
    } catch (e) {
      return null;
    } finally {
      socket?.close();
    }
  }
}
