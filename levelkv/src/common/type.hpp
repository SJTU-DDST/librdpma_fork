#pragma once

template <size_t N> class GenericType {
  static_assert(N > 0, "GenericType size must be greater than zero.");

public:
  GenericType() = default;

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  explicit GenericType(const T value) {
    static_assert(sizeof(T) <= N, "Input value exceeds storage size.");
    memset(data_.data(), 0, N);
    memcpy(data_.data(), &value, sizeof(T));
  }

  explicit GenericType(const std::string &str) {
    if (str.size() > N) {
      throw std::overflow_error("String size exceeds storage size.");
    }
    memset(data_.data(), 0, N);
    memcpy(data_.data(), str.data(), str.size());
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  T ToInteger() const {
    static_assert(sizeof(T) <= N, "Output type size exceeds storage size.");
    T value;
    memcpy(&value, data_.data(), sizeof(T));
    return value;
  }

  std::string ToString() const { return std::string(data_.begin(), data_.end()); }

  bool operator==(const GenericType<N> &other) const {
    return data_ == other.data_;
  }

  bool operator!=(const GenericType<N> &other) const {
    return !(*this == other);
  }

private:
  std::array<uint8_t, N> data_{};
} __attribute__((packed));

using FixedKey = GenericType<16>;
using FixedValue = GenericType<15>;