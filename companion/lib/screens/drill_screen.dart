import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../constants/app_colors.dart';
import '../constants/app_constants.dart';
import '../services/connection_service.dart';
import '../services/drill_settings.dart';
import '../models/drill_state.dart';
import '../widgets/drill_pass.dart';
import '../widgets/drill_tolerance.dart';

class DrillScreen extends StatefulWidget {
  const DrillScreen({super.key});
  @override
  State<DrillScreen> createState() => _DrillScreenState();
}

class _DrillScreenState extends State<DrillScreen> {
  final _pattern = TextEditingController(text: 'RLK');
  final _bpm = TextEditingController(
    text: AppConstants.drillStartBpm.toString(),
  );
  final _kick = TextEditingController(
    text: AppConstants.drillKickNote.toString(),
  );
  final _form = GlobalKey<FormState>();
  int _spacing = 2;
  double _pass = AppConstants.drillPassDefault;
  double _step = AppConstants.drillStepDefault;
  double? _tolerance;

  @override
  void initState() {
    super.initState();
    for (final controller in [_pattern, _kick]) {
      controller.addListener(_suggestTempo);
    }
    _suggestTempo();
    _restore();
  }

  Future<void> _restore() async {
    final saved = await DrillSettings.load();
    if (!mounted) return;
    setState(() {
      _spacing = saved.spacing;
      _pass = saved.passThreshold;
      _step = saved.step;
      _tolerance = saved.tolerance;
      _kick.text = saved.kick.toString();
      _pattern.text = saved.pattern;
      if (saved.bpm != null) {
        _bpm.text = saved.bpm.toString();
      } else {
        _suggestTempo();
      }
    });
  }

  double _setupTolerance() {
    final main =
        context.read<ConnectionService>().parameters['tolerance_ms']?.value ??
        AppConstants.drillToleranceMs;
    return _tolerance ?? main;
  }

  void _suggestTempo() {
    final pattern = _pattern.text.replaceAll(RegExp(r'\s'), '').toUpperCase();
    final tolerance = _setupTolerance();
    final key =
        '$pattern:$_spacing:${_kick.text}:${(tolerance * 10).round()}:${(_pass * 100).round()}';
    final best = context.read<ConnectionService>().drillHistory[key];
    _bpm.text = best == null
        ? (_spacing == 0
                  ? AppConstants.drillEighthStartBpm
                  : AppConstants.drillStartBpm)
              .toString()
        : (best * AppConstants.drillResumeRatio)
              .floor()
              .clamp(
                AppConstants.drillMinBpm.toInt(),
                AppConstants.bpmMax.toInt(),
              )
              .toString();
  }

  @override
  void dispose() {
    for (final controller in [_pattern, _bpm, _kick]) {
      controller.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final connection = context.watch<ConnectionService>();
    final drill = connection.drill;
    return Scaffold(
      backgroundColor: AppColors.bgMain,
      appBar: AppBar(title: const Text('Grouping Drill')),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (!connection.isConnected)
              const Text(
                'Disconnected. Reconnect from the main screen. Automatic increases are suspended.',
              ),
            const Text(
              'Timing and kick placement are checked. R/L sticking is your responsibility.',
            ),
            const SizedBox(height: 16),
            if (!drill.active) _setup(connection),
            if (connection.drillError != null)
              Text(
                connection.drillError!,
                style: const TextStyle(color: AppColors.skyBlue),
              ),
            if (drill.state != 'idle') _live(connection),
          ],
        ),
      ),
    );
  }

  Widget _live(ConnectionService connection) {
    final drill = connection.drill;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          '${drill.bpm.round()} BPM',
          textAlign: TextAlign.center,
          style: const TextStyle(fontSize: 48, fontWeight: FontWeight.bold),
        ),
        Text('Pattern: ${drill.pattern}', textAlign: TextAlign.center),
        if (drill.active) ...[
          Text(
            drill.state == 'paused'
                ? 'Detection paused'
                : drill.sequenceDetected
                ? '● Sequence detected'
                : drill.sequenceSeen
                ? '○ Sequence lost — listening'
                : '○ Listening for sequence',
            textAlign: TextAlign.center,
            style: TextStyle(
              color: drill.sequenceDetected
                  ? AppColors.emerald
                  : AppColors.warning,
            ),
          ),
          DrillTolerance(connection: connection),
          DrillPassThreshold(connection: connection),
        ],
        Text(_status(connection), textAlign: TextAlign.center),
        const SizedBox(height: 12),
        LinearProgressIndicator(value: drill.progress),
        Text(_passBar(drill)),
        Text('${drill.passes} of 2 blocks passed'),
        Text(_verdict(drill)),
        Text(
          'Missed ${drill.missing} · Wrong type ${drill.wrong} · Timing ${drill.late} · Extra ${drill.extras}',
        ),
        const SizedBox(height: 12),
        Text(
          drill.best > 0
              ? 'Highest confirmed: ${drill.best.round()} BPM'
              : 'No tempo confirmed yet',
        ),
        Text('Highest attempted: ${drill.attempted.round()} BPM'),
        Text('Session floor: ${drill.floorBpm.round()} BPM'),
        if (drill.nextBpm > 0)
          Text('Next repeat: ${drill.nextBpm.round()} BPM'),
        if (drill.active) _actions(connection),
      ],
    );
  }

  Widget _actions(ConnectionService connection) {
    final drill = connection.drill;
    return Wrap(
      spacing: 12,
      runSpacing: 8,
      children: [
        _button(connection, 'Hold tempo', 'hold'),
        _button(connection, 'Too fast', 'too_fast'),
        _button(
          connection,
          drill.state == 'paused' ? 'Resume' : 'Pause',
          drill.state == 'paused' ? 'resume' : 'pause',
        ),
        _button(connection, 'Try again', 'retry'),
        _button(connection, 'Finish', 'finish'),
      ],
    );
  }

  String _passBar(DrillState drill) =>
      'Need ≥${(drill.passThreshold * 100).round()}% + locked sequence, 2 in a row';

  String _verdict(DrillState drill) {
    final last = 'Last block: ${(drill.accuracy * 100).round()}%';
    if (!drill.hasBlock) return '$last on time and correct';
    final reasons = <String>[
      if (drill.failAccuracy)
        'need ≥${(drill.passThreshold * 100).round()}%',
      if (drill.failExtras) 'too many extras',
      if (drill.failSequence) 'sequence not locked',
    ];
    return reasons.isEmpty ? '$last — pass' : '$last — ${reasons.join(', ')}';
  }

  String _status(ConnectionService connection) {
    final d = connection.drill;
    if (d.state == 'waiting') {
      return 'Play when ready — any subdivision';
    }
    if (d.state == 'paused') {
      return d.noHits ? 'No hits detected — paused' : 'Paused';
    }
    if (d.state == 'finished') return 'Finished';
    if (d.limitReached) {
      return 'Tempo not confirmed. Practice here, try again, or finish.';
    }
    return d.automatic ? 'Building speed' : 'Holding tempo';
  }

  Widget _button(ConnectionService connection, String title, String action) =>
      FilledButton(
        onPressed: connection.isConnected
            ? () => connection.drillCommand(action)
            : null,
        child: Text(title),
      );

  Widget _number(
    TextEditingController controller,
    String label,
    double min,
    double max,
  ) => TextFormField(
    controller: controller,
    keyboardType: TextInputType.number,
    decoration: InputDecoration(labelText: label),
    validator: (text) {
      final value = double.tryParse(text ?? '');
      return value == null || !value.isFinite || value < min || value > max
          ? 'Enter $min–$max'
          : null;
    },
  );

  Widget _advanced() => ExpansionTile(
    title: const Text('Advanced'),
    children: [
      _number(_kick, 'Kick MIDI note (hi-hat pedal 44 is ignored)', 0, 127),
    ],
  );

  Widget _setup(ConnectionService connection) => Form(
    key: _form,
    child: Column(
      children: [
        TextFormField(
          controller: _pattern,
          decoration: const InputDecoration(labelText: 'Pattern, e.g. RLRLKK'),
          textCapitalization: TextCapitalization.characters,
          validator: (value) =>
              RegExp(r'^[RLK]{1,64}$').hasMatch(
                (value ?? '').replaceAll(RegExp(r'\s'), '').toUpperCase(),
              )
              ? null
              : 'Enter 1–64 R, L or K letters',
        ),
        DropdownButtonFormField<int>(
          initialValue: _spacing,
          decoration: const InputDecoration(labelText: 'One letter per'),
          items: const [
            DropdownMenuItem(value: 0, child: Text('Eighth note')),
            DropdownMenuItem(value: 1, child: Text('Triplet')),
            DropdownMenuItem(value: 2, child: Text('Sixteenth note')),
            DropdownMenuItem(value: 3, child: Text('Sixtuplet')),
          ],
          onChanged: (value) {
            setState(() {
              _spacing = value!;
              _suggestTempo();
            });
          },
        ),
        _number(_bpm, 'Start BPM', AppConstants.bpmMin, AppConstants.bpmMax),
        Builder(
          builder: (context) {
            final tolerance = _setupTolerance().clamp(
              AppConstants.toleranceMin,
              AppConstants.toleranceMax,
            );
            return Column(
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Slider(
                        value: tolerance,
                        min: AppConstants.toleranceMin,
                        max: AppConstants.toleranceMax,
                        divisions:
                            ((AppConstants.toleranceMax -
                                        AppConstants.toleranceMin) *
                                    2)
                                .round(),
                        label: '±${tolerance.toStringAsFixed(1)} ms',
                        onChanged: (value) {
                          setState(() {
                            _tolerance = value;
                            _suggestTempo();
                          });
                        },
                      ),
                    ),
                    Text('±${tolerance.toStringAsFixed(1)} ms'),
                  ],
                ),
                const Text(
                  'Start tolerance and pass bar apply to this drill only. Adjust both live after Start.',
                  textAlign: TextAlign.center,
                ),
              ],
            );
          },
        ),
        Row(
          children: [
            Expanded(
              child: Slider(
                value: _pass,
                min: AppConstants.drillPassMin,
                max: AppConstants.drillPassMax,
                divisions:
                    ((AppConstants.drillPassMax - AppConstants.drillPassMin) *
                            100)
                        .round(),
                label: '${(_pass * 100).round()}% to pass',
                onChanged: (value) {
                  setState(() {
                    _pass = value;
                    _suggestTempo();
                  });
                },
              ),
            ),
            Text('${(_pass * 100).round()}% to pass'),
          ],
        ),
        Row(
          children: [
            Expanded(
              child: Slider(
                value: _step,
                min: AppConstants.drillStepMin,
                max: AppConstants.drillStepMax,
                divisions:
                    (AppConstants.drillStepMax - AppConstants.drillStepMin)
                        .round(),
                label: '${_step.round()} BPM climb',
                onChanged: (value) {
                  setState(() {
                    _step = value;
                  });
                },
              ),
            ),
            Text('${_step.round()} BPM climb'),
          ],
        ),
        _advanced(),
        const SizedBox(height: 12),
        FilledButton(
          onPressed: connection.isConnected
              ? () async {
                  if (!_form.currentState!.validate()) return;
                  final tolerance = _setupTolerance();
                  connection.drillCommand(
                    'start',
                    settings: {
                      'pattern': _pattern.text,
                      'spacing': _spacing,
                      'bpm': double.parse(_bpm.text),
                      'kick': int.tryParse(_kick.text) ?? -1,
                      'tolerance': tolerance,
                      'passThreshold': _pass,
                      'step': _step,
                    },
                  );
                  await DrillSettings(
                    pattern: _pattern.text,
                    spacing: _spacing,
                    bpm: double.parse(_bpm.text),
                    kick:
                        int.tryParse(_kick.text) ??
                        AppConstants.drillKickNote,
                    tolerance: tolerance,
                    passThreshold: _pass,
                    step: _step,
                  ).save();
                }
              : null,
          child: const Text('Start'),
        ),
      ],
    ),
  );
}
