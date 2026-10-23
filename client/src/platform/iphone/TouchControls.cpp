//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "TouchControls.h"

#include "../shared/PlatformConfig.h"
#include "../shared/KeyCodes.h"
#include "../shared/GLCompat.h"

#include "IO/MobileUI.h"
#include "IO/NotificationCenter.h"
#include "IO/UI.h"
#include "IO/UITypes/UIChatBar.h"
#include "IO/UITypes/UIMobileInbox.h"
#include "IO/UITypes/UIMobileKeyConfig.h"
#include "IO/UITypes/UIMobileMenu.h"
#include "Constants.h"
#include "Gameplay/Stage.h"
#include "Data/ItemData.h"
#include "Data/SkillData.h"
#include "Graphics/GraphicsGL.h"
#include "Graphics/Text.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "IOSPlatform.h"

namespace ms
{
	namespace TouchControls
	{
		namespace
		{
			enum class Style { PRIMARY, ACTION, SKILL, SMALL, MENU, CHAT };

			struct Button
			{
				std::string label;
				int32_t key;
				Style style;
				float r, g, b;
				float x = 0.0f;
				float y = 0.0f;
				float radius = 0.0f;
				// Chips (the menu row and the small actions) are rounded
				// rectangles: half-extents in pixels.
				float half_w = 0.0f;
				float half_h = 0.0f;
				int64_t finger = -1;
			};

			// A direction pad: four arrow pads around a hub. The thumb can slide
			// from one to the next, pressing each arrow in turn (Aran's commands
			// are arrow sequences).
			struct DPad
			{
				float x = 0.0f;
				float y = 0.0f;
				// Centre to the outer edge of an arm, an arm's side, and the
				// distance from the centre to an arm's middle.
				float radius = 0.0f;
				float side = 0.0f;
				float reach = 0.0f;
				int64_t finger = -1;
				// Left, right, up, down.
				bool held[4] = { false, false, false, false };
			};

			// Not keys: these open panels themselves, whatever the keyboard
			// bindings say.
			constexpr int32_t KEY_OPEN_MENU = -106;
			constexpr int32_t KEY_TOGGLE_CHAT = -107;
			constexpr int32_t KEY_OPEN_INBOX = -108;

			constexpr int32_t PAD_KEYS[4] = { GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN };

			// Cosmic's default v83 keymap (GameConstants.DEFAULT_KEY): Ctrl
			// attack, Alt jump, Z pick up, X sit, Space talk to NPCs. The ten
			// action buttons press keys, and the Touch Controls panel
			// (UIMobileKeyConfig::SLOTS) decides what each key does - ATTACK
			// can become a skill after the first job advancement. A button
			// shows what its key does.
			std::vector<Button> buttons = {
				{ "ATTACK", GLFW_KEY_LEFT_CONTROL, Style::PRIMARY, 0.95f, 0.35f, 0.30f },
				{ "JUMP", GLFW_KEY_LEFT_ALT, Style::ACTION, 0.35f, 0.65f, 1.00f },
				{ "LOOT", GLFW_KEY_Z, Style::ACTION, 1.00f, 0.80f, 0.25f },
				{ "1", GLFW_KEY_LEFT_SHIFT, Style::SKILL, 0.85f, 0.85f, 0.90f },
				{ "2", GLFW_KEY_INSERT, Style::SKILL, 0.85f, 0.85f, 0.90f },
				{ "3", GLFW_KEY_HOME, Style::SKILL, 0.85f, 0.85f, 0.90f },
				{ "4", GLFW_KEY_PAGE_UP, Style::SKILL, 0.85f, 0.85f, 0.90f },
				{ "5", GLFW_KEY_DELETE, Style::SKILL, 0.85f, 0.85f, 0.90f },
				{ "6", GLFW_KEY_END, Style::SKILL, 0.85f, 0.85f, 0.90f },
				{ "7", GLFW_KEY_PAGE_DOWN, Style::SKILL, 0.85f, 0.85f, 0.90f },
				// Opens the chat window on the left, above the direction pad.
				{ "CHAT", KEY_TOGGLE_CHAT, Style::CHAT, 1.00f, 0.60f, 0.20f },
				// Unanswered invites and alerts; only there while some wait.
				{ "Inbox", KEY_OPEN_INBOX, Style::MENU, 0.33f, 0.60f, 1.00f },
				// The button along the top: every panel is in the menu.
				{ "Menu", KEY_OPEN_MENU, Style::MENU, 1.00f, 0.60f, 0.20f },
			};

			bool is_chip(const Button& b)
			{
				return b.style == Style::MENU || b.style == Style::SMALL;
			}

			DPad pad;

			int panel_w = 0;
			int panel_h = 0;
			int fit_x = 0, fit_y = 0, fit_w = 0, fit_h = 0;
			float pt = 1.0f;
			Insets safe;
			bool overlapping = false;
			bool gamepad = false;

			GLuint program = 0;
			GLuint vbo = 0;
			GLint u_screen = -1;

			constexpr GLuint A_POS = 8, A_UV = 9, A_COLOR = 10, A_PAR = 11;

			Button* find(const std::string& label)
			{
				for (Button& b : buttons)
					if (b.label == label)
						return &b;

				return nullptr;
			}

			void place(const std::string& label, float x, float y, float radius)
			{
				if (Button* b = find(label))
				{
					b->x = x;
					b->y = y;
					b->radius = radius;
				}
			}

			// Labels and icons go through the game's own renderer, in a space
			// scaled so its fonts read at a comfortable size.
			float label_scale()
			{
				return std::max(1.0f, pt * 0.92f);
			}

			bool chips_measured = false;
			float menu_top = 0.0f;
			// A panel covering the whole screen leaves the Menu button no room.
			bool menu_room = true;

			// The Menu button sits in the top-right corner of lo..hi: the
			// screen, or the part an open side panel leaves. Inbox goes on
			// its left.
			void place_menu(float, float hi)
			{
				float right = hi - 8.0f * pt;

				for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
				{
					Button& b = *it;

					if (b.style != Style::MENU)
						continue;

					b.x = right - b.half_w;
					b.y = menu_top;
					right = b.x - b.half_w - 8.0f * pt;
				}
			}

			void layout()
			{
				float lo = safe.left;
				float rhi = panel_w - safe.right;
				float top = safe.top;

				// The controls float over the game, down to the bottom edge: the
				// phone's HUD is in the top-left corner, not along the bottom.
				float bottom = panel_h - std::max(safe.bottom, 6.0f * pt);
				overlapping = true;

				float r = 27.0f * pt;
				float row = r * 2.2f;

				float outer = rhi - r * 1.55f - 4.0f * pt;
				float inner = outer - r * 2.35f;

				float jump_y = bottom - r * 1.05f;
				float attack_y = jump_y - r * 1.2f;

				place("ATTACK", outer, attack_y, r * 1.25f);
				place("JUMP", inner, jump_y, r);
				place("LOOT", inner - r * 2.1f, jump_y + r * 0.1f, r * 0.82f);
				place("1", outer, attack_y - row * 1.05f, r * 0.82f);
				place("2", inner, jump_y - row * 1.0f, r * 0.82f);
				place("3", outer, attack_y - row * 1.95f, r * 0.82f);
				place("4", inner, jump_y - row * 1.9f, r * 0.82f);
				place("5", inner - r * 2.1f, jump_y - row * 1.0f, r * 0.82f);
				place("6", inner - r * 2.1f, jump_y - row * 1.9f, r * 0.82f);
				place("7", outer, attack_y - row * 2.85f, r * 0.82f);

				// The Menu button in the top-right corner (the HUD holds the
				// top-left).
				float chip_h = 17.0f * pt;
				float k = label_scale();

				for (Button& b : buttons)
				{
					if (b.style != Style::MENU)
						continue;

					b.half_w = (MobileUI::text_width(b.label, MobileUI::Font::BODY_BOLD) * k) * 0.5f + 16.0f * pt;
					b.half_h = chip_h;
					b.radius = b.half_h;
				}

				menu_top = top + chip_h + 6.0f * pt;
				place_menu(lo, rhi);

				pad.side = 40.0f * pt;
				pad.reach = 42.0f * pt;
				pad.radius = pad.reach + pad.side * 0.5f;
				pad.x = lo + pad.radius + 18.0f * pt;
				pad.y = bottom - pad.radius - 2.0f * pt;

				// Chat sits above the pad, lined up with its left edge; the
				// window it opens fills the space above (UIChatBarMobile).
				if (Button* chat = find("CHAT"))
				{
					chat->radius = r * 0.82f;
					chat->x = pad.x - pad.radius + chat->radius;
					chat->y = pad.y - pad.radius - 16.0f * pt - chat->radius;
				}
			}

			bool chat_open()
			{
				auto chatbar = UI::get().get_element<UIChatBar>();

				return chatbar && chatbar->is_chatopen();
			}

			uint16_t chat_unread()
			{
				auto chatbar = UI::get().get_element<UIChatBar>();

				return chatbar ? chatbar->unread() : 0;
			}

			void press(int32_t key, bool down)
			{
				UI::get().send_key(key, down);
			}

			void set_pad_keys(const bool (&want)[4])
			{
				// Releases first, so sliding from one arm to the next lets go of
				// the old arrow before pressing the new one.
				for (int pass = 0; pass < 2; pass++)
				{
					for (int i = 0; i < 4; i++)
					{
						if (want[i] != pad.held[i] && want[i] == (pass == 1))
						{
							pad.held[i] = want[i];
							press(PAD_KEYS[i], want[i]);
						}
					}
				}
			}

			void move_pad(float px, float py)
			{
				float dx = px - pad.x;
				float dy = py - pad.y;
				float len = std::sqrt(dx * dx + dy * dy);
				bool want[4] = { false, false, false, false };

				// The hub presses nothing.
				if (len > pad.side * 0.35f)
				{
					// Straight within 30 degrees of an arm, both arrows in
					// between - so walking doesn't graze Up (portals, ropes) or
					// Down (lie down).
					float ax = std::fabs(dx) / len;
					float ay = std::fabs(dy) / len;

					if (ax >= 0.5f)
						want[dx < 0 ? 0 : 1] = true;

					if (ay >= 0.5f)
						want[dy < 0 ? 2 : 3] = true;
				}

				set_pad_keys(want);
			}

			void release_pad()
			{
				const bool none[4] = { false, false, false, false };

				pad.finger = -1;
				set_pad_keys(none);
			}

			// The pad stays put in the lower left and only takes a thumb that
			// lands on it (with a little slack), so NPCs and everything else on
			// the left of the screen stay tappable.
			bool on_pad(float px, float py)
			{
				float dx = px - pad.x;
				float dy = py - pad.y;
				float reach = pad.radius * 1.25f;

				return dx * dx + dy * dy <= reach * reach;
			}

			// While a side panel or an NPC conversation is open only the menu
			// row stays: it switches panels; walking and fighting wait. An
			// item menu or a notice card hides everything (see shown()).
			bool panels_open()
			{
				return UI::get().side_panel_open() || UI::get().npc_talk_open() || UI::get().popover_open();
			}

			bool shown(const Button& b)
			{
				// An item menu or a question sits on top: nothing may cover it.
				if (UI::get().popover_open())
					return false;

				// The chat window has its own close button over this one.
				if (b.style == Style::CHAT && chat_open())
					return false;

				if (b.key == KEY_OPEN_INBOX && NotificationCenter::get().empty())
					return false;

				if (b.style == Style::MENU)
					return menu_room;

				return !panels_open();
			}

			// ---- drawing -------------------------------------------------

			const char* VERTEX = R"(
				attribute vec2 a_pos;
				attribute vec2 a_uv;
				attribute vec4 a_color;
				attribute vec2 a_par;
				uniform vec2 u_screen;
				varying vec2 v_uv;
				varying vec4 v_color;
				varying vec2 v_par;
				void main()
				{
					v_uv = a_uv;
					v_color = a_color;
					v_par = a_par;
					gl_Position = vec4(a_pos.x / u_screen.x * 2.0 - 1.0, 1.0 - a_pos.y / u_screen.y * 2.0, 0.0, 1.0);
				})";

			// A disc (inner = 0) or ring (inner = radius ratio of the hole),
			// antialiased over v_par.y in uv units.
			const char* FRAGMENT = R"(
				precision mediump float;
				varying vec2 v_uv;
				varying vec4 v_color;
				varying vec2 v_par;
				void main()
				{
					float d = length(v_uv);
					float aa = v_par.y;
					float a = 1.0 - smoothstep(1.0 - aa, 1.0, d);
					if (v_par.x > 0.0)
						a *= smoothstep(v_par.x - aa, v_par.x, d);
					gl_FragColor = vec4(v_color.rgb, v_color.a * a);
				})";

			GLuint compile(GLenum type, const char* source)
			{
				GLuint shader = glCreateShader(type);
				glShaderSource(shader, 1, &source, nullptr);
				glCompileShader(shader);

				GLint ok = 0;
				glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

				if (!ok)
				{
					char log[512] = { 0 };
					glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
					printf("[!] touch controls shader: %s\n", log);
				}

				return shader;
			}

			bool ensure_program()
			{
				if (program)
					return true;

				program = glCreateProgram();
				glAttachShader(program, compile(GL_VERTEX_SHADER, VERTEX));
				glAttachShader(program, compile(GL_FRAGMENT_SHADER, FRAGMENT));

				// Away from 0/1, which the main renderer's attributes use and
				// whose pointers it sets only once.
				glBindAttribLocation(program, A_POS, "a_pos");
				glBindAttribLocation(program, A_UV, "a_uv");
				glBindAttribLocation(program, A_COLOR, "a_color");
				glBindAttribLocation(program, A_PAR, "a_par");
				glLinkProgram(program);

				GLint ok = 0;
				glGetProgramiv(program, GL_LINK_STATUS, &ok);

				if (!ok)
				{
					printf("[!] touch controls program failed to link\n");
					glDeleteProgram(program);
					program = 0;
					return false;
				}

				u_screen = glGetUniformLocation(program, "u_screen");
				glGenBuffers(1, &vbo);

				return true;
			}

			std::vector<float> verts;

			void disc(float x, float y, float radius, float r, float g, float b, float a, float hole = 0.0f)
			{
				if (radius <= 0.0f)
					return;

				float aa = std::min(0.5f, 1.5f / radius);
				float x0 = x - radius, x1 = x + radius;
				float y0 = y - radius, y1 = y + radius;

				const float quad[6][4] = {
					{ x0, y0, -1, -1 }, { x1, y0, 1, -1 }, { x1, y1, 1, 1 },
					{ x0, y0, -1, -1 }, { x1, y1, 1, 1 }, { x0, y1, -1, 1 }
				};

				for (const auto& v : quad)
				{
					verts.insert(verts.end(), { v[0], v[1], v[2], v[3], r, g, b, a, hole, aa });
				}
			}

			void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
				float r, float g, float b, float a, float aa)
			{
				const float q[6][4] = {
					{ x0, y0, u0, v0 }, { x1, y0, u1, v0 }, { x1, y1, u1, v1 },
					{ x0, y0, u0, v0 }, { x1, y1, u1, v1 }, { x0, y1, u0, v1 }
				};

				for (const auto& v : q)
					verts.insert(verts.end(), { v[0], v[1], v[2], v[3], r, g, b, a, 0.0f, aa });
			}

			// A rounded chip: two half discs and the flat middle, so no part
			// is blended twice.
			void capsule(float cx, float cy, float hw, float hh, float r, float g, float b, float a)
			{
				float aa = std::min(0.5f, 1.5f / hh);
				float left = cx - hw + hh;
				float right = cx + hw - hh;

				quad(left - hh, cy - hh, left, cy + hh, -1.0f, -1.0f, 0.0f, 1.0f, r, g, b, a, aa);
				quad(right, cy - hh, right + hh, cy + hh, 0.0f, -1.0f, 1.0f, 1.0f, r, g, b, a, aa);

				if (right > left)
					quad(left, cy - hh, right, cy + hh, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a, aa);
			}

			// A rounded rectangle: corner quarter discs, edge strips and the middle.
			void rounded_rect(float x0, float y0, float x1, float y1, float rad, float r, float g, float b, float a)
			{
				rad = std::min({ rad, (x1 - x0) * 0.5f, (y1 - y0) * 0.5f });

				if (rad <= 0.0f)
					return;

				float aa = std::min(0.5f, 1.5f / rad);
				float xa = x0 + rad, xb = x1 - rad;
				float ya = y0 + rad, yb = y1 - rad;

				quad(x0, y0, xa, ya, -1.0f, -1.0f, 0.0f, 0.0f, r, g, b, a, aa);
				quad(xb, y0, x1, ya, 0.0f, -1.0f, 1.0f, 0.0f, r, g, b, a, aa);
				quad(x0, yb, xa, y1, -1.0f, 0.0f, 0.0f, 1.0f, r, g, b, a, aa);
				quad(xb, yb, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f, r, g, b, a, aa);
				quad(xa, y0, xb, ya, 0.0f, -1.0f, 0.0f, 0.0f, r, g, b, a, aa);
				quad(xa, yb, xb, y1, 0.0f, 0.0f, 0.0f, 1.0f, r, g, b, a, aa);
				quad(x0, ya, xa, yb, -1.0f, 0.0f, 0.0f, 0.0f, r, g, b, a, aa);
				quad(xb, ya, x1, yb, 0.0f, 0.0f, 1.0f, 0.0f, r, g, b, a, aa);
				quad(xa, ya, xb, yb, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a, aa);
			}

			void triangle(float ax, float ay, float bx, float by, float cx, float cy, float r, float g, float b, float a)
			{
				const float pts[3][2] = { { ax, ay }, { bx, by }, { cx, cy } };

				for (const auto& v : pts)
					verts.insert(verts.end(), { v[0], v[1], 0.0f, 0.0f, r, g, b, a, 0.0f, 0.5f });
			}

			// Four arrow pads around a hub; a pressed arrow lights up.
			void draw_pad(float dim)
			{
				// Left, right, up, down: the unit direction of each arm.
				const float dirs[4][2] = { { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } };
				float h = pad.side * 0.5f;
				float corner = pad.side * 0.26f;
				float border = 2.0f * pt;

				// A faint disc ties the four pads together.
				disc(pad.x, pad.y, pad.radius + 6.0f * pt, 0.0f, 0.0f, 0.0f, 0.22f * dim);
				disc(pad.x, pad.y, pad.side * 0.30f, 1.0f, 1.0f, 1.0f, 0.18f * dim);

				for (int i = 0; i < 4; i++)
				{
					float cx = pad.x + dirs[i][0] * pad.reach;
					float cy = pad.y + dirs[i][1] * pad.reach;
					bool lit = pad.held[i];

					// A light rim, then the face inset by the border width.
					rounded_rect(cx - h, cy - h, cx + h, cy + h, corner, 1.0f, 1.0f, 1.0f, (lit ? 0.95f : 0.55f) * dim);

					if (lit)
						rounded_rect(cx - h + border, cy - h + border, cx + h - border, cy + h - border, corner - border,
							0.95f, 0.97f, 1.0f, 0.90f);
					else
						rounded_rect(cx - h + border, cy - h + border, cx + h - border, cy + h - border, corner - border,
							0.06f, 0.07f, 0.11f, 0.78f * dim);

					// The arrow, pointing out from the hub.
					float ink = lit ? 0.10f : 1.0f;
					float ox = dirs[i][0], oy = dirs[i][1];
					float px = -oy, py = ox;
					float tip = h * 0.40f, back = h * 0.22f, wing = h * 0.38f;

					triangle(cx + ox * tip, cy + oy * tip,
						cx - ox * back + px * wing, cy - oy * back + py * wing,
						cx - ox * back - px * wing, cy - oy * back - py * wing,
						ink, ink, ink, lit ? 1.0f : 0.92f * dim);
				}
			}

			// Whether the menu is showing (its button lights up).
			bool chip_open(const Button& b)
			{
				if (b.key == KEY_OPEN_INBOX)
				{
					auto inbox = UI::get().get_element<UIMobileInbox>();

					return inbox && inbox->is_active();
				}

				if (b.key != KEY_OPEN_MENU)
					return false;

				auto menu = UI::get().get_element<UIMobileMenu>();

				return menu && menu->is_active();
			}

			void draw_shapes()
			{
				verts.clear();

				float dim = 0.80f;

				if (!panels_open())
					draw_pad(dim);

				for (const Button& b : buttons)
				{
					if (!shown(b))
						continue;

					bool held = b.finger >= 0;

					if (is_chip(b))
					{
						if (held || chip_open(b))
							capsule(b.x, b.y, b.half_w, b.half_h, b.r, b.g, b.b, held ? 1.0f : 0.92f);
						else
							capsule(b.x, b.y, b.half_w, b.half_h, 0.05f, 0.06f, 0.09f, 0.66f);

						continue;
					}

					if (b.style == Style::CHAT)
					{
						bool lit = held || chat_open();

						if (lit)
							disc(b.x, b.y, b.radius, b.r, b.g, b.b, 0.88f);
						else
							disc(b.x, b.y, b.radius, 0.05f, 0.06f, 0.09f, 0.55f);

						disc(b.x, b.y, b.radius, 1.0f, 1.0f, 1.0f, lit ? 0.90f : 0.55f, 0.92f);
						continue;
					}

					float fill = held ? 0.55f : 0.30f;

					disc(b.x, b.y, b.radius, b.r * 0.35f, b.g * 0.35f, b.b * 0.35f, fill * dim);
					disc(b.x, b.y, b.radius, b.r, b.g, b.b, (held ? 0.95f : 0.70f) * dim, 0.90f);
				}

				if (verts.empty() || !ensure_program())
					return;

				glUseProgram(program);
				glUniform2f(u_screen, static_cast<float>(panel_w), static_cast<float>(panel_h));

				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);

				const GLsizei stride = 10 * sizeof(float);
				glEnableVertexAttribArray(A_POS);
				glEnableVertexAttribArray(A_UV);
				glEnableVertexAttribArray(A_COLOR);
				glEnableVertexAttribArray(A_PAR);
				glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, stride, (const void*)0);
				glVertexAttribPointer(A_UV, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(2 * sizeof(float)));
				glVertexAttribPointer(A_COLOR, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(4 * sizeof(float)));
				glVertexAttribPointer(A_PAR, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(8 * sizeof(float)));

				glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 10));

				glDisableVertexAttribArray(A_POS);
				glDisableVertexAttribArray(A_UV);
				glDisableVertexAttribArray(A_COLOR);
				glDisableVertexAttribArray(A_PAR);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			// What an action button is bound to, as an icon - the player's
			// own key bindings, so a button shows the skill or potion it fires.
			const Texture* bound_icon(int32_t key)
			{
				Keyboard::Mapping m = UI::get().get_keyboard().get_mapping(key);

				if (m.type == KeyType::Id::SKILL && m.action > 0)
				{
					const Texture& t = SkillData::get(m.action).get_icon(SkillData::Icon::NORMAL);
					return t.is_valid() ? &t : nullptr;
				}

				if ((m.type == KeyType::Id::ITEM || m.type == KeyType::Id::CASH) && m.action > 0)
				{
					const Texture& t = ItemData::get(m.action).get_icon(false);
					return t.is_valid() ? &t : nullptr;
				}

				return nullptr;
			}

			// What an action button shows: a skill or item icon, a basic
			// action's name, or its number while nothing is bound.
			std::string action_text(const Button& b, const Keyboard::Mapping& m)
			{
				if (m.type == KeyType::Id::ACTION)
					if (const char* label = UIMobileKeyConfig::action_label(m.action))
						return label;

				return b.style == Style::SKILL ? b.label : (m.type == KeyType::Id::NONE ? "-" : b.label);
			}

			// A speech bubble with three dots, and how many lines came in while
			// the window was shut.
			void draw_chat_glyph(const Button& b, int16_t cx, int16_t cy, float k)
			{
				int16_t rr = static_cast<int16_t>(b.radius / k);
				int16_t bw = static_cast<int16_t>(rr * 1.10f);
				int16_t bh = static_cast<int16_t>(rr * 0.80f);
				MobileUI::Rect bubble = MobileUI::rect(static_cast<int16_t>(cx - bw / 2), static_cast<int16_t>(cy - bh / 2 - 2), bw, bh);
				bool lit = b.finger >= 0 || chat_open();
				MobileUI::RGBA ink = lit ? MobileUI::RGBA{ 0.13f, 0.08f, 0.03f, 1.0f } : MobileUI::RGBA{ 0.96f, 0.97f, 1.0f, 0.95f };
				MobileUI::RGBA dots = lit ? MobileUI::RGBA{ 1.0f, 0.60f, 0.20f, 1.0f } : MobileUI::RGBA{ 0.10f, 0.12f, 0.18f, 1.0f };

				MobileUI::fill(bubble, ink, static_cast<int16_t>(bh / 2 - 1));

				int16_t tail = static_cast<int16_t>(std::max(4, bh * 2 / 5));
				GraphicsGL::get().drawrotatedrect(static_cast<int16_t>(bubble.left() + bw * 3 / 10), static_cast<int16_t>(bubble.bottom() - 1),
					tail, tail, 0.785f, ink.r, ink.g, ink.b, ink.a);

				int16_t d = static_cast<int16_t>(std::max(3, bh / 5));
				int16_t gap = static_cast<int16_t>(d * 2);
				int16_t dy = static_cast<int16_t>(MobileUI::centre(bubble).y() - d / 2);

				for (int i = -1; i <= 1; i++)
					MobileUI::fill(MobileUI::rect(static_cast<int16_t>(cx + i * gap - d / 2), dy, d, d), dots, static_cast<int16_t>(d / 2));

				uint16_t unread = chat_unread();

				if (unread > 0)
				{
					std::string n = unread > 9 ? "9+" : std::to_string(unread);
					int16_t w = static_cast<int16_t>(std::max<int>(18, MobileUI::text_width(n, MobileUI::Font::SMALL_BOLD) + 10));
					MobileUI::Rect badge = MobileUI::rect(static_cast<int16_t>(cx + rr - w + 4), static_cast<int16_t>(cy - rr - 4), w, 18);
					MobileUI::fill(badge, MobileUI::Theme::RED, 9);
					MobileUI::text_in(n, MobileUI::Font::SMALL_BOLD, MobileUI::Theme::TEXT, badge);
				}
			}

			void draw_labels()
			{
				float k = label_scale();
				int16_t w = static_cast<int16_t>(panel_w / k);
				int16_t h = static_cast<int16_t>(panel_h / k);

				GraphicsGL::get().begin_overlay(w, h);

				const Keyboard& keyboard = UI::get().get_keyboard();

				for (const Button& b : buttons)
				{
					if (!shown(b))
						continue;

					int16_t cx = static_cast<int16_t>(b.x / k);
					int16_t cy = static_cast<int16_t>(b.y / k);

					if (is_chip(b))
					{
						bool lit = b.finger >= 0 || chip_open(b);
						MobileUI::Rect r = MobileUI::rect(static_cast<int16_t>((b.x - b.half_w) / k), static_cast<int16_t>((b.y - b.half_h) / k),
							static_cast<int16_t>(b.half_w * 2.0f / k), static_cast<int16_t>(b.half_h * 2.0f / k));
						MobileUI::text_in(b.label, MobileUI::Font::BODY_BOLD,
							lit ? MobileUI::RGBA{ 0.13f, 0.08f, 0.03f, 1.0f } : MobileUI::Theme::TEXT, r);

						// How many are waiting, on the chip's corner.
						if (b.key == KEY_OPEN_INBOX)
						{
							size_t waiting = NotificationCenter::get().list().size();
							std::string n = waiting > 9 ? "9+" : std::to_string(waiting);
							int16_t bw = static_cast<int16_t>(std::max<int>(18, MobileUI::text_width(n, MobileUI::Font::SMALL_BOLD) + 10));
							MobileUI::Rect badge = MobileUI::rect(static_cast<int16_t>(r.right() - bw + 6), static_cast<int16_t>(r.top() - 5), bw, 18);
							MobileUI::fill(badge, MobileUI::Theme::RED, 9);
							MobileUI::text_in(n, MobileUI::Font::SMALL_BOLD, MobileUI::Theme::TEXT, badge);
						}

						continue;
					}

					if (b.style == Style::CHAT)
					{
						draw_chat_glyph(b, cx, cy, k);
						continue;
					}

					Keyboard::Mapping m = keyboard.get_mapping(b.key);
					const Texture* icon = bound_icon(b.key);
					int16_t rr = static_cast<int16_t>(b.radius / k);
					MobileUI::Rect disc = MobileUI::rect(static_cast<int16_t>(cx - rr), static_cast<int16_t>(cy - rr),
						static_cast<int16_t>(rr * 2), static_cast<int16_t>(rr * 2));

					if (icon)
					{
						int16_t side = static_cast<int16_t>(b.radius * 1.15f / k);
						Point<int16_t> origin = icon->get_origin();
						Point<int16_t> top_left(cx - side / 2, cy - side / 2 - (b.style == Style::SKILL ? 3 : 0));

						icon->draw(DrawArgument(top_left + origin, Point<int16_t>(side, side)));

						if (b.style == Style::SKILL)
							MobileUI::text(b.label, MobileUI::Font::SMALL_BOLD, MobileUI::Theme::TEXT,
								Point<int16_t>(cx, static_cast<int16_t>(cy + side / 2 - 7)), Text::Alignment::CENTER);
					}
					else
					{
						std::string label = action_text(b, m);
						bool idle = m.type == KeyType::Id::NONE;
						MobileUI::text_in(label, b.style == Style::PRIMARY ? MobileUI::Font::BODY_BOLD : MobileUI::Font::SMALL_BOLD,
							idle ? MobileUI::Theme::TEXT.alpha(0.55f) : MobileUI::Theme::TEXT, disc);
					}
				}

				GraphicsGL::get().end_overlay();
			}

			void open_panel(int32_t key)
			{
				switch (key)
				{
					case KEY_OPEN_MENU: MobileUI::open_menu(); break;
					case KEY_OPEN_INBOX: MobileUI::open_inbox(); break;
					case KEY_TOGGLE_CHAT: MobileUI::toggle_chat(); break;
				}
			}

			void haptic()
			{
				IOSPlatform::haptic_tap();
			}
		}

		void set_geometry(int pw, int ph, int fx, int fy, int fw, int fh, float px_per_pt, Insets insets)
		{
			if (pw == panel_w && ph == panel_h && fx == fit_x && fy == fit_y && fw == fit_w && fh == fit_h
				&& px_per_pt == pt && insets.left == safe.left && insets.right == safe.right
				&& insets.top == safe.top && insets.bottom == safe.bottom)
				return;

			panel_w = pw;
			panel_h = ph;
			fit_x = fx;
			fit_y = fy;
			fit_w = fw;
			fit_h = fh;
			pt = px_per_pt > 0.0f ? px_per_pt : 1.0f;
			safe = insets;

			layout();
		}

		void set_gamepad(bool connected)
		{
			if (connected && !gamepad)
				release_all();

			gamepad = connected;
		}

		bool active()
		{
			// MV83_SHOW_CONTROLS=1 draws them anywhere - for checking the
			// layout in the Simulator.
			static const bool forced = getenv("MV83_SHOW_CONTROLS") != nullptr;

			if (forced)
				return panel_w > 0;

			return panel_w > 0 && !gamepad && UI::get().get_state_id() == UI::State::GAME;
		}

		bool finger_down(int64_t id, float px, float py)
		{
			if (!active())
				return false;

			for (Button& b : buttons)
			{
				if (!shown(b))
					continue;

				float dx = px - b.x;
				float dy = py - b.y;
				float reach = b.radius * 1.25f;
				bool inside = is_chip(b)
					? std::fabs(dx) <= b.half_w + 3.0f * pt && std::fabs(dy) <= b.half_h + 6.0f * pt
					: dx * dx + dy * dy <= reach * reach;

				if (b.finger < 0 && inside)
				{
					b.finger = id;

					if (b.key < 0)
					{
						open_panel(b.key);
						haptic();
						return true;
					}

					press(b.key, true);
					haptic();
					return true;
				}
			}

			if (pad.finger < 0 && !panels_open() && on_pad(px, py))
			{
				pad.finger = id;
				move_pad(px, py);
				haptic();
				return true;
			}

			return false;
		}

		bool finger_move(int64_t id, float px, float py)
		{
			if (id == pad.finger)
			{
				bool before[4] = { pad.held[0], pad.held[1], pad.held[2], pad.held[3] };
				move_pad(px, py);

				// A tick each time the thumb slides onto another arrow.
				for (int i = 0; i < 4; i++)
					if (pad.held[i] && !before[i])
					{
						haptic();
						break;
					}

				return true;
			}

			for (const Button& b : buttons)
				if (b.finger == id)
					return true;

			return false;
		}

		bool finger_up(int64_t id, float px, float py)
		{
			if (id == pad.finger)
			{
				release_pad();
				return true;
			}

			for (Button& b : buttons)
			{
				if (b.finger == id)
				{
					b.finger = -1;

					if (b.key >= 0)
						press(b.key, false);

					return true;
				}
			}

			return false;
		}

		void release_all()
		{
			if (pad.finger >= 0)
				release_pad();

			for (Button& b : buttons)
			{
				if (b.finger >= 0)
				{
					b.finger = -1;

					if (b.key >= 0)
						press(b.key, false);
				}
			}
		}

		void draw()
		{
			if (!active())
			{
				// Controls that vanish mid-press (a map change, the cash
				// shop) must not leave a key held down.
				release_all();
				return;
			}

			// Same for the controls a side panel hides.
			if (panels_open())
			{
				if (pad.finger >= 0)
					release_pad();

				for (Button& b : buttons)
				{
					if (b.finger >= 0 && !shown(b))
					{
						b.finger = -1;

						if (b.key >= 0)
							press(b.key, false);
					}
				}
			}

			// Chip widths come from the fonts, which load after the first
			// layout.
			if (!chips_measured)
			{
				chips_measured = MobileUI::text_width("Menu", MobileUI::Font::BODY_BOLD) > 0;

				if (chips_measured)
					layout();
			}

			// Beside an open side panel the Menu button moves into the space
			// it leaves, so it never covers the panel's own close button.
			{
				float lo = safe.left;
				float hi = panel_w - safe.right;
				menu_room = true;

				if (UI::get().side_panel_open())
				{
					Range<int16_t> span = UI::get().free_span();
					float vw = static_cast<float>(Constants::Constants::get().get_viewwidth());
					float to_px = vw > 0.0f ? fit_w / vw : 1.0f;
					float a = fit_x + span.first() * to_px + 8.0f * pt;
					float b = fit_x + span.second() * to_px - 8.0f * pt;

					if (b - a > 120.0f * pt)
					{
						lo = a;
						hi = b;
					}
					else if (span.second() - span.first() < 40)
					{
						menu_room = false;
					}
				}

				place_menu(lo, hi);
			}

			// Tell the chat window where its button is, in game pixels.
			if (const Button* chat = find("CHAT"))
			{
				float vw = static_cast<float>(Constants::Constants::get().get_viewwidth());
				float vh = static_cast<float>(Constants::Constants::get().get_viewheight());

				if (fit_w > 0 && fit_h > 0)
				{
					float sx = vw / fit_w;
					float sy = vh / fit_h;
					MobileUI::set_chat_button(MobileUI::rect(
						static_cast<int16_t>((chat->x - chat->radius - fit_x) * sx), static_cast<int16_t>((chat->y - chat->radius - fit_y) * sy),
						static_cast<int16_t>(chat->radius * 2.0f * sx), static_cast<int16_t>(chat->radius * 2.0f * sy)));
				}
			}

			glViewport(0, 0, panel_w, panel_h);

			draw_shapes();
			draw_labels();
		}
	}
}
