#pragma once

#include <windows.h>
#include <array>

namespace Nilesoft::Shell
{
	struct MenuToolbar
	{
		static constexpr int count = 5;
		static constexpr int button_width = 80;
		static constexpr int padding = 8;
		static constexpr int height = 80;
		struct Button
		{
			UINT command = 0;
			bool enabled = false;
		};

		std::array<Button, count> buttons{};
		UINT id = 0;
		int hot = -1;
		int selected = -1;
		bool enabled = false;
		bool keyboard = false;

		bool hover(int index)
		{
			bool changed = hot != index || keyboard;
			hot = index;
			keyboard = false;
			if(command(index))
				selected = index;
			return changed;
		}

		static RECT button_rect(const RECT &row, int index, int inset, int width, bool rtl)
		{
			const int offset = index * width;
			if(rtl)
				return {row.right - inset - offset - width, row.top + inset,
					row.right - inset - offset, row.bottom - inset};
			return {row.left + inset + offset, row.top + inset,
				row.left + inset + offset + width, row.bottom - inset};
		}

		static int hit_test(const RECT &row, POINT point, int inset, int width, bool rtl)
		{
			if(width <= 0 || row.right - row.left < 2 * inset + count * width ||
				row.bottom - row.top <= 2 * inset)
				return -1;
			for(int i = 0; i < count; ++i)
			{
				auto rect = button_rect(row, i, inset, width, rtl);
				if(::PtInRect(&rect, point))
					return i;
			}
			return -1;
		}

		static int separator_x(const RECT &row, int index, int inset, int width, bool rtl)
		{
			auto rect = button_rect(row, index, inset, width, rtl);
			return rtl ? rect.left : rect.right;
		}

		UINT command(int index) const
		{
			return index >= 0 && index < count && buttons[index].enabled ?
				buttons[index].command : 0;
		}

		bool replaces(UINT commandId, bool native, bool popup) const
		{
			if(!id || !native || popup || !commandId)
				return false;
			for(const auto &button : buttons)
				if(button.command == commandId)
					return true;
			return false;
		}

		int move(int from, int direction) const
		{
			if(from < 0)
				from = direction > 0 ? count - 1 : 0;
			for(int i = 0; i < count; ++i)
			{
				from = (from + direction + count) % count;
				if(buttons[from].enabled)
					return from;
			}
			return -1;
		}
	};
}
