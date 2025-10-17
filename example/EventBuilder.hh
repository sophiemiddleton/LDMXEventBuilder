// EventBuilder.h
#ifndef EVENTBUILDER_H
#define EVENTBUILDER_H
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
        return std::make_unique<GenericEvent<Payload, ContributorType>>(
            m_contributor, std::move(m_payload)
        );
    }

private:
    const ContributorType& m_contributor;
    Payload m_payload;
};
#endif
