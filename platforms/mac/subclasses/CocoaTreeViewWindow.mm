#include "core/pch.h"

#ifdef QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN

#include "platforms/mac/subclasses/CocoaTreeViewWindow.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "adjunct/quick_toolkit/widgets/TreeViewWindow.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"

@implementation OperaNSPopUpButton
-(void) drawRect:(NSRect)rect
{
}
@end


@interface TreeViewDelegate : NSObject
{
	CocoaTreeViewWindow* listener;
}
-(void)popupChanged:(id)sender;
@end

@implementation TreeViewDelegate
-(id)initWithListener:(CocoaTreeViewWindow*)win
{
	self = [super init];
	listener = win;
	return self;
}
-(void)popupChanged:(id)sender
{
	listener->SetActiveItem([((NSPopUpButton*)sender) indexOfSelectedItem]);
}
@end



/***********************************************************************************
 **
 ** Create
 **
 ***********************************************************************************/
OP_STATUS OpTreeViewWindow::Create(OpTreeViewWindow **w,
								 OpTreeViewDropdown* dropdown,
								 const char *transp_window_skin)
{
	OpTypedObject::Type type = dropdown->GetType();
	if(type == OpTypedObject::WIDGET_TYPE_DROPDOWN_WITHOUT_EDITBOX || type == OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN || type == OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN)
	{
		TreeViewWindow *win = OP_NEW(TreeViewWindow, ());
		if(win == NULL)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = win->Construct(dropdown, transp_window_skin);
		if(OpStatus::IsError(status))
		{
			OP_DELETE(win);
			return status;
		}
		*w = win;
		return OpStatus::OK;
	}
	CocoaTreeViewWindow *win = OP_NEW(CocoaTreeViewWindow, (dropdown));
	if (win == NULL)
		return OpStatus::ERR_NO_MEMORY;

	*w = win;
	return OpStatus::OK;
}


CocoaTreeViewWindow::CocoaTreeViewWindow(OpTreeViewDropdown* dropdown)
: m_tree_model(dropdown->GetModel()),
  m_button(nil),
  m_selected(-1),
  m_listener(NULL),
  m_dropdown(dropdown),
  m_items_deletable(FALSE),
  m_timer(NULL),
  m_allow_wrapping_on_scrolling(FALSE)
{
	m_delegate = [[TreeViewDelegate alloc] initWithListener:this];
}

CocoaTreeViewWindow::~CocoaTreeViewWindow()
{
	delete m_timer;
	ClosePopup();
	[m_delegate release];
}

OP_STATUS CocoaTreeViewWindow::Clear(BOOL delete_model)
{
	if(m_tree_model)
	{
		if(m_items_deletable)
			m_tree_model->DeleteAll();
		else
			m_tree_model->RemoveAll();

		if(delete_model)
		{
			OP_DELETE(m_tree_model);
			m_tree_model = 0;
		}
	}

	if(m_dropdown)
		m_dropdown->OnClear();
	return OpStatus::OK;
}


void CocoaTreeViewWindow::SetModel(GenericTreeModel *tree_model, BOOL items_deletable)
{
	m_tree_model = tree_model;
	m_items_deletable = items_deletable;
}


GenericTreeModel *CocoaTreeViewWindow::GetModel()
{
	OP_ASSERT(m_tree_model);
	return m_tree_model;
}


OpTreeModelItem *CocoaTreeViewWindow::GetSelectedItem(int *position)
{
	if (position)
		*position = m_selected;
	if (m_selected >= 0)
	{
		return m_tree_model->GetItemByPosition(m_selected);
	}
	return NULL;
}


void CocoaTreeViewWindow::SetSelectedItem(OpTreeModelItem *item, BOOL selected)
{
}


OpRect CocoaTreeViewWindow::CalculateRect()
{
	return OpRect();
}


OpTreeView *CocoaTreeViewWindow::GetTreeView()
{
	return NULL;
}


OP_STATUS CocoaTreeViewWindow::AddDesktopWindowListener(DesktopWindowListener* listener)
{
	m_listener = listener;
	return OpStatus::OK;
}


void CocoaTreeViewWindow::ClosePopup()
{
	if(m_button)
	{
		[m_button removeFromSuperviewWithoutNeedingDisplay];
		[m_button setTarget:nil];
		[m_button setAction:nil];
		[m_button release];
		m_button = nil;
		m_dropdown->OnWindowDestroyed();
		Clear(TRUE);
		m_dropdown = NULL;
	}
}


BOOL CocoaTreeViewWindow::IsPopupVisible()
{
	return FALSE;
}


void CocoaTreeViewWindow::SetPopupOuterPos(INT32 x, INT32 y)
{
}


void CocoaTreeViewWindow::SetPopupOuterSize(UINT32 width, UINT32 height)
{
}

void CocoaTreeViewWindow::SetVisible(BOOL vis, OpRect rect)
{

}

void CocoaTreeViewWindow::SetVisible(BOOL vis, BOOL default_size)
{
	if (!m_button)
	{
		NSRect frame = NSMakeRect(0,0,0,0);
		m_button = [[OperaNSPopUpButton alloc] initWithFrame:frame pullsDown:YES];
		[m_button setTarget:m_delegate];
		[m_button setAction:@selector(popupChanged:)];
		[m_button setAutoenablesItems:NO];
	}
	if(vis && m_button)
	{
		[(NSPopUpButton*)m_button removeAllItems];
		[(NSPopUpButton*)m_button addItemWithTitle:@" "];
		NSMenu* menu = [(NSPopUpButton*)m_button menu];
		for(int i=0; i<m_tree_model->GetItemCount(); i++)
		{
			OpTreeModelItem *tree_item = m_tree_model->GetItemByPosition(i);
			
			OpString text;
			OpTreeModelItem::ItemData item_data(OpTreeModelItem::INIT_QUERY);
			tree_item->GetItemData(&item_data);
			BOOL disabled = (item_data.flags & OpTreeModelItem::FLAG_DISABLED) || (item_data.flags & OpTreeModelItem::FLAG_INITIALLY_DISABLED) || (item_data.flags & OpTreeModelItem::FLAG_TEXT_SEPARATOR) || (item_data.flags & OpTreeModelItem::FLAG_NO_SELECT);
			if (item_data.flags & OpTreeModelItem::FLAG_SEPARATOR) {
				[menu addItem:[NSMenuItem separatorItem]];
			} else {
				OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
				item_data.column_query_data.column_text = &text;
				tree_item->GetItemData(&item_data);
				NSString *title = [NSString stringWithCharacters:(unichar*)text.CStr() length:text.Length()];
				NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
				if (item_data.column_bitmap.ImageDecoded())
					[item setImage:(NSImage*)GetNSImageFromImage(&item_data.column_bitmap)];
				if (item_data.column_query_data.column_image) {
					OpSkinElement *elem = g_skin_manager->GetSkinElement(item_data.column_query_data.column_image);
					if (elem) {
						Image img = elem->GetImage(0);
						if (img.ImageDecoded())
							[item setImage:(NSImage*)GetNSImageFromImage(&img)];
					}
				}
				[item setEnabled:!disabled];
				[menu addItem:[item autorelease]];
			}
		}

	
		CocoaOpWindow *cocoa_window = (CocoaOpWindow *)m_dropdown->GetVisualDevice()->GetOpView()->GetRootWindow();
		if (!cocoa_window)
			return;
	
		[cocoa_window->GetContentView() addSubview:(NSView *)m_button];
		[(NSView*)m_button setHidden:NO];
		[(NSPopUpButton*)m_button setEnabled:YES];
		NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseDown
						location:[[cocoa_window->GetContentView() window] convertScreenToBase:[NSEvent mouseLocation]]
						modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
						windowNumber:[[cocoa_window->GetContentView() window] windowNumber]
						context:[[cocoa_window->GetContentView() window] graphicsContext]
						eventNumber:0
						clickCount:1
						pressure:0.0];

		UINT32 width, height;
		((OpWindow*)cocoa_window)->GetInnerSize(&width, &height);

		INT32 xpos,ypos;
		((OpWindow*)cocoa_window)->GetInnerPos(&xpos, &ypos);
		
		OpRect r = m_dropdown->GetScreenRect();
		m_selected = -1;
		
		NSRect frame = NSMakeRect(r.x - xpos, height - r.y + ypos - r.height, r.width, r.height);
		[(NSPopUpButton*)m_button setFrame:frame];
		[(NSPopUpButton*)m_button mouseDown:event];
		if (m_selected == -1)
			PopupClosed();
	}
}


BOOL CocoaTreeViewWindow::SendInputAction(OpInputAction* action)
{
	return FALSE;
}


void CocoaTreeViewWindow::SetExtraLineHeight(UINT32 extra_height)
{
}


void CocoaTreeViewWindow::SetAllowWrappingOnScrolling(BOOL wrapping)
{
	m_allow_wrapping_on_scrolling = wrapping;
}

void CocoaTreeViewWindow::SetMaxLines(BOOL max_lines)
{
}


BOOL CocoaTreeViewWindow::IsLastLineSelected()
{
	return FALSE;
}


BOOL CocoaTreeViewWindow::IsFirstLineSelected()
{
	return FALSE;
}


void CocoaTreeViewWindow::UnSelectAndScroll()
{
}

void CocoaTreeViewWindow::SetTreeViewName(const char*  name) 
{
}


void CocoaTreeViewWindow::SetActiveItem(int item)
{
	m_selected = item-1;
	m_dropdown->OnMouseEvent(m_dropdown, m_selected, 0, 0, MOUSE_BUTTON_1, FALSE, 1);
//	[(NSView*)m_button removeFromSuperviewWithoutNeedingDisplay];
	if (m_listener)
	{
		// Get some attention
		m_listener->OnDesktopWindowClosing(NULL, TRUE);
		OP_DELETE(this);
	}
}

void CocoaTreeViewWindow::PopupClosed()
{
	if (!m_timer) {
		m_timer = new OpTimer();
		m_timer->SetTimerListener(this);
		m_timer->Start(0);
	}
//	m_listener->OnDesktopWindowClosing(NULL, TRUE);
//	delete this;
}

void CocoaTreeViewWindow::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer==m_timer);
	
	m_listener->OnDesktopWindowClosing(NULL, TRUE);
	OP_DELETE(this);
}

#endif // QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN
