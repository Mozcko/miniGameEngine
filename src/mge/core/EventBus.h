#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace mge {

    namespace event_detail {

        inline std::size_t nextEventTypeId() {
            static std::size_t counter = 0;
            return counter++;
        }

        template <typename Event> std::size_t eventTypeId() {
            static const std::size_t kId = nextEventTypeId();
            return kId;
        }

        class ChannelBase {
        public:
            virtual ~ChannelBase() = default;
            virtual bool unsubscribe(std::uint64_t id) = 0;
            virtual void clear() = 0;
            virtual std::size_t subscriberCount() const = 0;
        };

        template <typename Event> class Channel : public ChannelBase {
        public:
            struct Listener {
                std::uint64_t id = 0;
                std::function<void(const Event&)> callback;
                bool once = false;
                bool removed = false;
            };

            void add(std::uint64_t id, std::function<void(const Event&)> callback, bool once) {
                Listener listener{id, std::move(callback), once, false};
                if (m_dispatchDepth > 0) {
                    m_pending.push_back(std::move(listener));
                } else {
                    m_listeners.push_back(std::move(listener));
                }
            }

            bool unsubscribe(std::uint64_t id) override {
                for (auto& listener : m_listeners) {
                    if (listener.id == id && !listener.removed) {
                        listener.removed = true;
                        if (m_dispatchDepth == 0) compact();
                        return true;
                    }
                }
                for (auto& listener : m_pending) {
                    if (listener.id == id && !listener.removed) {
                        listener.removed = true;
                        return true;
                    }
                }
                return false;
            }

            void emit(const Event& event) {
                m_dispatchDepth++;
                const std::size_t count = m_listeners.size();
                for (std::size_t i = 0; i < count; i++) {
                    if (m_listeners[i].removed) continue;
                    // Copy the callback in case a nested dispatch or clear mutates storage.
                    auto callback = m_listeners[i].callback;
                    if (m_listeners[i].once) m_listeners[i].removed = true;
                    if (callback) callback(event);
                }
                m_dispatchDepth--;

                if (m_dispatchDepth == 0) {
                    compact();
                    if (!m_pending.empty()) {
                        for (auto& pending : m_pending) {
                            if (!pending.removed) m_listeners.push_back(std::move(pending));
                        }
                        m_pending.clear();
                    }
                }
            }

            void clear() override {
                if (m_dispatchDepth > 0) {
                    for (auto& listener : m_listeners) listener.removed = true;
                    for (auto& listener : m_pending) listener.removed = true;
                } else {
                    m_listeners.clear();
                    m_pending.clear();
                }
            }

            std::size_t subscriberCount() const override {
                std::size_t count = 0;
                for (const auto& listener : m_listeners) {
                    if (!listener.removed) count++;
                }
                for (const auto& listener : m_pending) {
                    if (!listener.removed) count++;
                }
                return count;
            }

        private:
            void compact() {
                m_listeners.erase(
                    std::remove_if(m_listeners.begin(), m_listeners.end(),
                                   [](const Listener& l) { return l.removed; }),
                    m_listeners.end());
            }

            std::vector<Listener> m_listeners;
            std::vector<Listener> m_pending;
            std::size_t m_dispatchDepth = 0;
        };
    }

    // Handle returned by EventBus::subscribe() for manual unsubscription.
    struct SubscriptionHandle {
        std::size_t typeId = 0;
        std::uint64_t id = 0;

        bool isValid() const { return id != 0; }
        friend bool operator==(SubscriptionHandle a, SubscriptionHandle b) {
            return a.typeId == b.typeId && a.id == b.id;
        }
        friend bool operator!=(SubscriptionHandle a, SubscriptionHandle b) { return !(a == b); }
    };

    inline constexpr SubscriptionHandle kInvalidSubscription{};

    class EventBus;

    // Move-only RAII subscription guard: unsubscribes automatically on destruction.
    // The EventBus must outlive any active ScopedSubscription.
    class ScopedSubscription {
    public:
        ScopedSubscription() = default;
        ScopedSubscription(EventBus* bus, SubscriptionHandle handle) : m_bus(bus), m_handle(handle) {}
        ~ScopedSubscription() { reset(); }

        ScopedSubscription(const ScopedSubscription&) = delete;
        ScopedSubscription& operator=(const ScopedSubscription&) = delete;

        ScopedSubscription(ScopedSubscription&& other) noexcept
            : m_bus(other.m_bus), m_handle(other.m_handle) {
            other.m_bus = nullptr;
            other.m_handle = kInvalidSubscription;
        }

        ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
            if (this != &other) {
                reset();
                m_bus = other.m_bus;
                m_handle = other.m_handle;
                other.m_bus = nullptr;
                other.m_handle = kInvalidSubscription;
            }
            return *this;
        }

        bool isValid() const { return m_bus != nullptr && m_handle.isValid(); }
        SubscriptionHandle handle() const { return m_handle; }

        void reset();

    private:
        EventBus* m_bus = nullptr;
        SubscriptionHandle m_handle = kInvalidSubscription;
    };

    // Typed publish/subscribe event bus supporting immediate emit() and deferred enqueue()/flush().
    // Re-entrancy safe: listeners may subscribe, unsubscribe, emit, or enqueue during dispatch.
    // Not thread-safe (designed for main-thread gameplay communication).
    class EventBus {
    public:
        EventBus() = default;
        ~EventBus() = default;

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) = delete;
        EventBus& operator=(EventBus&&) = delete;

        template <typename Event>
        SubscriptionHandle subscribe(std::function<void(const Event&)> listener) {
            const std::size_t typeId = event_detail::eventTypeId<Event>();
            const std::uint64_t id = m_nextId++;
            channel<Event>().add(id, std::move(listener), false);
            return SubscriptionHandle{typeId, id};
        }

        // Listener is invoked at most once and automatically unsubscribed.
        template <typename Event>
        SubscriptionHandle subscribeOnce(std::function<void(const Event&)> listener) {
            const std::size_t typeId = event_detail::eventTypeId<Event>();
            const std::uint64_t id = m_nextId++;
            channel<Event>().add(id, std::move(listener), true);
            return SubscriptionHandle{typeId, id};
        }

        template <typename Event>
        ScopedSubscription subscribeScoped(std::function<void(const Event&)> listener) {
            return ScopedSubscription(this, subscribe<Event>(std::move(listener)));
        }

        bool unsubscribe(SubscriptionHandle handle) {
            if (!handle.isValid() || handle.typeId >= m_channels.size() || !m_channels[handle.typeId]) {
                return false;
            }
            return m_channels[handle.typeId]->unsubscribe(handle.id);
        }

        // Dispatches an event synchronously to all current subscribers of Event.
        template <typename Event> void emit(const Event& event) {
            auto* ch = findChannel<Event>();
            if (ch) ch->emit(event);
        }

        // Queues an event for deferred FIFO dispatch on the next flush().
        template <typename Event> void enqueue(Event event) {
            m_queue.push_back([this, ev = std::move(event)]() { emit<Event>(ev); });
        }

        // Dispatches all events currently in the queue in FIFO order.
        // Events enqueued during flush() are deferred to the next flush().
        void flush() {
            if (m_queue.empty()) return;
            std::vector<std::function<void()>> batch;
            batch.swap(m_queue);
            for (const auto& dispatch : batch) {
                dispatch();
            }
        }

        template <typename Event> std::size_t subscriberCount() const {
            const auto* ch = findChannel<Event>();
            return ch ? ch->subscriberCount() : 0;
        }

        std::size_t queuedCount() const { return m_queue.size(); }

        template <typename Event> void clearSubscribers() {
            auto* ch = findChannel<Event>();
            if (ch) ch->clear();
        }

        void clearQueue() { m_queue.clear(); }

        void clear() {
            for (auto& ch : m_channels) {
                if (ch) ch->clear();
            }
            m_queue.clear();
        }

    private:
        template <typename Event> event_detail::Channel<Event>& channel() {
            const std::size_t id = event_detail::eventTypeId<Event>();
            if (id >= m_channels.size()) m_channels.resize(id + 1);
            if (!m_channels[id]) m_channels[id] = std::make_unique<event_detail::Channel<Event>>();
            return static_cast<event_detail::Channel<Event>&>(*m_channels[id]);
        }

        template <typename Event> event_detail::Channel<Event>* findChannel() {
            const std::size_t id = event_detail::eventTypeId<Event>();
            if (id >= m_channels.size() || !m_channels[id]) return nullptr;
            return static_cast<event_detail::Channel<Event>*>(m_channels[id].get());
        }

        template <typename Event> const event_detail::Channel<Event>* findChannel() const {
            const std::size_t id = event_detail::eventTypeId<Event>();
            if (id >= m_channels.size() || !m_channels[id]) return nullptr;
            return static_cast<const event_detail::Channel<Event>*>(m_channels[id].get());
        }

        std::vector<std::unique_ptr<event_detail::ChannelBase>> m_channels;
        std::vector<std::function<void()>> m_queue;
        std::uint64_t m_nextId = 1;
    };

    inline void ScopedSubscription::reset() {
        if (m_bus && m_handle.isValid()) {
            m_bus->unsubscribe(m_handle);
        }
        m_bus = nullptr;
        m_handle = kInvalidSubscription;
    }
}
