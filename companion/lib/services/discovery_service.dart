import 'dart:async';
import 'dart:io';

import '../constants/app_constants.dart';

/// Discovers a Gridlock standalone app on the local network via UDP broadcast.
class DiscoveryService {
  static const _udpPort = AppConstants.udpPort;
  static const _probe = AppConstants.probeMessage;
  static const _replyPrefix = AppConstants.replyPrefix;

  /// Scan the network for a Gridlock instance.
  /// Returns (ip, wsPort) or null if nothing found within [timeout].
  static Future<({String ip, int port})?> discover({
    Duration timeout = const Duration(seconds: AppConstants.discoveryTimeoutSec),
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
              final wsPort = int.tryParse(reply.substring(_replyPrefix.length));
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

      final probeBytes = _probe.codeUnits;
      final broadcastAddr = InternetAddress('255.255.255.255');

      Timer? probeTimer;
      probeTimer = Timer.periodic(const Duration(milliseconds: AppConstants.probeIntervalMs), (timer) {
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
      probeTimer.cancel();
      return result;
    } catch (e) {
      return null;
    } finally {
      socket?.close();
    }
  }
}
