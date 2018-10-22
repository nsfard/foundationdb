#pragma once

#include "flat_buffers.h"
#include <boost/variant.hpp>

namespace flat_buffers {

template <class... Alternatives>
struct union_like_traits<boost::variant<Alternatives...>> : std::true_type {
	using Member = boost::variant<Alternatives...>;
	using alternatives = pack<Alternatives...>;
	static uint8_t index(const Member& variant) { return variant.which(); }
	static bool empty(const Member& variant) { return false; }

	template <int i>
	static const index_t<i, alternatives>& get(const Member& variant) {
		return boost::get<index_t<i, alternatives>>(variant);
	}

	template <size_t i, class Alternative>
	static const void assign(Member& member, Alternative&& a) {
		static_assert(std::is_same_v<index_t<i, alternatives>, Alternative>);
		member = std::move(a);
	}
};

} // namespace flat_buffers
