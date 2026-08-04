import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// A large, premium rotary dial for BPM control.
///
/// Supports drag gestures for continuous adjustment and tap-to-edit
/// for precise numeric entry.
class BpmDial extends StatefulWidget {
  final double value;
  final double min;
  final double max;
  final double step;
  final ValueChanged<double> onChanged;

  const BpmDial({
    super.key,
    required this.value,
    this.min = 40.0,
    this.max = 300.0,
    this.step = 1.0,
    required this.onChanged,
  });

  @override
  State<BpmDial> createState() => _BpmDialState();
}

class _BpmDialState extends State<BpmDial> with SingleTickerProviderStateMixin {
  late AnimationController _glowController;
  double _dragStartValue = 0;
  Offset _dragStartPos = Offset.zero;

  @override
  void initState() {
    super.initState();
    _glowController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1500),
    )..repeat(reverse: true);
  }

  @override
  void dispose() {
    _glowController.dispose();
    super.dispose();
  }

  void _onPanStart(DragStartDetails details) {
    _dragStartValue = widget.value;
    _dragStartPos = details.localPosition;
    HapticFeedback.selectionClick();
  }

  void _onPanUpdate(DragUpdateDetails details) {
    // Vertical drag: up = increase, down = decrease
    final dy = _dragStartPos.dy - details.localPosition.dy;
    final sensitivity = (widget.max - widget.min) / 300.0;
    final newValue = (_dragStartValue + dy * sensitivity).clamp(
      widget.min,
      widget.max,
    );
    final snapped = newValue.roundToDouble();
    if ((snapped - widget.value).abs() >= 1.0) {
      widget.onChanged(snapped);
      HapticFeedback.selectionClick();
    }
  }

  void _onTap() async {
    final controller = TextEditingController(
      text: widget.value.round().toString(),
    );
    final result = await showDialog<double>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF1a1d2e),
        title: const Text('Set BPM', style: TextStyle(color: Colors.white)),
        content: TextField(
          controller: controller,
          keyboardType: TextInputType.number,
          autofocus: true,
          style: const TextStyle(color: Colors.white, fontSize: 24),
          decoration: const InputDecoration(
            enabledBorder: OutlineInputBorder(
              borderSide: BorderSide(color: Colors.white24),
            ),
            focusedBorder: OutlineInputBorder(
              borderSide: BorderSide(color: Color(0xFF00FF88)),
            ),
          ),
          onSubmitted: (v) {
            final parsed = double.tryParse(v);
            if (parsed != null) {
              Navigator.pop(context, parsed.roundToDouble());
            }
          },
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
            onPressed: () {
              final parsed = double.tryParse(controller.text);
              if (parsed != null) {
                Navigator.pop(context, parsed.roundToDouble());
              }
            },
            child: const Text(
              'Set',
              style: TextStyle(color: Color(0xFF00FF88)),
            ),
          ),
        ],
      ),
    );
    if (result != null) {
      widget.onChanged(result.clamp(widget.min, widget.max).roundToDouble());
    }
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanStart: _onPanStart,
      onPanUpdate: _onPanUpdate,
      onTap: _onTap,
      child: AnimatedBuilder(
        animation: _glowController,
        builder: (context, child) {
          final glowOpacity = 0.15 + _glowController.value * 0.15;
          return Container(
            width: 160,
            height: 160,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: const RadialGradient(
                colors: [Color(0xFF1e2235), Color(0xFF0f1118)],
                stops: [0.5, 1.0],
              ),
              boxShadow: [
                BoxShadow(
                  color: const Color(0xFF00FF88).withValues(alpha: glowOpacity),
                  blurRadius: 30,
                  spreadRadius: 2,
                ),
                const BoxShadow(
                  color: Color(0x40000000),
                  blurRadius: 20,
                  offset: Offset(0, 8),
                ),
              ],
              border: Border.all(
                color: const Color(0xFF00FF88).withValues(alpha: 0.4),
                width: 2,
              ),
            ),
            child: CustomPaint(
              painter: _BpmArcPainter(
                value: widget.value,
                min: widget.min,
                max: widget.max,
              ),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(
                    widget.value.round().toString(),
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 36,
                      fontWeight: FontWeight.w800,
                      letterSpacing: -1,
                    ),
                  ),
                  const Text(
                    'BPM',
                    style: TextStyle(
                      color: Color(0xFF00FF88),
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                      letterSpacing: 3,
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}

class _BpmArcPainter extends CustomPainter {
  final double value;
  final double min;
  final double max;

  _BpmArcPainter({required this.value, required this.min, required this.max});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2 - 6;
    final ratio = ((value - min) / (max - min)).clamp(0.0, 1.0);

    // Background arc
    final bgPaint = Paint()
      ..color = const Color(0xFF2d3245)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4
      ..strokeCap = StrokeCap.round;

    const startAngle = math.pi * 0.75;
    const sweepAngle = math.pi * 1.5;

    canvas.drawArc(
      Rect.fromCircle(center: center, radius: radius),
      startAngle,
      sweepAngle,
      false,
      bgPaint,
    );

    // Value arc
    final valuePaint = Paint()
      ..shader = const SweepGradient(
        startAngle: 0.75 * math.pi,
        endAngle: 2.25 * math.pi,
        colors: [Color(0xFF00FF88), Color(0xFF38bdf8), Color(0xFF818cf8)],
      ).createShader(Rect.fromCircle(center: center, radius: radius))
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4
      ..strokeCap = StrokeCap.round;

    canvas.drawArc(
      Rect.fromCircle(center: center, radius: radius),
      startAngle,
      sweepAngle * ratio,
      false,
      valuePaint,
    );

    // Indicator dot
    final dotAngle = startAngle + sweepAngle * ratio;
    final dotPos = Offset(
      center.dx + radius * math.cos(dotAngle),
      center.dy + radius * math.sin(dotAngle),
    );
    canvas.drawCircle(dotPos, 5, Paint()..color = Colors.white);
  }

  @override
  bool shouldRepaint(covariant _BpmArcPainter oldDelegate) =>
      oldDelegate.value != value;
}
