#include "DropDown.hpp"
#include "MenuBar.hpp"

#include <UI/Graphics/Renderer.hpp>
#include <UI/Platforms/Common.h>
#include <UI/System/Menu.hpp>

#include <Hatch/Strings.h>

#define FONT_SIZE 20

#define ITEM_HEIGHT 40

#define MARGIN_TD 0
#define PADDING_L 40
#define PADDING_R 40
#define SHORTCUT_TEXT_SPACING 30
#define SEPARATOR_PADDING (PADDING_L - 5)
#define SEPARATOR_HEIGHT 10

#define CHECKBOX_OFFSET_X 12
#define CHECKBOX_OFFSET_Y 10
#define CHECKBOX_SIZE 20

#define RADIO_OFFSET_X 12
#define RADIO_OFFSET_Y 10
#define RADIO_SIZE 20

void DropDown::Init() {
	BackColor = Color(0x3D414C, 0xFF);
	OutlineColor = Color(0x30323B, 0xFF);
	ForeColor = Color(0xFFFFFF, 0xFF);
	HighlightColor = Color(0x007FFF, 0xFF);
	HighlightOutlineColor = Color(0x005FBF, 0xFF);
	SeparatorColor = Color(0x686D79, 0xFF);

	Dock = DOCK_TOP;

	HighlightedIndex = -1;

	ShapeRadioUnchecked = NULL;
	ShapeRadioChecked = NULL;

	CreateShapeTexture_EllipseStroke(&ShapeRadioUnchecked, RADIO_SIZE, RADIO_SIZE);
	CreateShapeTexture_Radio(&ShapeRadioChecked, RADIO_SIZE, RADIO_SIZE);
}

void DropDown::SetMenu(void *menuPtr) {
	MenuPtr = menuPtr;

	CalculateItemPositions();
}

void DropDown::CalculateItemPositions() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu) {
		return;
	}

	int widestItem = 0;

	auto Bounds = GetScreenRect();

	int itemX = Bounds.x;
	int itemY = Bounds.y + MARGIN_TD;
	int initialY = itemY;

	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[FONT_SIZE];

	ItemPositions.clear();

	for (int i = 0; i < menu->NumItems(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);

		::Size contentSize;
		contentSize.W = 0;

		if (item->Type != IT_SEPARATOR) {
			int lineWidth = 0;

			UI::Graphics::Renderer::MeasureFont(&item->Text, Typeface, &lineWidth, &contentSize.H);
			contentSize.W += lineWidth;

			if (item->ShortcutText.Length > 0) {
				UI::Graphics::Renderer::MeasureFont(&item->ShortcutText, Typeface, &lineWidth, &contentSize.H);
				contentSize.W += lineWidth + SHORTCUT_TEXT_SPACING;
			}

			if (contentSize.W > widestItem) {
				widestItem = contentSize.W;
			}
		}

		ItemPositions.push_back(DropDownItemPosition{itemX, itemY, contentSize.W, ITEM_HEIGHT});

		if (item->Type != IT_SEPARATOR) {
			itemY += ITEM_HEIGHT;
		}
		else {
			itemY += SEPARATOR_HEIGHT;
		}
	}

	Size = { widestItem + PADDING_R + 30, (itemY - initialY) + (MARGIN_TD * 2) };
	ResizeChildren();
}

void DropDown::Select(int index) {
	if (index == -1) {
		return;
	}

	IMenuItem* item = (IMenuItem*)((UI::Menu*)MenuPtr)->GetItem(index);
	if (item->Type == IT_SUBMENU) {
		if (Child != NULL) {
			CloseChild();
		}
		else {
			OpenChild(index);
		}
		return;
	}
	else if (item->Action) {
		item->Action();
	}

	Close();
}
void DropDown::Close() {
	if (MenuBarPtr) {
		((MenuBar*)MenuBarPtr)->Close();
	}
}

void DropDown::HighlightSelection(int index) {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (menu == NULL) {
		return;
	}

	if (internal_HighlightedIndex != index) {
		CloseChild();

		IMenuItem* item = (IMenuItem*)menu->GetItem(index);
		if (item != NULL && item->Type == IT_SUBMENU) {
			OpenChild(index);
		}
	}

	HighlightedIndex = index;
}
void DropDown::HighlightFirstAvailable() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return;
	}

	for (int i = 0; i < menu->NumItems(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);
		if (!item || !item->Enabled || item->Type == IT_SEPARATOR) {
			continue;
		}

		HighlightSelection(i);
		return;
	}
}
void DropDown::HighlightNext() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return;
	}

	int index = HighlightedIndex;

	IMenuItem* item = NULL;
	do {
		if (index + 1 >= menu->NumItems()) {
			index = 0;
		}
		else {
			index++;
		}

		if (index == HighlightedIndex) {
			return;
		}

		item = (IMenuItem*)menu->GetItem(index);
		if (!item) {
			return;
		}
	}
	while (!item->Enabled || item->Type == IT_SEPARATOR);

	HighlightSelection(index);
}
void DropDown::HighlightPrevious() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return;
	}

	int index = HighlightedIndex;

	IMenuItem* item = NULL;
	do {
		if (index - 1 < 0) {
			index = menu->NumItems() - 1;
		}
		else {
			index--;
		}

		if (index == HighlightedIndex) {
			return;
		}

		item = (IMenuItem*)menu->GetItem(index);
		if (!item) {
			return;
		}
	}
	while (!item->Enabled || item->Type == IT_SEPARATOR);

	HighlightSelection(index);
}

int DropDown::GetSelectionUnderCursor(int relX, int relY) {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu || menu->NumItems() == 0) {
		return -1;
	}

	int width = Size.Get().W;

	for (size_t i = 0; i < ItemPositions.size(); i++) {
		IMenuItem* item = (IMenuItem*)menu->GetItem(i);
		if (!item || !item->Enabled || item->Type == IT_SEPARATOR) {
			continue;
		}

		DropDownItemPosition pos = ItemPositions[i];
		if (relX >= pos.X && relX < pos.X + width && relY >= pos.Y && relY < pos.Y + pos.H) {
			return (int)i;
		}
	}

	return -1;
}

void DropDown::ConfirmSelection() {
	if (Child) {
		return ((DropDown*)Child)->ConfirmSelection();
	}
	else {
		Select(HighlightedIndex);
	}
}

bool DropDown::HandleAltShortcuts(SDL_Event* e, void* menuPtr) {
	UI::Menu* menu = (UI::Menu*)menuPtr;
	if (!menu) {
		return false;
	}

	if (Child) {
		((DropDown*)Child)->HandleAltShortcuts(e, ((DropDown*)Child)->MenuPtr);
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
		if (shortcut == key || shortcut == SDL_toupper(key)) {
			if (item->Type == IT_SUBMENU) {
				HighlightSelection(i);

				if (Child != NULL) {
					((DropDown*)Child)->HighlightFirstAvailable();
				}
			}
			else {
				Select(i);
			}

			FocusShortcut = false;

			return true;
		}
	}

	return false;
}

void DropDown::OpenChild(int index) {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu) {
		return;
	}

	IMenuItem* item = (IMenuItem*)menu->GetItem(index);
	if (!item) {
		CloseChild();
		return;
	}

	if (item->Type != IT_SUBMENU) {
		CloseChild();
		return;
	}

	UI::Menu* submenu = (UI::Menu*)item->Submenu;
	if (!submenu) {
		CloseChild();
		return;
	}

	DropDown* dropDown = new DropDown();
	dropDown->SetMenu(submenu);
	dropDown->Location.X = ItemPositions[index].X + Size.Get().W;
	dropDown->Location.Y = ItemPositions[index].Y;
	dropDown->Dock = DOCK_NONE;
	dropDown->MenuBarPtr = MenuBarPtr;
	dropDown->ParentDropdown = this;

	Child = dropDown;
	Controls.Add(dropDown);
}
void DropDown::CloseChild() {
	DropDown* dropdown = (DropDown*)Child;
	if (dropdown != NULL) {
		dropdown->CloseChild();
		delete dropdown;

		Child = NULL;

		Controls.Clear();
	}
}

bool DropDown::IsMouseOnTopOfChild(SDL_Event* e) {
	if (Child) {
		return ((DropDown*)Child)->IsMouseOnTopOfSelf(e);
	}

	return false;
}

bool DropDown::IsMouseOnTopOfSelf(SDL_Event* e) {
	SDL_Point mousePos = { e->button.x, e->button.y };
	SDL_Rect boundsInWindow = GetScreenRect();

	if (SDL_PointInRect(&mousePos, &boundsInWindow)) {
		return true;
	}

	return false;
}

void DropDown::OnMouseMove(MouseEventArgs* e) {
	auto location = GetPositionInWindowCoords();
	int relX = e->X - location.X;
	int relY = e->Y - location.Y;

	int index = GetSelectionUnderCursor(relX, relY);
	HighlightSelection(index);
}
void DropDown::OnMouseDown(MouseEventArgs* e) {
	auto location = GetPositionInWindowCoords();
	int relX = e->X - location.X;
	int relY = e->Y - location.Y;

	int index = GetSelectionUnderCursor(relX, relY);
	HighlightSelection(index);
}
void DropDown::OnMouseUp(MouseEventArgs* e) {
	auto location = GetPositionInWindowCoords();
	int relX = e->X - location.X;
	int relY = e->Y - location.Y;

	int index = GetSelectionUnderCursor(relX, relY);
	Select(index);
}
void DropDown::OnMouseLeave(MouseEventArgs* e) {
	HighlightSelection(-1);
}

void DropDown::HandleSDLEvent(SDL_Event* e) {
	switch (e->type) {
	case SDL_KEYDOWN: {
		switch (e->key.keysym.sym) {
		case SDLK_UP:
			if (Child != NULL && ((DropDown*)Child)->HighlightedIndex != -1) {
				Child->HandleSDLEvent(e);
				return;
			}
			if (HighlightedIndex == -1) {
				MenuBar* menuBar = (MenuBar*)MenuBarPtr;
				FocusShortcut = menuBar->FocusShortcut;
				menuBar->ChangeToAltFocus();
				HighlightFirstAvailable();
			}
			HighlightPrevious();
			return;
		case SDLK_DOWN:
			if (Child != NULL && ((DropDown*)Child)->HighlightedIndex != -1) {
				Child->HandleSDLEvent(e);
				return;
			}
			if (HighlightedIndex == -1) {
				MenuBar* menuBar = (MenuBar*)MenuBarPtr;
				FocusShortcut = menuBar->FocusShortcut;
				menuBar->ChangeToAltFocus();
				HighlightFirstAvailable();
			}
			else {
				HighlightNext();
			}
			return;
		case SDLK_LEFT:
			if (Child != NULL && ((DropDown*)Child)->HighlightedIndex != -1) {
				Child->HandleSDLEvent(e);
				return;
			}
			else if (ParentDropdown != NULL && HighlightedIndex != -1) {
				HighlightSelection(-1);
			}
			else if (MenuBarPtr) {
				((MenuBar*)MenuBarPtr)->SelectPrevious();
			}
			return;
		case SDLK_RIGHT:
			if (Child != NULL && ((DropDown*)Child)->HighlightedIndex == -1) {
				((DropDown*)Child)->HighlightFirstAvailable();
				return;
			}
			else if (MenuBarPtr) {
				((MenuBar*)MenuBarPtr)->SelectNext();
			}
			return;
		default:
			if (HandleAltShortcuts(e, MenuPtr)) {
				return;
			}
			break;
		}
		break;
	}
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
		if (Child && ((DropDown*)Child)->IsMouseOnTopOfSelf(e)) {
			Child->HandleSDLEvent(e);
			return;
		}
		break;
	case SDL_MOUSEMOTION:
		if (IsMouseOnTopOfChild(e)) {
			Child->HandleSDLEvent(e);
			return;
		}
		break;
	}

	Control::HandleSDLEvent(e);
}

void DropDown::Render() {
	UI::Menu* menu = (UI::Menu*)MenuPtr;
	if (!menu) {
		return;
	}

	auto Bounds = GetScreenRect();
	SDL_Rect boundsInWindow = GetScreenRect();
	int width = Size.Get().W;

	UI::Graphics::Renderer::DrawRect(&Bounds, BackColor);
	UI::Graphics::Renderer::StrokeRect(&Bounds, OutlineColor);

	SDL_Rect buffer;
	ClipStart(&buffer, &Bounds);

	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[FONT_SIZE];
	Color disabledColor = Color(ForeColor.Full, 0x7F);

	for (size_t i = 0; i < ItemPositions.size(); i++) {
		DropDownItemPosition pos = ItemPositions[i];

		int itemX = pos.X + boundsInWindow.x;
		int itemY = pos.Y + boundsInWindow.y;

		IMenuItem* item = (IMenuItem*)menu->GetItem(i);

		if (i == HighlightedIndex) {
			UI::Graphics::Renderer::DrawRect(itemX, itemY, width, ITEM_HEIGHT, ClickStart ? HighlightOutlineColor : HighlightColor);
			UI::Graphics::Renderer::StrokeRect(itemX, itemY, width, ITEM_HEIGHT, HighlightOutlineColor);
		}

		int textY = itemY + (ITEM_HEIGHT / 2);

		if (item->Type == IT_SEPARATOR) {
			UI::Graphics::Renderer::DrawRect(
				itemX + SEPARATOR_PADDING, itemY + (SEPARATOR_HEIGHT / 2),
				width - (SEPARATOR_PADDING * 2), 1,
				SeparatorColor);
			UI::Graphics::Renderer::DrawRect(
				itemX + SEPARATOR_PADDING, itemY + (SEPARATOR_HEIGHT / 2) + 1,
				width - (SEPARATOR_PADDING * 2), 1,
				Color(SeparatorColor.Full, 0x7F));
		}
		else {
			int textX = itemX + PADDING_L;

			Color textColor = item->Enabled ? ForeColor : disabledColor;

			// Draw checkbox or radio icon
			switch (item->Type) {
			case IT_CHECKMARK_CHECKED:
				UI::Graphics::Renderer::DrawRect(
					itemX + CHECKBOX_OFFSET_X + 4, itemY + CHECKBOX_OFFSET_Y + 4,
					CHECKBOX_SIZE - 8, CHECKBOX_SIZE - 8,
					textColor
				);
			case IT_CHECKMARK_UNCHECKED:
				UI::Graphics::Renderer::StrokeRect(
					itemX + CHECKBOX_OFFSET_X, itemY + CHECKBOX_OFFSET_Y,
					CHECKBOX_SIZE, CHECKBOX_SIZE,
					textColor
				);
				break;
			case IT_RADIO_CHECKED: {
				SDL_Rect pos = { itemX + RADIO_OFFSET_X, itemY + RADIO_OFFSET_Y, RADIO_SIZE, RADIO_SIZE };
				UI::Graphics::Renderer::DrawTexture(ShapeRadioChecked, NULL, &pos, textColor);
				break;
			}
			case IT_RADIO_UNCHECKED: {
				SDL_Rect pos = { itemX + RADIO_OFFSET_X, itemY + RADIO_OFFSET_Y, RADIO_SIZE, RADIO_SIZE };
				UI::Graphics::Renderer::DrawTexture(ShapeRadioUnchecked, NULL, &pos, textColor);
				break;
			}
			}

			UI::Graphics::Renderer::DrawFont(&item->Text, Typeface,
				textX,
				textY,
				TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, textColor);

			if (item->ShortcutText.Length > 0) {
				UI::Graphics::Renderer::DrawFont(&item->ShortcutText, Typeface,
					itemX + width - PADDING_R,
					textY,
					TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE, textColor);
			}

			// Draw underline if focused
			if (item->Enabled && FocusShortcut && item->AltShortcut) {
				float fx = textX;
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
	}

	ClipEnd(&buffer);

	for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
		auto Child = Controls.Items[i];
		Child->Render();
	}
}
