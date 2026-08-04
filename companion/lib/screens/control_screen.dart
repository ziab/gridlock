import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../services/connection_service.dart';
import '../services/discovery_service.dart';
import '../services/practice_timer_service.dart';
import '../widgets/bpm_ruler_selector.dart';
import '../widgets/practice_timer_ruler_display.dart';
import '../widgets/practice_setup_modal.dart';
import '../widgets/signature_picker.dart';
import '../widgets/subdivision_picker.dart';
import '../widgets/parameter_card.dart';

/// Main control screen — focused single-screen UI for the drum throne.
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
  late PracticeTimerService _practiceTimerService;

  @override
  void initState() {
    super.initState();
    _practiceTimerService = PracticeTimerService();
    _pulseController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1200),
    )..repeat(reverse: true);
    _startDiscovery();
  }

  @override
  void dispose() {
    _pulseController.dispose();
    _practiceTimerService.dispose();
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
            _errorMessage =
                'Found Gridlock at ${result.ip} but connection failed';
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
    return ChangeNotifierProvider<PracticeTimerService>.value(
      value: _practiceTimerService,
      child: Consumer2<ConnectionService, PracticeTimerService>(
        builder: (context, connection, timerService, _) {
          if (!connection.isConnected) {
            return _buildDiscoveryView(connection);
          }

          if (timerService.isPracticing) {
            return _buildPracticeModeView(connection, timerService);
          }

          return _buildControlView(connection, timerService);
        },
      ),
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
                            color: const Color(
                              0xFF00FF88,
                            ).withValues(alpha: 0.2),
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
                    valueColor: AlwaysStoppedAnimation<Color>(
                      Color(0xFF00FF88),
                    ),
                  ),
                ),
                const SizedBox(height: 16),
                const Text(
                  'Searching for Gridlock on your network…',
                  style: TextStyle(color: Color(0xFF6b7280), fontSize: 14),
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
              ],
              if (!_discovering) ...[
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    _buildRetryButton(),
                    const SizedBox(width: 12),
                    _buildManualConnectButton(),
                  ],
                ),
              ],
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
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
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
            fontSize: 13,
            fontWeight: FontWeight.w700,
            letterSpacing: 2,
          ),
        ),
      ),
    );
  }

  Widget _buildManualConnectButton() {
    return GestureDetector(
      onTap: _showManualConnectDialog,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
        decoration: BoxDecoration(
          color: const Color(0xFF1e2235),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
            color: const Color(0xFF38bdf8).withValues(alpha: 0.4),
            width: 1.5,
          ),
        ),
        child: const Text(
          'MANUAL IP',
          style: TextStyle(
            color: Color(0xFF38bdf8),
            fontSize: 13,
            fontWeight: FontWeight.w700,
            letterSpacing: 1.5,
          ),
        ),
      ),
    );
  }

  Future<void> _showManualConnectDialog() async {
    final controller = TextEditingController(text: '127.0.0.1:9876');
    final result = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF1a1d2e),
        title: const Text(
          'Manual Connection',
          style: TextStyle(color: Colors.white),
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Enter Gridlock PC IP & Port:',
              style: TextStyle(color: Color(0xFF8b92a8), fontSize: 13),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              autofocus: true,
              style: const TextStyle(color: Colors.white, fontSize: 16),
              decoration: const InputDecoration(
                hintText: 'e.g. 192.168.1.100:9876',
                hintStyle: TextStyle(color: Colors.white24),
                enabledBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: Colors.white24),
                ),
                focusedBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: Color(0xFF38bdf8)),
                ),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text(
              'Cancel',
              style: TextStyle(color: Colors.white54),
            ),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, controller.text.trim()),
            child: const Text(
              'Connect',
              style: TextStyle(color: Color(0xFF38bdf8)),
            ),
          ),
        ],
      ),
    );

    if (result != null && result.isNotEmpty && mounted) {
      final parts = result.split(':');
      final ip = parts[0];
      final port = parts.length > 1 ? (int.tryParse(parts[1]) ?? 9876) : 9876;

      setState(() {
        _discovering = true;
        _errorMessage = null;
      });

      final connection = context.read<ConnectionService>();
      final success = await connection.connect(ip, port);
      if (mounted) {
        setState(() {
          _discovering = false;
          if (!success) {
            _errorMessage = 'Failed to connect to $ip:$port';
          }
        });
      }
    }
  }

  // ── Main Control View ───────────────────────────────────────────
  Widget _buildControlView(
    ConnectionService connection,
    PracticeTimerService timerService,
  ) {
    final params = connection.parameters;

    final bpm = params['internal_bpm'];
    final timeSig = params['time_sig_num'];
    final clickSub = params['click_subdivision'];
    final currentBpm = (bpm?.value ?? 120.0).roundToDouble();

    return Scaffold(
      backgroundColor: const Color(0xFF0a0c10),
      body: SafeArea(
        child: Column(
          children: [
            // Status bar
            _buildStatusBar(connection),

            // Main Drum Throne Controls
            Expanded(
              child: _buildPrimaryControls(
                connection,
                timerService,
                bpm,
                currentBpm,
                timeSig,
                clickSub,
              ),
            ),

            // Quick Actions & Options Footer
            _buildFooterActions(connection, timerService, currentBpm, params),
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
                  color:
                      (connection.isConnected
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
          IconButton(
            tooltip: 'Clear Grid',
            icon: const Icon(
              Icons.cleaning_services_rounded,
              color: Color(0xFF00FF88),
              size: 18,
            ),
            onPressed: () {
              HapticFeedback.mediumImpact();
              connection.clearGrid();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Grid cleared'),
                  duration: Duration(seconds: 1),
                ),
              );
            },
          ),
          IconButton(
            tooltip: 'Options',
            icon: const Icon(
              Icons.tune_rounded,
              color: Color(0xFF38bdf8),
              size: 18,
            ),
            onPressed: () => _showOptionsModal(context, connection),
          ),
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
    PracticeTimerService timerService,
    dynamic bpm,
    double currentBpm,
    dynamic timeSig,
    dynamic clickSub,
  ) {
    return LayoutBuilder(
      builder: (context, constraints) {
        return SingleChildScrollView(
          physics: const BouncingScrollPhysics(),
          child: ConstrainedBox(
            constraints: BoxConstraints(minHeight: constraints.maxHeight),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  // Prominent BPM Header & Metro Style Ruler Selector
                  Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        '${currentBpm.round()}',
                        style: const TextStyle(
                          color: Colors.white,
                          fontSize: 64,
                          fontWeight: FontWeight.w900,
                          letterSpacing: -1.0,
                        ),
                      ),
                      const Text(
                        'BPM',
                        style: TextStyle(
                          color: Color(0xFF00FF88),
                          fontSize: 12,
                          fontWeight: FontWeight.w800,
                          letterSpacing: 3.0,
                        ),
                      ),
                      const SizedBox(height: 8),
                      BpmRulerSelector(
                        bpm: currentBpm,
                        minBpm: bpm?.min ?? 30.0,
                        maxBpm: bpm?.max ?? 300.0,
                        onChanged: (v) {
                          connection.updateLocalParameterValue(
                            'internal_bpm',
                            v,
                          );
                          connection.setParameter('internal_bpm', v);
                        },
                      ),
                    ],
                  ),
                  const SizedBox(height: 14),
                  SignaturePicker(
                    currentNumerator: (timeSig?.value ?? 4.0).round(),
                    onChanged: (n) =>
                        connection.setParameter('time_sig_num', n.toDouble()),
                  ),
                  const SizedBox(height: 14),
                  SubdivisionPicker(
                    currentIndex: (clickSub?.value ?? 1.0).round(),
                    onChanged: (i) => connection.setParameter(
                      'click_subdivision',
                      i.toDouble(),
                    ),
                  ),
                  const SizedBox(height: 14),
                  _buildHistoryBarsControl(
                    connection,
                    connection.parameters['bars_window'],
                  ),
                  const SizedBox(height: 14),
                  _buildToleranceControl(
                    connection,
                    connection.parameters['tolerance_ms'],
                  ),
                ],
              ),
            ),
          ),
        );
      },
    );
  }

  void _showPracticeSetupModal(
    BuildContext context,
    ConnectionService connection,
    PracticeTimerService timerService,
    double currentBpm,
  ) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (context) {
        return PracticeSetupModal(
          currentBpm: currentBpm,
          onStart: (durationSecs, hasEndBpm, endBpm) {
            timerService.configure(
              durationSeconds: durationSecs,
              hasEndBpm: hasEndBpm,
              startBpm: currentBpm,
              endBpm: endBpm,
              onBpmChanged: (newBpm) {
                connection.setParameter('internal_bpm', newBpm);
              },
            );
            timerService.startPractice();
          },
        );
      },
    );
  }

  // ── Dedicated Practice Mode View ─────────────────────────────────
  Widget _buildPracticeModeView(
    ConnectionService connection,
    PracticeTimerService timerService,
  ) {
    return Scaffold(
      backgroundColor: const Color(0xFF0a0c10),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.spaceEvenly,
            children: [
              // Header Badge
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                decoration: BoxDecoration(
                  color: const Color(0xFF00FF88).withValues(alpha: 0.15),
                  borderRadius: BorderRadius.circular(20),
                  border: Border.all(
                    color: const Color(0xFF00FF88).withValues(alpha: 0.5),
                  ),
                ),
                child: const Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(
                      Icons.fitness_center_rounded,
                      color: Color(0xFF00FF88),
                      size: 18,
                    ),
                    SizedBox(width: 8),
                    Text(
                      'PRACTICE MODE ACTIVE',
                      style: TextStyle(
                        color: Color(0xFF00FF88),
                        fontSize: 12,
                        fontWeight: FontWeight.w800,
                        letterSpacing: 2.0,
                      ),
                    ),
                  ],
                ),
              ),

              // Countdown Timer Display (Metro Style Ruler + Prominent Text)
              Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    timerService.formattedRemainingTime,
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 84,
                      fontWeight: FontWeight.w900,
                      letterSpacing: -2.0,
                    ),
                  ),
                  const Text(
                    'TIME REMAINING',
                    style: TextStyle(
                      color: Color(0xFF8b92a8),
                      fontSize: 12,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 3.0,
                    ),
                  ),
                  const SizedBox(height: 12),
                  PracticeTimerRulerDisplay(
                    totalDurationSeconds: timerService.totalDurationSeconds,
                    remainingSeconds: timerService.remainingSeconds,
                    isInteractive: false,
                  ),
                ],
              ),

              // Current Live BPM Display
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 32, vertical: 20),
                decoration: BoxDecoration(
                  color: const Color(0xFF141722),
                  borderRadius: BorderRadius.circular(20),
                  border: Border.all(
                    color: const Color(0xFF252a3a),
                    width: 1.5,
                  ),
                ),
                child: Column(
                  children: [
                    Text(
                      '${timerService.currentBpm.round()}',
                      style: const TextStyle(
                        color: Color(0xFF00FF88),
                        fontSize: 56,
                        fontWeight: FontWeight.w900,
                      ),
                    ),
                    Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        const Text(
                          'CURRENT BPM',
                          style: TextStyle(
                            color: Color(0xFF8b92a8),
                            fontSize: 11,
                            fontWeight: FontWeight.w700,
                            letterSpacing: 2.0,
                          ),
                        ),
                        if (timerService.hasEndBpm) ...[
                          const SizedBox(width: 8),
                          Icon(
                            timerService.endBpm >= timerService.startBpm
                                ? Icons.trending_up_rounded
                                : Icons.trending_down_rounded,
                            color: const Color(0xFF38bdf8),
                            size: 16,
                          ),
                        ],
                      ],
                    ),
                  ],
                ),
              ),

              // Prominent Stop Button
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFFFF1744),
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(vertical: 20),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(16),
                    ),
                  ),
                  icon: const Icon(Icons.stop_rounded, size: 28),
                  label: const Text(
                    'STOP PRACTICE',
                    style: TextStyle(
                      fontSize: 16,
                      fontWeight: FontWeight.w900,
                      letterSpacing: 2.0,
                    ),
                  ),
                  onPressed: () {
                    HapticFeedback.heavyImpact();
                    timerService.stopPractice();
                  },
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildHistoryBarsControl(
    ConnectionService connection,
    dynamic barsWindow,
  ) {
    final currentIndex = (barsWindow?.value ?? 2.0).round();
    const options = ['1 Bar', '2 Bars', '4 Bars', '8 Bars'];

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        const Text(
          'HISTORY BARS',
          style: TextStyle(
            color: Color(0xFF8b92a8),
            fontSize: 10,
            fontWeight: FontWeight.w700,
            letterSpacing: 2.5,
          ),
        ),
        const SizedBox(height: 10),
        Container(
          decoration: BoxDecoration(
            color: const Color(0xFF0f1118),
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: const Color(0xFF2d3245), width: 1.5),
          ),
          padding: const EdgeInsets.all(4),
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            physics: const BouncingScrollPhysics(),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: List.generate(options.length, (i) {
                final isActive = i == currentIndex;
                return Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 2),
                  child: GestureDetector(
                    onTap: () {
                      HapticFeedback.mediumImpact();
                      connection.setParameter('bars_window', i.toDouble());
                    },
                    child: AnimatedContainer(
                      duration: const Duration(milliseconds: 200),
                      curve: Curves.easeOut,
                      padding: const EdgeInsets.symmetric(
                        horizontal: 14,
                        vertical: 12,
                      ),
                      decoration: BoxDecoration(
                        color: isActive
                            ? const Color(0xFFA855F7).withValues(alpha: 0.15)
                            : Colors.transparent,
                        borderRadius: BorderRadius.circular(10),
                        border: Border.all(
                          color: isActive
                              ? const Color(0xFFA855F7).withValues(alpha: 0.5)
                              : Colors.transparent,
                          width: 1.5,
                        ),
                        boxShadow: isActive
                            ? [
                                BoxShadow(
                                  color: const Color(
                                    0xFFA855F7,
                                  ).withValues(alpha: 0.15),
                                  blurRadius: 12,
                                ),
                              ]
                            : null,
                      ),
                      child: Text(
                        options[i],
                        style: TextStyle(
                          color: isActive
                              ? const Color(0xFFA855F7)
                              : const Color(0xFF6b7280),
                          fontSize: 16,
                          fontWeight: isActive
                              ? FontWeight.w800
                              : FontWeight.w500,
                        ),
                      ),
                    ),
                  ),
                );
              }),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildToleranceControl(
    ConnectionService connection,
    dynamic tolerance,
  ) {
    final val = (tolerance?.value ?? 20.0).toDouble();
    final minVal = (tolerance?.min ?? 5.0).toDouble();
    final maxVal = (tolerance?.max ?? 30.0).toDouble();
    final divisions = ((maxVal - minVal) * 2).round();

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Text(
              'TIMING TOLERANCE',
              style: TextStyle(
                color: Color(0xFF8b92a8),
                fontSize: 10,
                fontWeight: FontWeight.w700,
                letterSpacing: 2.5,
              ),
            ),
            const SizedBox(width: 8),
            Text(
              '±${val.toStringAsFixed(1)} ms',
              style: const TextStyle(
                color: Color(0xFF00FF88),
                fontSize: 12,
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        SizedBox(
          width: 340,
          child: SliderTheme(
            data: SliderThemeData(
              activeTrackColor: const Color(0xFF00FF88),
              inactiveTrackColor: const Color(0xFF1e2e35),
              thumbColor: const Color(0xFF00FF88),
              overlayColor: const Color(0xFF00FF88).withValues(alpha: 0.2),
              trackHeight: 4,
              thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 8),
            ),
            child: Slider(
              value: val.clamp(minVal, maxVal),
              min: minVal,
              max: maxVal,
              divisions: divisions > 0 ? divisions : 50,
              onChanged: (v) {
                HapticFeedback.selectionClick();
                connection.updateLocalParameterValue(
                  'tolerance_ms',
                  (v * 2).round() / 2,
                );
              },
              onChangeEnd: (v) {
                connection.setParameter('tolerance_ms', (v * 2).round() / 2);
              },
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildFooterActions(
    ConnectionService connection,
    PracticeTimerService timerService,
    double currentBpm,
    Map<String, dynamic> params,
  ) {
    final clickEnabled = params['click_enabled'];
    final isPaused = params['is_paused'];

    final clickOn = (clickEnabled?.value ?? 1.0) > 0.5;
    final pausedOn = (isPaused?.value ?? 0.0) > 0.5;

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: const BoxDecoration(
        color: Color(0xFF141722),
        border: Border(top: BorderSide(color: Color(0xFF252a3a), width: 1)),
      ),
      child: Row(
        children: [
          // Practice Mode Button
          GestureDetector(
            onTap: () {
              HapticFeedback.mediumImpact();
              _showPracticeSetupModal(
                context,
                connection,
                timerService,
                currentBpm,
              );
            },
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
              decoration: BoxDecoration(
                color: const Color(0xFF00FF88).withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(
                  color: const Color(0xFF00FF88).withValues(alpha: 0.5),
                  width: 1.5,
                ),
              ),
              child: const Row(
                children: [
                  Icon(
                    Icons.timer_rounded,
                    color: Color(0xFF00FF88),
                    size: 18,
                  ),
                  SizedBox(width: 6),
                  Text(
                    'PRACTICE',
                    style: TextStyle(
                      color: Color(0xFF00FF88),
                      fontSize: 12,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 1.2,
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(width: 10),

          // Metronome Quick Toggle
          Expanded(
            child: GestureDetector(
              onTap: () {
                HapticFeedback.mediumImpact();
                connection.setParameter('click_enabled', clickOn ? 0.0 : 1.0);
              },
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 200),
                padding: const EdgeInsets.symmetric(vertical: 12),
                decoration: BoxDecoration(
                  color: clickOn
                      ? const Color(0xFF00c853).withValues(alpha: 0.2)
                      : const Color(0xFF1e2235),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                    color: clickOn
                        ? const Color(0xFF00c853).withValues(alpha: 0.6)
                        : const Color(0xFF2d3245),
                    width: 1.5,
                  ),
                ),
                alignment: Alignment.center,
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(
                      clickOn
                          ? Icons.volume_up_rounded
                          : Icons.volume_off_rounded,
                      color: clickOn
                          ? const Color(0xFF00c853)
                          : const Color(0xFF6b7280),
                      size: 18,
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: FittedBox(
                        fit: BoxFit.scaleDown,
                        child: Text(
                          clickOn ? 'CLICK ON' : 'CLICK OFF',
                          style: TextStyle(
                            color: clickOn
                                ? const Color(0xFF00c853)
                                : const Color(0xFF6b7280),
                            fontSize: 12,
                            fontWeight: FontWeight.w700,
                            letterSpacing: 1.2,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(width: 10),

          // Pause Quick Toggle
          Expanded(
            child: GestureDetector(
              onTap: () {
                HapticFeedback.mediumImpact();
                connection.setParameter('is_paused', pausedOn ? 0.0 : 1.0);
              },
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 200),
                padding: const EdgeInsets.symmetric(vertical: 12),
                decoration: BoxDecoration(
                  color: pausedOn
                      ? const Color(0xFFFF1744).withValues(alpha: 0.2)
                      : const Color(0xFF1e2235),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                    color: pausedOn
                        ? const Color(0xFFFF1744).withValues(alpha: 0.6)
                        : const Color(0xFF2d3245),
                    width: 1.5,
                  ),
                ),
                alignment: Alignment.center,
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(
                      pausedOn
                          ? Icons.pause_circle_filled
                          : Icons.play_arrow_rounded,
                      color: pausedOn
                          ? const Color(0xFFFF1744)
                          : const Color(0xFF8b92a8),
                      size: 18,
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: FittedBox(
                        fit: BoxFit.scaleDown,
                        child: Text(
                          pausedOn ? 'PAUSED' : 'RUNNING',
                          style: TextStyle(
                            color: pausedOn
                                ? const Color(0xFFFF1744)
                                : const Color(0xFF8b92a8),
                            fontSize: 12,
                            fontWeight: FontWeight.w700,
                            letterSpacing: 1.2,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(width: 10),

          // All Parameters Options Drawer Button
          GestureDetector(
            onTap: () => _showOptionsModal(context, connection),
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
              decoration: BoxDecoration(
                color: const Color(0xFF38bdf8).withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(
                  color: const Color(0xFF38bdf8).withValues(alpha: 0.5),
                  width: 1.5,
                ),
              ),
              child: const Icon(Icons.tune_rounded, color: Color(0xFF38bdf8), size: 18),
            ),
          ),
        ],
      ),
    );
  }

  // ── Options / All Parameters Bottom Sheet ───────────────────────
  void _showOptionsModal(BuildContext context, ConnectionService connection) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: const Color(0xFF0a0c10),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (context) {
        return Consumer<ConnectionService>(
          builder: (context, conn, _) {
            final params = conn.parameters;

            final secondaryParams = <_SecondaryDef>[
              _SecondaryDef('click_volume', 'Click Volume'),
              _SecondaryDef('click_pan', 'Click Panning'),
              _SecondaryDef('click_sample_preset', 'Click Sound Preset'),
              _SecondaryDef('subdivision', 'Grid Subdivision'),
              _SecondaryDef(
                'latency_offset_ms',
                'System Latency',
                suffix: ' ms',
              ),
              _SecondaryDef('min_velocity', 'Min Velocity'),
              _SecondaryDef('show_ms_labels', 'Display MS Offsets'),
              _SecondaryDef('note_filter', 'Display Mode'),
            ];

            final available = secondaryParams
                .where((def) => params.containsKey(def.id))
                .toList();

            return DraggableScrollableSheet(
              initialChildSize: 0.7,
              minChildSize: 0.4,
              maxChildSize: 0.92,
              expand: false,
              builder: (context, scrollController) {
                return Column(
                  children: [
                    // Handle & Header
                    Container(
                      padding: const EdgeInsets.symmetric(vertical: 12),
                      alignment: Alignment.center,
                      child: Container(
                        width: 40,
                        height: 4,
                        decoration: BoxDecoration(
                          color: const Color(0xFF2d3245),
                          borderRadius: BorderRadius.circular(2),
                        ),
                      ),
                    ),
                    const Padding(
                      padding: EdgeInsets.symmetric(
                        horizontal: 20,
                        vertical: 4,
                      ),
                      child: Row(
                        children: [
                          Icon(
                            Icons.tune_rounded,
                            color: Color(0xFF38bdf8),
                            size: 20,
                          ),
                          SizedBox(width: 10),
                          Text(
                            'GRIDLOCK OPTIONS',
                            style: TextStyle(
                              color: Colors.white,
                              fontSize: 16,
                              fontWeight: FontWeight.w800,
                              letterSpacing: 2,
                            ),
                          ),
                        ],
                      ),
                    ),
                    const Divider(color: Color(0xFF252a3a), height: 20),

                    // Scrollable Parameters Grid
                    Expanded(
                      child: GridView.builder(
                        controller: scrollController,
                        padding: const EdgeInsets.all(16),
                        gridDelegate:
                            const SliverGridDelegateWithFixedCrossAxisCount(
                              crossAxisCount: 2,
                              mainAxisSpacing: 10,
                              crossAxisSpacing: 10,
                              childAspectRatio: 1.65,
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
                            onChanged: (v) =>
                                conn.updateLocalParameterValue(def.id, v),
                            onChangeEnd: (v) => conn.setParameter(def.id, v),
                          );
                        },
                      ),
                    ),
                  ],
                );
              },
            );
          },
        );
      },
    );
  }
}

class _SecondaryDef {
  final String id;
  final String displayName;
  final String? suffix;
  const _SecondaryDef(this.id, this.displayName, {this.suffix});
}
