 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2011 Opera Software AS. All rights reserved.
  *
  * This file is part of the Opera web browser. It may not be distributed
  * under any circumstances.
  */

/** @file TipsConfig.h
  *
  * Contains the declaration of the classes TipsConfigInfo , TipsConfigMetaInfo, 
  * TipsConfigMetaInfo::TipsGlobalTweaks and TipsConfigManager
  *
  * @author Deepak Arora
  *
  */

#ifndef TIPS_CONFIG_H
#define TIPS_CONFIG_H

#include "modules/locale/locale-enum.h"

/** In reference to current tips configuration management, every tips have standard 
 * information. Using TipsConfigInfo class these information can be set or retrived 
 * for an individual tip. 
 */
class TipsConfigInfo
{
	protected:
	   	~TipsConfigInfo(){ }

	public:
		TipsConfigInfo() : m_frequency(-1) , m_message_id(Str::NOT_A_STRING) { }
		
	   /** This function provides tips appearance rate.
        *
		* @return tips appearance frequency. 
        */
		INT32				GetTipAppearanceFrequency()	{ return m_frequency; }

	   /** This function provides section name connected to a tip.
        *
		* @return section name
        */
		const OpStringC&	GetSectionName() const { return m_section_name; }

		/** This function provided string-ID associated with a tip. 
		 * with tip.
		 *
		 * @return string id associated with tips.
         */
		Str::LocaleString	GetTipStringID() { return m_message_id; }

	   /** This function sets tips appearance freq. Tips' appearance 
        * frequency defines number of time a tips can be displayed before
		* it can be turned off. 
		* 
		* @param tips appreance freq.
        */
		void				SetTipAppearanceFrequency(INT32 freq) { m_frequency = freq; } 

	   /** This functions sets string-ID associated to a tip.
        *
		* @param string-ID connected connected with tip.
        */
		void				SetTipStringID(Str::LocaleString str_id) { m_message_id = str_id; }
	   
		/** This functions sets the section name associated with tips.
         *
		 * @return OpStatus::OK on success and other error codes OpStatus::ERR_(XXX) on failure.
		 * @param string-ID connected connected with tip.
         */
		OP_STATUS			SetSectionName(const OpStringC& section_name) { return m_section_name.Set(section_name); }

	protected:
		INT32				m_frequency;		//Tips' appearance frequency defines number of time a tips can be displayed before it can be turned off. 
		OpString 			m_section_name;		//Tips' section name
		Str::LocaleString 	m_message_id;		//Tips' string-ID.
};

/** Tips related settings/configurations are divided and maintained in 2 seperate files. 
 * These are
 * a. Meta info file, which contains fixed, invariable, settings. Class TipsConfigMetaInfo 
 * addresses those.
 * b. General info file, which contains dynamic/variable information created at runtime. 
 * Class TipsConfigGeneralInfo addressess thoses. 
 */
class TipsConfigMetaInfo
	: public TipsConfigInfo
{
	public:
	 	TipsConfigMetaInfo(){ }

		virtual ~TipsConfigMetaInfo() { }

		/** Meta info can further be divided into two types of info.
		 * a. Global tips wise settings, which can be used by tips contention manager, TipsManager
		 * b. Invariable tips settings, pertinent to individual tips.
		 * 
		 * Ideally users shouldn't be allowed to modify tips setting/configuration.
		 */
		class TipsGlobalTweaks
		{
			public:
				enum Defaults
				{
					DEFAULT_TIPS_DISPLAY_INTERVAL = 3600,	//in secs.
					DEFAULT_RECURRENCE_INTERVAL	  = 7,		// in days.
				};

			public:
				TipsGlobalTweaks() 
					: m_tips_recurrence_lag(DEFAULT_RECURRENCE_INTERVAL)
					, m_is_tips_never_off(FALSE)
					, m_is_tips_system_enabled(FALSE)
					, m_tips_display_interval(DEFAULT_TIPS_DISPLAY_INTERVAL * 1000) //Covert into milliseconds. 
				{ 
				
				}
			   
				/** This function provides tips display rate. A tips display interval defines 
				 * display intervals between tips. 
				 *
				 * @return tips display interval.
				 */	
				UINT32		GetTipsDisplayInterval() { return  m_tips_display_interval; }

				/** This function provides tips recurrence interval. Tips recurrence 
				 * interval defines next appearence time for a tip which is defined 
				 * in days
				 *
				 * @return tips recurrence interval.
				 */	
				UINT32		GetTipsRecurrenceInterval() { return m_tips_recurrence_lag; }

				/** Whether tipping system is ON/OFF can be seeked from this function
				 *
				 * @return TRUE if tip system is enabled, otherwise FALSE.
				 */	
				BOOL		IsTipsSystemEnabled() const { return m_is_tips_system_enabled; }

			    /** For QA testing a special setting is provided through tips can be turned 
				 * OFF. This can be tested IsTipsNeverOff method. 
				 *
				 * @return TRUE if tips never turned off is enabled. 
				 */	
				BOOL		IsTipsNeverOff() const { return m_is_tips_never_off; }

				/** Sets of global tips wise setting can be loaded using this function. 
				 *
				 * @param sd_file points to tips_meta.ini file. 
				 * @param section points to TIPS_GLOBAL_SETTINGS_SECTION from tips_meta.ini file.
				 */	
				void		LoadConfiguration(PrefsFile &sd_file, const OpString &section);

			private:
				UINT32				m_tips_recurrence_lag;		// Tips recurrence interval in days. Default value is 7
				BOOL				m_is_tips_never_off;		// Having this setting enables tips to be never turned off. Usually used for testing. Default value is 0
				BOOL				m_is_tips_system_enabled;	// Identifies whether the feature tips is enabled or not. Default value is 1
				UINT32				m_tips_display_interval;	// Display interval between previously displayed tip and next tip which is going to be displayed.		
		};
};

/** Variable/nonfixed tips settings are created and maintained in a seperate file under
* profile folder. These information are updated as when tips are displayed. These settings 
* and file related to these setting are created from the tips meta information file. Class
* TipsConfigGeneralInfo provides getters and setters for such settings. 
*/
class TipsConfigGeneralInfo
	: public TipsConfigInfo
{
	public:
		TipsConfigGeneralInfo();

		TipsConfigGeneralInfo(const TipsConfigGeneralInfo &);

		virtual ~TipsConfigGeneralInfo() { }

		/** Individual tips can be enabled or disbaled and to know about it
		 * this function can be used. 
		 *
		 * @return BOOL if a tip is enabled .
		 */	
		BOOL				IsTipEnabled() { return m_enabled; }

		/** Whenever a tip appears on screen its time of display is recorded. 
		 * To get that this function can used. 
		 *
		 * @return last displayed time of a tip.
		 */	
		time_t				GetTipLastAppearance() { return m_last_appeared; }

		/** To enable/disable a tip this function can be used. 
		 *
		 * @param a flag which identifies whether to turn off / on a tip.
		 * @return last displayed time of a tip.
		 */	
		void				EnableTip(BOOL enable) { m_enabled = enable; }

		/** Tips' last appeared time can be set using this function. 
		 *
		 * @param time of last apperance. 
		 */	
		void				SetLastAppeared(time_t last_appeared) { m_last_appeared = last_appeared; }

		/** To check whether tip can be displayed or not this function be used.
		 * This function tests various criteria, like whether a tip is enabled or 
		 * not, when tip was last displayed. 
		 * 
		 * @return TRUE if a tip can be displayed.
		 */	
		BOOL				CanTipBeDisplayed();
		
		/** Tips' display time can be used using this function. 
		 * 
		 */	
		void				UpdateTipsLastDisplayedInfo();

		/** Overloaded the operator '='
		 * 
		 * @param const reference to TipsConfigGeneralInfo. 
		 * @return reference to TipsConfigGeneralInfo.
		 */	
		TipsConfigGeneralInfo& operator=(const TipsConfigGeneralInfo& r);

		/** Overloaded the operator '='
		 * 
		 * @param const reference to TipsConfigMetaInfo. 
		 * @return reference to TipsConfigGeneralInfo.
		 */	
		TipsConfigGeneralInfo& operator=(TipsConfigMetaInfo& r);

	private:
		BOOL				m_enabled; // Identifies whether tip is enabled or not.
		time_t				m_last_appeared; // Last appeared time. 
};

/** Tips configuration information can be read or written from 'tips.ini' and 'tips_meta.ini' 
* files using 'TipsConfigManager' class. This class also read information from 'operaprefs.ini'
* It manages all tips related files and setting from loading to writing. 
*/
class TipsConfigManager
{
	/* Note this class shouldn't return any information by reference */

	public:
		/** An instance of this can be created using this function. 
		* This class can only have singleton object. 
		* 
		* @return pointer to object if successfull, otherwise NULL if failed to create an instance. 
		*/
		static TipsConfigManager* GetInstance(){
			static TipsConfigManager *s_tips_configmanager = NULL;
			s_tips_configmanager = !s_tips_configmanager ? OP_NEW(TipsConfigManager, ()) : s_tips_configmanager;
			return s_tips_configmanager ? s_tips_configmanager->m_tips_configmanager = s_tips_configmanager : NULL;
		}
		
	   /** An instance of this can be distroyed using this method. 
		* 
		* @param an instance to this class which is created using GetInstance() method.
		* @return pointer to object if successfull, otherwise NULL if failed to create an instance. 
		*/
		static void Destroy(TipsConfigManager* ptr) {
			if (ptr && ptr->m_tips_configmanager == ptr)
			{
				OP_DELETE(ptr);
			}
		}

		/** One of the way to load tips' configuration from tips.ini and tips_meta.ini
		* files can be done via this function.
		*
		* @return OpStatus::OK if all configuration are loaded correctly, otherwise 
		* OpStatus::ERR_(XXX) when it failed to load for any reasons. 
		*/
		OP_STATUS			LoadConfiguration();

	   /** One of the way to save tips' configuration to tips.ini file
	    * can be done via this function.
		*
		* @return OpStatus::OK if all configuration are saved correctly, otherwise 
		* OpStatus::ERR_(XXX) when it failed to save for any reasons. 
		*/
		OP_STATUS			SaveConfiguration();

	   /** To fetch any given tips configuration can be done by this function.
	    * 
		* @param name of tip which is a tip's section name.
		* @param Retried data can be stored in 'tip_info'
		* @return OpStatus::OK if a tip's configuration is retrieved correctly, 
		* otherwise OpStatus::ERR_(XXX) when it failed to fetch for any reasons. 
		*/
		OP_STATUS			GetTipsConfigData(const OpStringC16& tip_name, TipsConfigGeneralInfo &tip_info);

	   /** Tip's last displayed information can be updated using this function. 
	    * This is an only interface which is available to do that. 
		* 
		* @param Retried data can be stored in 'tip_info'
		* @return OpStatus::OK a tip's last displayed information is updated
		* successfully, otherwise OpStatus::ERR_(XXX) when it failed for any reasons. 
		*/
		OP_STATUS			UpdateTipsLastDisplayedInfo(const TipsConfigGeneralInfo &tip_info);

	   /** One of the Tips' global tweak named 'Tips Display Interval' can be  
	    * fetched using this function. 
		* 
		* @return tips display interval. 
		*/
		UINT32				GetTipsDisplayInterval() { return  m_tips_global_tweaks.GetTipsDisplayInterval(); }

	   /** One of the Tips' global tweak named 'Tips Recurrence Interval ' can be  
	    * fetched using this function. 
		* 
		* @return tips recurrence interval. 
		*/
		UINT32				GetTipsRecurrenceInterval() { return m_tips_global_tweaks.GetTipsRecurrenceInterval(); }

		/** To query whether tips system is ON/OFF can be done via this function. 
		* 
		* @return TRUE if tip system is enabled, otherwise FALSE. 
		*/
		BOOL				IsTipsSystemEnabled() const { return m_tips_global_tweaks.IsTipsSystemEnabled(); }
		
		/** To check whether 'Never Turned Off' setting is ON/OFF this 
	    * function can be used. 
		* 
		* @return TRUE is setting enabled and otherwise FALSE.
		*/
		BOOL				IsTipsNeverOff() const { return m_tips_global_tweaks.IsTipsNeverOff(); }

	protected:
		TipsConfigManager();
		virtual ~TipsConfigManager();
		
	private:
		/** To combine and create a list of merged tips configuration from tips.ini 
		* and tips_meta.ini files this function can be used. 
		* 
		* @param List of configuration from the tips_meta.ini file.
		* @param List of configuration from the tips.ini file.
		* @return OpStatus::OK if function managed to combine tips sections from 
		* said file. On failed occassion it return OpStatus::ERR_(XXX) error code.
		*/
		OP_STATUS			CreateGlobalTipsInfo(const OpVector<TipsConfigMetaInfo> &meta_list, const OpVector<TipsConfigGeneralInfo> &general_list);

	   /** Loading of tips configuration from the tips_meta.ini file can be done 
	    * via this function.
		* 
		* @param location of tips_meta.ini file
		* @param list of fetched configuration are stored in second argument. 
		* @return OpStatus::OK if successfully fetched. On failed situation OpStatus::ERR_(XXX)
		* is returned. 
		*/
		OP_STATUS			LoadConfiguration(const OpFile &tips_configfile , OpVector<TipsConfigMetaInfo> &prefs_list);

	   /** Loading of tips configuration from the tips.ini file(created in user profile
	    * folder) can be done via this function.
		* 
		* @param location of tips.ini file
		* @param list of fetched configuration are stored in second argument. 
		* @return OpStatus::OK if successfully fetched. On failed situation OpStatus::ERR_(XXX)
		* is returned. 
		*/
		OP_STATUS			LoadConfiguration(const OpFile &tips_configfile , OpVector<TipsConfigGeneralInfo> &prefs_list);

	   /** Tips configuration can be saved using this function. 
	    * This function creates tips.ini file as specified in operaprefs.ini file
		* 
		* @param location of tips.ini file
		* @param list of maintained configuration which are required to save.
		* @return OpStatus::OK if successfully fetched. On failed situation OpStatus::ERR_(XXX)
		* is returned. 
		*/
		OP_STATUS			SaveConfiguration(const OpFile &tips_configfile , const OpVector<TipsConfigGeneralInfo> &prefs_list) const;

	private:
		TipsConfigManager  *m_tips_configmanager;	//Used to maintained a copy of object of this class. Take a look into the GetInstance() methode
		TipsConfigMetaInfo::TipsGlobalTweaks m_tips_global_tweaks; //List of tips' global configuration loaded from 'Global Settings' section located in tips_meta.ini
		OpAutoVector<TipsConfigGeneralInfo>	m_tips_general_configdata;	// Array holding the individual tip information
};

#endif //TIPS_CONFIG_DATA