#pragma once

#include <windows.h>
#include <array>

namespace Nilesoft::Shell
{
	struct MenuToolbar
	{
		static constexpr int count = 5;
		static constexpr int button_width = 80;
		static constexpr int padding = 4;
		static constexpr int height = 80;
		static constexpr int label_inset = 8;
		static constexpr int button_gap = 4;
		static constexpr int icon_top = 8;
		static constexpr int label_gap = 3;
		static constexpr int label_bottom = 4;
		static constexpr UINT label_format = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_HIDEPREFIX;
		struct Button
		{
			UINT command = 0;
			bool enabled = false;
		};

		std::array<Button, count> buttons{};
		UINT id = 0;
		int row_height = 0;
		int divider_height = 0;
		int hot = -1;
		int selected = -1;
		bool enabled = false;
		bool keyboard = false;

		static int label_height(HDC dc, const wchar_t *label, int length, int width)
		{
			RECT bounds{0, 0, width, 0};
			return ::DrawTextW(dc, label, length, &bounds, label_format | DT_CALCRECT);
		}

		static int required_row_height(int minimum, int inset, int top, int iconSize,
			int gap, int textHeight, int bottom)
		{
			const int required = 2 * inset + top + iconSize + gap + textHeight + bottom;
			return required > minimum ? required : minimum;
		}

		// Button area of the menu row; the divider line and its bottom margin sit below it.
		static RECT content_rect(const RECT &row, int dividerHeight)
		{
			RECT content = row;
			if(dividerHeight > 0 && row.bottom - dividerHeight > row.top)
				content.bottom = row.bottom - dividerHeight;
			return content;
		}

		int total_height() const { return row_height + divider_height; }

		// Highlight/focus area of a button slot, kept clear of the vertical separators on both sides.
		static RECT visual_rect(const RECT &slot, int gap)
		{
			RECT rect = slot;
			if(gap > 0 && slot.right - slot.left > 2 * gap)
			{
				rect.left += gap;
				rect.right -= gap;
			}
			return rect;
		}

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
