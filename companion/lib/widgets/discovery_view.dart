import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../constants/app_colors.dart';

class DiscoveryView extends StatelessWidget {
  final bool discovering;
  final String? errorMessage;
  final Animation<double> pulse;
  final VoidCallback onRetry;
  final VoidCallback onManual;

  const DiscoveryView({
    super.key,
    required this.discovering,
    required this.errorMessage,
    required this.pulse,
    required this.onRetry,
    required this.onManual,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.bgMain,
      body: SafeArea(
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              AnimatedBuilder(
                animation: pulse,
                builder: (context, child) {
                  final scale = 1.0 + pulse.value * 0.08;
                  return Transform.scale(
                    scale: scale,
                    child: Container(
                      width: 80,
                      height: 80,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        gradient: const RadialGradient(
                          colors: [Color(0xFF1e2235), Color(0xFF0f1118)],
                        ),
                        border: Border.all(color: AppColors.emerald.withValues(alpha: 0.4), width: 2),
                        boxShadow: [
                          BoxShadow(color: AppColors.emerald.withValues(alpha: 0.2), blurRadius: 20),
                        ],
                      ),
                      child: const Icon(Icons.grid_on_rounded, color: AppColors.emerald, size: 36),
                    ),
                  );
                },
              ),
              const SizedBox(height: 32),
              const Text('GRIDLOCK',
                  style: TextStyle(
                      color: Colors.white, fontSize: 28, fontWeight: FontWeight.w800, letterSpacing: 6)),
              const SizedBox(height: 4),
              const Text('COMPANION',
                  style: TextStyle(
                      color: AppColors.textMuted, fontSize: 12, fontWeight: FontWeight.w600, letterSpacing: 4)),
              const SizedBox(height: 40),
              if (discovering) ...[
                const SizedBox(
                    width: 24,
                    height: 24,
                    child: CircularProgressIndicator(
                        strokeWidth: 2, valueColor: AlwaysStoppedAnimation<Color>(AppColors.emerald))),
                const SizedBox(height: 16),
                const Text('Searching for Gridlock on your network…',
                    style: TextStyle(color: AppColors.textFaint, fontSize: 14)),
              ],
              if (errorMessage != null) ...[
                Container(
                  margin: const EdgeInsets.symmetric(horizontal: 32),
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    color: AppColors.error.withValues(alpha: 0.1),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: AppColors.error.withValues(alpha: 0.3)),
                  ),
                  child: Text(errorMessage!,
                      textAlign: TextAlign.center,
                      style: const TextStyle(color: AppColors.error, fontSize: 13)),
                ),
                const SizedBox(height: 20),
              ],
              if (!discovering) ...[
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    _RetryButton(onRetry: onRetry),
                    const SizedBox(width: 12),
                    _ManualButton(onManual: onManual),
                  ],
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _RetryButton extends StatelessWidget {
  final VoidCallback onRetry;
  const _RetryButton({required this.onRetry});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: () {
        HapticFeedback.mediumImpact();
        onRetry();
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
        decoration: BoxDecoration(
          color: AppColors.emerald.withValues(alpha: 0.12),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: AppColors.emerald.withValues(alpha: 0.4), width: 1.5),
        ),
        child: const Text('SCAN AGAIN',
            style: TextStyle(
                color: AppColors.emerald, fontSize: 13, fontWeight: FontWeight.w700, letterSpacing: 2)),
      ),
    );
  }
}

class _ManualButton extends StatelessWidget {
  final VoidCallback onManual;
  const _ManualButton({required this.onManual});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onManual,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
        decoration: BoxDecoration(
          color: AppColors.bgChipActive,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: AppColors.skyBlue.withValues(alpha: 0.4), width: 1.5),
        ),
        child: const Text('MANUAL IP',
            style: TextStyle(
                color: AppColors.skyBlue, fontSize: 13, fontWeight: FontWeight.w700, letterSpacing: 1.5)),
      ),
    );
  }
}
