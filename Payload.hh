#pragma once
#include <variant>
#include "TrkData.hh"
#include "HCalData.hh"

using Payload = std::variant<TrkData, HCalData>;
