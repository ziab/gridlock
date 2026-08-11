#pragma once

#include "Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <mutex>
#include <vector>

/**
 * RemoteControlServer — enables a Flutter companion app on the same LAN to
 * discover this running standalone instance and remote-control its APVTS
 * parameters over a lightweight WebSocket (RFC 6455) connection.
 *
 * Discovery:  UDP broadcast listener on port 9877.
 *             Responds to "GRIDLOCK_DISCOVER" with "GRIDLOCK_HERE:<wsPort>".
 *
 * Control:    WebSocket server on port 9876.
 *             On connect   → sends full parameter state snapshot (JSON).
 *             On receive   → {"type":"set","id":"<paramId>","value":<float>}
 *             On param Δ   → pushes {"type":"changed","id":"<paramId>","value":<float>}
 *             Calibration  → {"type":"calibrate"} / {"type":"calibration_apply","add":bool} /
 * {"type":"calibration_cancel"} Calibration  ← pushes {"type":"calibration","state":"idle|countin|recording|done",...}
 *             Heartbeat    → pushes {"type":"ping"} every 2 s.
 */
class RemoteControlServer : private juce::Thread, private juce::Timer {
public:
  explicit RemoteControlServer (juce::AudioProcessorValueTreeState &apvts, int wsPort = constants::network::wsPort,
                                int udpPort = constants::network::udpPort);
  ~RemoteControlServer () override;

  void start ();
  void stop ();

  // Calibration bridge (set by PluginProcessor to avoid circular include)
  void setCalibrationCallbacks (std::function<void ()> onCalibrate, std::function<void (bool)> onApply,
                                std::function<void ()> onCancel, std::function<juce::String ()> getCalibJson);

private:
  // ── Thread (WebSocket accept loop) ──────────────────────────────
  void run () override;

  // ── Timer (parameter change polling + heartbeat) ────────────────
  void timerCallback () override;

  // ── UDP Discovery ───────────────────────────────────────────────
  void runDiscoveryListener ();

  // ── WebSocket helpers ───────────────────────────────────────────
  bool performWebSocketHandshake (juce::StreamingSocket &client);
  juce::String readWebSocketTextFrame (juce::StreamingSocket &client);
  bool sendWebSocketTextFrame (juce::StreamingSocket &client, const juce::String &text);
  void handleClientMessage (juce::StreamingSocket &client, const juce::String &message);
  juce::String buildFullStateJson () const;
  void broadcastToClients (const juce::String &json);

  // ── Members ─────────────────────────────────────────────────────
  juce::AudioProcessorValueTreeState &apvts;

  int wsPort;
  int udpPort;

  juce::StreamingSocket serverSocket;

  struct ConnectedClient {
    std::unique_ptr<juce::StreamingSocket> socket;
  };
  std::vector<ConnectedClient> clients;
  std::mutex clientsMutex;

  // Snapshot of last-sent parameter values for change detection
  std::map<juce::String, float> lastParamValues;

  // Calibration callbacks
  std::function<void ()> onCalibrate;
  std::function<void (bool)> onCalibrationApply;
  std::function<void ()> onCalibrationCancel;
  std::function<juce::String ()> getCalibrationJson;

  // UDP discovery runs on its own thread
  std::unique_ptr<juce::Thread> discoveryThread;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RemoteControlServer)
};
