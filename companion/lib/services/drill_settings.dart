import 'package:shared_preferences/shared_preferences.dart';
import '../constants/app_constants.dart';

/// Remembers the last drill setup on the phone so repeat sessions start
/// where the previous one left off. Null bpm/tolerance mean "suggest".
class DrillSettings {
  final String pattern;
  final int spacing;
  final double? bpm;
  final int kick;
  final double? tolerance;
  final double passThreshold;

  const DrillSettings({
    this.pattern = 'RLK',
    this.spacing = 2,
    this.bpm,
    this.kick = AppConstants.drillKickNote,
    this.tolerance,
    this.passThreshold = AppConstants.drillPassDefault,
  });

  static Future<DrillSettings> load() async {
    final prefs = await SharedPreferences.getInstance();
    return DrillSettings(
      pattern: prefs.getString('drill.pattern') ?? 'RLK',
      spacing: prefs.getInt('drill.spacing') ?? 2,
      bpm: prefs.getDouble('drill.bpm'),
      kick: prefs.getInt('drill.kick') ?? AppConstants.drillKickNote,
      tolerance: prefs.getDouble('drill.tolerance'),
      passThreshold:
          prefs.getDouble('drill.pass') ?? AppConstants.drillPassDefault,
    );
  }

  Future<void> save() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('drill.pattern', pattern);
    await prefs.setInt('drill.spacing', spacing);
    if (bpm != null) {
      await prefs.setDouble('drill.bpm', bpm!);
    }
    await prefs.setInt('drill.kick', kick);
    if (tolerance != null) {
      await prefs.setDouble('drill.tolerance', tolerance!);
    }
    await prefs.setDouble('drill.pass', passThreshold);
  }
}
