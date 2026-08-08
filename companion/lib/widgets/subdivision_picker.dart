import 'package:flutter/material.dart';
import '../constants/app_colors.dart';
import '../constants/app_constants.dart';
import 'segmented_picker.dart';

/// Thin wrapper for click subdivisions.
class SubdivisionPicker extends StatelessWidget {
  final int currentIndex;
  final ValueChanged<int> onChanged;

  const SubdivisionPicker({
    super.key,
    required this.currentIndex,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return SegmentedPicker(
      title: 'CLICK SUBDIVISION',
      options: AppConstants.subdivisionOptions,
      selectedIndex: currentIndex,
      accent: AppColors.skyBlue,
      onChanged: onChanged,
    );
  }
}
