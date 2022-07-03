#pragma once

// source: https://www.bfilipek.com/2018/06/variant.html#overload
namespace visitor {
  template<class... Ts>
  struct overload : Ts... {
    using Ts::operator()...;
  };
  template<class... Ts>
  overload(Ts...) -> overload<Ts...>;
}  // namespace visitor