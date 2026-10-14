//
// Created by Shagu on 13.10.2026.
//

#ifndef SHUTTLEENGINE_NOTIFIER_HPP
#define SHUTTLEENGINE_NOTIFIER_HPP

namespace shuttle::engine {
    class Notifier {
    public:

        template <typename Predicate>
        void wait(Predicate&& predicate) {
            const auto observed = _generation.load(std::memory_order_acquire);
            while(!predicate()) {
                _generation.wait(observed, std::memory_order_acquire);
            }
        }

        void notify_one() noexcept {
            _generation.fetch_add(1, std::memory_order_release);
            _generation.notify_one();
        }

        void notify_all() noexcept {
            _generation.fetch_add(1, std::memory_order_release);
            _generation.notify_all();
        }

    private:
        std::atomic_uint64_t _generation {0};
    };
}

#endif //SHUTTLEENGINE_NOTIFIER_HPP
