#pragma once

#include "support/cpu_features.h"

#include <string>
#include <vector>

namespace avsut::test {

template <typename Function>
struct Variant {
  std::string name;
  Function function;
  IsaRequirement requirement;
};

template <typename Function>
std::vector<Variant<Function>> runnable_variants(
    const std::vector<Variant<Function>>& variants, CpuFeatures features) {
  std::vector<Variant<Function>> result;
  for (const auto& variant : variants) {
    if (features.supports(variant.requirement)) {
      result.push_back(variant);
    }
  }
  return result;
}

}  // namespace avsut::test
