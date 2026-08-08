import 'package:flutter/material.dart';
import '../constants/app_colors.dart';
import '../constants/app_constants.dart';
import 'segmented_picker.dart';

/// Thin wrapper around [SegmentedPicker] for time signatures.
class SignaturePicker extends StatelessWidget {
  final int currentNumerator;
  final ValueChanged<int> onChanged;

  const SignaturePicker({
    super.key,
    required this.currentNumerator,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    final labels = AppConstants.timeSignatures.map((e) => e.label).toList();
    final selected = AppConstants.timeSignatures.indexWhere((e) => e.num == currentNumerator);
    return SegmentedPicker(
      title: 'TIME SIGNATURE',
      options: labels,
      selectedIndex: selected < 0 ? 2 : selected,
      accent: AppColors.emerald,
      onChanged: (i) => onChanged(AppConstants.timeSignatures[i].num),
    );
  }
}
