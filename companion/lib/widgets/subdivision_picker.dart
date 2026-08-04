import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Large segmented click subdivision picker.
///
/// Choices: Off, 1/4, 1/8, 1/16, Triplets.
class SubdivisionPicker extends StatelessWidget {
  /// The current choice index (0-based).
  final int currentIndex;
  final ValueChanged<int> onChanged;

  const SubdivisionPicker({
    super.key,
    required this.currentIndex,
    required this.onChanged,
  });

  static const _options = ['Off', '1/4', '1/8', '1/16', 'Trip'];

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        const Text(
          'CLICK SUBDIVISION',
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
            border: Border.all(
              color: const Color(0xFF2d3245),
              width: 1.5,
            ),
          ),
          padding: const EdgeInsets.all(4),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: List.generate(_options.length, (i) {
              final isActive = i == currentIndex;
              return Padding(
                padding: const EdgeInsets.symmetric(horizontal: 2),
                child: GestureDetector(
                  onTap: () {
                    HapticFeedback.mediumImpact();
                    onChanged(i);
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
                          ? const Color(0xFF38bdf8).withValues(alpha: 0.15)
                          : Colors.transparent,
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: isActive
                            ? const Color(0xFF38bdf8).withValues(alpha: 0.5)
                            : Colors.transparent,
                        width: 1.5,
                      ),
                      boxShadow: isActive
                          ? [
                              BoxShadow(
                                color: const Color(0xFF38bdf8)
                                    .withValues(alpha: 0.15),
                                blurRadius: 12,
                              ),
                            ]
                          : null,
                    ),
                    child: Text(
                      _options[i],
                      style: TextStyle(
                        color: isActive
                            ? const Color(0xFF38bdf8)
                            : const Color(0xFF6b7280),
                        fontSize: 16,
                        fontWeight:
                            isActive ? FontWeight.w800 : FontWeight.w500,
                      ),
                    ),
                  ),
                ),
              );
            }),
          ),
        ),
      ],
    );
  }
}
