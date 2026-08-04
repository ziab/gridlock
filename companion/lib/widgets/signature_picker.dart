import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Large segmented time signature picker.
///
/// Displays common time signatures as tappable chips with the active
/// one highlighted in the accent color.
class SignaturePicker extends StatelessWidget {
  final int currentNumerator;
  final ValueChanged<int> onChanged;

  const SignaturePicker({
    super.key,
    required this.currentNumerator,
    required this.onChanged,
  });

  static const _signatures = [
    (num: 2, label: '2/4'),
    (num: 3, label: '3/4'),
    (num: 4, label: '4/4'),
    (num: 5, label: '5/4'),
    (num: 6, label: '6/8'),
    (num: 7, label: '7/8'),
  ];

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        const Text(
          'TIME SIGNATURE',
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
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: _signatures.map((sig) {
              final isActive = sig.num == currentNumerator;
              return Padding(
                padding: const EdgeInsets.symmetric(horizontal: 2),
                child: GestureDetector(
                  onTap: () {
                    HapticFeedback.mediumImpact();
                    onChanged(sig.num);
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
                          ? const Color(0xFF00FF88).withValues(alpha: 0.15)
                          : Colors.transparent,
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: isActive
                            ? const Color(0xFF00FF88).withValues(alpha: 0.5)
                            : Colors.transparent,
                        width: 1.5,
                      ),
                      boxShadow: isActive
                          ? [
                              BoxShadow(
                                color: const Color(
                                  0xFF00FF88,
                                ).withValues(alpha: 0.15),
                                blurRadius: 12,
                              ),
                            ]
                          : null,
                    ),
                    child: Text(
                      sig.label,
                      style: TextStyle(
                        color: isActive
                            ? const Color(0xFF00FF88)
                            : const Color(0xFF6b7280),
                        fontSize: 18,
                        fontWeight: isActive
                            ? FontWeight.w800
                            : FontWeight.w500,
                      ),
                    ),
                  ),
                ),
              );
            }).toList(),
          ),
        ),
      ],
    );
  }
}
