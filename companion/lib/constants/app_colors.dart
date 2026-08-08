import 'package:flutter/material.dart';

/// Central color palette — mirrors JUCE Theme.h + Gridlock dark theme.
/// Single source for `0xFF0a0c10` etc. scattered across widgets.
abstract final class AppColors {
  // Backgrounds
  static const bgMain = Color(0xFF0a0c10);
  static const bgHeader = Color(0xFF181b24);
  static const bgCard = Color(0xFF141722);
  static const bgChip = Color(0xFF0f1118);
  static const bgChipActive = Color(0xFF1e2235);
  static const bgInput = Color(0xFF1a1d2e);

  // Borders
  static const border = Color(0xFF2d3245);
  static const borderLight = Color(0xFF252a3a);
  static const borderFaint = Color(0xFF4b5267);

  // Accents
  static const emerald = Color(0xFF00FF88);
  static const emeraldDark = Color(0xFF00c853);
  static const skyBlue = Color(0xFF38bdf8);
  static const purple = Color(0xFFA855F7);
  static const lavender = Color(0xFF818cf8);

  // Text
  static const textPrimary = Colors.white;
  static const textMuted = Color(0xFF8b92a8);
  static const textFaint = Color(0xFF6b7280);
  static const textLabel = Color(0xFF94a3b8);

  // Semantic
  static const error = Color(0xFFFF1744);
  static const warning = Color(0xFFFACC15);
}
