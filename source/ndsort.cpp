#include <string>

#include "ndsort/ndsort.hpp"

#include <fmt/core.h>

exported_class::exported_class()
    : m_name {fmt::format("{}", "ndsort")}
{
}

auto exported_class::name() const -> char const*
{
  return m_name.c_str();
}
