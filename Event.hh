// Event.h
#pragma once
#include <memory>

// Base class for all events
class Event {
public:
    using DescriptorType = const char*;
    virtual ~Event() = default;
    virtual DescriptorType type() const = 0;
};
