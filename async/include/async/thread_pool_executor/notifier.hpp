//
// Created by Shagu on 13.10.2026.
//

#ifndef SHUTTLEENGINE_NOTIFIER_HPP
#define SHUTTLEENGINE_NOTIFIER_HPP

#include <atomic>

namespace shuttle::async {
    class Notifier {
    public:

        template <typename Predicate>
        void wait(Predicate&& predicate) {
            while (!predicate()) {
                const auto observed = m_generation.load(std::memory_order_acquire);
                if (predicate()) {
                    return;
                }
                m_generation.wait(observed, std::memory_order_acquire);
            }
        }

        void notify_one() noexcept {
            m_generation.fetch_add(1, std::memory_order_release);
            m_generation.notify_one();
        }

        void notify_all() noexcept {
            m_generation.fetch_add(1, std::memory_order_release);
            m_generation.notify_all();
        }

    private:
        std::atomic_uint64_t m_generation {0};
    };
}

#endif //SHUTTLEENGINE_NOTIFIER_HPP
