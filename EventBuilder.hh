// EventBuilder.h
#pragma once
#include "GenericEvent.hh"

template <typename Payload, typename ContributorType>
class EventBuilder {
public:
    explicit EventBuilder(const ContributorType& contributor)
        : m_contributor(contributor) {}

    EventBuilder<Payload, ContributorType>& with_payload(Payload&& payload) {
        m_payload = std::move(payload);
        return *this;
    }

    std::unique_ptr<GenericEvent<Payload, ContributorType>> build() {
        // Explicitly specify both template arguments to the GenericEvent constructor.
        // This is where the compiler was likely losing the Payload type.
        return std::make_unique<GenericEvent<Payload, ContributorType>>(
            m_contributor, std::move(m_payload)
        );
    }

private:
    const ContributorType& m_contributor;
    Payload m_payload;
};
