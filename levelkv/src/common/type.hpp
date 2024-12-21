#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <type_traits>

template <size_t N> class GenericType {
  static_assert(N > 0, "GenericType size must be greater than zero.");

public:
  GenericType() = default;

  template <typename T> explicit GenericType(const T &value) {
    std::string str = ConvertToString(value);
    if (str.size() > N) {
      throw std::overflow_error("Input size exceeds storage size.");
    }
    memset(data_.data(), 0, N);
    memcpy(data_.data(), str.data(), str.size());
  }

  std::string ToString() const {
    size_t len = strnlen(reinterpret_cast<const char *>(data_.data()), N);
    return std::string(data_.data(), data_.data() + len);
  }

  bool operator==(const GenericType<N> &other) const {
    return data_ == other.data_;
  }

  bool operator!=(const GenericType<N> &other) const {
    return !(*this == other);
  }

  friend std::ostream &operator<<(std::ostream &os, const GenericType<N> &obj) {
    os << obj.ToString();
    return os;
  }

private:
  std::array<uint8_t, N> data_{};

  template <typename T> static std::string ConvertToString(const T &value) {
    if constexpr (std::is_same_v<T, std::string>) {
      return value;
    } else {
      std::ostringstream oss;
      oss << value;
      return oss.str();
    }
  }
} __attribute__((packed));

using FixedKey = GenericType<16>;
using FixedValue = GenericType<15>;
