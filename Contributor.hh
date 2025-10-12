// Contributor.h
#pragma once
#include <string>

class Contributor {
public:
    virtual ~Contributor() = default;
    virtual std::string get_contributor_id() const = 0;
    virtual std::string get_system_id() const = 0;
};
