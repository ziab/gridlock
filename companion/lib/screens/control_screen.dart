import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../services/connection_service.dart';
import '../services/discovery_service.dart';
import '../widgets/bpm_dial.dart';
import '../widgets/signature_picker.dart';
import '../widgets/subdivision_picker.dart';
import '../widgets/parameter_card.dart';

/// Main control screen — the single-screen UI of the companion app.
///
/// Top section: BPM dial, time signature, click subdivision (primary)
/// Bottom section: scrollable grid of secondary parameter cards
class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen>
    with SingleTickerProviderStateMixin {
  bool _discovering = false;
  String? _errorMessage;
  late AnimationController _pulseController;

  @override
  void initState() {
    super.initState();
    _pulseController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1200),
    )..repeat(reverse: true);
    _startDiscovery();
  }

  @override
  void dispose() {
    _pulseController.dispose();
    super.dispose();
  }

  Future<void> _startDiscovery() async {
    final connection = context.read<ConnectionService>();
    if (connection.isConnected) return;

    setState(() {
      _discovering = true;
      _errorMessage = null;
    });

    final result = await DiscoveryService.discover(
      timeout: const Duration(seconds: 10),
    );

    if (!mounted) return;

    if (result != null) {
      final success = await connection.connect(result.ip, result.port);
      if (mounted) {
        setState(() {
          _discovering = false;
          if (!success) {
            _errorMessage = 'Found Gridlock at ${result.ip} but connection failed';
          }
        });
      }
    } else {
      if (mounted) {
        setState(() {
          _discovering = false;
          _errorMessage = 'No Gridlock instance found on this network';
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ConnectionService>(
      builder: (context, connection, _) {
        if (!connection.isConnected) {
          return _buildDiscoveryView(connection);
        }
        return _buildControlView(connection);
      },
    );
  }

  // ── Discovery / Connection View ─────────────────────────────────
  Widget _buildDiscoveryView(ConnectionService connection) {
    return Scaffold(
      backgroundColor: const Color(0xFF0a0c10),
      body: SafeArea(
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Animated logo/icon
              AnimatedBuilder(
                animation: _pulseController,
                builder: (context, child) {
                  final scale = 1.0 + _pulseController.value * 0.08;
                  return Transform.scale(
                    scale: scale,
                    child: Container(
                      width: 80,
                      height: 80,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        gradient: const RadialGradient(
                          colors: [Color(0xFF1e2235), Color(0xFF0f1118)],
                        ),
                        border: Border.all(
                          color: const Color(0xFF00FF88).withValues(alpha: 0.4),
                          width: 2,
                        ),
                        boxShadow: [
                          BoxShadow(
                            color: const Color(0xFF00FF88).withValues(alpha: 0.2),
                            blurRadius: 20,
                          ),
                        ],
                      ),
                      child: const Icon(
                        Icons.grid_on_rounded,
                        color: Color(0xFF00FF88),
                        size: 36,
                      ),
                    ),
                  );
                },
              ),
              const SizedBox(height: 32),
              const Text(
                'GRIDLOCK',
                style: TextStyle(
                  color: Colors.white,
                  fontSize: 28,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 6,
                ),
              ),
              const SizedBox(height: 4),
              const Text(
                'COMPANION',
                style: TextStyle(
                  color: Color(0xFF8b92a8),
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                  letterSpacing: 4,
                ),
              ),
              const SizedBox(height: 40),
              if (_discovering) ...[
                const SizedBox(
                  width: 24,
                  height: 24,
                  child: CircularProgressIndicator(
                    strokeWidth: 2,
                    valueColor:
                        AlwaysStoppedAnimation<Color>(Color(0xFF00FF88)),
                  ),
                ),
                const SizedBox(height: 16),
                const Text(
                  'Searching for Gridlock on your network…',
                  style: TextStyle(
                    color: Color(0xFF6b7280),
                    fontSize: 14,
                  ),
                ),
              ],
              if (_errorMessage != null) ...[
                Container(
                  margin: const EdgeInsets.symmetric(horizontal: 32),
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    color: const Color(0xFFFF1744).withValues(alpha: 0.1),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(
                      color: const Color(0xFFFF1744).withValues(alpha: 0.3),
                    ),
                  ),
                  child: Text(
                    _errorMessage!,
                    textAlign: TextAlign.center,
                    style: const TextStyle(
                      color: Color(0xFFFF1744),
                      fontSize: 13,
                    ),
                  ),
                ),
                const SizedBox(height: 20),
                _buildRetryButton(),
              ],
              if (!_discovering && _errorMessage == null) _buildRetryButton(),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildRetryButton() {
    return GestureDetector(
      onTap: () {
        HapticFeedback.mediumImpact();
        _startDiscovery();
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 28, vertical: 14),
        decoration: BoxDecoration(
          color: const Color(0xFF00FF88).withValues(alpha: 0.12),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
            color: const Color(0xFF00FF88).withValues(alpha: 0.4),
            width: 1.5,
          ),
        ),
        child: const Text(
          'SCAN AGAIN',
          style: TextStyle(
            color: Color(0xFF00FF88),
            fontSize: 14,
            fontWeight: FontWeight.w700,
            letterSpacing: 2,
          ),
        ),
      ),
    );
  }

  // ── Control View ────────────────────────────────────────────────
  Widget _buildControlView(ConnectionService connection) {
    final params = connection.parameters;

    // Extract primary parameter values with safe defaults
    final bpm = params['internal_bpm'];
    final timeSig = params['time_sig_num'];
    final clickSub = params['click_subdivision'];

    return Scaffold(
      backgroundColor: const Color(0xFF0a0c10),
      body: SafeArea(
        child: Column(
          children: [
            // ── Status Bar ──────────────────────────────────────
            _buildStatusBar(connection),

            // ── Primary Controls ────────────────────────────────
            Expanded(
              flex: 5,
              child: _buildPrimaryControls(connection, bpm, timeSig, clickSub),
            ),

            // ── Divider ─────────────────────────────────────────
            Container(
              height: 1,
              margin: const EdgeInsets.symmetric(horizontal: 20),
              color: const Color(0xFF2d3245),
            ),

            // ── Secondary Controls ──────────────────────────────
            Expanded(
              flex: 4,
              child: _buildSecondaryControls(connection, params),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatusBar(ConnectionService connection) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      color: const Color(0xFF181b24),
      child: Row(
        children: [
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: connection.isConnected
                  ? const Color(0xFF00c853)
                  : const Color(0xFFFF1744),
              boxShadow: [
                BoxShadow(
                  color: (connection.isConnected
                          ? const Color(0xFF00c853)
                          : const Color(0xFFFF1744))
                      .withValues(alpha: 0.5),
                  blurRadius: 6,
                ),
              ],
            ),
          ),
          const SizedBox(width: 10),
          Text(
            connection.isConnected
                ? 'Connected to ${connection.serverAddress}'
                : 'Disconnected',
            style: const TextStyle(
              color: Color(0xFF8b92a8),
              fontSize: 11,
              fontWeight: FontWeight.w500,
            ),
          ),
          const Spacer(),
          const Text(
            'GRIDLOCK',
            style: TextStyle(
              color: Color(0xFF4b5267),
              fontSize: 10,
              fontWeight: FontWeight.w700,
              letterSpacing: 2,
            ),
          ),
          const SizedBox(width: 8),
          GestureDetector(
            onTap: () {
              connection.disconnect();
              _startDiscovery();
            },
            child: const Icon(
              Icons.refresh_rounded,
              color: Color(0xFF4b5267),
              size: 18,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildPrimaryControls(
    ConnectionService connection,
    dynamic bpm,
    dynamic timeSig,
    dynamic clickSub,
  ) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
        children: [
          // BPM Dial
          BpmDial(
            value: bpm?.value ?? 120.0,
            min: bpm?.min ?? 40.0,
            max: bpm?.max ?? 300.0,
            step: bpm?.step ?? 0.1,
            onChanged: (v) => connection.setParameter('internal_bpm', v),
          ),

          // Time Signature
          SignaturePicker(
            currentNumerator: (timeSig?.value ?? 4.0).round(),
            onChanged: (num) =>
                connection.setParameter('time_sig_num', num.toDouble()),
          ),

          // Click Subdivision
          SubdivisionPicker(
            currentIndex: (clickSub?.value ?? 1.0).round(),
            onChanged: (i) =>
                connection.setParameter('click_subdivision', i.toDouble()),
          ),
        ],
      ),
    );
  }

  Widget _buildSecondaryControls(
    ConnectionService connection,
    Map<String, dynamic> params,
  ) {
    // Define which params to show and their display config
    // Excluded: show_velocity_labels, show_note_numbers, test_mode (UI-only)
    final secondaryParams = <_SecondaryDef>[
      _SecondaryDef('click_enabled', 'Metronome'),
      _SecondaryDef('is_paused', 'Pause'),
      _SecondaryDef('click_volume', 'Click Volume'),
      _SecondaryDef('click_pan', 'Click Pan'),
      _SecondaryDef('click_sample_preset', 'Click Sound'),
      _SecondaryDef('bars_window', 'History Bars'),
      _SecondaryDef('subdivision', 'Grid Subdiv'),
      _SecondaryDef('tolerance_ms', 'Tolerance', suffix: ' ms'),
      _SecondaryDef('latency_offset_ms', 'Latency', suffix: ' ms'),
      _SecondaryDef('min_velocity', 'Min Velocity'),
      _SecondaryDef('show_ms_labels', 'MS Offsets'),
      _SecondaryDef('note_filter', 'Display Mode'),
    ];

    final available = secondaryParams
        .where((def) => params.containsKey(def.id))
        .toList();

    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 8),
      child: GridView.builder(
        gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
          crossAxisCount: 2,
          mainAxisSpacing: 8,
          crossAxisSpacing: 8,
          childAspectRatio: 2.0,
        ),
        itemCount: available.length,
        itemBuilder: (context, index) {
          final def = available[index];
          final param = params[def.id];
          if (param == null) return const SizedBox();

          return ParameterCard(
            label: def.displayName,
            paramType: param.paramType,
            value: param.value,
            min: param.min,
            max: param.max,
            step: param.step > 0 ? param.step : 0.01,
            options: param.options,
            suffix: def.suffix,
            onChanged: (v) => connection.setParameter(def.id, v),
          );
        },
      ),
    );
  }
}

class _SecondaryDef {
  final String id;
  final String displayName;
  final String? suffix;
  const _SecondaryDef(this.id, this.displayName, {this.suffix});
}
