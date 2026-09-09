import 'package:flutter/material.dart';
import '../constants/app_constants.dart';
import '../services/connection_service.dart';

class DrillTolerance extends StatefulWidget {
  const DrillTolerance({super.key, required this.connection});
  final ConnectionService connection;
  @override
  State<DrillTolerance> createState() => _DrillToleranceState();
}

class _DrillToleranceState extends State<DrillTolerance> {
  double? _dragValue;
  @override
  Widget build(BuildContext context) {
    final drill = widget.connection.drill;
    final value = (_dragValue ?? drill.toleranceOverride).clamp(
      AppConstants.toleranceMin,
      AppConstants.toleranceMax,
    );
    return Column(
      children: [
        Text('Drill tolerance: ±${value.toStringAsFixed(1)} ms'),
        Slider(
          value: value,
          min: AppConstants.toleranceMin,
          max: AppConstants.toleranceMax,
          divisions:
              ((AppConstants.toleranceMax - AppConstants.toleranceMin) * 2)
                  .round(),
          label: '${value.toStringAsFixed(1)} ms',
          onChanged: widget.connection.isConnected
              ? (value) {
                  setState(() => _dragValue = value);
                  widget.connection.drillCommand(
                    'tolerance',
                    settings: {'tolerance': value},
                  );
                }
              : null,
          onChangeEnd: (_) => setState(() => _dragValue = null),
        ),
        Text('Effective window: ±${drill.tolerance.toStringAsFixed(1)} ms'),
        const Text(
          'Only this drill. Changing tolerance resets tempo confirmation.',
          textAlign: TextAlign.center,
        ),
      ],
    );
  }
}
