#ifndef PAYLOAD_H
#define PAYLOAD_H
#pragma once
#include <variant>
#include "TrkData.hh"
#include "HCalData.hh"
#include "ECalData.hh"

using Payload = std::variant<TrkData, HCalData, ECalData>;
#endif
