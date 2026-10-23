//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	Web Audio backend derived from the maplestory-wasm port by nmnsnv			//
//	(https://github.com/nmnsnv/maplestory-wasm), Audio/Audio.cpp, namespace jrc	//
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
#ifdef MS_PLATFORM_WASM

// Browser (Emscripten) implementation of the Sound / Music API declared in
// Audio.h. The BASS-based implementation lives in Audio.cpp and is compiled
// out under MS_PLATFORM_WASM; this file is compiled in its place.
//
// The audio graph lives entirely on the JS side (Module.MapleAudio):
//
//   BufferSource -> GainNode(volume) -> StereoPannerNode(pan) -> sfxGain -+
//                                                                         +-> destination
//   BufferSource(loop) ------------------------------------> bgmGain -----+
//
// sfxGain / bgmGain are the global SFX / BGM volume settings, so per-play
// volume multiplies with the user's slider exactly like BASS_ATTRIB_VOL did
// with BASS_CONFIG_GVOL_SAMPLE.
//
// Note for editing the EM_JS bodies below: they pass through the C
// preprocessor, so no apostrophes (not even in comments), no "//" sequences
// and only /* */ comments inside them.

#include "Audio.h"

#include "../Configuration.h"

#include <emscripten.h>

#include <cmath>
#include <cstdint>

#ifdef USE_NX
#include <nlnx/audio.hpp>
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// Every nx audio blob starts with the original client's 82 byte sound
		// header (SoundDX8 GUIDs + WAVEFORMATEX). BASS was told to start at
		// offset 82; the browser decoder needs the raw MP3/WAV stream too.
		constexpr uint32_t NX_AUDIO_HEADER_SIZE = 82;

		// Kinds passed to ms_audio_register_clip; must match the JS constants.
		// Sound effects are kept decoded for the lifetime of the page (like BASS
		// samples). Music is decoded PCM of several tens of MB per track, so
		// music clips are evicted when the next track starts.
		constexpr int CLIP_SFX_LAZY = 0;
		constexpr int CLIP_SFX_PRELOAD = 1;
		constexpr int CLIP_MUSIC = 2;
	}

	// Creates the AudioContext and the Module.MapleAudio helper object.
	// Returns 0 when Web Audio is unavailable.
	EM_JS(int, ms_audio_init, (), {
		if (Module.MapleAudio) {
			return 1;
		}

		const AudioContextCtor = globalThis.AudioContext || globalThis.webkitAudioContext;

		if (!AudioContextCtor) {
			console.error("[audio] Web Audio is not available in this browser");
			return 0;
		}

		const CLIP_MUSIC = 2;
		const CLIP_SFX_PRELOAD = 1;

		/* A sound effect that has to wait for decoding is dropped after this
		   long, so a slow decode cannot produce a click seconds after the
		   event that triggered it. Only the first play of a lazily decoded
		   clip (item sounds) can hit this. */
		const PENDING_SFX_TIMEOUT_MS = 500;

		const clamp = (value, lo, hi) => Math.max(lo, Math.min(hi, value));

		const context = new AudioContextCtor();
		const bgmGain = context.createGain();
		const sfxGain = context.createGain();
		bgmGain.connect(context.destination);
		sfxGain.connect(context.destination);

		const state = {
			context: context,
			bgmGain: bgmGain,
			sfxGain: sfxGain,
			/* id -> { bytes, buffer, promise, failed, music, pending } */
			clips: new Map(),
			/* { id, source } of the playing BGM, or null */
			music: null,
			/* Bumped on every playMusic so a late decode cannot resurrect an
			   older track over a newer one. */
			musicToken: 0,
			gestureSeen: false,
			unlockHandlers: null
		};

		/* Browsers keep a context created outside a user gesture suspended
		   until resume() is called from inside one. */
		const unlock = () => {
			state.gestureSeen = true;

			if (state.context.state !== "running") {
				state.context.resume().catch((error) => {
					console.debug("[audio] AudioContext resume deferred:", error);
				});
			}
		};

		const ensureUnlockHandlers = () => {
			if (state.unlockHandlers) {
				return;
			}

			const options = { passive: true };
			state.unlockHandlers = ["pointerdown", "keydown", "touchend"].map((type) => {
				document.addEventListener(type, unlock, options);
				return () => document.removeEventListener(type, unlock, options);
			});
		};

		const removeUnlockHandlers = () => {
			if (!state.unlockHandlers) {
				return;
			}

			state.unlockHandlers.forEach((remove) => remove());
			state.unlockHandlers = null;
		};

		/* Older WebKit only offers the callback form of decodeAudioData. */
		const decodeAudioData = (arrayBuffer) => new Promise((resolve, reject) => {
			const result = state.context.decodeAudioData(arrayBuffer, resolve, reject);

			if (result && typeof result.then === "function") {
				result.then(resolve, reject);
			}
		});

		const decodeClip = (id, clip) => {
			if (clip.buffer) {
				return Promise.resolve(clip.buffer);
			}

			if (clip.promise) {
				return clip.promise;
			}

			if (clip.failed || !clip.bytes) {
				return Promise.reject(new Error("[audio] clip " + id + " is not decodable"));
			}

			/* decodeAudioData detaches the ArrayBuffer it is handed, so hand it
			   over completely instead of keeping a dangling view around. */
			const bytes = clip.bytes;
			clip.bytes = null;

			clip.promise = decodeAudioData(bytes.buffer).then((buffer) => {
				clip.buffer = buffer;
				clip.promise = null;
				return buffer;
			}).catch((error) => {
				clip.failed = true;
				clip.promise = null;
				console.error("[audio] Failed to decode clip", id, error);
				throw error;
			});

			return clip.promise;
		};

		/* Builds BufferSource -> Gain -> StereoPanner -> sfxGain and starts it.
		   The per-play nodes are torn down when the source ends. */
		const startSound = (buffer, volume, pan) => {
			const source = state.context.createBufferSource();
			source.buffer = buffer;

			const gain = state.context.createGain();
			gain.gain.value = clamp(volume, 0, 1);
			source.connect(gain);

			let tail = gain;
			let panner = null;

			/* StereoPannerNode is missing from older WebKit; play centered
			   there rather than failing. */
			if (typeof state.context.createStereoPanner === "function") {
				panner = state.context.createStereoPanner();
				panner.pan.value = clamp(pan, -1, 1);
				gain.connect(panner);
				tail = panner;
			}

			tail.connect(state.sfxGain);

			source.onended = () => {
				source.disconnect();
				gain.disconnect();

				if (panner) {
					panner.disconnect();
				}
			};

			source.start(0);
		};

		const stopMusic = (keepClipId) => {
			const music = state.music;

			if (!music) {
				return;
			}

			state.music = null;

			try {
				music.source.stop();
			} catch (error) {
				console.debug("[audio] Ignoring stop failure during music switch:", error);
			}

			music.source.disconnect();

			/* The C++ side re-registers the nx bytes on every track change, so
			   the decoded PCM of the previous track can be thrown away. */
			const clip = state.clips.get(music.id);

			if (clip && clip.music && music.id !== keepClipId) {
				state.clips.delete(music.id);
			}
		};

		Module.MapleAudio = {
			unlock: unlock,

			close() {
				state.musicToken += 1;
				stopMusic();
				removeUnlockHandlers();
				state.clips.clear();

				if (typeof state.context.close === "function") {
					state.context.close().catch(() => {});
				}

				Module.MapleAudio = null;
			},

			setSfxVolume(value) {
				state.sfxGain.gain.value = clamp(value, 0, 1);
			},

			setBgmVolume(value) {
				state.bgmGain.gain.value = clamp(value, 0, 1);
			},

			/* Copies the encoded bytes out of the wasm heap right away: with
			   the LazyFS-backed nx reader the source pointer is only valid
			   until its chunk is evicted, and the heap itself may move when
			   memory grows. Returns 0 when the clip is known to be broken. */
			registerClip(id, ptr, len, kind) {
				ensureUnlockHandlers();

				let clip = state.clips.get(id);

				if (!clip) {
					clip = {
						bytes: HEAPU8.slice(ptr, ptr + len),
						buffer: null,
						promise: null,
						failed: false,
						music: kind === CLIP_MUSIC,
						pending: null
					};
					state.clips.set(id, clip);
				} else if (clip.failed) {
					return 0;
				}

				/* A clip that is also used as a sound effect must never be
				   evicted by the music path. */
				if (kind !== CLIP_MUSIC) {
					clip.music = false;
				}

				if (kind === CLIP_SFX_PRELOAD) {
					decodeClip(id, clip).catch(() => {});
				}

				return 1;
			},

			playSound(id, volume, pan) {
				const clip = state.clips.get(id);

				if (!clip || clip.failed) {
					return;
				}

				/* Before the first gesture the context is suspended and every
				   start() would queue up and fire as a burst on unlock. */
				if (!state.gestureSeen && state.context.state !== "running") {
					return;
				}

				unlock();

				if (clip.buffer) {
					startSound(clip.buffer, volume, pan);
					return;
				}

				/* Still decoding: remember the latest request only, so a
				   burst of plays during the decode yields one sound. */
				clip.pending = { volume: volume, pan: pan, at: performance.now() };

				decodeClip(id, clip).then((buffer) => {
					const pending = clip.pending;
					clip.pending = null;

					if (pending && performance.now() - pending.at <= PENDING_SFX_TIMEOUT_MS) {
						startSound(buffer, pending.volume, pending.pan);
					}
				}).catch(() => {
					clip.pending = null;
				});
			},

			playMusic(id, loop) {
				const clip = state.clips.get(id);

				if (!clip || clip.failed) {
					return;
				}

				state.musicToken += 1;
				const token = state.musicToken;

				stopMusic(id);

				decodeClip(id, clip).then((buffer) => {
					if (token !== state.musicToken) {
						return;
					}

					/* Starting while suspended is fine: the source begins
					   playing from its start once the first gesture resumes
					   the context, which gives "BGM starts on first click". */
					unlock();

					const source = state.context.createBufferSource();
					source.buffer = buffer;
					source.loop = loop !== 0;
					source.connect(state.bgmGain);

					source.onended = () => {
						source.disconnect();

						if (state.music && state.music.source === source) {
							state.music = null;

							/* A play_once track that ran to its end will not be
							   asked for again, so free its PCM as well. */
							const ended = state.clips.get(id);

							if (ended && ended.music) {
								state.clips.delete(id);
							}
						}
					};

					source.start(0);
					state.music = { id: id, source: source };
				}).catch(() => {});
			},

			stopMusic() {
				state.musicToken += 1;
				stopMusic();
			}
		};

		return 1;
	});

	EM_JS(void, ms_audio_close, (), {
		if (Module.MapleAudio) {
			Module.MapleAudio.close();
		}
	});

	EM_JS(int, ms_audio_set_sfx_volume, (double volume), {
		if (!Module.MapleAudio) {
			return 0;
		}

		Module.MapleAudio.setSfxVolume(volume);
		return 1;
	});

	EM_JS(int, ms_audio_set_bgm_volume, (double volume), {
		if (!Module.MapleAudio) {
			return 0;
		}

		Module.MapleAudio.setBgmVolume(volume);
		return 1;
	});

	EM_JS(int, ms_audio_register_clip, (int id, const void* data, int length, int kind), {
		if (!Module.MapleAudio) {
			return 0;
		}

		return Module.MapleAudio.registerClip(id, data, length, kind);
	});

	EM_JS(void, ms_audio_play_sound, (int id, double volume, double pan), {
		if (Module.MapleAudio) {
			Module.MapleAudio.playSound(id, volume, pan);
		}
	});

	EM_JS(void, ms_audio_play_music, (int id, int loop), {
		if (Module.MapleAudio) {
			Module.MapleAudio.playMusic(id, loop);
		}
	});

	namespace
	{
		// Hands the encoded audio of a node to the JS cache and returns the
		// cache id (nl::audio::id()), or 0 if the node carries no usable audio.
		size_t register_clip(const nl::audio& ad, int kind)
		{
			auto data = reinterpret_cast<const uint8_t*>(ad.data());

			if (!data || ad.length() <= NX_AUDIO_HEADER_SIZE)
				return 0;

			size_t id = ad.id();

			// size_t and pointers are 32 bit on wasm32, so the cast is lossless there.
			int registered = ms_audio_register_clip(
				static_cast<int>(id),
				data + NX_AUDIO_HEADER_SIZE,
				static_cast<int>(ad.length() - NX_AUDIO_HEADER_SIZE),
				kind
			);

			return registered ? id : 0;
		}

		// Registers a sound effect once; the samples map mirrors the BASS
		// version (id -> handle) but the handle is simply the id again since
		// the decoded buffer lives on the JS side.
		size_t cache_sample(std::unordered_map<size_t, uint64_t>& samples, nl::node src, int kind)
		{
			nl::audio ad = src;

			if (!ad.data())
				return 0;

			size_t id = ad.id();

			if (samples.find(id) != samples.end())
				return id;

			id = register_clip(ad, kind);

			if (id)
				samples[id] = id;

			return id;
		}

		// Both Music::play and Music::play_once drive the single BGM channel.
		// The BASS version kept a separate "current path" per function; here one
		// track plays at a time, so a request for the track that is already
		// playing in the same mode is a no-op and anything else replaces it.
		std::string bgmpath;
		bool bgmloop = false;

		void start_music(const std::string& path, bool loop)
		{
			if (path == bgmpath && loop == bgmloop)
				return;

			nl::audio ad = nl::nx::sound.resolve(path);
			size_t id = register_clip(ad, CLIP_MUSIC);

			if (!id)
				return;

			ms_audio_play_music(static_cast<int>(id), loop ? 1 : 0);

			bgmpath = path;
			bgmloop = loop;
		}
	}

	Sound::Sound(Name name)
	{
		id = soundids[name];
	}

	Sound::Sound(int32_t itemid)
	{
		auto fitemid = format_id(itemid);

		if (itemids.find(fitemid) != itemids.end())
		{
			id = itemids.at(fitemid);
		}
		else
		{
			auto pid = (10000 * (itemid / 10000));
			auto fpid = format_id(pid);

			if (itemids.find(fpid) != itemids.end())
				id = itemids.at(fpid);
			else
				id = itemids.at("02000000");
		}
	}

	Sound::Sound(nl::node src)
	{
		id = add_sound(src);
	}

	Sound::Sound()
	{
		id = 0;
	}

	void Sound::play() const
	{
		if (id > 0)
			play(id);
	}

	void Sound::play(Point<int16_t> world_position) const
	{
		if (id == 0)
			return;

		// Distance from the listener (local player). Within FULL_RANGE the
		// sound is at full volume; it then fades linearly to silence at
		// MAX_RANGE and is skipped entirely beyond it.
		constexpr double FULL_RANGE = 400.0;
		constexpr double MAX_RANGE = 1200.0;
		// Horizontal offset that maps to hard-left / hard-right panning.
		constexpr double PAN_RANGE = 800.0;

		double dx = static_cast<double>(world_position.x() - listener_position.x());
		double dy = static_cast<double>(world_position.y() - listener_position.y());
		double distance = std::sqrt(dx * dx + dy * dy);

		if (distance >= MAX_RANGE)
			return; // too far to hear at all

		float volume = 1.0f;

		if (distance > FULL_RANGE)
			volume = static_cast<float>((MAX_RANGE - distance) / (MAX_RANGE - FULL_RANGE));

		float pan = static_cast<float>(dx / PAN_RANGE);

		if (pan < -1.0f)
			pan = -1.0f;
		else if (pan > 1.0f)
			pan = 1.0f;

		play(id, volume, pan);
	}

	Error Sound::init()
	{
		if (!ms_audio_init())
			return Error::Code::AUDIO;

		nl::node uisrc = nl::nx::sound["UI.img"];

		add_sound(Sound::Name::BUTTONCLICK, uisrc["BtMouseClick"]);
		add_sound(Sound::Name::BUTTONOVER, uisrc["BtMouseOver"]);
		add_sound(Sound::Name::CHARSELECT, uisrc["CharSelect"]);
		add_sound(Sound::Name::DLGNOTICE, uisrc["DlgNotice"]);
		add_sound(Sound::Name::MENUDOWN, uisrc["MenuDown"]);
		add_sound(Sound::Name::MENUUP, uisrc["MenuUp"]);
		add_sound(Sound::Name::RACESELECT, uisrc["RaceSelect"]);
		add_sound(Sound::Name::SCROLLUP, uisrc["ScrollUp"]);
		add_sound(Sound::Name::SELECTMAP, uisrc["SelectMap"]);
		add_sound(Sound::Name::TAB, uisrc["Tab"]);
		add_sound(Sound::Name::WORLDSELECT, uisrc["WorldSelect"]);
		add_sound(Sound::Name::DRAGSTART, uisrc["DragStart"]);
		add_sound(Sound::Name::DRAGEND, uisrc["DragEnd"]);
		add_sound(Sound::Name::WORLDMAPOPEN, uisrc["WorldmapOpen"]);
		add_sound(Sound::Name::WORLDMAPCLOSE, uisrc["WorldmapClose"]);

		nl::node gamesrc = nl::nx::sound["Game.img"];

		add_sound(Sound::Name::GAMESTART, gamesrc["GameIn"]);
		add_sound(Sound::Name::JUMP, gamesrc["Jump"]);
		add_sound(Sound::Name::DROP, gamesrc["DropItem"]);
		add_sound(Sound::Name::PICKUP, gamesrc["PickUpItem"]);
		add_sound(Sound::Name::PORTAL, gamesrc["Portal"]);
		add_sound(Sound::Name::LEVELUP, gamesrc["LevelUp"]);
		add_sound(Sound::Name::HURTDAMAGE, gamesrc["Damage"]);
		add_sound(Sound::Name::QUESTCOMPLETE, gamesrc["QuestClear"]);
		add_sound(Sound::Name::QUESTALERT, gamesrc["QuestAlert"]);
		add_sound(Sound::Name::QUESTCOUNT, gamesrc["questCount"]);
		add_sound(Sound::Name::TOMBSTONE, gamesrc["Tombstone"]);

		nl::node itemsrc = nl::nx::sound["Item.img"];

		for (auto node : itemsrc)
			add_sound(node.name(), node["Use"]);

		uint8_t volume = Setting<SFXVolume>::get().load();

		if (!set_sfxvolume(volume))
			return Error::Code::AUDIO;

		return Error::Code::NONE;
	}

	void Sound::close()
	{
		ms_audio_close();
	}

	bool Sound::set_sfxvolume(uint8_t vol)
	{
		// Settings are 0..100; gain is linear 0..1 like BASS_CONFIG_GVOL_SAMPLE.
		return ms_audio_set_sfx_volume(static_cast<double>(vol) / 100.0) != 0;
	}

	void Sound::play(size_t id)
	{
		play(id, 1.0f, 0.0f);
	}

	void Sound::play(size_t id, float volume, float pan)
	{
		if (!samples.count(id))
			return;

		// Per-play volume multiplies with the global SFX gain, so the user's
		// volume setting is still respected.
		ms_audio_play_sound(static_cast<int>(id), static_cast<double>(volume), static_cast<double>(pan));
	}

	size_t Sound::add_sound(nl::node src)
	{
		// Decoded right away, like BASS_SampleLoad: these are UI, skill, mob
		// and weapon sounds that are expected to play without a first-use delay.
		return cache_sample(samples, src, CLIP_SFX_PRELOAD);
	}

	void Sound::add_sound(Name name, nl::node src)
	{
		size_t id = add_sound(src);

		if (id)
			soundids[name] = id;
	}

	void Sound::add_sound(std::string itemid, nl::node src)
	{
		// The whole Item.img table is registered at startup; decoding hundreds
		// of clips nobody may ever use would stall page load, so these decode
		// on first play instead.
		size_t id = cache_sample(samples, src, CLIP_SFX_LAZY);

		if (id)
			itemids[itemid] = id;
	}

	std::string Sound::format_id(int32_t itemid)
	{
		std::string strid = std::to_string(itemid);
		if (strid.size() < 8)
			strid.insert(0, 8 - strid.size(), '0');

		return strid;
	}

	void Sound::set_listener_position(Point<int16_t> position)
	{
		listener_position = position;
	}

	std::unordered_map<size_t, uint64_t> Sound::samples;
	EnumMap<Sound::Name, size_t> Sound::soundids;
	std::unordered_map<std::string, size_t> Sound::itemids;
	Point<int16_t> Sound::listener_position;

	Music::Music(std::string p)
	{
		path = p;
	}

	void Music::play() const
	{
		start_music(path, true);
	}

	void Music::play_once() const
	{
		start_music(path, false);
	}

	Error Music::init()
	{
		// Sound::init created the context; if it did not run, the volume call
		// fails and the same AUDIO error is reported as by the BASS version.
		uint8_t volume = Setting<BGMVolume>::get().load();

		if (!set_bgmvolume(volume))
			return Error::Code::AUDIO;

		return Error::Code::NONE;
	}

	bool Music::set_bgmvolume(uint8_t vol)
	{
		return ms_audio_set_bgm_volume(static_cast<double>(vol) / 100.0) != 0;
	}
}

#endif // MS_PLATFORM_WASM
