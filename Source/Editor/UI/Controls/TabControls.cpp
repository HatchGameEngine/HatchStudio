#include "TabControls.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

TabPage::TabPage() : Panel() {
    Strings::FromCString(&Title, "tabPage", 0);
    BackColor = Color(0x000000, 0xFF);

    CanFocus = true;
}
TabPage::TabPage(CString title) : Panel() {
    Strings::FromCString(&Title, title, 0);
    BackColor = Color(0x000000, 0xFF);

    CanFocus = true;
}

void TabPage::SetTitle(CString title) {
    Strings::FromCString(&Title, title, 0);
}
void TabPage::SetTitle(String* title) {
    Strings::Copy(&Title, title);
}

void TabPageCollection::Add(TabPage* item) {
    List<TabPage*>::Add(item);
    if (item) {
        item->Parent = Owner;
        Owner->ResizeChildren();
    }
}
void TabPageCollection::Insert(int index, TabPage* item) {
    List<TabPage*>::Insert(index, item);
    if (item) {
        item->Parent = Owner;
        Owner->ResizeChildren();
    }
}
void TabPageCollection::RemoveAt(int index) {
    List<TabPage*>::RemoveAt(index);
    Owner->ResizeChildren();
}


void TabControl::GetTabRackLayout(Position* position, ::Size* size) {
    ::Size tabSize = GetDefaultTabSize();
    switch (Alignment) {
    case TabAlignment::Top:
        *position = { 0, 0 };
        *size = { internal_Size.W, tabSize.H };
        break;
    case TabAlignment::Bottom:
        *position = { 0, internal_Size.H - tabSize.H };
        *size = { internal_Size.W, tabSize.H };
        break;
    case TabAlignment::Left:
        *position = { 0, 0 };
        *size = { tabSize.H, internal_Size.H };
        break;
    case TabAlignment::Right:
        *position = { internal_Size.W - tabSize.H, 0 };
        *size = { tabSize.H, internal_Size.H };
        break;
    }
}
TabPage* TabControl::GetCurrentTabPage() {
    if (SelectedIndex >= 0 && SelectedIndex < TabPages.Count()) {
        return TabPages[SelectedIndex];
    }
    return NULL;
}

void TabControl::Select(int index) {
    SelectedIndex = index;
    ResizeChildren();

    OnSelected(NULL);
}

void TabControl::OnTabMouseDown(MouseEventArgs* e, int tabIndex) { }
void TabControl::OnTabMouseMove(MouseEventArgs* e, int tabIndex) { }
void TabControl::OnTabMouseUp(MouseEventArgs* e, int tabIndex) { }

void TabControl::OnMouseWheel(MouseEventArgs* e) {
    ::Position screenPos = GetPositionInWindowCoords();
    ::Position mouseRelPos = ::Position { e->X, e->Y } - screenPos;

    ::Size tabRackSize;
    ::Position tabRackPos;
    GetTabRackLayout(&tabRackPos, &tabRackSize);

    // If in tab rack,
    if (PositionInBounds(mouseRelPos, tabRackPos, tabRackSize)) {
        TabRackScroll.X -= e->Delta * 8;
        if (TabRackScroll.X < 0)
            TabRackScroll.X = 0;
    }
}
void TabControl::OnMouseDown(MouseEventArgs* e) {
    ::Position screenPos = GetPositionInWindowCoords();
    ::Position mouseRelPos = ::Position { e->X, e->Y } - screenPos;

    ::Size tabRackSize;
    ::Position tabRackPos;
    GetTabRackLayout(&tabRackPos, &tabRackSize);

    // If in tab rack,
    if (PositionInBounds(mouseRelPos, tabRackPos, tabRackSize)) {
        ::Size tabSize = GetDefaultTabSize();
        ::Position mouseRelRackPos = mouseRelPos;
        mouseRelRackPos += TabRackScroll;

        int toIndex = mouseRelRackPos.X / tabSize.W;
        if (toIndex >= 0 && toIndex < TabPages.Count()) {
            Select(toIndex);
        }
    }
}
void TabControl::OnMouseMove(MouseEventArgs* e) {

}
void TabControl::OnMouseLeave(MouseEventArgs* e) {

}
void TabControl::OnMouseUp(MouseEventArgs* e) {

}

void TabControl::Update() {
    Control::Update();

    TabPage* currentTabPage = GetCurrentTabPage();
    if (currentTabPage)
        currentTabPage->Update();
}
void TabControl::ResizeChildren() {
    ::Size tabSize = GetDefaultTabSize();

    TabPage* currentTabPage = GetCurrentTabPage();
    if (currentTabPage) {
        switch (Alignment) {
        case TabAlignment::Top:
            currentTabPage->Location = { 0, tabSize.H };
            currentTabPage->Size = { internal_Size.W, internal_Size.H - tabSize.H };
            break;
        case TabAlignment::Bottom:
            currentTabPage->Location = { 0, 0 };
            currentTabPage->Size = { internal_Size.W, internal_Size.H - tabSize.H };
            break;
        case TabAlignment::Left:
            currentTabPage->Location = { tabSize.H, 0 };
            currentTabPage->Size = { internal_Size.W - tabSize.H, internal_Size.H };
            break;
        case TabAlignment::Right:
            currentTabPage->Location = { 0, 0 };
            currentTabPage->Size = { internal_Size.W - tabSize.H, internal_Size.H };
            break;
        }

        currentTabPage->ResizeChildren();
    }
}
void TabControl::HandleSDLEvent(SDL_Event* e) {
    TabPage* currentTabPage = GetCurrentTabPage();
    if (currentTabPage)
        currentTabPage->HandleSDLEvent(e);

    Control::HandleSDLEvent(e);
}
void TabControl::Render() {
    Control::Render();

    ::Size tabSize = GetDefaultTabSize();
    ::Position screenPos = GetPositionInWindowCoords();

    TabPage* currentTabPage = GetCurrentTabPage();
    if (currentTabPage)
        currentTabPage->Render();

    ::Size tabRackSize;
    ::Position tabRackPos;
    GetTabRackLayout(&tabRackPos, &tabRackSize);

    SDL_Rect buffer;
    SDL_Rect bounds = { tabRackPos.X + screenPos.X, tabRackPos.Y + screenPos.Y, tabRackSize.W, tabRackSize.H };
    ClipStart(&buffer, &bounds);

    auto font = UI::Graphics::Font::Arial[12];
    int maxTextWidth = tabSize.W - 32;
    for (int i = 0; i < TabPages.Count(); i++) {
        auto tabPage = TabPages[i];

        Position tabPos = { screenPos.X, screenPos.Y };

        tabPos.X += i * tabSize.W;
        tabPos -= TabRackScroll;

        Color tabBackColor = Color(0x181C14, 0x00);
        Color tabTextColor = Color(0x6b727d, 0xFF);
        Color tabHighlightColor = Color(0x007FFF, 0x00);
        if (i == SelectedIndex) {
            tabBackColor = Color(0x282C34, 0xFF);
            tabTextColor = Color(0xd7dae0, 0xFF);
            tabHighlightColor = Color(0x007FFF, 0xFF);
        }

        switch (Alignment) {
        case TabAlignment::Top:
            tabPos += Position { 0, 0 };
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y, tabSize.W, tabSize.H, tabBackColor);
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y + tabSize.H - 2, tabSize.W, 2, tabHighlightColor);
            UI::Graphics::Renderer::DrawFontEllipsis(&tabPage->Title, font,
                tabPos.X + tabSize.W / 2, tabPos.Y + tabSize.H / 2, maxTextWidth, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, tabTextColor);
            break;
        case TabAlignment::Bottom:
            tabPos += Position { 0, internal_Size.H - tabSize.H };
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y, tabSize.W, tabSize.H, tabBackColor);
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y + tabSize.H - 2, tabSize.W, 2, tabHighlightColor);
            UI::Graphics::Renderer::DrawFontEllipsis(&tabPage->Title, font,
                tabPos.X + tabSize.W / 2, tabPos.Y + tabSize.H / 2, maxTextWidth, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, tabTextColor);
            break;
        case TabAlignment::Left:
            tabPos += Position { 0, 0 };
            UI::Graphics::Renderer::DrawRect(screenPos.X, screenPos.Y, tabSize.H, tabSize.W, tabBackColor);
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y, 2, tabSize.H, tabHighlightColor);
            break;
        case TabAlignment::Right:
            tabPos += Position { internal_Size.W - tabSize.H, 0 };
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y, tabSize.H, tabSize.W, tabBackColor);
            UI::Graphics::Renderer::DrawRect(tabPos.X, tabPos.Y, 2, tabSize.H, tabHighlightColor);
            break;
        }
    }

    ClipEnd(&buffer);
}
