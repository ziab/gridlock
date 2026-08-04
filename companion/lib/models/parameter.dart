/// Data model for a remote APVTS parameter.
///
/// Mirrors the JSON schema sent by the JUCE RemoteControlServer.
class RemoteParameter {
  final String id;
  final String name;
  final String paramType; // 'float', 'int', 'bool', 'choice'
  double value;
  double norm;
  final double min;
  final double max;
  final double step;
  final List<String> options; // non-empty for choice params

  RemoteParameter({
    required this.id,
    required this.name,
    required this.paramType,
    required this.value,
    required this.norm,
    required this.min,
    required this.max,
    required this.step,
    this.options = const [],
  });

  factory RemoteParameter.fromJson(String id, Map<String, dynamic> json) {
    final optionsList = <String>[];
    if (json['options'] != null) {
      for (final opt in json['options'] as List) {
        optionsList.add(opt.toString());
      }
    }

    return RemoteParameter(
      id: id,
      name: json['name']?.toString() ?? id,
      paramType: json['paramType']?.toString() ?? 'float',
      value: (json['value'] as num?)?.toDouble() ?? 0.0,
      norm: (json['norm'] as num?)?.toDouble() ?? 0.0,
      min: (json['min'] as num?)?.toDouble() ?? 0.0,
      max: (json['max'] as num?)?.toDouble() ?? 1.0,
      step: (json['step'] as num?)?.toDouble() ?? 0.01,
      options: optionsList,
    );
  }

  bool get isBool => paramType == 'bool';
  bool get isChoice => paramType == 'choice';
  bool get isFloat => paramType == 'float';
  bool get isInt => paramType == 'int';

  /// For bool params, convenience getter
  bool get boolValue => value > 0.5;
}
