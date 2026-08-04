#include "RemoteControlServer.h"
#include <cstring>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// UDP Discovery Thread — listens for broadcast probes and replies with the WS port
// ─────────────────────────────────────────────────────────────────────────────
class DiscoveryListenerThread : public juce::Thread
{
public:
    DiscoveryListenerThread (int udpPort, int wsPort)
        : juce::Thread ("GridlockDiscovery"),
          udpPort (udpPort),
          wsPort (wsPort)
    {}

    void run() override
    {
        juce::DatagramSocket udpSocket;
        udpSocket.bindToPort (udpPort, "0.0.0.0");

        while (! threadShouldExit())
        {
            char buf[256] = {};
            juce::String senderIP;
            int senderPort = 0;

            const int bytesRead = udpSocket.read (buf, sizeof (buf) - 1, false,
                                                   senderIP, senderPort);

            if (bytesRead > 0)
            {
                juce::String msg (buf, static_cast<size_t> (bytesRead));

                if (msg.trimEnd() == "GRIDLOCK_DISCOVER")
                {
                    juce::String reply = "GRIDLOCK_HERE:" + juce::String (wsPort);
                    udpSocket.write (senderIP, senderPort,
                                    reply.toRawUTF8(),
                                    reply.getNumBytesAsUTF8());
                }
            }

            // Short sleep to avoid busy spinning when no data arrives
            if (! threadShouldExit())
                sleep (50);
        }
    }

private:
    int udpPort;
    int wsPort;
};

// ─────────────────────────────────────────────────────────────────────────────
// RemoteControlServer
// ─────────────────────────────────────────────────────────────────────────────

RemoteControlServer::RemoteControlServer (juce::AudioProcessorValueTreeState& apvts,
                                          int wsPort, int udpPort)
    : juce::Thread ("GridlockRemote"),
      apvts (apvts),
      wsPort (wsPort),
      udpPort (udpPort)
{
}

RemoteControlServer::~RemoteControlServer()
{
    stop();
}

void RemoteControlServer::start()
{
    // Start UDP discovery thread
    discoveryThread = std::make_unique<DiscoveryListenerThread> (udpPort, wsPort);
    discoveryThread->startThread();

    // Bind WebSocket server socket
    if (serverSocket.createListener (wsPort, "0.0.0.0"))
    {
        DBG ("RemoteControlServer: WebSocket listening on port " + juce::String (wsPort));
    }
    else
    {
        DBG ("RemoteControlServer: FAILED to bind WebSocket on port " + juce::String (wsPort));
    }

    // Snapshot initial parameter values
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* rparam = dynamic_cast<juce::RangedAudioParameter*> (param))
            lastParamValues[rparam->getParameterID()] = rparam->getValue();
    }

    startThread();           // Accept loop
    startTimerHz (10);       // 10 Hz parameter polling + heartbeat
}

void RemoteControlServer::stop()
{
    stopTimer();
    signalThreadShouldExit();
    serverSocket.close();

    if (discoveryThread)
    {
        discoveryThread->signalThreadShouldExit();
        discoveryThread->stopThread (500);
        discoveryThread.reset();
    }

    stopThread (1000);

    std::lock_guard<std::mutex> lock (clientsMutex);
    for (auto& c : clients)
        c.socket->close();
    clients.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Thread — accept incoming WebSocket connections
// ─────────────────────────────────────────────────────────────────────────────
void RemoteControlServer::run()
{
    while (! threadShouldExit())
    {
        auto client = std::make_unique<juce::StreamingSocket>();

        if (serverSocket.waitUntilReady (true, 200) == 1)
        {
            client.reset (serverSocket.waitForNextConnection());

            if (client != nullptr)
            {
                if (performWebSocketHandshake (*client))
                {
                    // Send full state snapshot
                    juce::String stateJson = buildFullStateJson();
                    sendWebSocketTextFrame (*client, stateJson);

                    std::lock_guard<std::mutex> lock (clientsMutex);
                    clients.push_back ({ std::move (client) });
                    DBG ("RemoteControlServer: Client connected. Total: "
                         + juce::String (clients.size()));
                }
                else
                {
                    client->close();
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer — poll for parameter changes, read client messages, send heartbeat
// ─────────────────────────────────────────────────────────────────────────────
void RemoteControlServer::timerCallback()
{
    // 1. Read messages from all clients
    {
        std::lock_guard<std::mutex> lock (clientsMutex);

        for (auto it = clients.begin(); it != clients.end(); )
        {
            auto& sock = *it->socket;

            if (! sock.isConnected())
            {
                it = clients.erase (it);
                continue;
            }

            // Non-blocking read
            if (sock.waitUntilReady (true, 0) == 1)
            {
                juce::String msg = readWebSocketTextFrame (sock);

                if (msg.isNotEmpty())
                    handleClientMessage (sock, msg);
                else if (! sock.isConnected())
                {
                    it = clients.erase (it);
                    continue;
                }
            }

            ++it;
        }
    }

    // 2. Detect parameter changes and push to clients
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* rparam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const juce::String& id = rparam->getParameterID();
            const float currentNorm = rparam->getValue();
            const float currentDenorm = rparam->convertFrom0to1 (currentNorm);

            auto it = lastParamValues.find (id);
            if (it == lastParamValues.end() || std::abs (it->second - currentNorm) > 1e-6f)
            {
                lastParamValues[id] = currentNorm;

                auto json = juce::DynamicObject::Ptr (new juce::DynamicObject());
                json->setProperty ("type", "changed");
                json->setProperty ("id", id);
                json->setProperty ("value", static_cast<double> (currentDenorm));
                json->setProperty ("norm", static_cast<double> (currentNorm));

                broadcastToClients (juce::JSON::toString (json.get()));
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket Handshake (RFC 6455 server-side)
// ─────────────────────────────────────────────────────────────────────────────
bool RemoteControlServer::performWebSocketHandshake (juce::StreamingSocket& client)
{
    // Read HTTP upgrade request
    char buf[4096] = {};
    int totalRead = 0;

    for (int attempt = 0; attempt < 50; ++attempt)
    {
        if (client.waitUntilReady (true, 100) == 1)
        {
            int n = client.read (buf + totalRead,
                                  static_cast<int> (sizeof (buf)) - totalRead - 1,
                                  false);
            if (n > 0)
                totalRead += n;

            // Check for end of HTTP headers
            if (juce::String (buf).contains ("\r\n\r\n"))
                break;
        }
    }

    juce::String request (buf, static_cast<size_t> (totalRead));

    if (! request.containsIgnoreCase ("Upgrade: websocket"))
        return false;

    // Extract Sec-WebSocket-Key
    juce::String key;
    for (auto line : juce::StringArray::fromLines (request))
    {
        if (line.trim().startsWithIgnoreCase ("Sec-WebSocket-Key:"))
        {
            key = line.fromFirstOccurrenceOf (":", false, false).trim();
            break;
        }
    }

    if (key.isEmpty())
        return false;

    // Compute accept hash — SHA-1 is required by RFC 6455
    juce::String magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // SHA-1 is required by RFC 6455 — use raw SHA-1 + Base64
    juce::MemoryBlock sha1Block;
    {
        // Manual SHA-1 via JUCE's built-in (juce_core has SHA256 but not SHA1 directly)
        // We'll use a compact SHA-1 implementation inline
        auto sha1Digest = [](const void* data, size_t len) -> std::array<uint8_t, 20>
        {
            // SHA-1 implementation (RFC 3174)
            auto leftRotate = [](uint32_t value, int bits) -> uint32_t {
                return (value << bits) | (value >> (32 - bits));
            };

            uint32_t h0 = 0x67452301;
            uint32_t h1 = 0xEFCDAB89;
            uint32_t h2 = 0x98BADCFE;
            uint32_t h3 = 0x10325476;
            uint32_t h4 = 0xC3D2E1F0;

            // Pre-processing
            uint64_t bitLen = static_cast<uint64_t>(len) * 8;
            std::vector<uint8_t> msg(static_cast<const uint8_t*>(data),
                                     static_cast<const uint8_t*>(data) + len);
            msg.push_back(0x80);
            while (msg.size() % 64 != 56)
                msg.push_back(0x00);

            for (int i = 7; i >= 0; --i)
                msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));

            // Process each 512-bit block
            for (size_t offset = 0; offset < msg.size(); offset += 64)
            {
                uint32_t w[80];
                for (int i = 0; i < 16; ++i)
                {
                    w[i] = (static_cast<uint32_t>(msg[offset + i * 4]) << 24)
                         | (static_cast<uint32_t>(msg[offset + i * 4 + 1]) << 16)
                         | (static_cast<uint32_t>(msg[offset + i * 4 + 2]) << 8)
                         | (static_cast<uint32_t>(msg[offset + i * 4 + 3]));
                }
                for (int i = 16; i < 80; ++i)
                    w[i] = leftRotate(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

                uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

                for (int i = 0; i < 80; ++i)
                {
                    uint32_t f, k;
                    if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                    else if (i < 40) { f = b ^ c ^ d;             k = 0x6ED9EBA1; }
                    else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                    else             { f = b ^ c ^ d;             k = 0xCA62C1D6; }

                    uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
                    e = d; d = c; c = leftRotate(b, 30); b = a; a = temp;
                }

                h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
            }

            std::array<uint8_t, 20> digest;
            auto store = [&digest](int offset, uint32_t val) {
                digest[offset]     = static_cast<uint8_t>((val >> 24) & 0xFF);
                digest[offset + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
                digest[offset + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
                digest[offset + 3] = static_cast<uint8_t>(val & 0xFF);
            };
            store(0, h0); store(4, h1); store(8, h2); store(12, h3); store(16, h4);
            return digest;
        };

        auto digest = sha1Digest (magic.toRawUTF8(), magic.getNumBytesAsUTF8());
        sha1Block.append (digest.data(), digest.size());
    }

    juce::String acceptKey = juce::Base64::toBase64 (sha1Block.getData(),
                                                      sha1Block.getSize());

    juce::String response = "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
                            "\r\n";

    client.write (response.toRawUTF8(), response.getNumBytesAsUTF8());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Read a WebSocket text frame (simplified — handles frames up to 64 KB)
// ─────────────────────────────────────────────────────────────────────────────
juce::String RemoteControlServer::readWebSocketTextFrame (juce::StreamingSocket& client)
{
    uint8_t header[2];
    if (client.read (header, 2, false) != 2)
        return {};

    const uint8_t opcode = header[0] & 0x0F;
    const bool masked = (header[1] & 0x80) != 0;
    uint64_t payloadLen = header[1] & 0x7F;

    // Handle close frame
    if (opcode == 0x08)
    {
        client.close();
        return {};
    }

    // Pong in response to ping
    if (opcode == 0x09)
    {
        if (payloadLen > 0 && payloadLen < 126)
        {
            std::vector<uint8_t> pingData (payloadLen);
            client.read (pingData.data(), static_cast<int> (payloadLen), false);
        }
        uint8_t pong[2] = { 0x8A, 0x00 }; // FIN + Pong, 0 length
        client.write (pong, 2);
        return {};
    }

    if (opcode != 0x01) // Only text frames
        return {};

    if (payloadLen == 126)
    {
        uint8_t ext[2];
        if (client.read (ext, 2, false) != 2) return {};
        payloadLen = (static_cast<uint64_t> (ext[0]) << 8) | ext[1];
    }
    else if (payloadLen == 127)
    {
        uint8_t ext[8];
        if (client.read (ext, 8, false) != 8) return {};
        payloadLen = 0;
        for (int i = 0; i < 8; ++i)
            payloadLen = (payloadLen << 8) | ext[i];
    }

    uint8_t maskKey[4] = {};
    if (masked)
    {
        if (client.read (maskKey, 4, false) != 4) return {};
    }

    if (payloadLen > 65536) // Safety limit
        return {};

    std::vector<uint8_t> payload (static_cast<size_t> (payloadLen));
    int remaining = static_cast<int> (payloadLen);
    int offset = 0;
    int retries = 0;

    while (remaining > 0 && retries < 10)
    {
        int n = client.read (payload.data() + offset, remaining, false);
        if (n > 0)
        {
            offset += n;
            remaining -= n;
        }
        else
        {
            client.waitUntilReady (true, 5);
            retries++;
        }
    }

    if (remaining > 0)
        return {};

    if (masked)
    {
        for (size_t i = 0; i < payload.size(); ++i)
            payload[i] ^= maskKey[i % 4];
    }

    return juce::String (reinterpret_cast<const char*> (payload.data()),
                          payload.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Send a WebSocket text frame (server → client, unmasked)
// ─────────────────────────────────────────────────────────────────────────────
bool RemoteControlServer::sendWebSocketTextFrame (juce::StreamingSocket& client,
                                                   const juce::String& text)
{
    auto utf8 = text.toUTF8();
    const size_t len = text.getNumBytesAsUTF8();

    std::vector<uint8_t> frame;
    frame.push_back (0x81); // FIN + text opcode

    if (len < 126)
    {
        frame.push_back (static_cast<uint8_t> (len));
    }
    else if (len < 65536)
    {
        frame.push_back (126);
        frame.push_back (static_cast<uint8_t> ((len >> 8) & 0xFF));
        frame.push_back (static_cast<uint8_t> (len & 0xFF));
    }
    else
    {
        frame.push_back (127);
        for (int i = 7; i >= 0; --i)
            frame.push_back (static_cast<uint8_t> ((len >> (i * 8)) & 0xFF));
    }

    frame.insert (frame.end(),
                   reinterpret_cast<const uint8_t*> (utf8.getAddress()),
                   reinterpret_cast<const uint8_t*> (utf8.getAddress()) + len);

    int written = client.write (frame.data(), static_cast<int> (frame.size()));
    return written == static_cast<int> (frame.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Handle incoming client JSON message
// ─────────────────────────────────────────────────────────────────────────────
void RemoteControlServer::handleClientMessage (juce::StreamingSocket& /*client*/,
                                                const juce::String& message)
{
    auto parsed = juce::JSON::parse (message);
    if (! parsed.isObject())
        return;

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    juce::String type = obj->getProperty ("type").toString();

    if (type == "set")
    {
        juce::String paramId = obj->getProperty ("id").toString();
        double value = obj->getProperty ("value");

        if (auto* param = apvts.getParameter (paramId))
        {
            float normalized = param->convertTo0to1 (static_cast<float> (value));
            param->setValueNotifyingHost (normalized);
            // Immediately record in lastParamValues to prevent echo loop back to clients
            lastParamValues[paramId] = normalized;
        }
    }
    else if (type == "get_state")
    {
        // Client requests full state refresh
        juce::String stateJson = buildFullStateJson();
        std::lock_guard<std::mutex> lock (clientsMutex);
        // Send to all clients (broadcast)
        for (auto& c : clients)
            sendWebSocketTextFrame (*c.socket, stateJson);
    }
    else if (type == "clear_grid")
    {
        // Acknowledge clear grid request to clients
        broadcastToClients ("{\"type\":\"clear_ack\"}");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Build JSON snapshot of all parameters
// ─────────────────────────────────────────────────────────────────────────────
juce::String RemoteControlServer::buildFullStateJson() const
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("type", "state");

    auto params = juce::DynamicObject::Ptr (new juce::DynamicObject());

    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* rparam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            auto paramObj = juce::DynamicObject::Ptr (new juce::DynamicObject());
            paramObj->setProperty ("value", static_cast<double> (rparam->convertFrom0to1 (rparam->getValue())));
            paramObj->setProperty ("norm", static_cast<double> (rparam->getValue()));
            paramObj->setProperty ("name", rparam->getName (128));

            // Include range info for the client to build UI
            auto& range = rparam->getNormalisableRange();
            paramObj->setProperty ("min", static_cast<double> (range.start));
            paramObj->setProperty ("max", static_cast<double> (range.end));
            paramObj->setProperty ("step", static_cast<double> (range.interval));

            // Detect choice parameters and include options
            if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (rparam))
            {
                juce::var optionsArray;
                for (auto& choice : choiceParam->choices)
                    optionsArray.append (choice);
                paramObj->setProperty ("options", optionsArray);
                paramObj->setProperty ("paramType", "choice");
            }
            else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*> (rparam))
            {
                paramObj->setProperty ("paramType", "bool");
            }
            else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (rparam))
            {
                paramObj->setProperty ("paramType", "int");
            }
            else
            {
                paramObj->setProperty ("paramType", "float");
            }

            params->setProperty (rparam->getParameterID(), juce::var (paramObj.get()));
        }
    }

    root->setProperty ("params", juce::var (params.get()));
    return juce::JSON::toString (root.get());
}

// ─────────────────────────────────────────────────────────────────────────────
// Broadcast a JSON message to all connected clients
// ─────────────────────────────────────────────────────────────────────────────
void RemoteControlServer::broadcastToClients (const juce::String& json)
{
    std::lock_guard<std::mutex> lock (clientsMutex);

    for (auto it = clients.begin(); it != clients.end(); )
    {
        if (! it->socket->isConnected())
        {
            it = clients.erase (it);
            continue;
        }

        if (! sendWebSocketTextFrame (*it->socket, json))
        {
            it->socket->close();
            it = clients.erase (it);
        }
        else
        {
            ++it;
        }
    }
}
