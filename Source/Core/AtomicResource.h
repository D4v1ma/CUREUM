#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

namespace cureum
{

// Hands immutable objects (wavetables, LFO shapes, compiled matrices...) from the message thread
// to the audio thread without locks. The audio thread only reads a raw pointer; the message
// thread keeps every published object alive for a grace period after it has been replaced, so a
// block that started with the old pointer can always finish safely.
template <typename T>
class AtomicResource
{
public:
    void publish (std::shared_ptr<const T> object)
    {
        const juce::ScopedLock sl (lock);
        const auto now = juce::Time::getMillisecondCounter();
        for (auto& e : retired)
            if (e.retiredAt == 0) e.retiredAt = now;
        if (latest != nullptr)
            retired.push_back ({ latest, now });
        latest = std::move (object);
        current.store (latest.get(), std::memory_order_release);
        prune (now);
    }

    const T* get() const noexcept { return current.load (std::memory_order_acquire); }
    std::shared_ptr<const T> getShared() const { const juce::ScopedLock sl (lock); return latest; }

    void prune() { const juce::ScopedLock sl (lock); prune (juce::Time::getMillisecondCounter()); }

private:
    struct Retired { std::shared_ptr<const T> object; juce::uint32 retiredAt; };

    void prune (juce::uint32 now)
    {
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [now] (const Retired& r) { return now - r.retiredAt > 3000; }),
                       retired.end());
    }

    std::atomic<const T*> current { nullptr };
    std::shared_ptr<const T> latest;
    std::vector<Retired> retired;
    juce::CriticalSection lock;
};

} // namespace cureum
