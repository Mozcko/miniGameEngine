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
        // Id único por tipo de evento (0, 1, 2...), asignado la primera vez que se usa el tipo
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
            virtual void unsubscribe(std::uint64_t id) = 0;
            virtual void clear() = 0;
            virtual std::size_t activeCount() const = 0;
        };

        template <typename Event> class Channel : public ChannelBase {
        public:
            void subscribe(std::uint64_t id, std::function<void(const Event&)> callback) {
                m_listeners.push_back(Listener{id, std::move(callback), true});
            }

            void unsubscribe(std::uint64_t id) override {
                for (Listener& listener : m_listeners) {
                    if (listener.id == id && listener.active) {
                        listener.active = false;
                        listener.callback = nullptr;
                        m_dirty = true;
                        break;
                    }
                }
                if (m_dispatchDepth == 0 && m_dirty) compact();
            }

            void clear() override {
                if (m_dispatchDepth > 0) {
                    for (Listener& listener : m_listeners) {
                        listener.active = false;
                        listener.callback = nullptr;
                    }
                    m_dirty = true;
                } else {
                    m_listeners.clear();
                    m_dirty = false;
                }
            }

            void publish(const Event& event) {
                // Guard RAII para decrementar la profundidad y compactar incluso si un callback lanza
                struct DispatchGuard {
                    Channel& channel;
                    explicit DispatchGuard(Channel& ch) : channel(ch) { channel.m_dispatchDepth++; }
                    ~DispatchGuard() {
                        channel.m_dispatchDepth--;
                        if (channel.m_dispatchDepth == 0 && channel.m_dirty) channel.compact();
                    }
                };

                DispatchGuard guard(*this);
                // Fijamos el tamaño inicial: suscripciones nuevas durante publish() no reciben este evento
                const std::size_t count = m_listeners.size();
                for (std::size_t i = 0; i < count; ++i) {
                    if (!m_listeners[i].active) continue;
                    // Copiamos el callback por si el vector se realoja durante la invocación
                    auto callback = m_listeners[i].callback;
                    if (callback) callback(event);
                }
            }

            std::size_t activeCount() const override {
                std::size_t count = 0;
                for (const Listener& listener : m_listeners) {
                    if (listener.active) count++;
                }
                return count;
            }

        private:
            struct Listener {
                std::uint64_t id = 0;
                std::function<void(const Event&)> callback;
                bool active = true;
            };

            void compact() {
                m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                                 [](const Listener& listener) { return !listener.active; }),
                                  m_listeners.end());
                m_dirty = false;
            }

            std::vector<Listener> m_listeners;
            std::size_t m_dispatchDepth = 0;
            bool m_dirty = false;
        };
    }

    // Identificador de suscripción en un EventBus. Un valor 0 indica suscripción inválida/nula.
    struct SubscriptionId {
        std::uint64_t value = 0;
        std::size_t typeId = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(SubscriptionId a, SubscriptionId b) {
            return a.value == b.value && a.typeId == b.typeId;
        }
        friend bool operator!=(SubscriptionId a, SubscriptionId b) { return !(a == b); }
    };

    inline constexpr SubscriptionId kInvalidSubscription{};

    class EventBus;

    // Guarda RAII que cancela la suscripción al destruirse (solo movible).
    // El EventBus debe vivir más tiempo que cualquier ScopedSubscription asociada.
    class ScopedSubscription {
    public:
        ScopedSubscription() = default;
        ScopedSubscription(EventBus* bus, SubscriptionId id) : m_bus(bus), m_id(id) {}
        ~ScopedSubscription() { reset(); }

        ScopedSubscription(const ScopedSubscription&) = delete;
        ScopedSubscription& operator=(const ScopedSubscription&) = delete;

        ScopedSubscription(ScopedSubscription&& other) noexcept
            : m_bus(other.m_bus), m_id(other.m_id) {
            other.m_bus = nullptr;
            other.m_id = kInvalidSubscription;
        }

        ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
            if (this != &other) {
                reset();
                m_bus = other.m_bus;
                m_id = other.m_id;
                other.m_bus = nullptr;
                other.m_id = kInvalidSubscription;
            }
            return *this;
        }

        void reset();

        // Desvincula sin cancelar la suscripción y devuelve el id
        SubscriptionId release() {
            const SubscriptionId released = m_id;
            m_bus = nullptr;
            m_id = kInvalidSubscription;
            return released;
        }

        SubscriptionId id() const { return m_id; }
        bool isValid() const { return m_bus != nullptr && m_id.isValid(); }

    private:
        EventBus* m_bus = nullptr;
        SubscriptionId m_id = kInvalidSubscription;
    };

    // Bus de eventos tipado sin singletons: soporta despacho inmediato (publish) y diferido (enqueue/flush).
    // Seguro ante reentrada: los handlers pueden suscribir, desuscribir o publicar eventos durante el despacho.
    // No es thread-safe (uso en el hilo principal de gameplay).
    class EventBus {
    public:
        EventBus() = default;
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) noexcept = default;
        EventBus& operator=(EventBus&&) noexcept = default;

        template <typename Event>
        SubscriptionId subscribe(std::function<void(const Event&)> handler) {
            if (!handler) return kInvalidSubscription;
            const std::size_t typeId = event_detail::eventTypeId<Event>();
            const std::uint64_t id = m_nextId++;
            channel<Event>(typeId).subscribe(id, std::move(handler));
            return SubscriptionId{id, typeId};
        }

        template <typename Event>
        ScopedSubscription subscribeScoped(std::function<void(const Event&)> handler) {
            const SubscriptionId id = subscribe<Event>(std::move(handler));
            return id.isValid() ? ScopedSubscription(this, id) : ScopedSubscription();
        }

        void unsubscribe(SubscriptionId id) {
            if (!id.isValid() || id.typeId >= m_channels.size() || !m_channels[id.typeId]) return;
            m_channels[id.typeId]->unsubscribe(id.value);
        }

        template <typename Event> void unsubscribeAll() {
            if (auto* ch = findChannel<Event>()) ch->clear();
        }

        // Despacho inmediato y síncrono a los suscriptores actuales de Event
        template <typename Event> void publish(const Event& event) {
            if (auto* ch = findChannel<Event>()) ch->publish(event);
        }

        // Encola un evento para despacharlo en orden FIFO al llamar a flush()
        template <typename Event> void enqueue(Event event) {
            m_queue.push_back([ev = std::move(event)](EventBus& bus) { bus.publish<Event>(ev); });
        }

        // Despacha todos los eventos encolados hasta este momento.
        // Los eventos encolados durante flush() quedan para el siguiente flush().
        void flush() {
            if (m_queue.empty()) return;
            std::vector<std::function<void(EventBus&)>> batch;
            batch.swap(m_queue);
            for (const auto& dispatch : batch) dispatch(*this);
        }

        template <typename Event> std::size_t subscriberCount() const {
            const auto* ch = findChannel<Event>();
            return ch ? ch->activeCount() : 0;
        }

        std::size_t queuedCount() const { return m_queue.size(); }

        void clear() {
            for (const auto& ch : m_channels) {
                if (ch) ch->clear();
            }
            m_queue.clear();
        }

    private:
        template <typename Event> event_detail::Channel<Event>& channel(std::size_t typeId) {
            if (typeId >= m_channels.size()) m_channels.resize(typeId + 1);
            if (!m_channels[typeId]) m_channels[typeId] = std::make_unique<event_detail::Channel<Event>>();
            return static_cast<event_detail::Channel<Event>&>(*m_channels[typeId]);
        }

        template <typename Event> event_detail::Channel<Event>* findChannel() {
            const std::size_t typeId = event_detail::eventTypeId<Event>();
            if (typeId >= m_channels.size() || !m_channels[typeId]) return nullptr;
            return static_cast<event_detail::Channel<Event>*>(m_channels[typeId].get());
        }

        template <typename Event> const event_detail::Channel<Event>* findChannel() const {
            const std::size_t typeId = event_detail::eventTypeId<Event>();
            if (typeId >= m_channels.size() || !m_channels[typeId]) return nullptr;
            return static_cast<const event_detail::Channel<Event>*>(m_channels[typeId].get());
        }

        std::vector<std::unique_ptr<event_detail::ChannelBase>> m_channels;
        std::vector<std::function<void(EventBus&)>> m_queue;
        std::uint64_t m_nextId = 1;
    };

    inline void ScopedSubscription::reset() {
        if (m_bus && m_id.isValid()) m_bus->unsubscribe(m_id);
        m_bus = nullptr;
        m_id = kInvalidSubscription;
    }
}
