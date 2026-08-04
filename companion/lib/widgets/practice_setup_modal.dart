import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../widgets/bpm_ruler_selector.dart';
import '../widgets/practice_timer_ruler_display.dart';

class PracticeSetupModal extends StatefulWidget {
  final double currentBpm;
  final Function(int durationSeconds, bool hasEndBpm, double endBpm) onStart;

  const PracticeSetupModal({
    super.key,
    required this.currentBpm,
    required this.onStart,
  });

  @override
  State<PracticeSetupModal> createState() => _PracticeSetupModalState();
}

class _PracticeSetupModalState extends State<PracticeSetupModal> {
  int _durationSeconds = 300; // 5 mins
  bool _hasEndBpm = false;
  late double _endBpm;

  @override
  void initState() {
    super.initState();
    _endBpm = (widget.currentBpm + 20).clamp(30.0, 300.0);
  }

  @override
  Widget build(BuildContext context) {
    final minutes = (_durationSeconds / 60).round();

    return Container(
      decoration: const BoxDecoration(
        color: Color(0xFF0e111a),
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      padding: EdgeInsets.only(
        left: 24,
        right: 24,
        top: 16,
        bottom: MediaQuery.of(context).padding.bottom + 24,
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Drag handle
          Center(
            child: Container(
              width: 36,
              height: 4,
              decoration: BoxDecoration(
                color: const Color(0xFF282d3f),
                borderRadius: BorderRadius.circular(2),
              ),
            ),
          ),
          const SizedBox(height: 16),

          // Title
          const Row(
            children: [
              Icon(Icons.fitness_center_rounded, color: Color(0xFF00FF88), size: 22),
              SizedBox(width: 10),
              Text(
                'PRACTICE TIMER SETUP',
                style: TextStyle(
                  color: Colors.white,
                  fontSize: 16,
                  fontWeight: FontWeight.w900,
                  letterSpacing: 2.0,
                ),
              ),
            ],
          ),
          const SizedBox(height: 20),

          // Section 1: Duration
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              const Text(
                'PRACTICE DURATION',
                style: TextStyle(
                  color: Color(0xFF8b92a8),
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                  letterSpacing: 1.5,
                ),
              ),
              Text(
                '$minutes min',
                style: const TextStyle(
                  color: Color(0xFF38bdf8),
                  fontSize: 16,
                  fontWeight: FontWeight.w900,
                ),
              ),
            ],
          ),
          const SizedBox(height: 6),
          PracticeTimerRulerDisplay(
            totalDurationSeconds: _durationSeconds,
            remainingSeconds: _durationSeconds,
            isInteractive: true,
            onDurationChanged: (secs) {
              setState(() {
                _durationSeconds = secs;
              });
            },
          ),
          const SizedBox(height: 20),

          // Section 2: Optional End BPM ramp
          Container(
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: const Color(0xFF141824),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: _hasEndBpm
                    ? const Color(0xFF00FF88).withValues(alpha: 0.4)
                    : const Color(0xFF222738),
                width: 1.5,
              ),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Row(
                      children: [
                        Icon(Icons.trending_up_rounded, color: Color(0xFF00FF88), size: 18),
                        SizedBox(width: 8),
                        Text(
                          'Gradual BPM Transition',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 13,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ],
                    ),
                    Switch(
                      value: _hasEndBpm,
                      activeTrackColor: const Color(0xFF00FF88),
                      onChanged: (val) {
                        setState(() {
                          _hasEndBpm = val;
                        });
                      },
                    ),
                  ],
                ),

                if (_hasEndBpm) ...[
                  const Divider(color: Color(0xFF222738), height: 20),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(
                        'Start: ${widget.currentBpm.round()} BPM',
                        style: const TextStyle(
                          color: Color(0xFF8b92a8),
                          fontSize: 12,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                      Text(
                        'Target End: ${_endBpm.round()} BPM',
                        style: const TextStyle(
                          color: Color(0xFF00FF88),
                          fontSize: 14,
                          fontWeight: FontWeight.w900,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  BpmRulerSelector(
                    bpm: _endBpm,
                    minBpm: 30.0,
                    maxBpm: 300.0,
                    onChanged: (v) {
                      setState(() {
                        _endBpm = v;
                      });
                    },
                  ),
                ],
              ],
            ),
          ),
          const SizedBox(height: 24),

          // Start Button
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF00FF88),
                foregroundColor: Colors.black,
                padding: const EdgeInsets.symmetric(vertical: 16),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(14),
                ),
                elevation: 4,
              ),
              icon: const Icon(Icons.play_arrow_rounded, size: 28),
              label: const Text(
                'BEGIN PRACTICE',
                style: TextStyle(
                  fontSize: 15,
                  fontWeight: FontWeight.w900,
                  letterSpacing: 2.0,
                ),
              ),
              onPressed: () {
                HapticFeedback.heavyImpact();
                Navigator.pop(context);
                widget.onStart(_durationSeconds, _hasEndBpm, _endBpm);
              },
            ),
          ),
        ],
      ),
    );
  }
}
