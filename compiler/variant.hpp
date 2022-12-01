#ifndef AOC2022_VARIANT_HPP_
#define AOC2022_VARIANT_HPP_

#include <type_traits>
#include <variant>

namespace aoc2022 {

template <typename T>
struct Wrapper {};

template <typename T>
struct Holder {
  void CanHold(Wrapper<T>) {}
};

template <typename... Ts>
struct MultiHolder : Holder<Ts>... {
  using Holder<Ts>::CanHold...;
};

template <typename T, typename... Ts>
concept ElementOf = requires (MultiHolder<Ts...> h) {
  h.CanHold(Wrapper<T>());
};

static_assert(ElementOf<int, char, int, double>);

template <typename Variant, typename T>
struct CanHoldImpl;

template <typename... Ts, typename T>
struct CanHoldImpl<std::variant<Ts...>, T> {
  static constexpr bool value = ElementOf<T, Ts...>;
};

template <typename T, typename Variant>
concept HoldableBy =
    CanHoldImpl<std::decay_t<decltype(std::declval<Variant>().value)>,
                T>::value;

}  // namespace aoc2022

#endif  // AOC2022_VARIANT_HPP_
