//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
// Keeps the client's NX files in line with the converted set on the server Mac
// (tools/data_server.py, port 8485). The iPhone app carries the full set in its
// bundle; only files whose content changed on the Mac are downloaded into the
// data folder, where they take precedence. The Mac app has no bundled set and
// downloads everything the first time.
#pragma once

#include "../shared/PlatformConfig.h"

#if defined(PLATFORM_SDL)

#include <cstdint>
#include <string>

namespace ms
{
	namespace DataDownload
	{
		constexpr int PORT = 8485;

		enum class State
		{
			IDLE,
			CHECKING,
			DOWNLOADING,
			DONE,
			FAILED
		};

		struct Progress
		{
			State state = State::IDLE;
			std::string file;
			std::string error;
			int64_t received = 0;
			int64_t total = 0;
		};

		// The folder holding NX files built into the app with their
		// manifest.json, ending in '/'; empty when the build carries none.
		void set_bundled_directory(const std::string& directory);

		// Fetches the manifest from http://<server>:PORT and downloads every
		// file whose content differs from both the bundled and the downloaded
		// copy. Interrupted files resume. Does nothing while a sync is running.
		void start(const std::string& server);

		Progress progress();

		// The path to load an NX file from: a downloaded copy when it is newer
		// than the bundled one, else the bundled one; "" when there is neither.
		// Not for use while a sync is running.
		std::string path_for(const std::string& name);
	}
}

#endif
