#include "AsciiTabRenderer.h"
#include "ClickGenerator.h"
#include "Crypto.h"
#include "DrumMap.h"
#include "GridComponent.h"
#include "PluginProcessor.h"
#include "RingBuffer.h"
#include "Timing.h"

#include <juce_core/juce_core.h>

static std::string toHex (const std::array<uint8_t, 20> &d) {
    static const char *hex = "0123456789abcdef";
    std::string s;
    s.reserve (40);
    for (auto b : d) {
        s.push_back (hex[b >> 4]);
        s.push_back (hex[b & 0xF]);
    }
    return s;
}

// ── Timing ──
class TimingTest : public juce::UnitTest {
  public:
    TimingTest () : UnitTest ("Timing", "P1") {}
    void runTest () override {
        beginTest ("on-grid exact");
        {
            auto r = Timing::compute (1.0, 0.25, 120.0, 20.0f);
            expectWithinAbsoluteError (r.deltaMs, 0.0, 1e-9);
            expect (r.state == TimingState::OnGrid);
            expectWithinAbsoluteError (r.normalizedDeviation, 0.0f, 1e-6f);
        }
        beginTest ("small drag within tolerance");
        {
            auto r = Timing::compute (0.02, 0.25, 120.0, 20.0f);
            expectWithinAbsoluteError (r.deltaMs, 10.0, 0.01);
            expect (r.state == TimingState::OnGrid);
            expectWithinAbsoluteError (r.normalizedDeviation, 0.5f, 1e-5f);
        }
        beginTest ("small rush within tolerance");
        {
            auto r = Timing::compute (-0.02, 0.25, 120.0, 20.0f);
            expectWithinAbsoluteError (r.deltaMs, -10.0, 0.01);
            expect (r.state == TimingState::OnGrid);
            expectWithinAbsoluteError (r.normalizedDeviation, -0.5f, 1e-5f);
        }
        beginTest ("beyond tolerance -> Drag");
        {
            auto r = Timing::compute (0.06, 0.25, 120.0, 20.0f);
            expectWithinAbsoluteError (r.deltaMs, 30.0, 0.01);
            expect (r.state == TimingState::Drag);
            expectWithinAbsoluteError (r.normalizedDeviation, 1.0f, 1e-5f);
        }
        beginTest ("beyond tolerance -> Rush");
        {
            auto r = Timing::compute (-0.06, 0.25, 120.0, 20.0f);
            expectWithinAbsoluteError (r.deltaMs, -30.0, 0.01);
            expect (r.state == TimingState::Rush);
            expectWithinAbsoluteError (r.normalizedDeviation, -1.0f, 1e-5f);
        }
        beginTest ("clamp extreme");
        {
            auto r = Timing::compute (10.06, 0.25, 120.0, 20.0f);
            expectWithinAbsoluteError (r.normalizedDeviation, 1.0f, 1e-5f);
            expect (r.state == TimingState::Drag);
        }
        beginTest ("BPM scaling");
        {
            auto r120 = Timing::compute (0.05, 0.25, 120.0, 20.0f);
            auto r60 = Timing::compute (0.05, 0.25, 60.0, 20.0f);
            expectWithinAbsoluteError (r60.deltaMs, r120.deltaMs * 2.0, 0.01);
        }
        beginTest ("grid interval matters");
        {
            auto r025 = Timing::compute (0.26, 0.25, 120.0, 20.0f);
            auto r05 = Timing::compute (0.26, 0.5, 120.0, 20.0f);
            expect (r025.deltaMs > 0 && r05.deltaMs < 0);
        }
    }
};
static TimingTest timingTest;

// ── HitColor ──
class HitColorTest : public juce::UnitTest {
  public:
    HitColorTest () : UnitTest ("HitColor", "P1") {}
    void runTest () override {
        const float tol = 20.0f, maxErr = 100.0f;
        auto emerald = juce::Colour (0xff00ff88);

        beginTest ("inside tolerance");
        expect (GridComponent::getContinuousHitColor (0.0f, tol, maxErr) == emerald);
        expect (GridComponent::getContinuousHitColor (19.9f, tol, maxErr) == emerald);
        expect (GridComponent::getContinuousHitColor (-19.9f, tol, maxErr) == emerald);
        expect (GridComponent::getContinuousHitColor (20.0f, tol, maxErr) == emerald);

        beginTest ("just beyond diverges");
        {
            auto rush = GridComponent::getContinuousHitColor (-25.0f, tol, maxErr);
            auto drag = GridComponent::getContinuousHitColor (25.0f, tol, maxErr);
            expect (rush != emerald && drag != emerald);
            expect (rush != drag);
            expectEquals (rush.getRed (), (juce::uint8)255);
            expectEquals (drag.getBlue (), (juce::uint8)255);
        }
        beginTest ("endpoints");
        {
            auto rushMax = GridComponent::getContinuousHitColor (-100.0f, tol, maxErr);
            auto dragMax = GridComponent::getContinuousHitColor (100.0f, tol, maxErr);
            expect (rushMax.getRed () == 255 && rushMax.getGreen () < 30 && rushMax.getBlue () > 60);
            expect (dragMax.getRed () > 200 && dragMax.getGreen () == 0);
        }
    }
};
static HitColorTest hitColorTest;

// ── HiHat debounce ──
class HiHatDebounceTest : public juce::UnitTest {
  public:
    HiHatDebounceTest () : UnitTest ("HiHatDebounce", "P1") {}
    void runTest () override {
        beginTest ("endpoint values");
        expectWithinAbsoluteError (DrumMap::hiHatDebounceWindowMs (127), 60.0, 0.5);
        expectWithinAbsoluteError (DrumMap::hiHatDebounceWindowMs (7), 200.0, 1.5);

        beginTest ("monotonic");
        expectGreaterThan (DrumMap::hiHatDebounceWindowMs (10), DrumMap::hiHatDebounceWindowMs (64));
        expectGreaterThan (DrumMap::hiHatDebounceWindowMs (64), DrumMap::hiHatDebounceWindowMs (120));

        beginTest ("bounds");
        for (int v = 0; v <= 127; ++v) {
            double w = DrumMap::hiHatDebounceWindowMs ((juce::uint8)v);
            expect (w >= 60.0 - 1e-9 && w <= 200.0 + 1e-9);
        }
    }
};
static HiHatDebounceTest hiHatDebounceTest;

// ── Subdivisions ──
class SubdivisionTest : public juce::UnitTest {
  public:
    SubdivisionTest () : UnitTest ("Subdivisions", "P1") {}
    void runTest () override {
        beginTest ("PluginProcessor grid");
        expectWithinAbsoluteError (MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (0), 0.5, 1e-9);
        expectWithinAbsoluteError (MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (1), 0.5 * 2.0 / 3.0, 1e-9);
        expectWithinAbsoluteError (MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (2), 0.25, 1e-9);
        expectWithinAbsoluteError (MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (3), 0.25 * 2.0 / 3.0, 1e-9);
        expectWithinAbsoluteError (MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (4), 0.125, 1e-9);
        expectWithinAbsoluteError (MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (99), 0.25, 1e-9);

        beginTest ("ClickGenerator subdivisions");
        ClickGenerator cg;
        expectWithinAbsoluteError (cg.getClickSubdivisionPpq (0), 0.0, 1e-9);
        expectWithinAbsoluteError (cg.getClickSubdivisionPpq (1), 1.0, 1e-9);
        expectWithinAbsoluteError (cg.getClickSubdivisionPpq (2), 0.5, 1e-9);
        expectWithinAbsoluteError (cg.getClickSubdivisionPpq (3), 0.25, 1e-9);
        expectWithinAbsoluteError (cg.getClickSubdivisionPpq (4), 1.0 / 3.0, 1e-9);
        expectWithinAbsoluteError (cg.getClickSubdivisionPpq (99), 0.0, 1e-9);
    }
};
static SubdivisionTest subdivisionTest;

// ── Crypto ──
class CryptoTest : public juce::UnitTest {
  public:
    CryptoTest () : UnitTest ("Crypto", "P1") {}
    void runTest () override {
        beginTest ("empty string");
        expectEquals (toHex (Crypto::sha1 ("", 0)), std::string ("da39a3ee5e6b4b0d3255bfef95601890afd80709"));

        beginTest ("abc");
        expectEquals (toHex (Crypto::sha1 ("abc", 3)), std::string ("a9993e364706816aba3e25717850c26c9cd0d89d"));

        beginTest ("quick brown fox");
        {
            const char *msg = "The quick brown fox jumps over the lazy dog";
            expectEquals (toHex (Crypto::sha1 (msg, strlen (msg))),
                          std::string ("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));
        }
        beginTest ("websocket handshake vector");
        {
            juce::String magic = juce::String ("dGhlIHNhbXBsZSBub25jZQ==") + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            auto d = Crypto::sha1 (magic.toRawUTF8 (), magic.getNumBytesAsUTF8 ());
            juce::MemoryBlock mb (d.data (), d.size ());
            juce::String accept = juce::Base64::toBase64 (mb.getData (), mb.getSize ());
            expectEquals (accept, juce::String ("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
        }
    }
};
static CryptoTest cryptoTest;

// ── RingBuffer ──
class RingBufferTest : public juce::UnitTest {
  public:
    RingBufferTest () : UnitTest ("RingBuffer", "P1") {}
    void runTest () override {
        beginTest ("push/pop/overflow (N-1 semantics)");
        RingBuffer<4> rb;
        expectEquals (rb.getNumReady (), 0);

        HitEvent a{};
        a.noteNumber = 36;
        HitEvent b{};
        b.noteNumber = 38;
        HitEvent c{};
        c.noteNumber = 42;
        HitEvent d{};
        d.noteNumber = 46;
        HitEvent e{};
        e.noteNumber = 49;

        expect (rb.push (a));
        expect (rb.push (b));
        expect (rb.push (c));
        expectEquals (rb.getNumReady (), 3);
        expect (!rb.push (d));
        expectEquals (rb.getNumReady (), 3);

        HitEvent out{};
        expect (rb.pop (out) && out.noteNumber == 36);
        expectEquals (rb.getNumReady (), 2);
        expect (rb.push (d));
        expectEquals (rb.getNumReady (), 3);
        expect (!rb.push (e));

        expect (rb.pop (out) && out.noteNumber == 38);
        expect (rb.pop (out) && out.noteNumber == 42);
        expect (rb.pop (out) && out.noteNumber == 46);
        expectEquals (rb.getNumReady (), 0);
        expect (!rb.pop (out));

        beginTest ("reset");
        rb.push (a);
        rb.push (b);
        rb.reset ();
        expectEquals (rb.getNumReady (), 0);
        expect (!rb.pop (out));

        beginTest ("4096 capacity holds 4095");
        RingBuffer<4096> big;
        for (int i = 0; i < 4095; ++i) {
            HitEvent h{};
            h.noteNumber = (juce::uint8)i;
            expect (big.push (h));
        }
        expect (!big.push (a));
        expectEquals (big.getNumReady (), 4095);
        expect (big.pop (out));
        expect (big.push (a));
        expectEquals (big.getNumReady (), 4095);
    }
};
static RingBufferTest ringBufferTest;

// ── DrumMap lanes ──
class DrumMapLanesTest : public juce::UnitTest {
  public:
    DrumMapLanesTest () : UnitTest ("DrumMapLanes", "P1") {}
    void runTest () override {
        beginTest ("standard lanes");
        auto &lanes = DrumMap::getStandardDrumLanes ();
        expectEquals ((int)lanes.size (), 6);
        expectEquals (lanes[0].label, juce::String ("CYMBALS"));
        expectEquals (lanes[5].label, juce::String ("OTHER"));
        auto contains = [] (const std::vector<juce::uint8> &v, juce::uint8 n) {
            return std::find (v.begin (), v.end (), n) != v.end ();
        };
        expect (contains (lanes[0].notes, DrumMap::Crash1));
        expect (contains (lanes[3].notes, DrumMap::SnareHead));
        expect (contains (lanes[4].notes, DrumMap::Kick));
        expect (!DrumMap::isExcluded (36));
    }
};
static DrumMapLanesTest drumMapLanesTest;

// ── AsciiTabRenderer ──
class AsciiTabRendererTest : public juce::UnitTest {
  public:
    AsciiTabRendererTest () : UnitTest ("AsciiTabRenderer", "P1") {}
    void runTest () override {
        auto makeView = [] (int bars, double interval, double bpm, int ts, float tol, double ppq) {
            GridViewState v;
            v.numBars = bars;
            v.gridSubdivisionPpq = interval;
            v.bpm = (float)bpm;
            v.timeSigNum = ts;
            v.toleranceMs = tol;
            v.currentPpq = ppq;
            v.latencyOffsetMs = 0.0f;
            return v;
        };
        auto makeHit = [] (uint8_t note, double ppq, uint8_t vel = 100) {
            HitEvent h{};
            h.noteNumber = note;
            h.velocity = vel;
            h.rawHitPpqPosition = ppq;
            h.hitPpqPosition = ppq;
            return h;
        };

        beginTest ("empty returns header");
        {
            auto view = makeView (4, 0.25, 120.0, 4, 20.0f, 16.0);
            auto res = AsciiTab::render ({}, view);
            expect (res.text.find ("Gridlock Tab") != std::string::npos);
            expect (res.text.find ("Bars 4") != std::string::npos);
            expectEquals (res.totalCols, 64); // 4*4/0.25
        }

        beginTest ("single kick at bar start");
        {
            auto view = makeView (1, 0.25, 120.0, 4, 20.0f, 4.0);
            std::vector<HitEvent> ev = {makeHit (DrumMap::Kick, 0.1)};
            auto res = AsciiTab::render (ev, view);
            // Kick lane label exists and row contains K
            expect (res.text.find ("KICK") != std::string::npos);
            expect (res.text.find ("K") != std::string::npos);
        }

        beginTest ("auto expands 1/16 -> 1/32 on double-bass collision");
        {
            // 4/4, 1 bar, current 4.0, hits at 0.125 apart (32nd) -> collide at 0.25
            auto view = makeView (1, 0.25, 120.0, 4, 20.0f, 4.0);
            std::vector<HitEvent> ev = {
                makeHit (DrumMap::Kick, 0.0),
                makeHit (DrumMap::Kick, 0.125),
            };
            // At 1/16 both map to col 0? Let's see: min 0.0 max 4.0
            // col = floor((ppq -0)/0.25): 0 and 0 -> collision
            auto resAuto = AsciiTab::render (ev, view, {}); // auto
            expectWithinAbsoluteError (resAuto.usedIntervalPpq, 0.125, 1e-9);
            expectEquals (resAuto.totalCols, 32); // 1*4/0.125

            AsciiTab::RenderOptions fixed;
            fixed.fixedIntervalPpq = 0.25;
            auto resFixed = AsciiTab::render (ev, view, fixed);
            expectWithinAbsoluteError (resFixed.usedIntervalPpq, 0.25, 1e-9);
            // stacked cell shows '2'
            expect (resFixed.text.find ("2") != std::string::npos);
        }

        beginTest ("auto stays at 1/16 when sparse");
        {
            auto view = makeView (1, 0.25, 120.0, 4, 20.0f, 4.0);
            std::vector<HitEvent> ev = {
                makeHit (DrumMap::Kick, 0.0),
                makeHit (DrumMap::Kick, 1.0),
            };
            auto res = AsciiTab::render (ev, view, {});
            expectWithinAbsoluteError (res.usedIntervalPpq, 0.25, 1e-9);
        }

        beginTest ("metalcore 8 bars 32nd blast wraps into systems");
        {
            auto view = makeView (8, 0.25, 200.0, 4, 20.0f, 32.0);
            // Create 16th double bass for entire 8 bars: every 0.25
            std::vector<HitEvent> ev;
            for (double p = 0.0; p < 32.0; p += 0.25) {
                ev.push_back (makeHit (DrumMap::Kick, p + 0.02));
            }
            // Also add hi-hat every 0.5
            for (double p = 0.0; p < 32.0; p += 0.5) {
                ev.push_back (makeHit (DrumMap::ClosedHiHat, p + 0.01));
            }
            auto res = AsciiTab::render (ev, view, {});
            expectGreaterThan (res.numSystems, 1);
            expect (res.text.find ("System 1/") != std::string::npos);
            expect (res.text.find ("System 4/") != std::string::npos ||
                    res.text.find ("System 2/") != std::string::npos);
            // Should not be monstrous single line: each system ruler <=64 cols
            // Check used interval still 0.25 (no collision)
            expectWithinAbsoluteError (res.usedIntervalPpq, 0.25, 1e-9);
        }

        beginTest ("1/64 overflow when still colliding at 1/32");
        {
            auto view = makeView (1, 0.25, 120.0, 4, 20.0f, 4.0);
            std::vector<HitEvent> ev = {
                makeHit (DrumMap::Kick, 0.05),
                makeHit (DrumMap::Kick, 0.06), // 0.01 ppq apart = 5ms at 120bpm -> still same 1/32 cell
            };
            auto res = AsciiTab::render (ev, view, {});
            expectWithinAbsoluteError (res.usedIntervalPpq, 0.0625, 1e-9);
            // At 1/64 they still collide (0.01 <0.0625) so stacked '2' remains
            expect (res.text.find ("2") != std::string::npos);
        }

        beginTest ("deviation row shown");
        {
            auto view = makeView (1, 0.25, 120.0, 4, 20.0f, 4.0);
            // hit 30ms late -> drag '>'
            HitEvent h = makeHit (DrumMap::SnareHead, 0.06); // 30ms at 120bpm (0.06 ppq = 0.06/2*1000=30ms)
            std::vector<HitEvent> ev2 = {h};
            auto res = AsciiTab::render (ev2, view);
            expect (res.text.find (">") != std::string::npos || res.text.find ("+") != std::string::npos);
        }
    }
};
static AsciiTabRendererTest asciiTabRendererTest;

// ── runner ──
int main () {
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (true);
    runner.runAllTests ();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults (); ++i) {
        if (auto *r = runner.getResult (i)) {
            failures += r->failures;
        }
    }

    if (failures == 0) {
        std::cout << "=== ALL LOGIC TESTS PASSED ===\n";
    } else {
        std::cout << "=== " << failures << " TEST(S) FAILED ===\n";
    }

    return failures == 0 ? 0 : 1;
}
