import 'package:flutter/material.dart';

class PracticeTimerRulerDisplay extends StatefulWidget {
  final int totalDurationSeconds;
  final int remainingSeconds;
  final ValueChanged<int>? onDurationChanged;
  final bool isInteractive;

  const PracticeTimerRulerDisplay({
    super.key,
    required this.totalDurationSeconds,
    required this.remainingSeconds,
    this.onDurationChanged,
    this.isInteractive = false,
  });

  @override
  State<PracticeTimerRulerDisplay> createState() =>
      _PracticeTimerRulerDisplayState();
}

class _PracticeTimerRulerDisplayState extends State<PracticeTimerRulerDisplay> {
  late double _currentOffset;
  final double _pixelsPerMinute = 25.0; // Control sensitivity

  @override
  void initState() {
    super.initState();
    _currentOffset = (widget.totalDurationSeconds / 60.0) * _pixelsPerMinute;
  }

  @override
  void didUpdateWidget(PracticeTimerRulerDisplay oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!widget.isInteractive &&
        widget.remainingSeconds != oldWidget.remainingSeconds) {
      _currentOffset = (widget.remainingSeconds / 60.0) * _pixelsPerMinute;
    } else if (widget.isInteractive &&
        widget.totalDurationSeconds != oldWidget.totalDurationSeconds) {
      _currentOffset = (widget.totalDurationSeconds / 60.0) * _pixelsPerMinute;
    }
  }

  void _handleDragUpdate(DragUpdateDetails details) {
    if (!widget.isInteractive || widget.onDurationChanged == null) return;
    setState(() {
      _currentOffset -= details.primaryDelta!;

      final minOffset = 1.0 * _pixelsPerMinute; // 1 min min
      final maxOffset = 60.0 * _pixelsPerMinute; // 60 mins max
      _currentOffset = _currentOffset.clamp(minOffset, maxOffset);

      final newMinutes = (_currentOffset / _pixelsPerMinute).round();
      final newSecs = newMinutes * 60;
      if (newSecs != widget.totalDurationSeconds) {
        widget.onDurationChanged!(newSecs);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return GestureDetector(
      onHorizontalDragUpdate: _handleDragUpdate,
      behavior: HitTestBehavior.opaque,
      child: SizedBox(
        height: 100,
        width: double.infinity,
        child: CustomPaint(
          painter: _TimerRulerPainter(
            offset: _currentOffset,
            pixelsPerMinute: _pixelsPerMinute,
            color: theme.colorScheme.onSurface,
            accentColor: const Color(0xFF38bdf8),
            isInteractive: widget.isInteractive,
          ),
        ),
      ),
    );
  }
}

class _TimerRulerPainter extends CustomPainter {
  final double offset;
  final double pixelsPerMinute;
  final Color color;
  final Color accentColor;
  final bool isInteractive;

  _TimerRulerPainter({
    required this.offset,
    required this.pixelsPerMinute,
    required this.color,
    required this.accentColor,
    required this.isInteractive,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color.withValues(alpha: 0.3)
      ..strokeWidth = 1.0;

    final centerX = size.width / 2;
    final topY = 20.0;
    final bottomY = size.height - 24.0;
    final midY = size.height / 2;

    // Background gradient fade
    final rect = Offset.zero & size;
    final gradient = LinearGradient(
      colors: [
        Colors.transparent,
        accentColor.withValues(alpha: 0.05),
        accentColor.withValues(alpha: 0.05),
        Colors.transparent,
      ],
      stops: const [0.0, 0.2, 0.8, 1.0],
    ).createShader(rect);
    canvas.drawRect(rect, Paint()..shader = gradient);

    final startMin = ((offset - centerX) / pixelsPerMinute).floor();
    final endMin = ((offset + centerX) / pixelsPerMinute).ceil();

    for (int i = startMin; i <= endMin; i++) {
      if (i < 0 || i > 60) continue;

      final x = centerX + (i * pixelsPerMinute) - offset;

      if (i % 5 == 0) {
        // Major tick (5m intervals)
        paint.color = accentColor.withValues(alpha: 0.85);
        paint.strokeWidth = 2.0;
        canvas.drawLine(Offset(x, topY), Offset(x, bottomY), paint);

        final textPainter = TextPainter(
          text: TextSpan(
            text: '${i}m',
            style: TextStyle(
              color: accentColor.withValues(alpha: 0.8),
              fontSize: 11,
              fontWeight: FontWeight.bold,
            ),
          ),
          textDirection: TextDirection.ltr,
        );
        textPainter.layout();
        textPainter.paint(
          canvas,
          Offset(x - textPainter.width / 2, bottomY + 2),
        );
      } else {
        // Minor tick (1m intervals)
        paint.color = color.withValues(alpha: 0.3);
        paint.strokeWidth = 1.0;
        canvas.drawLine(Offset(x, midY - 10), Offset(x, midY + 10), paint);
      }
    }

    // Glow effect
    final glowPaint = Paint()
      ..color = accentColor.withValues(alpha: 0.35)
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 8);
    canvas.drawLine(
      Offset(centerX, 0),
      Offset(centerX, size.height - 12),
      glowPaint..strokeWidth = 6.0,
    );

    // Center marker line
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
  bool shouldRepaint(covariant _TimerRulerPainter oldDelegate) {
    return oldDelegate.offset != offset ||
        oldDelegate.color != color ||
        oldDelegate.accentColor != accentColor;
  }
}
