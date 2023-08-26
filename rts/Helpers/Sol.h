/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "lib/sol2/sol.hpp"
#include "lib/lua/include/lua.h" //for lua_Number

#include <tuple>
#include <type_traits>

namespace Sol
{

using Number = lua_Number;

// todo: common util
namespace Impl {
	template<int Counter, class... Types>
	struct MultipleNumbers {
		using type = typename MultipleNumbers<Counter-1, lua_Number, Types...>::type;
	};
	template<class... Types>
	struct MultipleNumbers<0, Types...> {
		using type = std::tuple<Types...>;
	};
}

template<int Count>
using MultipleNumbers = typename Impl::MultipleNumbers<Count>::type;

}