/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef MASTERPASSWORDLIFETIME_H_INCLUDED_
#define MASTERPASSWORDLIFETIME_H_INCLUDED_

/**
 * @brief Class to hold master password timeout
 */
class MasterPasswordLifetime
{
public:
	enum Values
	{
		ALWAYS = 5,
		SHORT_TIME = 300,
		LONGER_TIME = 1800,
		FAIR_TIME = 3600,
		LONG_TIME = 7200,
		ONCE = 315568800, // Ask for master password every 10 years
	};

	/**
	 * @brief Default constructor assuring creation of master password
	 *        lifetime with defined and safe/restrictive initial value
	 */
	MasterPasswordLifetime(): m_value(ALWAYS) { }

	/**
	 * @brief Comparison operator
	 *
	 * @param other Compared agrument
	 */
	bool operator==(const MasterPasswordLifetime &other) const
	{
		return m_value == other.m_value;
	}

	/**
	 * @brief Get lifetime in seconds
	 *
	 * @return Lifetime in seconds
	 */
	int GetValue() const { return m_value; }

	/**
	 * @brief Set master password lifetime
	 *
	 * @param lifetime_in_seconds value in seconds
	 * @return Reference to self for statement cascading
	 */
	MasterPasswordLifetime &SetValue(int lifetime_in_seconds);

	/**
	 * @brief Convert to textual description of value
	 *
	 * @param result Output string
	 * @return OpStatus::OK in case of successful operation, other otherwise
	 */
	OP_STATUS ToString(OpString &result) const;

	/**
	 * @brief Save the lifetime to Opera preferences
	 */
	void Write() const;

	/**
	 * @brief Load the lifetime from Opera preferences
	 */
	void Read();

private:

	int m_value;
};


#endif /* MASTERPASSWORDLIFETIME_H_INCLUDED_ */
