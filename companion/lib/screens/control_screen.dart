// ignore_for_file: unused_element_parameter
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../constants/app_colors.dart';
import '../constants/app_constants.dart';
import '../models/parameter.dart';
import '../services/connection_service.dart';
import '../services/discovery_service.dart';
import '../services/practice_timer_service.dart';
import '../utils/net_utils.dart';
import '../widgets/bpm_ruler_selector.dart';
import '../widgets/discovery_view.dart';
import '../widgets/practice_timer_ruler_display.dart';
import '../widgets/practice_setup_modal.dart';
import '../widgets/signature_picker.dart';
import '../widgets/status_bar.dart';
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
      timeout: const Duration(seconds: AppConstants.discoveryUiTimeoutSec),
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
    return DiscoveryView(
      discovering: _discovering,
      errorMessage: _errorMessage,
      pulse: _pulseController,
      onRetry: _startDiscovery,
      onManual: _showManualConnectDialog,
    );
  }

  Future<void> _showManualConnectDialog() async {
    final controller =
        TextEditingController(text: '${AppConstants.wsDefaultHost}:${AppConstants.wsPort}');
    final result = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: AppColors.bgInput,
        title: const Text('Manual Connection', style: TextStyle(color: Colors.white)),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Enter Gridlock PC IP & Port:',
                style: TextStyle(color: AppColors.textMuted, fontSize: 13)),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              autofocus: true,
              style: const TextStyle(color: Colors.white, fontSize: 16),
              decoration: const InputDecoration(
                hintText: 'e.g. 192.168.1.100:9876',
                hintStyle: TextStyle(color: Colors.white24),
                enabledBorder: OutlineInputBorder(borderSide: BorderSide(color: Colors.white24)),
                focusedBorder: OutlineInputBorder(borderSide: BorderSide(color: AppColors.skyBlue)),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancel', style: TextStyle(color: Colors.white54))),
          TextButton(
              onPressed: () => Navigator.pop(context, controller.text.trim()),
              child: const Text('Connect', style: TextStyle(color: AppColors.skyBlue))),
        ],
      ),
    );

    if (result != null && result.isNotEmpty && mounted) {
      final parsed = parseHostPort(result);
      final ip = parsed.ip;
      final port = parsed.port;

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
      backgroundColor: AppColors.bgMain,
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
    return StatusBar(
      connection: connection,
      onClearGrid: () => connection.clearGrid(),
      onOptions: () => _showOptionsModal(context, connection),
      onRefresh: () {
        connection.disconnect();
        _startDiscovery();
      },
    );
  }

  Widget _buildPrimaryControls(
    ConnectionService connection,
    PracticeTimerService timerService,
    RemoteParameter? bpm,
    double currentBpm,
    RemoteParameter? timeSig,
    RemoteParameter? clickSub,
  ) {
    return LayoutBuilder(
      builder: (context, constraints) {
        return SingleChildScrollView(
          physics: const BouncingScrollPhysics(),
          child: ConstrainedBox(
            constraints: BoxConstraints(minHeight: constraints.maxHeight),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
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
                          fontSize: 52,
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
                        },
                        onChangeEnd: (v) {
                          connection.setParameter('internal_bpm', v);
                        },
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  SignaturePicker(
                    currentNumerator: (timeSig?.value ?? 4.0).round(),
                    onChanged: (n) =>
                        connection.setParameter('time_sig_num', n.toDouble()),
                  ),
                  const SizedBox(height: 8),
                  SubdivisionPicker(
                    currentIndex: (clickSub?.value ?? 1.0).round(),
                    onChanged: (i) => connection.setParameter(
                      'click_subdivision',
                      i.toDouble(),
                    ),
                  ),
                  const SizedBox(height: 8),
                  _buildHistoryBarsControl(
                    connection,
                    connection.parameters['bars_window'],
                  ),
                  const SizedBox(height: 8),
                  _buildToleranceControl(
                    connection,
                    connection.parameters['tolerance_ms'],
                  ),
                  const SizedBox(height: 8),
                  _buildLatencyControl(
                    connection,
                    connection.parameters['latency_offset_ms'],
                  ),
                  const SizedBox(height: 8),
                  _buildCalibrateControl(connection),
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
      backgroundColor: AppColors.bgMain,
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
                  color: AppColors.emerald.withValues(alpha: 0.15),
                  borderRadius: BorderRadius.circular(20),
                  border: Border.all(
                    color: AppColors.emerald.withValues(alpha: 0.5),
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
                  color: AppColors.bgCard,
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
                            color: AppColors.skyBlue,
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
                    backgroundColor: AppColors.error,
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
    RemoteParameter? barsWindow,
  ) {
    final currentIndex = (barsWindow?.value ?? 2.0).round();
    const options = AppConstants.historyOptions;

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
                              : AppColors.textFaint,
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
    RemoteParameter? tolerance,
  ) {
    final val = (tolerance?.value ?? 20.0).toDouble();
    final minVal = (tolerance?.min ?? AppConstants.toleranceMin).toDouble();
    final maxVal = (tolerance?.max ?? AppConstants.toleranceMax).toDouble();
    final divisions = ((maxVal - minVal) * 2).round();

    final tier = _getToleranceTier(val);

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        // Fixed-width header line so TIMING TOLERANCE label stays completely stationary
        SizedBox(
          width: 340,
          child: Row(
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
              const Spacer(),
              AnimatedDefaultTextStyle(
                duration: const Duration(milliseconds: 150),
                style: TextStyle(
                  color: tier.color,
                  fontSize: 12,
                  fontWeight: FontWeight.w900,
                ),
                child: Text('±${val.toStringAsFixed(1)} ms'),
              ),
              const SizedBox(width: 8),
              SizedBox(
                width: 78,
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 150),
                  padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
                  decoration: BoxDecoration(
                    color: tier.color.withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(6),
                    border: Border.all(
                      color: tier.color.withValues(alpha: 0.5),
                      width: 1,
                    ),
                  ),
                  alignment: Alignment.center,
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    child: Text(
                      tier.title,
                      style: TextStyle(
                        color: tier.color,
                        fontSize: 10,
                        fontWeight: FontWeight.w800,
                        letterSpacing: 0.8,
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 8),

        // Spectrum Gradient Slider
        SizedBox(
          width: 340,
          child: Stack(
            alignment: Alignment.center,
            children: [
              // Spectrum Gradient Track Background accurately mapped to 5-40 ms linear scale
              Container(
                height: 6,
                margin: const EdgeInsets.symmetric(horizontal: 10),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(3),
                  gradient: const LinearGradient(
                    colors: [
                      Color(0xFF00FF88), // < 5ms (Emerald)
                      Color(0xFFA3E635), // 5-10ms (Lime)
                      Color(0xFFFACC15), // 10-20ms (Amber)
                      Color(0xFFFB923C), // 20-40ms (Orange)
                    ],
                    stops: [0.0, 0.14, 0.43, 1.0], // 5ms=0.0, 10ms=0.14, 20ms=0.43, 40ms=1.0
                  ),
                ),
              ),

              SliderTheme(
                data: SliderThemeData(
                  activeTrackColor: tier.color,
                  inactiveTrackColor: Colors.black.withValues(alpha: 0.4),
                  thumbColor: tier.color,
                  overlayColor: tier.color.withValues(alpha: 0.2),
                  trackHeight: 4,
                  thumbShape: const RoundSliderThumbShape(
                    enabledThumbRadius: 10,
                    elevation: 4,
                  ),
                ),
                child: Slider(
                  value: val.clamp(minVal, maxVal),
                  min: minVal,
                  max: maxVal,
                  divisions: divisions > 0 ? divisions : 70,
                  onChanged: (v) {
                    HapticFeedback.selectionClick();
                    connection.updateLocalParameterValue(
                      'tolerance_ms',
                      (v * 2).round() / 2,
                    );
                  },
                  onChangeEnd: (v) {
                    connection.setParameter(
                      'tolerance_ms',
                      (v * 2).round() / 2,
                    );
                  },
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildLatencyControl(
    ConnectionService connection,
    RemoteParameter? latency,
  ) {
    final val = (latency?.value ?? 0.0).toDouble();
    final minVal = (latency?.min ?? -500.0).toDouble();
    final maxVal = (latency?.max ?? 500.0).toDouble();
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        SizedBox(
          width: 340,
          child: Row(
            children: [
              const Text(
                'SYSTEM LATENCY',
                style: TextStyle(
                  color: Color(0xFF8b92a8),
                  fontSize: 10,
                  fontWeight: FontWeight.w700,
                  letterSpacing: 2.5,
                ),
              ),
              const Spacer(),
              Text(
                '${val.toStringAsFixed(0)} ms',
                style: const TextStyle(
                  color: Color(0xFF38bdf8),
                  fontSize: 12,
                  fontWeight: FontWeight.w900,
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 8),
        SizedBox(
          width: 340,
          child: SliderTheme(
            data: SliderThemeData(
              activeTrackColor: const Color(0xFF38bdf8),
              inactiveTrackColor: Colors.black.withValues(alpha: 0.4),
              thumbColor: const Color(0xFF38bdf8),
              overlayColor: const Color(0xFF38bdf8).withValues(alpha: 0.2),
              trackHeight: 4,
              thumbShape: const RoundSliderThumbShape(
                enabledThumbRadius: 10,
                elevation: 4,
              ),
            ),
            child: Slider(
              value: val.clamp(minVal, maxVal),
              min: minVal,
              max: maxVal,
              divisions: 1000,
              onChanged: (v) {
                HapticFeedback.selectionClick();
                connection.updateLocalParameterValue('latency_offset_ms', v.roundToDouble());
              },
              onChangeEnd: (v) {
                connection.setParameter('latency_offset_ms', v.roundToDouble());
              },
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildCalibrateControl(ConnectionService connection) {
    final state = connection.calibrationState;
    final progress = connection.calibrationProgress;
    final isIdle = state == 'idle';
    final isCountIn = state == 'countin';
    final isRecording = state == 'recording';
    final isDone = state == 'done';
    String label;
    Color bg;
    Color border;
    String subtitle = '';
    if (isIdle) {
      label = 'CALIBRATE LATENCY';
      bg = const Color(0xFF38bdf8).withValues(alpha: 0.15);
      border = const Color(0xFF38bdf8).withValues(alpha: 0.5);
    } else if (isCountIn) {
      label = 'COUNT-IN...';
      bg = const Color(0xFFFACC15).withValues(alpha: 0.2);
      border = const Color(0xFFFACC15).withValues(alpha: 0.6);
      subtitle = 'Get ready — ${connection.calibrationTimeSigNum}/4 @ ${connection.calibrationBpm.toStringAsFixed(0)} BPM — hits every subdiv';
    } else if (isRecording) {
      final pct = (progress * 100).round();
      label = 'REC $pct%';
      bg = const Color(0xFFFB923C).withValues(alpha: 0.2);
      border = const Color(0xFFFB923C).withValues(alpha: 0.6);
      subtitle = '${connection.calibrationHitCount}/${connection.calibrationExpectedHits} hits';
    } else if (isDone) {
      final hasRes = connection.calibrationHasResult && connection.calibrationHitCount > 0;
      if (!hasRes) {
        final reason = connection.calibrationReason;
        final hits = connection.calibrationHitCount;
        final exp = connection.calibrationExpectedHits;
        final sd = connection.calibrationSdMs;
        final bpmTxt = connection.calibrationBpm.toStringAsFixed(0);
        bg = const Color(0xFF6b7280).withValues(alpha: 0.15);
        border = const Color(0xFF6b7280).withValues(alpha: 0.5);
        if (reason == 'noHits' || hits == 0) {
          label = 'NO HITS — RETRY';
          subtitle = '0/$exp hits @ $bpmTxt BPM — hit every subdiv';
        } else if (reason == 'tooFew') {
          label = 'TOO FEW — RETRY';
          subtitle = '$hits/$exp hits — need ${(exp * 0.5).ceil()} + steady hits';
        } else if (reason == 'jitter') {
          label = 'UNSTABLE — RETRY';
          subtitle = 'SD ${sd.toStringAsFixed(1)}ms — keep time steadier';
        } else {
          label = 'NO HITS — RETRY';
          subtitle = 'Hit every subdiv @ $bpmTxt BPM — try again';
        }
      } else {
        final clamped = connection.calibrationMeanMs.clamp(0.0, 500.0);
        label = 'APPLY ${clamped.toStringAsFixed(1)}ms?';
        bg = const Color(0xFF00FF88).withValues(alpha: 0.2);
        border = const Color(0xFF00FF88).withValues(alpha: 0.6);
        subtitle =
            'Median ${connection.calibrationMedianMs.clamp(0.0, 500.0).toStringAsFixed(1)} SD ${connection.calibrationSdMs.toStringAsFixed(1)}'
            '${connection.calibrationMeanMs < 0 ? ' (rush, clamped to 0)' : ''}';
      }
    } else {
      label = state.toUpperCase();
      bg = const Color(0xFF2d3245).withValues(alpha: 0.3);
      border = const Color(0xFF2d3245);
    }
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        GestureDetector(
          onTap: () {
            HapticFeedback.mediumImpact();
            if (isIdle) {
              connection.startCalibration();
            } else if (isCountIn || isRecording) {
              connection.cancelCalibration();
            } else if (isDone) {
              _showCalibrationDoneDialog(connection);
            }
          },
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 200),
            width: 340,
            padding: const EdgeInsets.symmetric(vertical: 14),
            decoration: BoxDecoration(
              color: bg,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: border, width: 1.5),
            ),
            alignment: Alignment.center,
            child: Text(
              label,
              style: TextStyle(
                color: border,
                fontSize: 14,
                fontWeight: FontWeight.w900,
                letterSpacing: 1.5,
              ),
            ),
          ),
        ),
        if (subtitle.isNotEmpty) ...[
          const SizedBox(height: 6),
          Text(
            subtitle,
            style: const TextStyle(
              color: Color(0xFF8b92a8),
              fontSize: 11,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
        if (isDone) ...[
          const SizedBox(height: 8),
          Builder(builder: (context) {
            final hasRes = connection.calibrationHasResult && connection.calibrationHitCount > 0;
            if (!hasRes) {
              return TextButton(
                onPressed: () {
                  HapticFeedback.selectionClick();
                  connection.cancelCalibration();
                },
                child: const Text('OK — Keep old latency', style: TextStyle(color: Colors.white70)),
              );
            }
            return Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                ElevatedButton(
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFF00FF88),
                    foregroundColor: const Color(0xFF0a0c10),
                    padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 10),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  onPressed: () {
                    HapticFeedback.mediumImpact();
                    connection.applyCalibration(addToExisting: false);
                  },
                  child: const Text('Apply', style: TextStyle(fontWeight: FontWeight.w800)),
                ),
                const SizedBox(width: 8),
                OutlinedButton(
                  style: OutlinedButton.styleFrom(
                    foregroundColor: const Color(0xFF00FF88),
                    side: BorderSide(color: const Color(0xFF00FF88).withValues(alpha: 0.5)),
                    padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 10),
                  ),
                  onPressed: () {
                    HapticFeedback.selectionClick();
                    connection.applyCalibration(addToExisting: true);
                  },
                  child: const Text('Add'),
                ),
                const SizedBox(width: 8),
                TextButton(
                  onPressed: () {
                    HapticFeedback.selectionClick();
                    connection.cancelCalibration();
                  },
                  child: const Text('Cancel', style: TextStyle(color: Colors.white54)),
                ),
              ],
            );
          }),
        ],
      ],
    );
  }

  void _showCalibrationDoneDialog(ConnectionService connection) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF141722),
        title: const Text('Calibration Done', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w800)),
        content: Text(
          'Mean ${connection.calibrationMeanMs.toStringAsFixed(1)}ms '
          '(median ${connection.calibrationMedianMs.toStringAsFixed(1)} SD ${connection.calibrationSdMs.toStringAsFixed(1)})\n'
          'Hits ${connection.calibrationHitCount}/${connection.calibrationExpectedHits} on subdiv grid.',
          style: const TextStyle(color: Color(0xFF8b92a8), fontSize: 13),
        ),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              connection.cancelCalibration();
            },
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              connection.applyCalibration(addToExisting: true);
            },
            child: const Text('Add to existing'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF00FF88)),
            onPressed: () {
              Navigator.pop(context);
              connection.applyCalibration(addToExisting: false);
            },
            child: const Text('Apply', style: TextStyle(color: Color(0xFF0a0c10))),
          ),
        ],
      ),
    );
  }

  Widget _buildFooterActions(
    ConnectionService connection,
    PracticeTimerService timerService,
    double currentBpm,
    Map<String, RemoteParameter> params,
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
                color: AppColors.emerald.withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(
                  color: AppColors.emerald.withValues(alpha: 0.5),
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
                          : AppColors.textFaint,
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
                                : AppColors.textFaint,
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
                      ? AppColors.error.withValues(alpha: 0.2)
                      : const Color(0xFF1e2235),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                    color: pausedOn
                        ? AppColors.error.withValues(alpha: 0.6)
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
                          ? AppColors.error
                          : AppColors.textMuted,
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
                                ? AppColors.error
                                : AppColors.textMuted,
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
                color: AppColors.skyBlue.withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(
                  color: AppColors.skyBlue.withValues(alpha: 0.5),
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
      backgroundColor: AppColors.bgMain,
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

class _ToleranceTier {
  final String title;
  final Color color;

  const _ToleranceTier({
    required this.title,
    required this.color,
  });
}

_ToleranceTier _getToleranceTier(double ms) {
  if (ms < 5.0) {
    return const _ToleranceTier(
      title: 'Perfect',
      color: Color(0xFF00FF88),
    );
  } else if (ms < 10.0) {
    return const _ToleranceTier(
      title: 'Tight',
      color: Color(0xFFA3E635),
    );
  } else if (ms < 20.0) {
    return const _ToleranceTier(
      title: 'Groove',
      color: Color(0xFFFACC15),
    );
  } else if (ms < 40.0) {
    return const _ToleranceTier(
      title: 'Early / Late',
      color: Color(0xFFFB923C),
    );
  } else if (ms < 70.0) {
    return const _ToleranceTier(
      title: 'Off-Beat',
      color: Color(0xFFF87171),
    );
  } else {
    return const _ToleranceTier(
      title: 'Wrong',
      color: Color(0xFFC084FC),
    );
  }
}
