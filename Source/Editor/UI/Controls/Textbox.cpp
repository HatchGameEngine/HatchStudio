#include "Textbox.hpp"
#include <UI/Graphics/Renderer.hpp>
#include <UI/System/Application.hpp>
#include <UI/System/Clipboard.hpp>

#include <Hatch/Memory.h>
#include <Hatch/Strings.h>

TextboxBase::TextboxBase() : Control() {
	BackColor = Color(0x1C1E24, 0xFF);
	ForeColor = Color(0xFFFFFF, 0xFF);
	Highlight = Color(0x007FFF, 0xFF);

	Padding = 4;

	Dock = DOCK_NONE;

	Strings::Init(&Text, 8);
	Strings::Init(&SelectedText, 8);

	LastSelectionCursor = -1;

	TextPtr = &Text;
}
TextboxBase::TextboxBase(CString string) : TextboxBase() {
	Strings::FromCString(&Text, string, 0);
}
TextboxBase::TextboxBase(String* string) : TextboxBase() {
	Strings::Copy(&Text, string);
}

::Size TextboxBase::get_Size() {
	::Size outputSize = internal_Size;

	if (!Multiline) {
		UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];
		outputSize.H = Typeface->Ascent - Typeface->Descent + Padding.Vertical();
	}

	return outputSize;
}

void TextboxBase::OnClick(EventArgs* e) {

}
void TextboxBase::OnDoubleClick(EventArgs* e) {

}

int TextboxBase::MouseToTextCursorPosition(Position* mousePos) {
	auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	int localMousePosX = mousePos->X - (bounds.x + Padding.Left - ScrollLocation.X);

	float x = 0.0f;
	for (size_t i = 0; i < TextPtr->Length; i++) {
		int character = TextPtr->Text[i] & 0xFF;
		UI::Graphics::Font::Glyph* glyph = &Typeface->Glyphs[character];
		if (localMousePosX < x + glyph->Advance * 0.5f)
			return (int)i;

		x += glyph->Advance;
	}

	return TextPtr->Length;
}
void TextboxBase::TextCursorToLocation(int cursorPos, float* drawPosX) {
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	float cursorX = 0.0f;
	for (int i = 0; i < cursorPos; i++) {
		int character = TextPtr->Text[i] & 0xFF;
		UI::Graphics::Font::Glyph* glyph = &Typeface->Glyphs[character];
		cursorX += glyph->Advance;
	}

	drawPosX[0] = cursorX;
}

void TextboxBase::SetCursorPosition(int position) {
	if (LastSelectionCursor == -1)
		LastSelectionCursor = SelectionCursor;

	SelectionCursor = position;
	if (SelectionCursor < 0)
		SelectionCursor = 0;
	if (SelectionCursor > TextPtr->Length)
		SelectionCursor = TextPtr->Length;

	float drawPosX;
	TextCursorToLocation(SelectionCursor, &drawPosX);
	if (ScrollLocation.X < drawPosX + Padding.Left - (internal_Size.W - Padding.Right))
		ScrollLocation.X = drawPosX + Padding.Left - (internal_Size.W - Padding.Right);
	if (ScrollLocation.X > drawPosX)
		ScrollLocation.X = drawPosX;
}
void TextboxBase::HighlightBetweenCursorPositions() {
	int highlightL = M_MIN(SelectionCursor, LastSelectionCursor);
	int highlightR = M_MAX(SelectionCursor, LastSelectionCursor);

	SelectionStart = highlightL;
	SelectionLength = highlightR - highlightL;
}
void TextboxBase::ScrollToCursor() {
	float drawPosX;
	TextCursorToLocation(SelectionCursor, &drawPosX);
	if (ScrollLocation.X < drawPosX + Padding.Left - (internal_Size.W - Padding.Right))
		ScrollLocation.X = drawPosX + Padding.Left - (internal_Size.W - Padding.Right);
	if (ScrollLocation.X > drawPosX)
		ScrollLocation.X = drawPosX;
}

void TextboxBase::InsertText(int position, CString string, size_t length) {
	if (!length)
		return;

	size_t neededCapacity = TextPtr->Length + length;
	if (TextPtr->Capacity < neededCapacity) {
		TextPtr->Capacity = neededCapacity * 2;
		if (!Memory::Realloc(&TextPtr->Text, TextPtr->Capacity * sizeof(*TextPtr->Text), Memory::MEMPOOL_STRING)) {
			return;
		}
	}

	// Move characters forward
	for (int i = TextPtr->Length - 1; i >= position; i--) {
		TextPtr->Text[i + length] = TextPtr->Text[i];
	}

	// Insert characters
	for (int i = 0; i < length; i++) {
		TextPtr->Text[i + position] = string[i];
	}

	TextPtr->Length += length;

	OnTextChanged(NULL);
}
void TextboxBase::RemoveText(int position, size_t length) {
	if (position < 0)
		return;

	if (length > TextPtr->Length - position)
		length = TextPtr->Length - position;

	if (!length)
		return;

	// Remove characters
	for (int i = position; i < TextPtr->Length - length; i++) {
		TextPtr->Text[i] = TextPtr->Text[i + length];
	}

	TextPtr->Length -= length;

	OnTextChanged(NULL);
}

void TextboxBase::OnMouseDown(MouseEventArgs* e) {
	if (!(e->Button & SDL_BUTTON(SDL_BUTTON_LEFT)))
		return;

	Position mousePos = { e->X, e->Y };

	FocusCaptured = this;
	if ((e->Modifier & KMOD_SHIFT)) {
		SetCursorPosition(MouseToTextCursorPosition(&mousePos));
		HighlightBetweenCursorPositions();
	}
	else {
		LastSelectionCursor = MouseToTextCursorPosition(&mousePos);
		SetCursorPosition(LastSelectionCursor);
		SelectionStart = SelectionLength = 0;
	}

	if (CaptureMouse()) {
		// ClickStart = true;
		SDL_StartTextInput();
		UI::System::Application::CancelShortcuts = true;

		auto bounds = GetScreenRect();
		SDL_SetTextInputRect(&bounds);
	}
}
void TextboxBase::OnMouseClick(MouseEventArgs* e) {
	if (e->Button & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
		FocusCaptured = this;
		// Show context menu
		return;
	}
}
void TextboxBase::OnMouseUp(MouseEventArgs* e) {
	if (MouseCaptured == this) {
		UncaptureMouse();
	}
}
void TextboxBase::OnMouseDoubleClick(MouseEventArgs* e) {

}
void TextboxBase::OnMouseMove(MouseEventArgs* e) {
	Position mousePos = { e->X, e->Y };

	if (e->Button & SDL_BUTTON(SDL_BUTTON_LEFT)) {
		if (MouseCaptured == this) {
			SelectionCursor = MouseToTextCursorPosition(&mousePos);
			ScrollToCursor();

			HighlightBetweenCursorPositions();
		}
	}
}
void TextboxBase::OnMouseEnter(MouseEventArgs* e) {

}
void TextboxBase::OnMouseLeave(MouseEventArgs* e) {

}
void TextboxBase::OnMouseWheel(MouseEventArgs* e) {

}

void TextboxBase::OnTextInputted(TextEventArgs* e) {
	if (SelectionLength) {
		RemoveText(SelectionStart, SelectionLength);
		SelectionLength = 0;

		SelectionCursor = SelectionStart;
	}

	InsertText(SelectionCursor, e->Text, e->Length);
	SetCursorPosition(SelectionCursor + 1);

	LastSelectionCursor = -1;
}
void TextboxBase::OnTextEdited(TextEventArgs* e) {
	if (SelectionLength) {

	}
	else {

	}
}

void TextboxBase::OnKeyDown(KeyEventArgs* e) {
	// Left: CursorPosition--
	// Right: CursorPosition++

	if (FocusCaptured == this) {
		switch (e->Keycode) {
			// Common to all TextboxBase derived classes
		case SDLK_LEFT:
			SetCursorPosition(SelectionCursor - 1);

			if (!(e->Modifier & KMOD_SHIFT)) {
				LastSelectionCursor = -1;
				SelectionLength = 0;
			}
			else HighlightBetweenCursorPositions();
			break;
		case SDLK_RIGHT:
			SetCursorPosition(SelectionCursor + 1);

			if (!(e->Modifier & KMOD_SHIFT)) {
				LastSelectionCursor = -1;
				SelectionLength = 0;
			}
			else HighlightBetweenCursorPositions();
			break;
		case SDLK_HOME:
			SetCursorPosition(0);

			if (!(e->Modifier & KMOD_SHIFT)) {
				LastSelectionCursor = -1;
				SelectionLength = 0;
			}
			else HighlightBetweenCursorPositions();
			break;
		case SDLK_END:
			SetCursorPosition(TextPtr->Length);

			if (!(e->Modifier & KMOD_SHIFT)) {
				LastSelectionCursor = -1;
				SelectionLength = 0;
			}
			else HighlightBetweenCursorPositions();
			break;
		case SDLK_DELETE:
			if (SelectionLength) {
				RemoveText(SelectionStart, SelectionLength);
				SelectionLength = 0;

				SelectionCursor = SelectionStart;
			}
			else RemoveText(SelectionCursor, 1);

			ScrollToCursor();
			break;
		case SDLK_BACKSPACE:
			if (SelectionLength) {
				RemoveText(SelectionStart, SelectionLength);
				SelectionLength = 0;

				SelectionCursor = SelectionStart;
			}
			else {
				RemoveText(SelectionCursor - 1, 1);
				SetCursorPosition(SelectionCursor - 1);
			}

			ScrollToCursor();
			break;

			// Select All (Ctrl+A)
		case SDLK_a:
			if ((e->Modifier & KMOD_CTRL)) {
				LastSelectionCursor = 0;
				SelectionCursor = TextPtr->Length;
				HighlightBetweenCursorPositions();
			}
			break;
			// Copy (Ctrl+C)
		case SDLK_c:
			if ((e->Modifier & KMOD_CTRL) && SelectionLength) {
				char* stringBuffer = (char*)malloc(SelectionLength + 1);
				if (stringBuffer) {
					auto textStart = &TextPtr->Text[SelectionStart];
					for (int i = 0; i < SelectionLength; i++) {
						stringBuffer[i] = textStart[i];
					}
					stringBuffer[SelectionLength] = '\0';

					UI::Clipboard::SetText(stringBuffer);
					free(stringBuffer);
				}
			}
			break;
			// Cut (Ctrl+X)
		case SDLK_x:
			if ((e->Modifier & KMOD_CTRL) && SelectionLength) {
				char* stringBuffer = (char*)malloc(SelectionLength + 1);
				if (stringBuffer) {
					auto textStart = &TextPtr->Text[SelectionStart];
					for (int i = 0; i < SelectionLength; i++) {
						stringBuffer[i] = textStart[i];
					}
					stringBuffer[SelectionLength] = '\0';

					UI::Clipboard::SetText(stringBuffer);
					free(stringBuffer);

					RemoveText(SelectionStart, SelectionLength);
					SelectionLength = 0;

					SetCursorPosition(SelectionStart);
				}
			}
			break;
			// Paste (Ctrl+V)
		case SDLK_v:
			if ((e->Modifier & KMOD_CTRL) && UI::Clipboard::HasText()) {
				const char* pasteText = UI::Clipboard::GetText();

				if (SelectionLength) {
					RemoveText(SelectionStart, SelectionLength);
					SelectionLength = 0;

					SelectionCursor = SelectionStart;
				}

				InsertText(SelectionCursor, pasteText, strlen(pasteText));
				SetCursorPosition(SelectionCursor + strlen(pasteText));
			}
			break;

			// Textbox-only: Up/Down moves cursor
		case SDLK_UP:
			if (Multiline)
				break;

			SetCursorPosition(SelectionCursor - 1);

			if (!(e->Modifier & KMOD_SHIFT)) {
				LastSelectionCursor = -1;
				SelectionLength = 0;
			}
			else HighlightBetweenCursorPositions();
			break;

		case SDLK_DOWN:
			if (Multiline)
				break;

			SetCursorPosition(SelectionCursor + 1);

			if (!(e->Modifier & KMOD_SHIFT)) {
				LastSelectionCursor = -1;
				SelectionLength = 0;
			}
			else HighlightBetweenCursorPositions();
			break;

			// NumericUpDown-only: Up/Down inc/decrements value
		}
	}
}
void TextboxBase::OnKeyUp(KeyEventArgs* e) {

}

void TextboxBase::OnFocusLost(EventArgs* e) {
	// This has to be when the FocusCaptured is changed, before the Control that's newly focused runs the event
	SDL_StopTextInput();
	UI::System::Application::CancelShortcuts = false;

	// CancelEventArgs cea;
	// cea.Cancel = false;
	// OnValidating(&cea);
	// if (cea.Cancel == false) {
	// 	EventArgs ea;
	// 	ea.Cancel = false;
	// 	OnValidated(&ea);
	// }

	Control::OnFocusLost(e);
}

void TextboxBase::Render() {
	auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	SDL_Rect buffer;
	ClipStart(&buffer, &bounds);

	int textStartX = bounds.x + Padding.Left - ScrollLocation.X;
	int textStartY = bounds.y + bounds.h / 2;

	UI::Graphics::Renderer::DrawRect(&bounds, BackColor);
	if (FocusCaptured == this)
		UI::Graphics::Renderer::StrokeRect(&bounds, Highlight);

	UI::Graphics::Renderer::DrawFont(TextPtr, Typeface,
		textStartX, textStartY, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);

	// Selection
	Color selection = Color(Highlight.R, Highlight.G, Highlight.B, 0x7F);
	float cursorHeight = Typeface->Ascent - Typeface->Descent;
	float cursorX, highlightStartX, highlightEndX;

	if (SelectionLength) {
		TextCursorToLocation(SelectionStart, &highlightStartX);
		TextCursorToLocation(SelectionStart + SelectionLength, &highlightEndX);

		UI::Graphics::Renderer::DrawRect(
			textStartX + highlightStartX, textStartY - cursorHeight / 2,
			highlightEndX - highlightStartX, cursorHeight,
			selection);
	}

	// Draw Cursor
	if (FocusCaptured == this) {
		TextCursorToLocation(SelectionCursor, &cursorX);

		UI::Graphics::Renderer::DrawRect(
			textStartX + cursorX - 1, textStartY - cursorHeight / 2,
			2, cursorHeight,
			Highlight);
	}

	ClipEnd(&buffer);
}
