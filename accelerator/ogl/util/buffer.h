/*
* Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Robert Nagy, ronag89@gmail.com
*/

#pragma once

#include <common/spl/memory.h>
#include <common/enum_class.h>

#include <cstdint>

namespace caspar { namespace accelerator { namespace ogl {
			
class buffer sealed
{
	buffer(const buffer&);
	buffer& operator=(const buffer&);
public:

	// Static Members

	struct usage_def
	{
		enum type
		{
			write_only,
			read_only
		};
	};
	typedef enum_class<usage_def> usage;
	
	// Constructors

	buffer(std::size_t size, usage usage);
	~buffer();

	// Methods
	
	void map();
	void unmap();

	void bind() const;
	void unbind() const;

	// Properties

	uint8_t* data();
	std::size_t size() const;	
private:
	struct impl;
	spl::unique_ptr<impl> impl_;
};

}}}