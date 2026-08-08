#pragma once
#include "HitEvent.h"

#include <array>
#include <juce_core/juce_core.h>

template <size_t Capacity = 4096> class RingBuffer {
  public:
    RingBuffer () : fifo (static_cast<int> (Capacity)) {}

    bool push (const HitEvent &event) noexcept {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0) {
            buffer[static_cast<size_t> (start1)] = event;
            fifo.finishedWrite (1);
            return true;
        }
        if (size2 > 0) {
            buffer[static_cast<size_t> (start2)] = event;
            fifo.finishedWrite (1);
            return true;
        }
        return false; // Buffer full
    }

    bool pop (HitEvent &event) noexcept {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 > 0) {
            event = buffer[static_cast<size_t> (start1)];
            fifo.finishedRead (1);
            return true;
        }
        if (size2 > 0) {
            event = buffer[static_cast<size_t> (start2)];
            fifo.finishedRead (1);
            return true;
        }
        return false; // Buffer empty
    }

    void reset () noexcept {
        fifo.reset ();
    }

    int getNumReady () const noexcept {
        return fifo.getNumReady ();
    }

  private:
    juce::AbstractFifo fifo;
    std::array<HitEvent, Capacity> buffer{};
};
