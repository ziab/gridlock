import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../constants/app_colors.dart';
import '../services/connection_service.dart';

class StatusBar extends StatelessWidget {
  final ConnectionService connection;
  final VoidCallback onClearGrid;
  final VoidCallback onOptions;
  final VoidCallback onRefresh;

  const StatusBar({
    super.key,
    required this.connection,
    required this.onClearGrid,
    required this.onOptions,
    required this.onRefresh,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      color: AppColors.bgHeader,
      child: Row(
        children: [
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: connection.isConnected ? AppColors.emeraldDark : AppColors.error,
              boxShadow: [
                BoxShadow(
                    color: (connection.isConnected ? AppColors.emeraldDark : AppColors.error)
                        .withValues(alpha: 0.5),
                    blurRadius: 6),
              ],
            ),
          ),
          const SizedBox(width: 10),
          Text(
            connection.isConnected ? 'Connected to ${connection.serverAddress}' : 'Disconnected',
            style: const TextStyle(color: AppColors.textMuted, fontSize: 11, fontWeight: FontWeight.w500),
          ),
          const Spacer(),
          IconButton(
            tooltip: 'Clear Grid',
            icon: const Icon(Icons.cleaning_services_rounded, color: AppColors.emerald, size: 18),
            onPressed: () {
              HapticFeedback.mediumImpact();
              onClearGrid();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Grid cleared'), duration: Duration(seconds: 1)),
              );
            },
          ),
          IconButton(
            tooltip: 'Options',
            icon: const Icon(Icons.tune_rounded, color: AppColors.skyBlue, size: 18),
            onPressed: onOptions,
          ),
          GestureDetector(
            onTap: onRefresh,
            child: const Icon(Icons.refresh_rounded, color: AppColors.borderFaint, size: 18),
          ),
        ],
      ),
    );
  }
}
