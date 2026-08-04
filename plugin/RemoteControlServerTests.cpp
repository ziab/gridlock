#include "RemoteControlServer.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <thread>

// Minimal dummy AudioProcessor for APVTS testing
class DummyAudioProcessor : public juce::AudioProcessor
{
  public:
    DummyAudioProcessor ()
        : AudioProcessor (BusesProperties ().withOutput ("Output", juce::AudioChannelSet::stereo (), true))
    {
    }

    const juce::String getName () const override
    {
        return "DummyProcessor";
    }
    void prepareToPlay (double, int) override {}
    void releaseResources () override {}
    void processBlock (juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
    juce::AudioProcessorEditor *createEditor () override
    {
        return nullptr;
    }
    bool hasEditor () const override
    {
        return false;
    }
    bool acceptsMidi () const override
    {
        return false;
    }
    bool producesMidi () const override
    {
        return false;
    }
    double getTailLengthSeconds () const override
    {
        return 0.0;
    }
    int getNumPrograms () override
    {
        return 1;
    }
    int getCurrentProgram () override
    {
        return 0;
    }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override
    {
        return {};
    }
    void changeProgramName (int, const juce::String &) override {}
    void getStateInformation (juce::MemoryBlock &) override {}
    void setStateInformation (const void *, int) override {}
};

static juce::AudioProcessorValueTreeState::ParameterLayout createTestLayout ()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{"internal_bpm", 1}, "Internal BPM",
                                                                   juce::NormalisableRange<float> (40.0f, 300.0f, 0.1f),
                                                                   120.0f));

    params.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID{"time_sig_num", 1},
                                                                 "Time Sig Numerator", 2, 12, 4));

    params.push_back (
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID{"subdivision", 1}, "Grid Subdivision",
                                                      juce::StringArray{"1/8", "1/8T", "1/16", "1/16T", "1/32"}, 2));

    return {params.begin (), params.end ()};
}

int main (int argc, char *argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "=== Running RemoteControlServer Integration Tests ===" << std::endl;

    const int testWsPort = 9886;
    const int testUdpPort = 9887;

    DummyAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts (processor, nullptr, "TestParams", createTestLayout ());

    RemoteControlServer server (apvts, testWsPort, testUdpPort);
    server.start ();

    std::this_thread::sleep_for (std::chrono::milliseconds (300));

    // ── Test 1: UDP Discovery Probe ─────────────────────────────────
    std::cout << "[Test 1] Testing UDP discovery probe..." << std::flush;
    {
        juce::DatagramSocket clientUdp;
        clientUdp.bindToPort (0, "127.0.0.1");

        juce::String probe = "GRIDLOCK_DISCOVER";
        int sent = clientUdp.write ("127.0.0.1", testUdpPort, probe.toRawUTF8 (), probe.getNumBytesAsUTF8 ());
        assert (sent > 0);

        char replyBuf[256] = {};
        juce::String senderIP;
        int senderPort = 0;
        int ready = clientUdp.waitUntilReady (true, 2000);
        assert (ready == 1);
        int readBytes = clientUdp.read (replyBuf, sizeof (replyBuf) - 1, false, senderIP, senderPort);

        assert (readBytes > 0);
        juce::String reply (replyBuf, static_cast<size_t> (readBytes));
        assert (reply == ("GRIDLOCK_HERE:" + juce::String (testWsPort)));
    }
    std::cout << " PASSED" << std::endl;

    // ── Test 2: WebSocket Handshake & State Snapshot ────────────────
    std::cout << "[Test 2] Testing WebSocket RFC 6455 handshake & state..." << std::flush;
    juce::StreamingSocket clientSocket;
    bool connected = clientSocket.connect ("127.0.0.1", testWsPort, 1000);
    assert (connected);

    juce::String handshakeReq = "GET / HTTP/1.1\r\n"
                                "Host: 127.0.0.1\r\n"
                                "Upgrade: websocket\r\n"
                                "Connection: Upgrade\r\n"
                                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                "Sec-WebSocket-Version: 13\r\n\r\n";

    clientSocket.write (handshakeReq.toRawUTF8 (), handshakeReq.getNumBytesAsUTF8 ());

    clientSocket.waitUntilReady (true, 1000);
    char headerBuf[1024] = {};
    int n = clientSocket.read (headerBuf, sizeof (headerBuf) - 1, false);
    assert (n > 0);

    juce::String handshakeResp (headerBuf, static_cast<size_t> (n));
    assert (handshakeResp.contains ("101 Switching Protocols"));
    assert (handshakeResp.contains ("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
    std::cout << " PASSED" << std::endl;

    // ── Test 3: Read Initial State JSON Frame ───────────────────────
    std::cout << "[Test 3] Testing initial parameter snapshot frame..." << std::flush;
    clientSocket.waitUntilReady (true, 1000);
    uint8_t frameHeader[2] = {};
    clientSocket.read (frameHeader, 2, false);
    assert ((frameHeader[0] & 0x0F) == 0x01); // Text frame

    int payloadLen = frameHeader[1] & 0x7F;
    if (payloadLen == 126)
    {
        uint8_t ext[2];
        clientSocket.read (ext, 2, false);
        payloadLen = (ext[0] << 8) | ext[1];
    }

    std::vector<char> payload (payloadLen + 1, 0);
    clientSocket.read (payload.data (), payloadLen, false);

    auto parsedState = juce::JSON::parse (juce::String (payload.data ()));
    assert (parsedState.isObject ());
    assert (parsedState.getDynamicObject ()->getProperty ("type").toString () == "state");
    std::cout << " PASSED" << std::endl;

    // ── Test 4: Parameter Set via WebSocket ─────────────────────────
    std::cout << "[Test 4] Testing parameter set command..." << std::flush;
    {
        juce::String setMsg = "{\"type\":\"set\",\"id\":\"internal_bpm\",\"value\":145.0}";
        auto utf8Msg = setMsg.toUTF8 ();
        size_t len = setMsg.getNumBytesAsUTF8 ();

        std::vector<uint8_t> setFrame;
        setFrame.push_back (0x81);                              // FIN + Text
        setFrame.push_back (0x80 | static_cast<uint8_t> (len)); // Masked flag + length
        uint8_t maskKey[4] = {0x12, 0x34, 0x56, 0x78};
        setFrame.insert (setFrame.end (), maskKey, maskKey + 4);

        for (size_t i = 0; i < len; ++i)
            setFrame.push_back (static_cast<uint8_t> (utf8Msg[i]) ^ maskKey[i % 4]);

        clientSocket.write (setFrame.data (), static_cast<int> (setFrame.size ()));

        for (int i = 0; i < 10; ++i)
        {
            juce::MessageManager::getInstance ()->runDispatchLoopUntil (50);
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        auto *bpmParam = apvts.getParameter ("internal_bpm");
        assert (bpmParam != nullptr);
        float currentBpm = bpmParam->convertFrom0to1 (bpmParam->getValue ());
        assert (std::abs (currentBpm - 145.0f) < 0.2f);
    }
    std::cout << " PASSED" << std::endl;

    // ── Cleanup ─────────────────────────────────────────────────────
    clientSocket.close ();
    server.stop ();

    std::cout << "=== ALL REMOTE CONTROL SERVER INTEGRATION TESTS PASSED ===" << std::endl;
    return 0;
}
