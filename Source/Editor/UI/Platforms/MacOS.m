#import <AppKit/AppKit.h>

#include "Common.h"

/* setAppleMenu disappeared from the headers in 10.4 */
@interface NSApplication(NSAppleMenu)
- (void)setAppleMenu:(NSMenu *)menu;
@end

@interface IMenuItem : NSMenuItem {
	void (*action)(void);
}
@end

@implementation IMenuItem
- (void) setCAction:(void (*)(void))func {
	action = func;
}
- (void) setShortcut:(int)shortcut {
	if ((shortcut & 0xFF) != 0) {
		char shortcutString[2];
		shortcutString[0] = (shortcut & 0xFF);
		shortcutString[1] = 0;

		NSString* nsShortcut = [NSString stringWithUTF8String:shortcutString];
		[self setKeyEquivalent:nsShortcut];

		NSEventModifierFlags flag = 0;
		if ((shortcut & SM_SHIFT) != 0)
			flag |= NSEventModifierFlagShift;
		if ((shortcut & SM_CONTROL) != 0)
			flag |= NSEventModifierFlagControl;
		if ((shortcut & SM_COMMAND) != 0)
			flag |= NSEventModifierFlagCommand;
		if ((shortcut & SM_OPTION) != 0 || (shortcut & SM_ALT) != 0)
			flag |= NSEventModifierFlagOption;

		if (flag != 0)
			[self setKeyEquivalentModifierMask:flag];
	}
}
- (void) runCAction:(id)sender {
	if (action != NULL)
		action();
}
@end

// IMenu implementation
struct MacOS_IMenu {
    NSMenu* nsMenu;
};

struct IMenu* IMenu_Create(void) {
    struct IMenu* menu = (struct IMenu*)calloc(1, sizeof(struct IMenu));
    if (menu == NULL)
        return NULL;

    struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)calloc(1, sizeof(struct MacOS_IMenu));
    if (macos_menu == NULL)
        return NULL;

    macos_menu->nsMenu = [[NSMenu alloc] initWithTitle:@""];
    [macos_menu->nsMenu setAutoenablesItems:NO];

    menu->Data = macos_menu;
    return menu;
}
void IMenu_Dispose(struct IMenu* menu) {

}

int  IMenu_AddItem(struct IMenu* menu, const char* title, void (*action)(void), int shortcut, int enabled, int type, int altShortcut) {
    struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

	NSString* nsTitle = [NSString stringWithUTF8String:title];

	// Create menu item
	IMenuItem* menuItem = [[IMenuItem alloc] init];
	[menuItem setTitle:nsTitle];
	[menuItem setTarget:menuItem];
	[menuItem setEnabled:enabled];
	[menuItem setAction:@selector(runCAction:)];
	[menuItem setCAction:action];
	[menuItem setShortcut:shortcut];

	[macos_menu->nsMenu addItem:menuItem];

	return (int)[macos_menu->nsMenu indexOfItem:menuItem];
}
int  IMenu_AddSubmenu(struct IMenu* menu, struct IMenu* submenu, const char* title, int altShortcut) {
    struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;
    struct MacOS_IMenu* macos_submenu = (struct MacOS_IMenu*)submenu->Data;

	// Update submenu's title
	NSString* nsTitle = [NSString stringWithUTF8String:title];
	[macos_submenu->nsMenu setTitle:nsTitle];

	// Add the submenu as an item to the menu
    NSMenuItem* itemWrapper = [[NSMenuItem alloc] initWithTitle:nsTitle action:nil keyEquivalent:@""];
	[itemWrapper setSubmenu:macos_submenu->nsMenu];
	[macos_menu->nsMenu addItem:itemWrapper];

    return (int)[macos_menu->nsMenu indexOfItem:itemWrapper];
}
int  IMenu_AddSeparator(struct IMenu* menu) {
    struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

    [macos_menu->nsMenu addItem:[NSMenuItem separatorItem]];
    return 0;
}
void IMenu_EditItem(struct IMenu* menu, int index, const char* title, void (*action)(void), int shortcut, int enabled, int type) {
	struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

	NSString* nsTitle = [NSString stringWithUTF8String:title];

	// Get menu item
	IMenuItem* menuItem = (IMenuItem*)[macos_menu->nsMenu itemAtIndex:index];
	[menuItem setTitle:nsTitle];
	[menuItem setTarget:menuItem];
	[menuItem setEnabled:enabled];
	[menuItem setAction:@selector(runCAction:)];
	[menuItem setCAction:action];
	[menuItem setShortcut:shortcut];
	
	switch (type) {
		case IT_RADIO_CHECKED:
			[menuItem setState:NSControlStateValueMixed];
			break;
		case IT_RADIO_UNCHECKED:
			[menuItem setState:NSControlStateValueOff];
			break;
		case IT_CHECKMARK_CHECKED:
			[menuItem setState:NSControlStateValueOn];
			break;
		case IT_CHECKMARK_UNCHECKED:
			[menuItem setState:NSControlStateValueOff];
			break;
	}
}
void IMenu_ClearItems(struct IMenu* menu) {
    struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

	[macos_menu->nsMenu removeAllItems];
}
void IMenu_SetAppMenu(struct IMenu* menu) {
    struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

    [NSApp setMainMenu:macos_menu->nsMenu];
}

void IMenu_SetAppleMenu(struct IMenu* menu) {
	struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

	NSMenu* serviceMenu;
	NSMenu* appleMenu = macos_menu->nsMenu;
    NSMenuItem* menuItem;


	NSString *appName = @"HatchStudio";

	NSString* title = [@"About " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    [appleMenu addItemWithTitle:@"Preferences…" action:nil keyEquivalent:@","];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    serviceMenu = [[NSMenu alloc] initWithTitle:@""];
    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:serviceMenu];

    [NSApp setServicesMenu:serviceMenu];
    // [serviceMenu release];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Hide " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:(NSEventModifierFlagOption|NSEventModifierFlagCommand)];

    [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Quit " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

	[NSApp setAppleMenu:appleMenu];
}
void IMenu_SetWindowMenu(struct IMenu* menu) {
	struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

	NSMenu* menuWindow = macos_menu->nsMenu;
	[menuWindow addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[menuWindow addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
	[menuWindow addItem:[NSMenuItem separatorItem]];

	// Add the fullscreen toggle menu option, if supported
	if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6) {
		NSMenuItem* itemWrapper = [[NSMenuItem alloc] initWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
		[itemWrapper setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];
		[menuWindow addItem:itemWrapper];
		// [menuItem release];
	}

	[NSApp setWindowsMenu:menuWindow];
}
void IMenu_SetHelpMenu(struct IMenu* menu) {
	struct MacOS_IMenu* macos_menu = (struct MacOS_IMenu*)menu->Data;

	NSMenu* menuHelp = macos_menu->nsMenu;

	[NSApp setHelpMenu:menuHelp];
}

void IMenu_Init() {
	// Do nothing as nothing is needed.
}
