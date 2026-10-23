//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "NxFiles.h"

#ifdef USE_NX
#include <fstream>
#include <string>
#include <unistd.h>

#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>

#ifdef MS_PLATFORM_WASM
#include "../LazyFS/LazyFS.h"

#include <cstring>
#include <string>
#endif

#ifdef OPENSTORY_GLKIT_IOS
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Defined in platform/ios/NxPathIOS.mm
extern std::string ios_get_bundle_resource_path();
extern std::string ios_get_documents_path();

// Returns the directory containing NX files on iOS.
// Checks app bundle first (bundled resources), then Documents folder.
static std::string get_ios_nx_directory()
{
	// 1. Check inside the app bundle (for bundled NX files)
	std::string bundleDir = ios_get_bundle_resource_path();
	if (!bundleDir.empty())
	{
		std::string testFile = bundleDir + "/Base.nx";
		struct stat st;
		if (stat(testFile.c_str(), &st) == 0)
			return bundleDir + "/";
	}

	// 2. Check the Documents directory (user-transferred via Files app)
	std::string docs = ios_get_documents_path();
	if (!docs.empty())
	{
		std::string testFile = docs + "/Base.nx";
		struct stat st;
		if (stat(testFile.c_str(), &st) == 0)
			return docs + "/";
	}

	return "";
}
#endif

namespace ms
{
	namespace NxFiles
	{
		namespace
		{
			std::function<std::string(const std::string&)> path_of;
		}

		void set_resolver(std::function<std::string(const std::string&)> resolver)
		{
			path_of = resolver;
#ifndef MS_PLATFORM_WASM
			nl::nx::set_path_resolver(std::move(resolver));
#endif
		}

		Error init()
		{
#ifdef OPENSTORY_GLKIT_IOS
			std::string nx_dir = get_ios_nx_directory();

			if (nx_dir.empty())
				return Error(Error::Code::MISSING_FILE, "NX files not found in app bundle or Documents folder");

			chdir(nx_dir.c_str());
#endif

#ifdef MS_PLATFORM_WASM
			// The browser has no local files: register every NX file with
			// LazyFS so nlnx fetches it on demand from the assets server.
			constexpr const char* REQUIRED_ROOT_NX = "Base.nx";

			LazyFS::Initialize();

			if (!LazyFS::RegisterFile(REQUIRED_ROOT_NX, std::string("/assets/") + REQUIRED_ROOT_NX))
				return Error(Error::Code::MISSING_FILE, REQUIRED_ROOT_NX);

			for (auto filename : filenames)
			{
				if (std::strcmp(filename, REQUIRED_ROOT_NX) == 0)
					continue;

				if (!LazyFS::RegisterFile(filename, std::string("/assets/") + filename))
					return Error(Error::Code::MISSING_FILE, filename);
			}
#else
			for (auto filename : filenames)
			{
				std::string path = path_of ? path_of(filename) : filename;

				if (path.empty() || std::ifstream{ path }.good() == false)
					return Error(Error::Code::MISSING_FILE, filename);
			}
#endif

			try
			{
				nl::nx::load_all();
			}
			catch (const std::exception& ex)
			{
				static const std::string message = ex.what();

				return Error(Error::Code::NLNX, message.c_str());
			}

			return Error::Code::NONE;
		}
	}
}
#endif