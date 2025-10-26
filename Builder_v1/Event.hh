// Event.h
#ifndef EVENT_H
#define EVENT_H

#pragma once
#include <memory>

// Base class for all events
class Event {
public:
    using DescriptorType = const char*;
    virtual ~Event() = default;
    virtual DescriptorType type() const = 0;
};

#endif
