import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:wakelock_plus/wakelock_plus.dart';
import 'constants/app_theme.dart';
import 'services/connection_service.dart';
import 'screens/control_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();

  // Prevent display sleep while using the companion app on drum throne
  WakelockPlus.enable();

  // Lock to portrait for a focused drum-throne experience
  SystemChrome.setPreferredOrientations([
    DeviceOrientation.portraitUp,
    DeviceOrientation.portraitDown,
  ]);

  SystemChrome.setSystemUIOverlayStyle(AppTheme.systemUiOverlay);

  runApp(const GridlockCompanionApp());
}

class GridlockCompanionApp extends StatelessWidget {
  const GridlockCompanionApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => ConnectionService(),
      child: MaterialApp(
        title: 'Gridlock Companion',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.dark,
        home: const ControlScreen(),
      ),
    );
  }
}
