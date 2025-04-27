#include "ListView.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

// ColumnHeader
ColumnHeader::ColumnHeader(CString text, int width, int index) {
	Strings::Init(&Text, 1);
	SetText(text);

	Width = width;
	DataIndex = index;
}
ColumnHeader::ColumnHeader(String* text, int width, int index) {
	Strings::Init(&Text, 1);
	SetText(text);

	Width = width;
	DataIndex = index;
}
void ColumnHeader::SetText(CString text) {
	Strings::FromCString(&Text, text, 0);
}
void ColumnHeader::SetText(String* text) {
	Strings::Copy(&Text, text);
}

// ListViewSubItem
ListViewSubItem::ListViewSubItem(CString text) {
	Strings::Init(&Text, 1);
	SetText(text);
}
ListViewSubItem::ListViewSubItem(String* text) {
	Strings::Init(&Text, 1);
	SetText(text);
}
void ListViewSubItem::SetText(CString text) {
	Strings::FromCString(&Text, text, 0);
}
void ListViewSubItem::SetText(String* text) {
	Strings::Copy(&Text, text);
}

// ListViewItem
ListViewItem::ListViewItem(CString text) {
	Strings::Init(&Text, 1);
	SetText(text);
}
ListViewItem::ListViewItem(String* text) {
	Strings::Init(&Text, 1);
	SetText(text);
}
void ListViewItem::SetText(CString text) {
	Strings::FromCString(&Text, text, 0);
}
void ListViewItem::SetText(String* text) {
	Strings::Copy(&Text, text);
}

void ListViewItemCollection::Add(ListViewItem* item) {
	List<ListViewItem*>::Add(item);

	// if (Parent) Parent->ResizeChildren();
}

void ListView::Select(int index) {
	if (internal_SelectedIndex != index) {
		SelectedIndex = index;

		OnSelectedIndexChanged(NULL);
	}
	else {
		SelectedIndex = index;
	}

	int header = HeaderSize;
	if (LayoutType == ListViewLayout::List)
		header = 0;

	auto Bounds = GetScreenRect();
	int indexPosition = index * ItemSize;

	int underScroll = VScrollControl->Value - indexPosition;
	int overScroll = indexPosition + ItemSize - (VScrollControl->Value + Bounds.h);
	if (overScroll >= 0) {
		VScrollControl->Value = VScrollControl->Value + overScroll;
	}
	else if (underScroll >= 0) {
		VScrollControl->Value = VScrollControl->Value - underScroll;
	}
}

void ListView::OnMouseDown(MouseEventArgs* e) {
	auto location = GetPositionInWindowCoords();
	int relY = e->Y - location.Y;
	int header = HeaderSize;
	if (LayoutType == ListViewLayout::List)
		header = 0;

	if (relY < header) {

	}
	else {
		int index = (relY - header + VScrollControl->Value) / ItemSize;
		if (index < 0 || index >= Items.Count())
			index = -1;

		Select(index);
	}
}

::Size ListView::GetContentSize() {
    ::Size contentSize = { 1, Items.Count() * ItemSize };
	VScrollControl->SmallChange = ItemSize;
    return contentSize;
}
void ListView::HandleSDLEvent(SDL_Event* e) {
	Panel::HandleSDLEvent(e);
}

void ListView::ResizeChildren() {
	// Bounds: the size of the container, as we want it set
	// ContentBounds: the size of the content inside the container
	auto Bounds = GetScreenRect();
	auto contentBounds = GetContentSize();

	int header = HeaderSize;
	if (LayoutType == ListViewLayout::List)
		header = 0;

	ContentBounds.w = contentBounds.W;
	ContentBounds.h = contentBounds.H;
	DisplayBounds.w = Bounds.w;
	DisplayBounds.h = Bounds.h - header;

	bool showHScrollBar = DoHScroll && (!HideEmptyHScroll || DisplayBounds.w < contentBounds.W);
	bool showVScrollBar = DoVScroll && (!HideEmptyVScroll || DisplayBounds.h < contentBounds.H);
	::Size hScrollBarSize = HScrollControl->Size;
	::Size vScrollBarSize = VScrollControl->Size;

	if (showHScrollBar)
		DisplayBounds.h -= hScrollBarSize.H;
	if (showVScrollBar)
		DisplayBounds.w -= vScrollBarSize.W;

	HScrollControl->Location = { 0, DisplayBounds.h + header };
	HScrollControl->Size = { DisplayBounds.w, hScrollBarSize.H };
	HScrollControl->Minimum = 0;
	HScrollControl->Maximum = ContentBounds.w - DisplayBounds.w;

	VScrollControl->Location = { DisplayBounds.w, header };
	VScrollControl->Size = { vScrollBarSize.W, DisplayBounds.h };
	VScrollControl->Minimum = 0;
	VScrollControl->Maximum = ContentBounds.h - DisplayBounds.h;

	Control::ResizeChildren();
}
void ListView::Render() {
	SDL_Rect buffer;

	Panel::Render();

	int paddingLR = 5;
	auto Bounds = GetScreenRect();

	if (LayoutType == ListViewLayout::Details) {
		// Header
		UI::Graphics::Renderer::DrawRect(Bounds.x, Bounds.y, Bounds.w, HeaderSize, Color(0x000000, 0x80));
		{
			int shareCount = 0;
			int cellX = Bounds.x + paddingLR;
			int rowSpace = Bounds.w - paddingLR * 2;
			for (int c = 0; c < Columns.Count(); c++) {
				int columnWidth = Columns[c]->Width;
				if (columnWidth < 0) {
					shareCount++;
					continue;
				}

				rowSpace -= columnWidth;
			}

			for (int c = 0; c < Columns.Count(); c++) {
				String* text = &Columns[c]->Text;
				int columnWidth = Columns[c]->Width;
				if (columnWidth < 0)
					columnWidth = rowSpace / shareCount;
				Columns[c]->DisplayWidth = columnWidth;

				UI::Graphics::Renderer::DrawFontEllipsis(text, UI::Graphics::Font::Arial[12],
					cellX,
					Bounds.y + HeaderSize / 2,
					columnWidth, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);

				cellX += columnWidth;
			}
		}
		Bounds.y += HeaderSize;

		// Items
		Bounds.w = DisplayBounds.w;
		Bounds.h = DisplayBounds.h;
		ClipStart(&buffer, &Bounds);
		Bounds.y -= VScrollControl->Value;

		for (int i = 0; i < Items.Count(); i++) {
			int itemY = Bounds.y + i * ItemSize;

			if (i == SelectedIndex)
				UI::Graphics::Renderer::DrawRect(Bounds.x, itemY, Bounds.w, ItemSize, Highlight);

			int cellX = Bounds.x + paddingLR;
			int rowSpace = Bounds.w - paddingLR * 2;
			for (int c = 0; c < Columns.Count(); c++) {
				int dataIndex = Columns[c]->DataIndex;
				int columnWidth = Columns[c]->DisplayWidth;
				if (c == Columns.Count() - 1)
					columnWidth = rowSpace;

				String* text;
				if (dataIndex == 0) {
					text = &Items[i]->Text;
				}
				else if (dataIndex - 1 < Items[i]->SubItems.Count()) {
					text = &Items[i]->SubItems[dataIndex - 1]->Text;
				}
				else {
					rowSpace -= columnWidth;
					cellX += columnWidth;
					continue;
				}

				UI::Graphics::Renderer::DrawFontEllipsis(text, UI::Graphics::Font::Arial[12],
					cellX,
					itemY + ItemSize / 2,
					columnWidth, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);

				rowSpace -= columnWidth;
				cellX += columnWidth;
			}
		}

		ClipEnd(&buffer);
	}
	else if (LayoutType == ListViewLayout::List) {
		// Items
		Bounds.w = DisplayBounds.w;
		Bounds.h = DisplayBounds.h;
		ClipStart(&buffer, &Bounds);
		Bounds.y -= VScrollControl->Value;

		for (int i = 0; i < Items.Count(); i++) {
			int itemY = Bounds.y + i * ItemSize;

			if (i == SelectedIndex)
				UI::Graphics::Renderer::DrawRect(Bounds.x, itemY, Bounds.w, ItemSize, Highlight);

			int cellX = Bounds.x + paddingLR;
			int rowSpace = Bounds.w - paddingLR * 2;
			UI::Graphics::Renderer::DrawFontEllipsis(&Items[i]->Text, UI::Graphics::Font::Arial[12],
				cellX,
				itemY + ItemSize / 2,
				rowSpace, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);
		}

		ClipEnd(&buffer);
	}
}
