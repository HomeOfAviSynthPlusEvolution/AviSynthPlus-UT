#pragma once

namespace avsut::test {

enum class IsaRequirement { Scalar, Sse2, Ssse3, Sse41, Avx2 };

struct CpuFeatures {
  bool sse2{};
  bool ssse3{};
  bool sse41{};
  bool avx2{};

  bool supports(IsaRequirement requirement) const noexcept {
    switch (requirement) {
      case IsaRequirement::Scalar: return true;
      case IsaRequirement::Sse2: return sse2;
      case IsaRequirement::Ssse3: return ssse3;
      case IsaRequirement::Sse41: return sse41;
      case IsaRequirement::Avx2: return avx2;
    }
    return false;
  }

  static CpuFeatures detect() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init();
    return {static_cast<bool>(__builtin_cpu_supports("sse2")),
            static_cast<bool>(__builtin_cpu_supports("ssse3")),
            static_cast<bool>(__builtin_cpu_supports("sse4.1")),
            static_cast<bool>(__builtin_cpu_supports("avx2"))};
#else
    return {};
#endif
  }
};

}  // namespace avsut::test
