 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2011 Opera Software AS. All rights reserved.
  *
  * This file is part of the Opera web browser. It may not be distributed
  * under any circumstances.
  */

/** @file TipsManager.h
  *
  * Contains the declaration of the class TipsManager
  *
  * @author Deepak Arora
  *
  */

#ifndef TIPS_MANAGER_H
#define TIPS_MANAGER_H

//Core header files
#include "modules/widgets/OpWidget.h"

//Platform header files
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/tips/TipsConfig.h"

//Global objects and constant definations
#define g_tips_manager (TipsManager::GetInstance())

/** Contention between display of tips can be handled using TipsManager class. This class uses 
* TipsConfigManager class to coordinate tips display related activities. Tips related information
* can be accessed using interface from this class. 
*/

class  TipsManager
	: public DesktopManager<TipsManager>
	, public OpTimerListener

{
	public:
		TipsManager(); 

		~TipsManager();

		/** Overridden form of GenericDesktopManager::Init().
		*
		* @param OpStatus::OK if it manages to initilize various modules.
        */
		OP_STATUS					Init();	

	   /** To fetch any given tips configuration can be done by this function.
	    * 
		* @param name of tip which is a tip's section name.
		* @param Retried data can be stored in 'tip_info'
		* @return OpStatus::OK if a tip's configuration is retrieved correctly, 
		* otherwise OpStatus::ERR_(XXX) when it failed to fetch for any reasons. 
		*/
		OP_STATUS					GetTipsConfigData(const OpStringC16& tip_name, TipsConfigGeneralInfo &tip_info);
		
		/** The timed-token protocol is used to resolve tips' display contention.
		* Components associated with tips if like to display tip, after certain
		* criteria are met, they should call this method. If token is not grabbed by 
	    * anyone else, a permission is granted, in form return status-code, to display 
		* tip.
		*
		* @param Configuration connected to a tip.
		* @return OpStatus::OK if token can granted and on failed cases OpStatus::ERR_XXX.
		*/
		OP_STATUS					GrabTipsToken(TipsConfigGeneralInfo &tip_info);

	   /** A helper function which marks tips as shown. 
		*
		* @param Configuration connected to a tip.
		* @return OpStatus::OK if token can granted and on failed cases OpStatus::ERR_XXX.
		*/
		OP_STATUS					MarkTipShown(TipsConfigGeneralInfo &tip_info);

	   /** To query whether tips system is ON/OFF can be done via this function. 
		* 
		* @return TRUE if tip system is enabled, otherwise FALSE. 
		*/
		BOOL						IsTipsSystemEnabled() { return m_tips_config_mgr->IsTipsSystemEnabled(); }

		/** To check whether tip can be displayed or not this function be used.
		 * This function tests various criteria, like whether a tip is enabled or 
		 * not, when tip was last displayed. 
		 * 
		 * @param section name
		 * @param if tip can be displayed then tip configuration, identified via 'tip_name', are fetched
		 * and stored here. 
		 * @return TRUE if a tip can be displayed.
		 */	
		BOOL						CanTipBeDisplayed(const OpStringC16& tip_name, TipsConfigGeneralInfo &tip_info);

	public: //Inherited, overriden methods

	   /** Since tip is displayed using the timed-token protocol, a listener of OpTimerListener
		* is used here . 
		*
		* @param pointer to timer object.
        */
		virtual void				OnTimeOut(OpTimer* timer);

	private:
		BOOL						m_tips_token;   //This flag identifies whether anyone in the tip system has displayed a tip or not.
		OpTimer						m_timer_tips_token; //A timer which runs after a tip is displayed. Once a timer expires, 'm_tips_token' flag is reseted.
		TipsConfigManager			*m_tips_config_mgr; // An object of TipsConfigManager.
};

#endif //TIPS_MANAGER_H