#include "core/pch.h"
#include "modules/libgogi/mde.h"

#ifdef MDE_SUPPORT_TOUCH

void MDE_View::TrigTouchDown(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data)
{
	OP_ASSERT(id >= 0 && id < LIBGOGI_MULTI_TOUCH_LIMIT);
	MDE_Screen::TouchState* state = &m_screen->m_touch[id];

	state->m_x = x;
	state->m_y = y;

	if (!state->m_captured_input)
		state->m_captured_input = GetViewAt(x, y, true);

	if (state->m_captured_input)
	{
		state->m_captured_input->ConvertFromScreen(x, y);
		state->m_captured_input->OnTouchDown(id, x, y, radius, modifiers, user_data);
	}
}

void MDE_View::TrigTouchUp(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data)
{
	OP_ASSERT(id >= 0 && id < LIBGOGI_MULTI_TOUCH_LIMIT);
	MDE_Screen::TouchState* state = &m_screen->m_touch[id];

	state->m_x = x;
	state->m_y = y;

	if (state->m_captured_input)
	{
		state->m_captured_input->ConvertFromScreen(x, y);
		state->m_captured_input->OnTouchUp(id, x, y, radius, modifiers, user_data);
		state->m_captured_input = NULL;
	}
}

void MDE_View::TrigTouchMove(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data)
{
	OP_ASSERT(id >= 0 && id < LIBGOGI_MULTI_TOUCH_LIMIT);
	MDE_Screen::TouchState* state = &m_screen->m_touch[id];

	state->m_x = x;
	state->m_y = y;

	if (state->m_captured_input)
	{
		state->m_captured_input->ConvertFromScreen(x, y);
		state->m_captured_input->OnTouchMove(id, x, y, radius, modifiers, user_data);
	}
}

void MDE_View::OnTouchDown(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data) {}
void MDE_View::OnTouchUp(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data) {}
void MDE_View::OnTouchMove(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data) {}

void MDE_View::ReleaseFromTouchCapture()
{
	if (m_screen)
	{
		for (int i = 0; i < LIBGOGI_MULTI_TOUCH_LIMIT; i++)
		{
			MDE_View* captured_view = m_screen->m_touch[i].m_captured_input;
			while (captured_view)
			{
				if (captured_view == this)
				{
					m_screen->m_touch[i].m_captured_input = NULL;
					break;
				}
				captured_view = captured_view->m_parent;
			}
		}
	}
}

#endif // MDE_SUPPORT_TOUCH
