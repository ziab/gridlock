import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:google_fonts/google_fonts.dart';
import 'app_colors.dart';

abstract final class AppTheme {
  static ThemeData get dark {
    return ThemeData(
      brightness: Brightness.dark,
      scaffoldBackgroundColor: AppColors.bgMain,
      colorScheme: const ColorScheme.dark(
        primary: AppColors.emerald,
        secondary: AppColors.skyBlue,
        surface: AppColors.bgCard,
        error: AppColors.error,
      ),
      textTheme: GoogleFonts.interTextTheme(ThemeData.dark().textTheme),
      useMaterial3: true,
    );
  }

  static const systemUiOverlay = SystemUiOverlayStyle(
    statusBarColor: AppColors.bgMain,
    statusBarIconBrightness: Brightness.light,
    systemNavigationBarColor: AppColors.bgMain,
    systemNavigationBarIconBrightness: Brightness.light,
  );
}
