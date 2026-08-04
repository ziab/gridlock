import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:wakelock_plus/wakelock_plus.dart';
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

  // Dark status bar to match the app theme
  SystemChrome.setSystemUIOverlayStyle(
    const SystemUiOverlayStyle(
      statusBarColor: Color(0xFF0a0c10),
      statusBarIconBrightness: Brightness.light,
      systemNavigationBarColor: Color(0xFF0a0c10),
      systemNavigationBarIconBrightness: Brightness.light,
    ),
  );

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
        theme: ThemeData(
          brightness: Brightness.dark,
          scaffoldBackgroundColor: const Color(0xFF0a0c10),
          colorScheme: const ColorScheme.dark(
            primary: Color(0xFF00FF88),
            secondary: Color(0xFF38bdf8),
            surface: Color(0xFF141722),
            error: Color(0xFFFF1744),
          ),
          textTheme: GoogleFonts.interTextTheme(ThemeData.dark().textTheme),
          useMaterial3: true,
        ),
        home: const ControlScreen(),
      ),
    );
  }
}
