#include "MenuBar.hpp"

#include <UI/Graphics/Renderer.hpp>
#include <UI/Platforms/Common.h>
#include <UI/Controls/DropDown.hpp>
#include <UI/System/Menu.hpp>

#include <Hatch/Strings.h>

#include <cctype>

#define FONT_SIZE 20

#define PADDING_LR 12

void MenuBar::SetMenu(void *menuPtr) {
	MenuPtr = menuPtr;

	CalculateItemPositions();
}

void MenuBar::CalculateItemPositions() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu) {
		return;
	}

	auto Bounds = GetScreenRect();

	int itemX = Bounds.x;
	int itemY = Bounds.y;

	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[FONT_SIZE];

	ItemPositions.clear();

	for (int i = 0; i < menu->NumItems(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);

		::Size contentSize;

		UI::Graphics::Renderer::MeasureFont(&item->Text, Typeface, &contentSize.W, &contentSize.H);

		int width = contentSize.W + (PADDING_LR * 2);

		ItemPositions.push_back(MenuBarItemPosition{itemX, itemY, width, ItemHeight});

		itemX += width;
	}
}

void MenuBar::Select(int index) {
	if (internal_SelectedIndex != index) {
		CloseDropdown();

		SelectedIndex = index;

		if (SelectedIndex != -1) {
			OpenDropdown(SelectedIndex);
		}
	}
	else {
		SelectedIndex = index;
	}
}
void MenuBar::SelectNext() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return;
	}

	int index = SelectedIndex;

	IMenuItem* item = NULL;
	do {
		if (index + 1 >= menu->NumItems()) {
			index = 0;
		}
		else {
			index++;
		}

		if (index == SelectedIndex) {
			return;
		}

		item = (IMenuItem*)menu->GetItem(index);
		if (!item) {
			return;
		}
	}
	while (!item->Enabled || item->Type == IT_SEPARATOR);

	HighlightSelection(-1);
	Select(index);

	if (FocusShortcut && AltFocus && Dropdown != NULL) {
		((DropDown*)Dropdown)->HighlightFirstAvailable();
	}
}
void MenuBar::SelectPrevious() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return;
	}

	int index = SelectedIndex;

	IMenuItem* item = NULL;
	do {
		if (index - 1 < 0) {
			index = menu->NumItems() - 1;
		}
		else {
			index--;
		}

		if (index == SelectedIndex) {
			return;
		}

		item = (IMenuItem*)menu->GetItem(index);
		if (!item) {
			return;
		}
	}
	while (!item->Enabled || item->Type == IT_SEPARATOR);

	HighlightSelection(-1);
	Select(index);

	if (FocusShortcut && AltFocus && Dropdown != NULL) {
		((DropDown*)Dropdown)->HighlightFirstAvailable();
	}
}

void MenuBar::HighlightSelection(int index) {
	HighlightedIndex = index;
}

int MenuBar::GetSelectionUnderCursor(int relX, int relY) {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return -1;
	}

	for (size_t i = 0; i < ItemPositions.size(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);
		if (!item || !item->Enabled || item->Type == IT_SEPARATOR) {
			continue;
		}

		MenuBarItemPosition pos = ItemPositions[i];
		if (relX >= pos.X && relX < pos.X + pos.W && relY >= pos.Y && relY < pos.Y + pos.H) {
			return (int)i;
		}
	}

	return -1;
}

void MenuBar::OpenDropdown(int index) {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu) {
		return;
	}

	IMenuItem* item = (IMenuItem*)menu->GetItem(index);
	if (!item) {
		CloseDropdown();
		return;
	}

	if (item->Type != IT_SUBMENU) {
		CloseDropdown();
		return;
	}

	UI::Menu* submenu = (UI::Menu*)item->Submenu;
	if (!submenu) {
		CloseDropdown();
		return;
	}

	DropDown* dropdown = (DropDown*)Dropdown;
	if (dropdown) {
		dropdown->CloseChild();
		delete dropdown;
		Controls.Clear();
	}

	DropDown* dropDown = new DropDown();
	dropDown->SetMenu(submenu);
	dropDown->Location.X = ItemPositions[index].X;
	dropDown->Location.Y = Location.Y + Size.Get().H;
	dropDown->Dock = DOCK_NONE;
	dropDown->MenuBarPtr = this;

	if (AltFocus) {
		dropDown->FocusShortcut = FocusShortcut;
	}

	Dropdown = dropDown;
	Controls.Add(dropDown);
}
void MenuBar::CloseDropdown() {
	DropDown* dropdown = (DropDown*)Dropdown;

	if (dropdown) {
		dropdown->CloseChild();
		delete dropdown;
		Dropdown = NULL;

		Controls.Clear();
	}

	HighlightSelection(-1);

	SelectedIndex = -1;
}

void MenuBar::ConfirmSelection() {
	DropDown* dropdown = (DropDown*)Dropdown;
	if (dropdown) {
		dropdown->ConfirmSelection();
	}
}
void MenuBar::Close() {
	CloseDropdown();
	StopFocus();
}

void MenuBar::ChangeToAltFocus() {
	AltFocus = true;
}
void MenuBar::StopFocus() {
	FocusShortcut = false;
	AltFocus = false;
	AltTimerStart = 0;
}

bool MenuBar::HandleShortcuts(SDL_Event* e, void* menuPtr) {
	UI::Menu* menu = (UI::Menu*)menuPtr;
	if (!menu) {
		return false;
	}

	int key = e->key.keysym.sym;
	int mod = e->key.keysym.mod;

	for (int i = 0; i < menu->NumItems(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);
		if (item->Type == IT_SUBMENU) {
			if (HandleShortcuts(e, item->Submenu)) {
				return true;
			}
			continue;
		}
		else if (item->Type == IT_SEPARATOR || !item->Enabled || !item->Shortcut || !item->Action) {
			continue;
		}

		if ((item->Shortcut & UI::Menu::SM_CONTROL) != 0) {
			if ((mod & KMOD_CTRL) == 0) {
				continue;
			}
		}
		if ((item->Shortcut & UI::Menu::SM_ALT) != 0) {
			if ((mod & KMOD_ALT) == 0) {
				continue;
			}
		}
		if ((item->Shortcut & UI::Menu::SM_SHIFT) != 0) {
			if ((mod & KMOD_SHIFT) == 0) {
				continue;
			}
		}

		if ((item->Shortcut & 0xFF) == key) {
			item->Action();
			return true;
		}
	}

	return false;
}

bool MenuBar::HandleAltShortcuts(SDL_Event* e, void* menuPtr) {
	UI::Menu* menu = (UI::Menu*)menuPtr;
	if (!menu) {
		return false;
	}

	int key = e->key.keysym.sym;

	for (int i = 0; i < menu->NumItems(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);
		if (item->Type == IT_SEPARATOR || !item->Enabled || !item->AltShortcut) {
			continue;
		}

		// TODO: Handle items that have the same shortcut
		int shortcut = item->AltShortcut & 0xFF;
		if (shortcut == key || shortcut == toupper(key)) {
			Select(i);

			DropDown* dropdown = (DropDown*)Dropdown;
			if (dropdown != NULL) {
				dropdown->FocusShortcut = true;
				dropdown->HighlightFirstAvailable();
			}

			return true;
		}
	}

	return false;
}

void MenuBar::OnMouseMove(MouseEventArgs* e) {
	auto location = GetPositionInWindowCoords();
	int relX = e->X - location.X;
	int relY = e->Y - location.Y;

	int index = GetSelectionUnderCursor(relX, relY);
	if (SelectedIndex != -1) {
		if (index != -1) {
			HighlightSelection(-1);
			Select(index);
		}
	}
	else {
		HighlightSelection(index);
	}
}
void MenuBar::OnMouseDown(MouseEventArgs* e) {
	OpenDropdown(SelectedIndex);

	FocusShortcut = false;
	AltFocus = false;
	AltTimerStart = 0;
}
void MenuBar::OnMouseLeave(MouseEventArgs* e) {
	HighlightSelection(-1);
}

void MenuBar::HandleSDLEvent(SDL_Event* e) {
	switch (e->type) {
	case SDL_KEYDOWN:
		if (Dropdown == NULL) {
			if (e->key.keysym.mod != 0) {
				HandleShortcuts(e, MenuPtr);
			}

			if (FocusShortcut && HandleAltShortcuts(e, MenuPtr)) {
				AltFocus = true;
			}

			if (e->key.keysym.sym == SDLK_LALT && (e->key.keysym.mod & KMOD_CTRL) == 0) {
				AltTimerStart = SDL_GetTicks();
			}
			else if (e->key.keysym.sym == SDLK_F10) {
				FocusShortcut = true;
				AltTimerStart = 0;

				HighlightSelection(-1);
				Select(0);
				OpenDropdown(0);
			}
		}
		else {
			if (FocusShortcut && HandleAltShortcuts(e, MenuPtr)) {
				FocusShortcut = false;
			}

			switch (e->key.keysym.sym) {
			case SDLK_ESCAPE:
				Close();
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				ConfirmSelection();
				break;
			}
		}
		break;
	case SDL_KEYUP:
		if (AltTimerStart != 0) {
			if (e->key.keysym.sym == SDLK_LALT && Dropdown == NULL) {
				FocusShortcut = false;
			}

			AltTimerStart = 0;
		}
		break;
	case SDL_MOUSEBUTTONUP: {
		DropDown* dropdown = (DropDown*)Dropdown;
		if (dropdown != NULL) {
			if (dropdown->IsMouseOnTopOfSelf(e) || dropdown->IsMouseOnTopOfChild(e)) {
				Dropdown->HandleSDLEvent(e);
				return;
			}
		}
		break;
	}
	case SDL_MOUSEBUTTONDOWN: {
		DropDown* dropdown = (DropDown*)Dropdown;
		if (dropdown != NULL) {
			if (dropdown->IsMouseOnTopOfSelf(e) || dropdown->IsMouseOnTopOfChild(e)) {
				Dropdown->HandleSDLEvent(e);
				return;
			}
		}

		auto location = GetPositionInWindowCoords();
		int relX = e->button.x - location.X;
		int relY = e->button.y - location.Y;

		int index = GetSelectionUnderCursor(relX, relY);
		if (index != -1 && SelectedIndex != -1 && index == SelectedIndex) {
			// Deselect if the clicked menu is the same as the currently selected
			index = -1;
		}

		HighlightSelection(-1);
		Select(index);
		break;
	}
	}

	Control::HandleSDLEvent(e);
}

void MenuBar::Update() {
	if (AltTimerStart != 0 && SDL_GetTicks() - AltTimerStart > AltWaitDuration) {
		FocusShortcut = true;
	}

	for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
		auto Child = Controls.Items[i];
		Child->Update();
	}
}

void MenuBar::Render() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu) {
		return;
	}

	auto Bounds = GetScreenRect();

	UI::Graphics::Renderer::DrawRect(&Bounds, BackColor);

	SDL_Rect buffer;
	ClipStart(&buffer, &Bounds);

	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[FONT_SIZE];
	Color disabledColor = Color(ForeColor.Full, 0x7F);

	for (size_t i = 0; i < ItemPositions.size(); i++) {
		MenuBarItemPosition pos = ItemPositions[i];

		int itemX = pos.X + PADDING_LR;
		int itemY = pos.Y;

		IMenuItem* item = (IMenuItem*)menu->GetItem(i);

		if (i == HighlightedIndex || i == SelectedIndex) {
			UI::Graphics::Renderer::DrawRect(itemX - PADDING_LR, itemY, pos.W, pos.H, HighlightColor);
			UI::Graphics::Renderer::StrokeRect(itemX - PADDING_LR, itemY, pos.W, pos.H, HighlightOutlineColor);
		}

		int textY = itemY + (ItemHeight / 2);

		Color textColor = item->Enabled ? ForeColor : disabledColor;

		UI::Graphics::Renderer::DrawFont(&item->Text, Typeface,
			itemX,
			textY,
			TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, textColor);

		// Draw underline if focused
		if (FocusShortcut && !(Dropdown != NULL && AltFocus) && item->AltShortcut && item->Enabled) {
			float fx = itemX;
			float fy = textY;

			for (size_t i = 0; i < item->Text.Length; i++) {
				int character = item->Text.Text[i];

				UI::Graphics::Font::Glyph* glyph = &Typeface->Glyphs[character];

				if (character != item->AltShortcut) {
					fx += glyph->Advance;
					continue;
				}

				fy += Typeface->Ascent;
				fy -= (Typeface->Ascent - Typeface->Descent) / 2;
				fy += 3.0f;

				float fw = glyph->Width / Typeface->sampleSize;

				UI::Graphics::Renderer::DrawRect(fx, fy, fw, 1, textColor);
				UI::Graphics::Renderer::DrawRect(fx, fy + 1.0f, fw, 1, Color(textColor.Full, 0x7F));

				break;
			}
		}
	}

	ClipEnd(&buffer);

	for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
		auto Child = Controls.Items[i];
		Child->Render();
	}
}
