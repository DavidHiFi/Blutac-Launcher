#pragma once
#include <atomic>
#include <memory>

// The cancel flag one download row owns. The job reads it through
// std::function<bool()>-style callable syntax between stages; the download
// layer polls it inside transfers; the job locks it at the commit point
// (live files being swapped), where a stop would leave the player's folder
// half-written and the button turns into Finishing....
struct CancelState
{
    std::atomic_bool requested{false};
    std::atomic_bool lockedFlag{false}; // past the commit point once true

    bool operator()() const { return requested.load(); }
    explicit operator bool() const { return requested.load(); }
    void cancel() { requested.store(true); }
    void lock() { lockedFlag.store(true); }
    bool locked() const { return lockedFlag.load(); }
};
using CancelHandle = std::shared_ptr<CancelState>;
