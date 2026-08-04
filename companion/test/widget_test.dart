import 'package:flutter_test/flutter_test.dart';
import 'package:gridlock_companion/main.dart';

void main() {
  testWidgets('Gridlock Companion App renders initial discovery screen', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(const GridlockCompanionApp());
    expect(find.text('GRIDLOCK'), findsOneWidget);
    expect(find.text('COMPANION'), findsOneWidget);

    // Fast-forward pending timers to settle discovery service
    await tester.pump(const Duration(seconds: 11));
  });
}
