import 'package:flutter_test/flutter_test.dart';
import 'package:gridlock_companion/models/parameter.dart';
import 'package:gridlock_companion/services/connection_service.dart';

void main() {
  group('RemoteParameter', () {
    test('fromJson parses float parameter correctly', () {
      final json = {
        'name': 'Internal BPM',
        'paramType': 'float',
        'value': 120.0,
        'norm': 0.307,
        'min': 40.0,
        'max': 300.0,
        'step': 0.1,
      };

      final param = RemoteParameter.fromJson('internal_bpm', json);

      expect(param.id, equals('internal_bpm'));
      expect(param.name, equals('Internal BPM'));
      expect(param.paramType, equals('float'));
      expect(param.value, equals(120.0));
      expect(param.min, equals(40.0));
      expect(param.max, equals(300.0));
      expect(param.isFloat, isTrue);
      expect(param.isBool, isFalse);
    });

    test('fromJson parses choice parameter correctly', () {
      final json = {
        'name': 'Grid Subdivision',
        'paramType': 'choice',
        'value': 2.0,
        'norm': 0.5,
        'min': 0.0,
        'max': 4.0,
        'step': 1.0,
        'options': ['1/8', '1/8T', '1/16', '1/16T', '1/32'],
      };

      final param = RemoteParameter.fromJson('subdivision', json);

      expect(param.isChoice, isTrue);
      expect(param.options.length, equals(5));
      expect(param.options[2], equals('1/16'));
    });

    test('boolValue convenience getter works', () {
      final paramTrue = RemoteParameter(
        id: 'click_enabled',
        name: 'Metronome',
        paramType: 'bool',
        value: 1.0,
        norm: 1.0,
        min: 0.0,
        max: 1.0,
        step: 1.0,
      );

      final paramFalse = RemoteParameter(
        id: 'click_enabled',
        name: 'Metronome',
        paramType: 'bool',
        value: 0.0,
        norm: 0.0,
        min: 0.0,
        max: 1.0,
        step: 1.0,
      );

      expect(paramTrue.boolValue, isTrue);
      expect(paramFalse.boolValue, isFalse);
    });
  });

  group('ConnectionService State', () {
    test('initial state is disconnected', () {
      final service = ConnectionService();
      expect(service.isConnected, isFalse);
      expect(service.serverAddress, equals('Not connected'));
      expect(service.parameters, isEmpty);
    });
  });
}
