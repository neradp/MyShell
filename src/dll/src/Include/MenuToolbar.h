#pragma once

#include <windows.h>
#include <array>

namespace Nilesoft::Shell
{
	struct MenuToolbar
	{
		static constexpr int count = 5;
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

		static RECT button_rect(const RECT &row, int index, int padding, bool rtl)
		{
			if(rtl)
				index = count - 1 - index;
			const auto width = row.right - row.left - 2 * padding;
			return {row.left + padding + width * index / count, row.top + padding,
				row.left + padding + width * (index + 1) / count, row.bottom - padding};
		}

		static int hit_test(const RECT &row, POINT point, int padding, bool rtl)
		{
			if(row.right - row.left <= 2 * padding || row.bottom - row.top <= 2 * padding)
				return -1;
			for(int i = 0; i < count; ++i)
			{
				auto rect = button_rect(row, i, padding, rtl);
				if(::PtInRect(&rect, point))
					return i;
			}
			return -1;
		}

		UINT command(int index) const
		{
			return index >= 0 && index < count && buttons[index].enabled ?
				buttons[index].command : 0;
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
