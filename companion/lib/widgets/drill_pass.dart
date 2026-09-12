import 'package:flutter/material.dart';
import '../constants/app_constants.dart';
import '../services/connection_service.dart';

class DrillPassThreshold extends StatefulWidget {
  const DrillPassThreshold({super.key, required this.connection});
  final ConnectionService connection;
  @override
  State<DrillPassThreshold> createState() => _DrillPassThresholdState();
}

class _DrillPassThresholdState extends State<DrillPassThreshold> {
  double? _dragValue;
  @override
  Widget build(BuildContext context) {
    final drill = widget.connection.drill;
    final value = (_dragValue ?? drill.passThreshold).clamp(
      AppConstants.drillPassMin,
      AppConstants.drillPassMax,
    );
    return Column(
      children: [
        Text('Pass bar: ≥${(value * 100).round()}% on time and correct'),
        Slider(
          value: value,
          min: AppConstants.drillPassMin,
          max: AppConstants.drillPassMax,
          divisions:
              ((AppConstants.drillPassMax - AppConstants.drillPassMin) * 100)
                  .round(),
          label: '${(value * 100).round()}%',
          onChanged: widget.connection.isConnected
              ? (value) {
                  setState(() => _dragValue = value);
                  widget.connection.drillCommand(
                    'pass_threshold',
                    settings: {'passThreshold': value},
                  );
                }
              : null,
          onChangeEnd: (_) => setState(() => _dragValue = null),
        ),
        const Text(
          'Only this drill. Changing the pass bar resets tempo confirmation.',
          textAlign: TextAlign.center,
        ),
      ],
    );
  }
}
