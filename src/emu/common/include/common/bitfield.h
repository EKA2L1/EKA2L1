/*
 * Copyright (c) 2019 EKA2L1 Team / RPCS3 Team.
 * 
 * This file is part of EKA2L1 project / RPCS3 Project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <common/types.h>

namespace eka2l1::common {
	template<typename T, uint N>
	struct bf_base {
		using type = T;
		using utype = typename std::make_unsigned<type>::type;

		// Datatype bitsize
		static constexpr uint bitmax = sizeof(T) * 8; static_assert(N - 1 < bitmax, "bf_base<> error: N out of bounds");
		
		// Field bitsize
		static constexpr uint bitsize = N;

		// Value mask
		static constexpr utype vmask = static_cast<utype>(~utype{} >> (bitmax - bitsize));

		// All ones mask
		static constexpr utype mask1 = static_cast<utype>(~utype{});

	protected:
		type m_data;
	};

	// Bitfield accessor (N bits from I position, 0 is LSB)
	template<typename T, uint I, uint N>
	struct bf_t : bf_base<T, N> {
		using type = typename bf_t::type;
		using vtype = typename bf_t::vtype;
		using utype = typename bf_t::utype;

		// Field offset
		static constexpr uint bitpos = I; static_assert(bitpos + N <= bf_t::bitmax, "bf_t<> error: I out of bounds");

		// Get bitmask of size N, at I pos
		static constexpr utype data_mask()
		{
			return static_cast<utype>(static_cast<utype>(bf_t::mask1 >> (bf_t::bitmax - bf_t::bitsize)) << bitpos);
		}

		// Bitfield extraction helper
		template<typename T2, typename = void>
		struct extract_impl
		{
			static_assert(!sizeof(T2), "bf_t<> error: Invalid type");
		};

		template<typename T2>
		struct extract_impl<T2, std::enable_if_t<std::is_unsigned<T2>::value>>
		{
			// Load unsigned value
			static constexpr T2 extract(const T& data)
			{
				return static_cast<T2>((static_cast<utype>(data) >> bitpos) & bf_t::vmask);
			}
		};

		template<typename T2>
		struct extract_impl<T2, std::enable_if_t<std::is_signed<T2>::value>>
		{
			// Load signed value (sign-extended)
			static constexpr T2 extract(const T& data)
			{
				return static_cast<T2>(static_cast<vtype>(static_cast<utype>(data) << (bf_t::bitmax - bitpos - N)) >> (bf_t::bitmax - N));
			}
		};

		// Bitfield extraction
		static constexpr vtype extract(const T& data)
		{
			return extract_impl<vtype>::extract(data);
		}

		// Bitfield insertion
		static constexpr vtype insert(vtype value)
		{
			return static_cast<vtype>((value & bf_t::vmask) << bitpos);
		}

		// Load bitfield value
		constexpr operator vtype() const
		{
			return extract(this->m_data);
		}

		// Load raw data with mask applied
		constexpr T unshifted() const
		{
			return static_cast<T>(this->m_data & data_mask());
		}

		// Optimized bool conversion (must be removed if inappropriate)
		explicit constexpr operator bool() const
		{
			return unshifted() != 0;
		}

		// Store bitfield value
		bf_t& operator =(vtype value)
		{
			this->m_data = static_cast<vtype>((this->m_data & ~data_mask()) | insert(value));
			return *this;
		}

		vtype operator ++(int)
		{
			utype result = *this;
			*this = static_cast<vtype>(result + 1);
			return result;
		}

		bf_t& operator ++()
		{
			return *this = *this + 1;
		}

		vtype operator --(int)
		{
			utype result = *this;
			*this = static_cast<vtype>(result - 1);
			return result;
		}

		bf_t& operator --()
		{
			return *this = *this - 1;
		}

		bf_t& operator +=(vtype right)
		{
			return *this = *this + right;
		}

		bf_t& operator -=(vtype right)
		{
			return *this = *this - right;
		}

		bf_t& operator *=(vtype right)
		{
			return *this = *this * right;
		}

		bf_t& operator &=(vtype right)
		{
			this->m_data &= static_cast<vtype>((static_cast<utype>(right) & bf_t::vmask) << bitpos);
			return *this;
		}

		bf_t& operator |=(vtype right)
		{
			this->m_data |= static_cast<vtype>((static_cast<utype>(right) & bf_t::vmask) << bitpos);
			return *this;
		}

		bf_t& operator ^=(vtype right)
		{
			this->m_data ^= static_cast<vtype>((static_cast<utype>(right) & bf_t::vmask) << bitpos);
			return *this;
		}
	};

	/**
	 * \brief A bit array
	 */
	template <size_t N>
	struct ba_t {
		static constexpr std::uint8_t total_size = (N >> 3) + 1;

		std::uint8_t bytes_[total_size];

		ba_t() {
			clear();
		}

		constexpr void clear() {
			for (std::size_t i = 0; i < total_size; i++) {
				bytes_[i] = 0;
			}
		}

		void set(std::size_t idx) {
			if (idx >= N) {
				return;
			}

			const std::size_t bytes_idx = idx >> 3;
			const std::uint8_t mask = 1 << (idx & 7);

			bytes_[bytes_idx] |= mask;
		}

		void unset(const std::size_t idx) {
			if (idx >= N) {
				return;
			}
			
			const std::size_t bytes_idx = idx >> 3;
			const std::uint8_t mask = ~(1 << (idx & 7));

			bytes_[bytes_idx] &= mask;
		}

		void unset(ba_t<N> aba) {
			for (std::size_t i = 0; i < total_size; i++) {
				bytes_[i] &= ~aba.bytes_[i];
			}
		}

		bool not_empty() {
			std::uint32_t result = 0;

			for (std::size_t i = 0; i < total_size; i++) {
				result |= bytes_[i];
			}

			return result;
		}

		bool get(const std::size_t idx) {
			std::size_t bytes_idx = idx >> 3;
			return (bytes_[bytes_idx] >> (idx & 7)) & 1;
		}
	};

	// Field pack (concatenated from left to right)
	template<typename F = void, typename... Fields>
	struct cf_t : bf_base<typename F::type, F::bitsize + cf_t<Fields...>::bitsize> {
		using type = typename cf_t::type;
		using vtype = typename cf_t::vtype;
		using utype = typename cf_t::utype;

		// Get disjunction of all "data" masks of concatenated values
		static constexpr vtype data_mask()
		{
			return static_cast<vtype>(F::data_mask() | cf_t<Fields...>::data_mask());
		}

		// Extract all bitfields and concatenate
		static constexpr vtype extract(const type& data)
		{
			return static_cast<vtype>(static_cast<utype>(F::extract(data)) << cf_t<Fields...>::bitsize | cf_t<Fields...>::extract(data));
		}

		// Split bitfields and insert them
		static constexpr vtype insert(vtype value)
		{
			return static_cast<vtype>(F::insert(value >> cf_t<Fields...>::bitsize) | cf_t<Fields...>::insert(value));
		}

		// Load value
		constexpr operator vtype() const
		{
			return extract(this->m_data);
		}

		// Store value
		cf_t& operator =(vtype value)
		{
			this->m_data = (this->m_data & ~data_mask()) | insert(value);
			return *this;
		}
	};

	// Empty field pack (recursion terminator)
	template<>
	struct cf_t<void> {
		static constexpr uint bitsize = 0;

		static constexpr uint data_mask()
		{
			return 0;
		}

		template<typename T>
		static constexpr auto extract(const T& data) -> decltype(+T())
		{
			return 0;
		}

		template<typename T>
		static constexpr T insert(T value)
		{
			return 0;
		}
	};

	// Fixed field (provides constant values in field pack)
	template<typename T, T V, uint N>
	struct ff_t : bf_base<T, N> {
		using type = typename ff_t::type;
		using vtype = typename ff_t::vtype;

		// Return constant value
		static constexpr vtype extract(const type& data)
		{
			static_assert((V & ff_t::vmask) == V, "ff_t<> error: V out of bounds");
			return V;
		}

		// Get value
		operator vtype() const
		{
			return V;
		}
	};
}