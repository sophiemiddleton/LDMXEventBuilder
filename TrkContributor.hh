// TrkContributor.h
#pragma once
#include "Contributor.hh"
#include <string>

class TrkContributor : public Contributor {
public:
    TrkContributor(std::string contributor_id, std::string system_id)
        : m_contributor_id(std::move(contributor_id)), m_system_id(std::move(system_id)) {}

    // Implement the pure virtual methods from the base class
    std::string get_contributor_id() const override { return m_contributor_id; }
    std::string get_system_id() const override { return m_system_id; }
private:
    std::string m_contributor_id;
    std::string m_system_id;
};
