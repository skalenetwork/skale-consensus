#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include "SkaleCommon.h"

#ifdef BITE
class AESKeyDecryptionShareList;
#endif

namespace TestHooks {

/**
 * @brief Generic, thread-safe, zero-lock fast-path test hook point.
 *
 * When no hook is installed (standard production run), `fire()` executes a single
 * atomic load (`acquire`), avoiding any mutex lock or memory allocation overhead.
 *
 * Why the `Tag` parameter?
 * Static class members are instantiated per unique template specialization.
 * The empty `Tag` struct guarantees that distinct hooks with identical function signatures
 * maintain isolated static variables (`active`, `mtx`, `cb`) without colliding or sharing state.
 *
 * To define a new hook anywhere in the codebase, declare an empty tag struct and typedef HookPoint:
 *   struct MyNewHookTag {};
 *   using MyNewHook = HookPoint<MyNewHookTag, void(int param1, std::string& param2)>;
 */
template <typename Tag, typename Signature>
class HookPoint;

template <typename Tag, typename ReturnType, typename... Args>
class HookPoint<Tag, ReturnType(Args...)> {
public:
    using Callback = std::function<ReturnType(Args...)>;

    template <typename... CallArgs>
    static void fire(CallArgs&&... args) {
        // Fast path: 1 atomic check, 0 mutex contention in production
        if (!active.load(std::memory_order_acquire)) {
            return;
        }
        std::lock_guard<std::mutex> lock(mtx);
        if (cb) {
            cb(std::forward<CallArgs>(args)...);
        }
    }

    static void set(Callback callback) {
        std::lock_guard<std::mutex> lock(mtx);
        cb = std::move(callback);
        active.store(true, std::memory_order_release);
    }

    static void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        cb = nullptr;
        active.store(false, std::memory_order_release);
    }

    static bool isEnabled() {
        return active.load(std::memory_order_relaxed);
    }

    class Scope {
    public:
        explicit Scope(Callback callback) { HookPoint::set(std::move(callback)); }
        ~Scope() { HookPoint::clear(); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

private:
    inline static std::atomic<bool> active{false};
    inline static std::mutex mtx;
    inline static Callback cb = nullptr;
};

#ifdef BITE
// Block finalization response hook: mutate shares or ask the client to retry.
struct BlockFinalizeResponseTag {};
using BlockFinalizeResponseHook = HookPoint<
    BlockFinalizeResponseTag,
    void(node_id senderNodeId,
         node_id receiverNodeId,
         block_id blockId,
         std::shared_ptr<AESKeyDecryptionShareList>& shares,
         bool& retryLater)
>;

// Suppress catchup while a test verifies that a block finalized locally.
struct BlockCatchupResponseTag {};
using BlockCatchupResponseHook = HookPoint<
    BlockCatchupResponseTag,
    void(node_id senderNodeId, node_id receiverNodeId, block_id afterBlockId, bool& suppress)
>;
#endif

}  // namespace TestHooks
