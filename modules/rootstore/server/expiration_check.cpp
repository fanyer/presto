/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include <stdio.h>
#include <limits.h>
#include <string.h>


#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "modules/rootstore/server/types.h"
#include "modules/rootstore/rootstore_base.h"
#include "modules/rootstore/server/api.h"

int GetMonthCode(int year, int month)
{
	return year*12+(month-1);
}

int main()
{
	unsigned int i;

	time_t current_time = time(NULL);
	struct tm *time_fields = gmtime(&current_time);

	if(!time_fields)
		return FALSE;

	int current_year = time_fields->tm_year+1900;
	int current_month = time_fields->tm_mon+1;

	int current_month_code = GetMonthCode(current_year, current_month);
	int expiration_month_code = current_month_code +2;

	printf("The current status of the EV expirations per  %02d-%d is:\n\n", current_month, current_year);

	int listed_count =0;
	for(i=0; i< GetRootCount(); i++)
	{
		const DEFCERT_st *entry = GetRoot(i);

		if(entry == NULL || entry->dependencies == NULL)
			continue;

		int EV_expiration_code = GetMonthCode(entry->dependencies->EV_valid_to_year, entry->dependencies->EV_valid_to_month);

		if(EV_expiration_code< current_month_code - 2)
			continue; // Ignore old expired

		if(EV_expiration_code>= expiration_month_code)
			continue; // Ignore thos that will not expire soon

		listed_count++;
		printf(" * The certificate \"");
		if(entry->nickname != NULL)
			printf("%s", entry->nickname);
		else
		{
			const unsigned char *data = entry->dercert_data;
			X509 *cert = d2i_X509(NULL, &data, entry->dercert_len);
			if(cert == NULL)
			{
				printf("OOM error");
				break;
			}

			// Write oneline issuer name
			char *name = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
			if(name)
				printf("%s", name);
			else
			{
				printf("OOM error");
				X509_free(cert);
				break;
			}

			OPENSSL_free(name);
			X509_free(cert);
		}
		if(EV_expiration_code< current_month_code)
			printf("\" *expired* ");
		else
			printf("\" will expire ");

		printf("%02d-%d\n\n", entry->dependencies->EV_valid_to_month, entry->dependencies->EV_valid_to_year);
	}

	if(!listed_count)
		printf("No EV enablings will expire soon");
}
