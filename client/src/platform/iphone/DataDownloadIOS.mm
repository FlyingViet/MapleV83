//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "DataDownload.h"

#if defined(PLATFORM_SDL)

#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonDigest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	using ms::DataDownload::Progress;
	using ms::DataDownload::State;

	std::mutex status_mutex;
	Progress status;
	std::string bundled_dir;
	std::atomic<bool> running{ false };
	std::atomic<int64_t> received_bytes{ 0 };

	void set_status(State state, const std::string& file = "", const std::string& error = "")
	{
		std::lock_guard<std::mutex> lock(status_mutex);
		status.state = state;
		status.file = file;
		status.error = error;

		if (!error.empty())
			std::cout << "[Data] " << (file.empty() ? "" : file + ": ") << error << std::endl;
	}

	int64_t file_size(NSString* path)
	{
		struct stat st;
		return stat(path.fileSystemRepresentation, &st) == 0 ? st.st_size : -1;
	}

	// 3.9 GB of game data that the Mac can always resend has no place in
	// the phone's iCloud backup.
	void exclude_from_backup(NSString* path)
	{
		[[NSURL fileURLWithPath:path] setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
	}

	std::string describe(NSError* error)
	{
		return error.localizedDescription.UTF8String ?: "unknown error";
	}

	std::string gigabytes(int64_t bytes)
	{
		char text[32];
		snprintf(text, sizeof(text), "%.1f GB", bytes / 1e9);
		return text;
	}
}

// Appends one HTTP response body to a .part file.
@interface MV83PartWriter : NSObject <NSURLSessionDataDelegate>
@property(nonatomic) NSFileHandle* handle;
@property(nonatomic) int64_t offset;
@property(nonatomic) NSInteger httpStatus;
@property(nonatomic) NSError* error;
@property(nonatomic) dispatch_semaphore_t finished;
@end

@implementation MV83PartWriter

- (void)URLSession:(NSURLSession*)session
		  dataTask:(NSURLSessionDataTask*)task
didReceiveResponse:(NSURLResponse*)response
 completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
	self.httpStatus = [response isKindOfClass:NSHTTPURLResponse.class] ? ((NSHTTPURLResponse*)response).statusCode : 0;

	if (self.httpStatus == 200 && self.offset > 0)
	{
		// The server ignored the Range header: start the file over.
		received_bytes -= self.offset;
		self.offset = 0;
		[self.handle truncateAtOffset:0 error:nil];
	}

	completionHandler(self.httpStatus == 200 || self.httpStatus == 206
		? NSURLSessionResponseAllow : NSURLSessionResponseCancel);
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)task didReceiveData:(NSData*)data
{
	NSError* error = nil;

	if (![self.handle writeData:data error:&error])
	{
		self.error = error;
		[task cancel];
		return;
	}

	self.offset += data.length;
	received_bytes += data.length;
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
	if (!self.error)
		self.error = error;

	dispatch_semaphore_signal(self.finished);
}

@end

namespace
{
	NSURLSessionConfiguration* session_config()
	{
		NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
		config.timeoutIntervalForRequest = 20;
		config.timeoutIntervalForResource = 24 * 3600;
		config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
		config.URLCache = nil;
		return config;
	}

	NSArray* fetch_manifest(NSString* base, std::string& error)
	{
		__block NSData* body = nil;
		__block NSInteger code = 0;
		__block NSError* failure = nil;
		dispatch_semaphore_t done = dispatch_semaphore_create(0);

		NSURLSession* session = [NSURLSession sessionWithConfiguration:session_config()];
		NSURL* url = [NSURL URLWithString:[base stringByAppendingString:@"manifest.json"]];

		[[session dataTaskWithURL:url completionHandler:^(NSData* data, NSURLResponse* response, NSError* err) {
			body = data;
			failure = err;
			code = [response isKindOfClass:NSHTTPURLResponse.class] ? ((NSHTTPURLResponse*)response).statusCode : 0;
			dispatch_semaphore_signal(done);
		}] resume];

		dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
		[session finishTasksAndInvalidate];

		if (failure)
		{
			error = describe(failure);
			return nil;
		}

		if (code != 200 || !body)
		{
			error = code == 503 ? "the Mac has no converted data yet (run tools/convert_data.sh)"
				: "manifest request failed (HTTP " + std::to_string(code) + ")";
			return nil;
		}

		NSDictionary* json = [NSJSONSerialization JSONObjectWithData:body options:0 error:nil];
		NSArray* files = [json isKindOfClass:NSDictionary.class] ? json[@"files"] : nil;
		NSRegularExpression* valid = [NSRegularExpression regularExpressionWithPattern:@"^[A-Za-z]+\\.nx$" options:0 error:nil];

		for (NSDictionary* entry in files)
		{
			NSString* name = [entry isKindOfClass:NSDictionary.class] ? entry[@"name"] : nil;

			if (![name isKindOfClass:NSString.class] || ![entry[@"size"] isKindOfClass:NSNumber.class]
				|| ![entry[@"sha256"] isKindOfClass:NSString.class] || ![entry[@"mtime"] isKindOfClass:NSNumber.class]
				|| [valid numberOfMatchesInString:name options:0 range:NSMakeRange(0, name.length)] != 1)
			{
				error = "the manifest is malformed (update tools/data_server.py on the Mac)";
				return nil;
			}
		}

		if (files.count == 0)
			error = "the manifest is empty";

		return files.count ? files : nil;
	}


	// ---- Content identity ----
	//
	// nx-index.json in the data folder records, for every downloaded NX file,
	// its SHA-256 and the Mac-side modification time it was downloaded at, keyed
	// by the local file's size and mtime so it is only re-hashed after a change.
	// The bundled set describes itself the same way in nx/manifest.json.

	NSString* data_folder()
	{
		return NSFileManager.defaultManager.currentDirectoryPath;
	}

	NSString* index_path()
	{
		return [data_folder() stringByAppendingPathComponent:@"nx-index.json"];
	}

	NSMutableDictionary* load_index()
	{
		NSData* data = [NSData dataWithContentsOfFile:index_path()];
		id json = data ? [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil] : nil;
		return [json isKindOfClass:NSMutableDictionary.class] ? json : [NSMutableDictionary dictionary];
	}

	void save_index(NSDictionary* index)
	{
		NSData* data = [NSJSONSerialization dataWithJSONObject:index options:NSJSONWritingPrettyPrinted error:nil];
		[data writeToFile:index_path() atomically:YES];
	}

	std::string local_key(NSString* path)
	{
		struct stat st;

		if (stat(path.fileSystemRepresentation, &st) != 0)
			return "";

		return std::to_string(st.st_size) + ":" + std::to_string(st.st_mtimespec.tv_sec) + "."
			+ std::to_string(st.st_mtimespec.tv_nsec);
	}

	std::string sha256_of(NSString* path)
	{
		FILE* file = std::fopen(path.fileSystemRepresentation, "rb");

		if (!file)
			return "";

		CC_SHA256_CTX ctx;
		CC_SHA256_Init(&ctx);
		std::vector<unsigned char> buffer(8 << 20);
		size_t n;

		while ((n = std::fread(buffer.data(), 1, buffer.size(), file)) > 0)
			CC_SHA256_Update(&ctx, buffer.data(), static_cast<CC_LONG>(n));

		std::fclose(file);

		unsigned char digest[CC_SHA256_DIGEST_LENGTH];
		CC_SHA256_Final(digest, &ctx);

		char hex[CC_SHA256_DIGEST_LENGTH * 2 + 1];

		for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++)
			snprintf(hex + i * 2, 3, "%02x", digest[i]);

		return hex;
	}

	// The index entry for a downloaded file, if it still describes that file.
	NSDictionary* indexed(NSDictionary* index, NSString* name, NSString* path)
	{
		NSDictionary* entry = index[name];

		if (![entry isKindOfClass:NSDictionary.class])
			return nil;

		return [entry[@"key"] isEqual:@(local_key(path).c_str())] ? entry : nil;
	}

	// name -> manifest entry of the set built into the app.
	NSDictionary* bundled_manifest()
	{
		if (bundled_dir.empty())
			return @{};

		NSString* path = [@(bundled_dir.c_str()) stringByAppendingPathComponent:@"manifest.json"];
		NSData* data = [NSData dataWithContentsOfFile:path];
		NSDictionary* json = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
		NSMutableDictionary* byName = [NSMutableDictionary dictionary];

		if ([json isKindOfClass:NSDictionary.class] && [json[@"files"] isKindOfClass:NSArray.class])
			for (NSDictionary* entry in json[@"files"])
				if ([entry isKindOfClass:NSDictionary.class] && [entry[@"name"] isKindOfClass:NSString.class])
					byName[entry[@"name"]] = entry;

		return byName;
	}

	void remove_download(NSMutableDictionary* index, NSString* name, NSString* path)
	{
		[NSFileManager.defaultManager removeItemAtPath:path error:nil];
		[index removeObjectForKey:name];
	}

	// Brings one .part file up to `size` bytes, resuming from what is there.
	bool download_part(NSString* base, NSString* name, NSString* part, int64_t size, std::string& error)
	{
		if (file_size(part) < 0)
			[NSFileManager.defaultManager createFileAtPath:part contents:nil attributes:nil];

		NSFileHandle* handle = [NSFileHandle fileHandleForWritingAtPath:part];

		if (!handle)
		{
			error = "cannot write to the app's Documents folder";
			return false;
		}

		unsigned long long offset = 0;
		[handle seekToEndReturningOffset:&offset error:nil];

		if ((int64_t)offset > size)
		{
			received_bytes -= size;
			[handle truncateAtOffset:0 error:nil];
			offset = 0;
		}

		if ((int64_t)offset == size)
		{
			[handle closeAndReturnError:nil];
			return true;
		}

		MV83PartWriter* writer = [MV83PartWriter new];
		writer.handle = handle;
		writer.offset = (int64_t)offset;
		writer.finished = dispatch_semaphore_create(0);

		NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[base stringByAppendingString:name]]];

		if (offset > 0)
			[request setValue:[NSString stringWithFormat:@"bytes=%llu-", offset] forHTTPHeaderField:@"Range"];

		NSURLSession* session = [NSURLSession sessionWithConfiguration:session_config() delegate:writer delegateQueue:nil];
		[[session dataTaskWithRequest:request] resume];
		dispatch_semaphore_wait(writer.finished, DISPATCH_TIME_FOREVER);
		[session finishTasksAndInvalidate];
		[handle synchronizeAndReturnError:nil];
		[handle closeAndReturnError:nil];

		if (writer.httpStatus != 200 && writer.httpStatus != 206 && writer.httpStatus != 0)
			error = "HTTP " + std::to_string(writer.httpStatus);
		else if (writer.error)
			error = describe(writer.error);
		else if (writer.offset != size)
			error = "the download ended early";
		else
			return true;

		return false;
	}

	void run(std::string server)
	{
		@autoreleasepool
		{
			NSString* base = [NSString stringWithFormat:@"http://%s:%d/", server.c_str(), ms::DataDownload::PORT];
			NSString* docs = data_folder();

			set_status(State::CHECKING);
			std::cout << "[Data] Checking " << base.UTF8String << "manifest.json" << std::endl;

			std::string error;
			NSArray* files = fetch_manifest(base, error);

			if (!files)
			{
				set_status(State::FAILED, "", error);
				running = false;
				return;
			}

			NSMutableDictionary* index = load_index();
			NSDictionary* bundled = bundled_manifest();

			// Files whose content matches neither the downloaded nor the bundled
			// copy, in manifest order.
			NSMutableArray<NSDictionary*>* needed = [NSMutableArray array];
			int64_t total = 0;
			int64_t have = 0;

			for (NSDictionary* entry in files)
			{
				NSString* name = entry[@"name"];
				NSString* path = [docs stringByAppendingPathComponent:name];
				NSString* sha = entry[@"sha256"];

				if (file_size(path) >= 0)
				{
					NSDictionary* known = indexed(index, name, path);
					NSString* local_sha = known[@"sha256"];

					if (!local_sha)
					{
						// A copy from an older build, or the USB push: hash it once.
						set_status(State::CHECKING, name.UTF8String);
						local_sha = @(sha256_of(path).c_str());
					}

					if ([local_sha isEqualToString:sha])
					{
						index[name] = @{ @"key": @(local_key(path).c_str()), @"sha256": sha, @"mtime": entry[@"mtime"] };
						exclude_from_backup(path);
						continue;
					}

					remove_download(index, name, path);
				}

				NSString* bundled_sha = bundled[name][@"sha256"];
				NSString* bundled_path = [@(bundled_dir.c_str()) stringByAppendingPathComponent:name];

				if (!bundled_dir.empty() && [bundled_sha isEqualToString:sha] && file_size(bundled_path) >= 0)
					continue;

				int64_t size = [entry[@"size"] longLongValue];
				[needed addObject:entry];
				total += size;
				have += std::min<int64_t>(std::max<int64_t>(file_size([path stringByAppendingString:@".part"]), 0), size);
			}

			save_index(index);

			if (needed.count == 0)
			{
				std::cout << "[Data] Game data is up to date" << std::endl;
				set_status(State::DONE);
				running = false;
				return;
			}

			NSDictionary* volume = [[NSURL fileURLWithPath:docs]
				resourceValuesForKeys:@[NSURLVolumeAvailableCapacityForImportantUsageKey] error:nil];
			int64_t free_bytes = [volume[NSURLVolumeAvailableCapacityForImportantUsageKey] longLongValue];

			if (free_bytes > 0 && free_bytes < total - have + 256LL * 1024 * 1024)
			{
				set_status(State::FAILED, "", "not enough free disk space: needs "
					+ gigabytes(total - have) + ", " + gigabytes(free_bytes) + " free");
				running = false;
				return;
			}

			{
				std::lock_guard<std::mutex> lock(status_mutex);
				status.total = total;
			}

			received_bytes = have;
			std::cout << "[Data] Downloading " << needed.count << " changed files, " << gigabytes(total - have) << std::endl;

			for (NSDictionary* entry in needed)
			{
				NSString* name = entry[@"name"];
				NSString* path = [docs stringByAppendingPathComponent:name];
				NSString* part = [path stringByAppendingString:@".part"];
				int64_t size = [entry[@"size"] longLongValue];
				int failures = 0;

				set_status(State::DOWNLOADING, name.UTF8String);

				while (true)
				{
					if (download_part(base, name, part, size, error))
					{
						if (sha256_of(part) == std::string([entry[@"sha256"] UTF8String]))
							break;

						error = "the download did not match the Mac's copy";
						received_bytes -= size;
						[NSFileManager.defaultManager removeItemAtPath:part error:nil];
					}

					std::cout << "[Data] " << name.UTF8String << ": " << error << std::endl;

					if (++failures >= 5)
					{
						set_status(State::FAILED, name.UTF8String, error);
						save_index(index);
						running = false;
						return;
					}

					sleep(2);
				}

				[NSFileManager.defaultManager removeItemAtPath:path error:nil];

				if (![NSFileManager.defaultManager moveItemAtPath:part toPath:path error:nil])
				{
					set_status(State::FAILED, name.UTF8String, "cannot rename the finished download");
					save_index(index);
					running = false;
					return;
				}

				exclude_from_backup(path);
				index[name] = @{ @"key": @(local_key(path).c_str()), @"sha256": entry[@"sha256"], @"mtime": entry[@"mtime"] };
				save_index(index);
				std::cout << "[Data] " << name.UTF8String << " done" << std::endl;
			}

			std::cout << "[Data] Download complete" << std::endl;
			set_status(State::DONE);
			running = false;
		}
	}
}

namespace ms
{
	namespace DataDownload
	{
		void start(const std::string& server)
		{
			bool expected = false;

			if (!running.compare_exchange_strong(expected, true))
				return;

			{
				std::lock_guard<std::mutex> lock(status_mutex);
				status = Progress();
				status.state = State::CHECKING;
			}

			received_bytes = 0;
			std::thread(run, server).detach();
		}

		Progress progress()
		{
			std::lock_guard<std::mutex> lock(status_mutex);
			Progress copy = status;
			copy.received = received_bytes;
			return copy;
		}

		void set_bundled_directory(const std::string& directory)
		{
			bundled_dir = directory;
		}

		std::string path_for(const std::string& filename)
		{
			@autoreleasepool
			{
				NSString* name = @(filename.c_str());
				NSString* downloaded = [data_folder() stringByAppendingPathComponent:name];
				NSString* bundled = bundled_dir.empty() ? nil : [@(bundled_dir.c_str()) stringByAppendingPathComponent:name];
				bool have_bundled = bundled && file_size(bundled) >= 0;

				if (file_size(downloaded) >= 0)
				{
					if (!have_bundled)
						return downloaded.fileSystemRepresentation;

					// A download only wins over the bundled copy while it is the
					// newer version: an app update may since carry a newer one.
					NSMutableDictionary* index = load_index();
					NSDictionary* known = indexed(index, name, downloaded);
					NSDictionary* shipped = bundled_manifest()[name];

					if (known && [known[@"mtime"] longLongValue] > [shipped[@"mtime"] longLongValue]
						&& ![known[@"sha256"] isEqual:shipped[@"sha256"]])
						return downloaded.fileSystemRepresentation;

					remove_download(index, name, downloaded);
					save_index(index);
				}

				return have_bundled ? bundled.fileSystemRepresentation : "";
			}
		}
	}
}

#endif
