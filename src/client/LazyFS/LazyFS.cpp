//////////////////////////////////////////////////////////////////////////////////
// LazyFS - Lazy Filesystem for WebAssembly
// Generic HTTP Range-based file loading system
//////////////////////////////////////////////////////////////////////////////////
#include "LazyFS.h"

#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#include <iostream>
#include <LazyFS/LazyFileBackend.h>

namespace LazyFS
{
	void Initialize()
	{
		// Sync C++ CHUNK_SIZE to JavaScript and set WebSocket URL
		EM_ASM({
			if (typeof Module.LazyFS !== 'undefined') {
				Module.LazyFS.CHUNK_SIZE = $0;
				console.log('[LazyFS] Chunk size synced from C++:', $0, 'bytes');
				
				// Check if already configured via index.html or query parameters
				if (!Module.LazyFS.ASSETS_WS_URL) {
					var urlParams = new URLSearchParams(window.location.search);
					var customWsUrl = urlParams.get('assets_url');
					
					if (customWsUrl) {
						Module.LazyFS.ASSETS_WS_URL = customWsUrl;
					} else if (Module.LazyFS.AssetsServerIP !== undefined && Module.LazyFS.AssetsServerIP !== null) {
						// Construct URL dynamically from configured IP
						var ip = Module.LazyFS.AssetsServerIP;
						var port = (Module.LazyFS.AssetsServerPort !== undefined && Module.LazyFS.AssetsServerPort !== null) ? Module.LazyFS.AssetsServerPort : '8765';
						var protocol = Module.LazyFS.AssetsServerProtocol ? Module.LazyFS.AssetsServerProtocol : 'ws';
						var path = Module.LazyFS.AssetsServerPath ? Module.LazyFS.AssetsServerPath : "";
						Module.LazyFS.ASSETS_WS_URL = protocol + "://" + ip + ":" + port + path;
					} else {
						// Auto-detect WebSocket URL based on page location
						// Default to same host, port 8765
						var protocol = Module.LazyFS.AssetsServerProtocol ? (Module.LazyFS.AssetsServerProtocol + ':') : (window.location.protocol === 'https:' ? 'wss:' : 'ws:');
						var host = window.location.hostname;
						var port = (Module.LazyFS.AssetsServerPort !== undefined && Module.LazyFS.AssetsServerPort !== null) ? Module.LazyFS.AssetsServerPort : '8765';
						var path = Module.LazyFS.AssetsServerPath ? Module.LazyFS.AssetsServerPath : "";
						Module.LazyFS.ASSETS_WS_URL = protocol + '//' + host + ':' + port + path;
					}
				}
			} else {
				console.error('[LazyFS] JavaScript backend not loaded during Initialize!');
			}
		}, (int)LazyFileBackend::CHUNK_SIZE);
	}

	bool RegisterFile(const std::string& filepath, const std::string& url)
	{
		std::cout << "[LazyFS] Registering file: " << filepath << std::endl;

		// Register the file with LazyFS and fetch its size via WebSocket
		int result = EM_ASM_INT({
			var filepath = UTF8ToString($0);
			
			if (typeof Module.LazyFS !== 'undefined' && Module.LazyFS.registerFile) {
				// Use Asyncify to await the WebSocket connection and file size fetch
				return Asyncify.handleAsync(async () => {
					Module.LazyFS.registerFile(filepath);
					// Connect to WebSocket and fetch file size
					try {
						await Module.LazyFS.connect();
						var size = await Module.LazyFS.getFileSize(filepath);
						var fileEntry = Module.LazyFS.files.get(filepath);
						if (fileEntry) {
							fileEntry.size = size;
						}
						return size > 0 ? 1 : 0;
					} catch (e) {
						console.error('[LazyFS] Failed to get file size:', e);
						return 0;
					}
				});
			} else {
				console.error('[LazyFS] JavaScript backend not loaded!');
				return 0;
			}
		}, filepath.c_str());

		if (result == 1) {
			std::cout << "[LazyFS] File registered: " << filepath << std::endl;
			return true;
		} else {
			std::cerr << "[LazyFS] Failed to register file: " << filepath << std::endl;
			return false;
		}
	}
}

#else
// Non-Emscripten platforms: LazyFS is not available
namespace LazyFS
{
	void Initialize()
	{
		// No-op on non-WASM platforms
	}

	bool PreloadFile(const std::string& filepath, const std::string& url)
	{
		// LazyFS is only available on Emscripten/WASM
		return false;
	}
}
#endif
