import 'package:flutter/material.dart';
import '../constants/app_constants.dart';

class BpmRulerSelector extends StatefulWidget {
  final double bpm;
  final ValueChanged<double> onChanged;
  final ValueChanged<double>? onChangeEnd;
  final double minBpm;
  final double maxBpm;

  const BpmRulerSelector({
    super.key,
    required this.bpm,
    required this.onChanged,
    this.onChangeEnd,
    this.minBpm = AppConstants.bpmRulerMin,
    this.maxBpm = AppConstants.bpmRulerMax,
  });

  @override
  State<BpmRulerSelector> createState() => _BpmRulerSelectorState();
}

class _BpmRulerSelectorState extends State<BpmRulerSelector> {
  late double _currentOffset;
  final double _pixelsPerBpm = AppConstants.pixelsPerBpm;

  @override
  void initState() {
    super.initState();
    _currentOffset = widget.bpm * _pixelsPerBpm;
  }

  @override
  void didUpdateWidget(BpmRulerSelector oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Sync offset if BPM changed externally (e.g. practice mode automation)
    if (widget.bpm != oldWidget.bpm) {
      _currentOffset = widget.bpm * _pixelsPerBpm;
    }
  }

  void _handleDragUpdate(DragUpdateDetails details) {
    setState(() {
      // Invert delta because dragging left moves ruler right (increases BPM)
      _currentOffset -= details.primaryDelta!;

      final minOffset = widget.minBpm * _pixelsPerBpm;
      final maxOffset = widget.maxBpm * _pixelsPerBpm;
      _currentOffset = _currentOffset.clamp(minOffset, maxOffset);

      final newBpm = (_currentOffset / _pixelsPerBpm).roundToDouble();
      if (newBpm != widget.bpm) {
        widget.onChanged(newBpm);
      }
    });
  }

  void _handleDragEnd(DragEndDetails details) {
    final newBpm = (_currentOffset / _pixelsPerBpm).roundToDouble();
    widget.onChangeEnd?.call(newBpm);
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return GestureDetector(
      onHorizontalDragUpdate: _handleDragUpdate,
      onHorizontalDragEnd: _handleDragEnd,
      behavior: HitTestBehavior.opaque,
      child: SizedBox(
        height: 110,
        width: double.infinity,
        child: CustomPaint(
          painter: _RulerPainter(
            offset: _currentOffset,
            pixelsPerBpm: _pixelsPerBpm,
            minBpm: widget.minBpm,
            maxBpm: widget.maxBpm,
            color: theme.colorScheme.onSurface,
            accentColor: theme.colorScheme.primary,
          ),
        ),
      ),
    );
  }
}

class _RulerPainter extends CustomPainter {
  final double offset;
  final double pixelsPerBpm;
  final double minBpm;
  final double maxBpm;
  final Color color;
  final Color accentColor;

  _RulerPainter({
    required this.offset,
    required this.pixelsPerBpm,
    required this.minBpm,
    required this.maxBpm,
    required this.color,
    required this.accentColor,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color.withValues(alpha: 0.3)
      ..strokeWidth = 1.0;

    final centerX = size.width / 2;
    final topY = 24.0;
    final bottomY = size.height - 24.0;
    final midY = size.height / 2;

    // Draw background fading gradient
    final rect = Offset.zero & size;
    final gradient = LinearGradient(
      colors: [
        Colors.transparent,
        color.withValues(alpha: 0.05),
        color.withValues(alpha: 0.05),
        Colors.transparent,
      ],
      stops: const [0.0, 0.2, 0.8, 1.0],
    ).createShader(rect);
    canvas.drawRect(rect, Paint()..shader = gradient);

    final startBpm = ((offset - centerX) / pixelsPerBpm).floor();
    final endBpm = ((offset + centerX) / pixelsPerBpm).ceil();

    for (int i = startBpm; i <= endBpm; i++) {
      if (i < minBpm || i > maxBpm) continue;

      final x = centerX + (i * pixelsPerBpm) - offset;

      if (i % 10 == 0) {
        // Major tick
        paint.color = color.withValues(alpha: 0.85);
        paint.strokeWidth = 2.0;
        canvas.drawLine(Offset(x, topY), Offset(x, bottomY), paint);

        // Label
        final textPainter = TextPainter(
          text: TextSpan(
            text: '$i',
            style: TextStyle(
              color: color.withValues(alpha: 0.7),
              fontSize: 12,
              fontWeight: FontWeight.bold,
              letterSpacing: 1.0,
            ),
          ),
          textDirection: TextDirection.ltr,
        );
        textPainter.layout();
        textPainter.paint(
          canvas,
          Offset(x - textPainter.width / 2, bottomY + 4),
        );
      } else if (i % 5 == 0) {
        // Medium tick
        paint.color = color.withValues(alpha: 0.5);
        paint.strokeWidth = 1.5;
        canvas.drawLine(Offset(x, midY - 15), Offset(x, midY + 15), paint);
      } else {
        // Minor tick
        paint.color = color.withValues(alpha: 0.2);
        paint.strokeWidth = 1.0;
        canvas.drawLine(Offset(x, midY - 8), Offset(x, midY + 8), paint);
      }
    }

    // Center indicator glow
    final glowPaint = Paint()
      ..color = accentColor.withValues(alpha: 0.3)
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 8);
    canvas.drawLine(
      Offset(centerX, 0),
      Offset(centerX, size.height - 12),
      glowPaint..strokeWidth = 6.0,
    );

    // Center indicator line
    final indicatorPaint = Paint()
      ..color = accentColor
      ..strokeWidth = 3.0
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(
      Offset(centerX, 0),
      Offset(centerX, size.height - 12),
      indicatorPaint,
    );

    // Pointer triangle
    final path = Path()
      ..moveTo(centerX - 8, 0)
      ..lineTo(centerX + 8, 0)
      ..lineTo(centerX, 12)
      ..close();
    canvas.drawPath(path, Paint()..color = accentColor);
  }

  @override
  bool shouldRepaint(covariant _RulerPainter oldDelegate) {
    return oldDelegate.offset != offset ||
        oldDelegate.color != color ||
        oldDelegate.accentColor != accentColor;
  }
}
