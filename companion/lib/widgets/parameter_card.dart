import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// A reusable, dark-themed card for secondary parameters.
///
/// Supports slider, toggle, and popup choice variants based on [paramType].
class ParameterCard extends StatelessWidget {
  final String label;
  final String paramType; // 'float', 'int', 'bool', 'choice'
  final double value;
  final double min;
  final double max;
  final double step;
  final List<String> options;
  final ValueChanged<double> onChanged;
  final ValueChanged<double>? onChangeEnd;
  final String? suffix;

  const ParameterCard({
    super.key,
    required this.label,
    required this.paramType,
    required this.value,
    this.min = 0,
    this.max = 1,
    this.step = 0.01,
    this.options = const [],
    required this.onChanged,
    this.onChangeEnd,
    this.suffix,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF141722),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: const Color(0xFF252a3a), width: 1),
      ),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          // Header row
          Row(
            children: [
              Expanded(
                child: Text(
                  label.toUpperCase(),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(
                    color: Color(0xFF8b92a8),
                    fontSize: 10,
                    fontWeight: FontWeight.w700,
                    letterSpacing: 1.5,
                  ),
                ),
              ),
              if (paramType != 'bool' && paramType != 'choice')
                Text(
                  _formatValue(),
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                  ),
                ),
            ],
          ),
          const SizedBox(height: 4),
          // Control
          Expanded(
            child: Align(
              alignment: Alignment.centerLeft,
              child: _buildControl(),
            ),
          ),
        ],
      ),
    );
  }

  String _formatValue() {
    if (paramType == 'int') {
      return '${value.round()}${suffix ?? ''}';
    }
    if (step >= 1.0) {
      return '${value.round()}${suffix ?? ''}';
    }
    return '${value.toStringAsFixed(1)}${suffix ?? ''}';
  }

  Widget _buildControl() {
    switch (paramType) {
      case 'bool':
        return _buildToggle();
      case 'choice':
        return _buildChoiceSelector();
      default:
        return _buildSlider();
    }
  }

  Widget _buildSlider() {
    final rawDivisions = (step > 0 && max > min)
        ? ((max - min) / step).round()
        : 0;
    final divisions =
        (paramType == 'int' && rawDivisions > 0 && rawDivisions <= 200)
        ? rawDivisions
        : null;

    return SliderTheme(
      data: SliderThemeData(
        activeTrackColor: const Color(0xFF00FF88),
        inactiveTrackColor: const Color(0xFF2d3245),
        thumbColor: Colors.white,
        overlayColor: const Color(0xFF00FF88).withValues(alpha: 0.15),
        trackHeight: 4,
        thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 7),
      ),
      child: Slider(
        value: value.clamp(min, max),
        min: min,
        max: max,
        divisions: divisions,
        onChanged: (v) {
          HapticFeedback.selectionClick();
          onChanged(v);
        },
        onChangeEnd: (v) {
          onChangeEnd?.call(v);
        },
      ),
    );
  }

  Widget _buildToggle() {
    final isOn = value > 0.5;
    return GestureDetector(
      onTap: () {
        HapticFeedback.mediumImpact();
        onChanged(isOn ? 0.0 : 1.0);
        onChangeEnd?.call(isOn ? 0.0 : 1.0);
      },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 250),
        curve: Curves.easeOut,
        height: 38,
        decoration: BoxDecoration(
          color: isOn
              ? const Color(0xFF00c853).withValues(alpha: 0.2)
              : const Color(0xFF2d3245).withValues(alpha: 0.3),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: isOn
                ? const Color(0xFF00c853).withValues(alpha: 0.5)
                : const Color(0xFF2d3245),
            width: 1.5,
          ),
        ),
        alignment: Alignment.center,
        child: Text(
          isOn ? 'ON' : 'OFF',
          style: TextStyle(
            color: isOn ? const Color(0xFF00c853) : const Color(0xFF6b7280),
            fontSize: 13,
            fontWeight: FontWeight.w700,
            letterSpacing: 1.5,
          ),
        ),
      ),
    );
  }

  Widget _buildChoiceSelector() {
    final selectedIndex = value.round().clamp(0, options.length - 1);
    final currentLabel = options.isNotEmpty
        ? options[selectedIndex]
        : 'Select…';

    return PopupMenuButton<int>(
      onSelected: (int newIndex) {
        HapticFeedback.selectionClick();
        onChanged(newIndex.toDouble());
        onChangeEnd?.call(newIndex.toDouble());
      },
      color: const Color(0xFF1e2235),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: const BorderSide(color: Color(0xFF2d3245)),
      ),
      itemBuilder: (context) {
        return List.generate(options.length, (i) {
          final isSelected = i == selectedIndex;
          return PopupMenuItem<int>(
            value: i,
            child: Row(
              children: [
                Icon(
                  isSelected
                      ? Icons.check_circle_rounded
                      : Icons.circle_outlined,
                  color: isSelected
                      ? const Color(0xFF38bdf8)
                      : const Color(0xFF6b7280),
                  size: 16,
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    options[i],
                    style: TextStyle(
                      color: isSelected
                          ? Colors.white
                          : const Color(0xFF94a3b8),
                      fontSize: 13,
                      fontWeight: isSelected
                          ? FontWeight.w700
                          : FontWeight.w500,
                    ),
                  ),
                ),
              ],
            ),
          );
        });
      },
      child: Container(
        height: 38,
        padding: const EdgeInsets.symmetric(horizontal: 12),
        decoration: BoxDecoration(
          color: const Color(0xFF1a1d2e),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: const Color(0xFF38bdf8).withValues(alpha: 0.4),
            width: 1,
          ),
        ),
        child: Row(
          children: [
            Expanded(
              child: Text(
                currentLabel,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  color: Color(0xFF38bdf8),
                  fontSize: 12,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ),
            const Icon(
              Icons.arrow_drop_down_rounded,
              color: Color(0xFF38bdf8),
              size: 20,
            ),
          ],
        ),
      ),
    );
  }
}
