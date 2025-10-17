// GenericEvent.h
#ifndef GENERICEVENT_H
#define GENERICEVENT_H
#pragma once
#include "Event.hh"
#include "Contributor.hh"
#include "Payload.hh"
// Generic event for a specific payload and contributor

template <typename Payload, typename ContributorType>
class GenericEvent : public Event {
public:
    GenericEvent(const ContributorType& contributor, Payload&& payload)
        : m_contributor(contributor), m_payload(std::move(payload)) {}

    // A static method to define the event type descriptor
    static constexpr DescriptorType descriptor() { return "GenericEvent"; }
    DescriptorType type() const override { return descriptor(); }

    const ContributorType& get_contributor() const { return m_contributor; }
    const Payload& get_payload() const { return m_payload; }

private:
    const ContributorType& m_contributor;
    Payload m_payload;
};
#endif
