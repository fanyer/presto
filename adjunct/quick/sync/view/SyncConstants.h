/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef SYNC_CONSTANTS_H
#define SYNC_CONSTANTS_H

namespace SyncConstant
{
	const unsigned RETRY_BUTTON = 0;  //YES
	const unsigned DELETE_BUTTON = 1; //NO
	const unsigned CANCEL_BUTTON = 2; //CANCEL

	const char WARNING_ICON[] = "warning_icon";
	const char HEADER_LABEL[] = "header_label";
	const char PASSWORD_STAR[] = "old_password_slabel";
	const char PASSWORD_LABEL[] = "old_password_label";
	const char PASSWORD_EDIT[] = "old_passwd_edit";
	const char RETRY_RADIO[] = "radio_retry";
	const char REPLACE_RADIO[] = "radio_replace";
	const char INCORR_LABEL[] = "label_incorrect";
	const char ERROR_LABEL[] = "error_label";
	const char PROGRESS_SPINNER[] = "progress_spinner";
	const char PROGRESS_LABEL[] = "progress_label";

	const char WINDOW_NAME_AREYOUSURE[] = "AreYouSure Password Recovery Dialog";

	const char OLD_PASSWD_EDIT[] = "old_passwd_edit";
	const char NEW_PASSWD_EDIT[] = "new_passwd_edit";
	const char CONFIRM_NEW_PASSWD_EDIT[] = "confirm_passwd_edit";
	const char NEW_PASSWD_STRENGTH[] = "new_passwd_strength";

	const char ONLY_ON_THIS_RADIO[] = "only_on_this_radio";
	const char ON_ALL_RADIO[] = "on_all_radio";

	const unsigned HEADER_LABEL_REL_SIZE = 140;
	const COLORREF RED = OP_RGB(255, 0, 0);
	const int BIGGER = 150;
}

#endif // SYNC_CONSTANTS_H

