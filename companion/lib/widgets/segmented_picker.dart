import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../constants/app_colors.dart';

/// Generic segmented picker — extracts 90% duplicated layout from
/// `SignaturePicker` and `SubdivisionPicker`.
class SegmentedPicker extends StatelessWidget {
  final String title;
  final List<String> options;
  final int selectedIndex;
  final ValueChanged<int> onChanged;
  final Color accent;

  const SegmentedPicker({
    super.key,
    required this.title,
    required this.options,
    required this.selectedIndex,
    required this.onChanged,
    this.accent = AppColors.emerald,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          title,
          style: const TextStyle(
            color: AppColors.textMuted,
            fontSize: 10,
            fontWeight: FontWeight.w700,
            letterSpacing: 2.5,
          ),
        ),
        const SizedBox(height: 10),
        Container(
          decoration: BoxDecoration(
            color: AppColors.bgChip,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: AppColors.border, width: 1.5),
          ),
          padding: const EdgeInsets.all(4),
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            physics: const BouncingScrollPhysics(),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: List.generate(options.length, (i) {
                final isActive = i == selectedIndex;
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
                      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
                      decoration: BoxDecoration(
                        color: isActive ? accent.withValues(alpha: 0.15) : Colors.transparent,
                        borderRadius: BorderRadius.circular(10),
                        border: Border.all(
                          color: isActive ? accent.withValues(alpha: 0.5) : Colors.transparent,
                          width: 1.5,
                        ),
                        boxShadow: isActive
                            ? [BoxShadow(color: accent.withValues(alpha: 0.15), blurRadius: 12)]
                            : null,
                      ),
                      child: Text(
                        options[i],
                        style: TextStyle(
                          color: isActive ? accent : AppColors.textFaint,
                          fontSize: options[i].length > 4 ? 16 : 18,
                          fontWeight: isActive ? FontWeight.w800 : FontWeight.w500,
                        ),
                      ),
                    ),
                  ),
                );
              }),
            ),
          ),
        ),
      ],
    );
  }
}
