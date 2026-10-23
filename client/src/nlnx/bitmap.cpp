//////////////////////////////////////////////////////////////////////////////
// NoLifeNx - Part of the NoLifeStory project                               //
// Copyright © 2013 Peter Atashian                                          //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////

#include "bitmap.hpp"
#include "file_impl.hpp"
#include <lz4.h>
#include <unistd.h>
#include <vector>

namespace nl
{
	bitmap::bitmap(void* f, uint64_t o, uint16_t w, uint16_t h) : m_file(f), m_offset(o), m_width(w), m_height(h)
	{
	}
	bool bitmap::operator<(bitmap const& o) const
	{
		return m_offset < o.m_offset;
	}
	bool bitmap::operator==(bitmap const& o) const
	{
		return m_offset == o.m_offset;
	}
	bitmap::operator bool() const
	{
		return m_file != nullptr;
	}
	std::vector<char> bitmap_buf;
	void const* bitmap::data() const
	{
		if (!m_file)
			return nullptr;
		_file_data* fd = static_cast<_file_data*>(m_file);

		auto const l = length();
		if (l + 0x20 > bitmap_buf.size())
			bitmap_buf.resize(l + 0x20);

		uint64_t data_offset = m_offset + 4;

#ifdef MS_PLATFORM_WASM
		uint32_t read_len = l;
#else
		// The compressed size precedes the data. Reading the uncompressed size
		// instead runs past the end of the file for its last bitmaps.
		uint32_t read_len = 0;
		if (!io_read(fd, &read_len, sizeof(read_len), m_offset) || read_len == 0)
			return nullptr;
#endif
		std::vector<char> src_buf(read_len);

		if (!io_read(fd, src_buf.data(), read_len, data_offset))
			return nullptr;

#ifdef MS_PLATFORM_WASM
		::LZ4_decompress_fast(src_buf.data(), bitmap_buf.data(), static_cast<int>(l));
#else
		if (::LZ4_decompress_safe(src_buf.data(), bitmap_buf.data(), static_cast<int>(read_len), static_cast<int>(l)) < 0)
			return nullptr;
#endif
		return bitmap_buf.data();
	}
	uint16_t bitmap::width() const
	{
		return m_width;
	}
	uint16_t bitmap::height() const
	{
		return m_height;
	}
	uint32_t bitmap::length() const
	{
		return 4u * m_width * m_height;
	}
	size_t bitmap::id() const
	{
#ifndef MS_PLATFORM_WASM
		// The offset alone repeats across files (Map.nx and UI.nx both have a
		// bitmap at some offset), and the renderer caches textures by this id.
		// Every file is mapped at its own address, so base + offset is unique.
		if (m_file)
		{
			auto const* fd = static_cast<_file_data const*>(m_file);

			if (fd->base)
				return reinterpret_cast<size_t>(static_cast<char const*>(fd->base) + m_offset);
		}
#endif
		return static_cast<size_t>(m_offset);
	}
} // namespace nl
