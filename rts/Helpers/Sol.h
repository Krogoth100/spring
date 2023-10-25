/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "lib/sol2/sol.hpp"
#include "lib/lua/include/lua.h" //for lua_Number

#include <tuple>
#include <type_traits>

namespace Sol
{

using Number = lua_Number;

#define SOL_OPTIONAL_2(type, a,b) sol::optional<type> a, sol::optional<type> b
#define SOL_OPTIONAL_3(type, a,b,c) sol::optional<type> a, sol::optional<type> b, sol::optional<type> c
#define SOL_OPTIONAL_4(type, a,b,c,d) sol::optional<type> a, sol::optional<type> b, sol::optional<type> c, sol::optional<type> d
#define SOL_OPTIONAL_TYPE_2(type) sol::optional<type>, sol::optional<type>
#define SOL_OPTIONAL_TYPE_3(type) sol::optional<type>, sol::optional<type>, sol::optional<type>
#define SOL_OPTIONAL_TYPE_4(type) sol::optional<type>, sol::optional<type>, sol::optional<type>, sol::optional<type>

// todo: common util
namespace Impl {
	template<int Counter, class... Types>
	struct MultipleNumbers {
		using type = typename MultipleNumbers<Counter-1, Number, Types...>::type;
	};
	template<class... Types>
	struct MultipleNumbers<0, Types...> {
		using type = std::tuple<Types...>;
	};
}

template<int Count>
using MultipleNumbers = typename Impl::MultipleNumbers<Count>::type;

}