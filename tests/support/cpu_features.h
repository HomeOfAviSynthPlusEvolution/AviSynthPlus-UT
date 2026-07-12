#pragma once

namespace avsut::test {

enum class IsaRequirement {
  Scalar,
  Sse2,
  Ssse3,
  Sse41,
  Avx2,
  Avx2Fma,
  Avx512F,
  Avx512Base,
  Avx512Vbmi,
  Avx512Fast,
};

struct CpuFeatures {
  bool sse2{};
  bool ssse3{};
  bool sse41{};
  bool avx2{};
  bool fma3{};
  bool avx512f{};
  bool avx512cd{};
  bool avx512bw{};
  bool avx512dq{};
  bool avx512vl{};
  bool avx512vbmi{};
  bool avx512vnni{};
  bool avx512vbmi2{};
  bool avx512bitalg{};
  bool avx512vpopcntdq{};

  bool supports(IsaRequirement requirement) const noexcept {
    switch (requirement) {
      case IsaRequirement::Scalar:
        return true;
      case IsaRequirement::Sse2:
        return sse2;
      case IsaRequirement::Ssse3:
        return ssse3;
      case IsaRequirement::Sse41:
        return sse41;
      case IsaRequirement::Avx2:
        return avx2;
      case IsaRequirement::Avx2Fma:
        return avx2 && fma3;
      case IsaRequirement::Avx512F:
        return avx512f;
      case IsaRequirement::Avx512Base:
        return avx512f && avx512cd && avx512bw && avx512dq && avx512vl;
      case IsaRequirement::Avx512Vbmi:
        return avx512vbmi;
      case IsaRequirement::Avx512Fast:
        return supports(IsaRequirement::Avx512Base) && avx512vnni && avx512vbmi && avx512vbmi2 &&
               avx512bitalg && avx512vpopcntdq;
    }
    return false;
  }

  static CpuFeatures detect() {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    // GCC and Clang include OS support for the vector register state here.
    __builtin_cpu_init();
    return {static_cast<bool>(__builtin_cpu_supports("sse2")),
            static_cast<bool>(__builtin_cpu_supports("ssse3")),
            static_cast<bool>(__builtin_cpu_supports("sse4.1")),
            static_cast<bool>(__builtin_cpu_supports("avx2")),
            static_cast<bool>(__builtin_cpu_supports("fma")),
            static_cast<bool>(__builtin_cpu_supports("avx512f")),
            static_cast<bool>(__builtin_cpu_supports("avx512cd")),
            static_cast<bool>(__builtin_cpu_supports("avx512bw")),
            static_cast<bool>(__builtin_cpu_supports("avx512dq")),
            static_cast<bool>(__builtin_cpu_supports("avx512vl")),
            static_cast<bool>(__builtin_cpu_supports("avx512vbmi")),
            static_cast<bool>(__builtin_cpu_supports("avx512vnni")),
            static_cast<bool>(__builtin_cpu_supports("avx512vbmi2")),
            static_cast<bool>(__builtin_cpu_supports("avx512bitalg")),
            static_cast<bool>(__builtin_cpu_supports("avx512vpopcntdq"))};
#else
    return {};
#endif
  }
};

}  // namespace avsut::test
