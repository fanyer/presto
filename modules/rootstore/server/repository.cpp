/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
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

struct dep_item_st
{
	unsigned int item;
	unsigned int before;
	unsigned int after;
};

class repository_item 
{
private:
	char *filename;

	unsigned int item_count;
	// Which certificate items are directly included in this file? (not dependents)
	unsigned int *items;

	unsigned int dep_item_count;
	dep_item_st *dep_items;
	
	// Expiration of update
	char *update_expiration;
	int update_before;
	int update_after;

	// Next item in the list
	repository_item *next_item;

public:

	repository_item():filename(NULL), item_count(0),  items(NULL), dep_item_count(0),  dep_items(NULL), update_expiration(NULL), next_item(NULL), update_before(0), update_after(0){};
	~repository_item();

	/** Takes over name */
	BOOL Construct(char *name);

	const char *GetName() const{return filename;}
	
	unsigned int GetItem(unsigned int i) const;
	void AddItem(unsigned int i);
	unsigned int GetItemCount() const {return item_count;}

	const dep_item_st *GetDepItem(unsigned int i) const;
	void AddDepItem(unsigned int i, unsigned int before, unsigned int after);
	unsigned int GetDepItemCount() const {return dep_item_count;}
	
	BOOL SetExpirationDate(const char *exp, size_t len);
	const char *GetExpiration() const {return update_expiration;}

	void SetUpdate(int bef, int aft){update_before = bef;update_after = aft;}
	int GetUpdateBefore(){return update_before;}
	int GetUpdateAfter(){return update_after;}

	repository_item *Next(){return next_item;}
	repository_item *Pop(){repository_item *temp = next_item; next_item = NULL; return temp;}

	void Append(repository_item *new_item){if(new_item){new_item->next_item = next_item; next_item = new_item;}}
	void InsertInfront(repository_item *later_item);
};

// Head of repository list
class repository
{
private:
	repository_item *head;

public:

	repository():head(NULL){}
	~repository();

	repository_item *GetFirst(){return head;}

	void Insert(repository_item *item){if(head){ item->InsertInfront(head);} head = item;}
};

/** Destructor */
repository_item::~repository_item()
{
	delete [] filename;
	filename = NULL;
	delete [] items;
	items = NULL;
	item_count = 0;
	delete [] dep_items;
	dep_items = NULL;
	dep_item_count = 0;
	delete [] update_expiration;
	update_expiration = NULL;
	delete next_item;
	next_item = NULL;
}

/** Construct item*/
BOOL repository_item::Construct(char *name)
{
	if(name == NULL)
		return FALSE;

	filename = name;

	items = new unsigned int[GetRootCount()];
	if(items == NULL)
		return FALSE;

	dep_items = new dep_item_st[GetRootCount()];
	if(dep_items == NULL)
		return FALSE;

    return TRUE;
}

/** Get the indicated Item ID */
unsigned int repository_item::GetItem(unsigned int i) const
{
	if(items && i < item_count)
		return items[i];

	return UINT_MAX;
}

/** Add another item to the list */
void repository_item::AddItem(unsigned int item_index)
{
	unsigned int i;

	if(!items || item_count >= GetRootCount())
		return;

	for(i=0;i<item_count;i++)
	{
		if(items[i] == item_index)
			return; // already in list;
	}
    items[item_count++] = item_index;
}

/** Get a dependency item */
const dep_item_st *repository_item::GetDepItem(unsigned int i) const
{
	if(dep_items && i < dep_item_count)
		return &dep_items[i];

	return NULL;
}

/** Add another dependency item */
void repository_item::AddDepItem(unsigned int item_index, unsigned int before, unsigned int after)
{
	unsigned int i;

	if(!dep_items || dep_item_count >= GetRootCount())
		return;

	for(i=0;i<dep_item_count;i++)
	{
		if(dep_items[i].item == item_index)
			return; // already in list;
	}
    dep_items[dep_item_count].item = item_index;
	dep_items[dep_item_count].before = before;
	dep_items[dep_item_count++].after = after;

}

/** Set the expiration date for certificates that have to be updated. */
BOOL repository_item::SetExpirationDate(const char *exp, size_t len)
{
	if(!exp || !*exp || !len)
		return TRUE;
	
	update_expiration = new char[len+1];
	if(!update_expiration)
		return FALSE;
	
	strncpy(update_expiration, exp, len+1);
	update_expiration[len] = '\0';
	
	return TRUE;
}

/** Insert new repository item */
void repository_item::InsertInfront(repository_item *later_item)
{
	if(later_item == NULL)
		return;
	if(next_item)
	{
		repository_item *end_item = next_item;
		while(end_item->Next() != NULL)
			end_item = end_item->Next();

		end_item->Append(later_item);
	}
	else
		next_item = later_item;
}

/** Destructor */
repository::~repository()
{
	while(head)
	{
		repository_item *current = head;
		head = current->Pop();

		delete current;
	}
}

/** Add a new item to the repository, for a specific version, pubkey and keyID */
BOOL AddRepositoryItem(repository &list, unsigned int version_num, unsigned int item_index, BOOL with_pub_key, const DEFCERT_keyid_item *key_id_item)
{
	const DEFCERT_st *item = GetRoot(item_index);
	if(item == NULL || item->dercert_data == NULL)
		return TRUE;

	if(item->dependencies && 
		!((version_num == 1 && item->dependencies->include_last_version == 0) ||
		  (version_num != 1 && item->dependencies->include_first_version <= version_num &&
			(item->dependencies->include_last_version == 0 || item->dependencies->include_last_version >= version_num)  )))
		return TRUE; 

	BOOL ret = FALSE;
	X509 *cert = NULL;
	char *name = NULL;
	const unsigned char *data = item->dercert_data;
	cert = d2i_X509(NULL, &data, item->dercert_len);
	if(cert == NULL)
		return FALSE;

	do{
		ASN1_TIME *exp = X509_get_notAfter(cert);
		if(X509_cmp_current_time(exp) <=0)
		{
			ret = TRUE; // Expired certificate; don't include, but return success since this is not an error
			break;
		}

		name = GenerateFileName(version_num, item, with_pub_key, key_id_item);
		if(name == NULL)
			break;

		repository_item *current = list.GetFirst();
		while(current)
		{
			// Do we already have a file with the same name?
			if(current->GetName() && strcmp(name, current->GetName()) == 0)
			{
				// If so, use it
				current->AddItem(item_index);
				break;
			}

			current = current->Next();
		}

		if(!current)
		{
			// Or we make a new one
			current = new repository_item;

			if(!current)
			{
				break;
			}

			if(!current->Construct(name))
			{
				name = NULL; // name deleted by object
				delete current;
				break;
			}
			name = NULL;

			current->AddItem(item_index);

			list.Insert(current);
		}

		if(current && with_pub_key && !key_id_item && item->def_replace)
		{
			// If it is a replace item, specify the date.
			ret = FALSE;
			do{
				// If data already set, warn, as there are more than one certificate with the same ID
				if(current->GetExpiration())
				{
					printf("   !!!Warning!!!: Multiple certificates with same ID are updated!!!");
					ret = TRUE;
					break;
				}

				// Get the date from the certificate
				current->SetExpirationDate((const char *) exp->data,exp->length);
				if(item->dependencies != NULL && (item->dependencies->update_before != 0 || item->dependencies->update_after != 0))
					current->SetUpdate(item->dependencies->update_before, item->dependencies->update_after);
				ret=TRUE;
			}
			while(0);

			if(!ret)
				break;

			ret = FALSE;
		}


		if(current && with_pub_key && !key_id_item && item->dependencies && item->dependencies->dependencies)
		{
			// Add list of dependencies, if any.
			const DEFCERT_depcertificate_item *deps = item->dependencies->dependencies;

			for(;deps->cert;deps++)
			{
				if(deps->cert == item->dercert_data)
					continue; // Just in case

				unsigned int j;
				for (j=0; j< GetRootCount();j++)
				{
					const DEFCERT_st *item2 = GetRoot(j);
					if(!item2)
						continue;

					if(item2->dercert_data == deps->cert)
					{
						current->AddDepItem(j, deps->before, deps->after);
						break; // There can only be one
					}
				}
			}
		}
		ret = TRUE;
	}while(0);

	delete [] name;
	if(cert)
		X509_free(cert);
	cert = NULL;
	return ret;
}


/** Build the repository list */
BOOL ConstructRepositoryList(repository &list, unsigned int version_num)
{
	unsigned int i;

	for(i=0; i< GetRootCount(); i++)
	{
        const DEFCERT_st *item = GetRoot(i);
        if(item == NULL)
            continue;

        // Add items with ID determined by just the name
        if(!AddRepositoryItem(list, version_num, i, FALSE, NULL))
			return FALSE;;
		// Add items with ID determined by name and pubkey
        if(!AddRepositoryItem(list, version_num, i, TRUE, NULL))
			return FALSE;;
			
		// If there is a KeyID list, add items based on name and the key ID
        if(item->dependencies && item->dependencies->key_ids)
        {
            const DEFCERT_keyid_item * const *dep_item = item->dependencies->key_ids;
            
            while(*dep_item)
				if(!AddRepositoryItem(list, version_num, i, FALSE, *(dep_item++)))
					return FALSE;
        }
	}
    return TRUE;
}

/** Open a file for specified version, path and name **/
BIO *GenerateFile(const char *version, const char *path, const char *name)
{
	char *buffer = new char[2048+1];
    if(buffer == NULL)
        return NULL;

	snprintf(buffer, 2048, "output/%s/%s%s.xml", version, path, name);

	BIO *ret = BIO_new_file(buffer, "wb");

	delete [] buffer;
	return ret;
}


/** Master for generating the certificate repository */ 
BOOL CreateRepository(const char *version, unsigned int version_num, EVP_PKEY *key, int sig_alg)
{
	repository list;
    unsigned int j;

	if(!key)
		return FALSE;

	// Get a list over all the files to generate
	if(!ConstructRepositoryList(list, version_num))
		return FALSE;

	unsigned int to_visit_len = GetRootCount();
	BOOL *to_visit = new BOOL[to_visit_len];

	if(to_visit == NULL)
		return FALSE;

	// Open the repository index file
	BIO *repository_file = GenerateFile(version, "", "repository");
	if(!repository_file)
	{
		delete [] to_visit;
		return FALSE;
	}

	// First write content to a mem file, to facilitate signing
	BIO *repository_file1 = BIO_new(BIO_s_mem());
	if(!repository_file1)
	{
		BIO_free(repository_file);
		delete [] to_visit;
		return FALSE;
	}

	BOOL ret = FALSE;
	BIO *target = NULL;
	BIO *target1 = NULL;

	do{
		// Header before signature goes direct to target file, not part of the signature
		if(!BIO_puts(repository_file, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"))
					break;

		// First signed content
		if(!BIO_puts(repository_file1, "-->\n<!-- Copyright (C) 2008-2009 Opera Software ASA. All Rights Reserved. -->\n\n<repository>\n"))
					break;

		repository_item *current = list.GetFirst();
		while(current)
		{
			ret = FALSE;
			if(current->GetName() && *current->GetName())
			{
				// Open target fil
				target = GenerateFile(version, "roots/", current->GetName());
				if(!target)
					break;

				// Memfile for signing
				target1 = BIO_new(BIO_s_mem());
				if(!target1)
					break;

				// Add to repository list
				if(!BIO_puts(repository_file1, "<repository-item>"))
					break;
				if(!BIO_puts(repository_file1, current->GetName()))
					break;
				
				BOOL first = TRUE;
				
				// add extra update conditions to repository index 
				if(current->GetExpiration())
				{
					first = FALSE;
					if(!BIO_puts(repository_file1, "\n    <update-item"))
						break;

					if((version_num == 1 || version_num >= 3) && current->GetUpdateBefore())
					{
						if(BIO_printf(repository_file1, " before=\"%u\"", current->GetUpdateBefore()) <= 0)
							break;
					}

					if((version_num == 1 || version_num >= 3) && current->GetUpdateAfter())
					{
						if(BIO_printf(repository_file1, " after=\"%u\"", current->GetUpdateAfter()) <= 0)
							break;
					}

					if(!BIO_puts(repository_file1, ">"))
						break;
					if(!BIO_puts(repository_file1, current->GetExpiration()))
						break;
					if(!BIO_puts(repository_file1, "</update-item>\n"))
						break;
				}
				
				if(current->GetDepItemCount())
				{
					ret = TRUE;
					
					// Write list of certificates required by this one
					for(j=0;j<current->GetDepItemCount();j++)
					{
						const dep_item_st *current_item = current->GetDepItem(j);
						const DEFCERT_st *item2 = GetRoot(current_item->item);
						if(!item2)
							continue;
						
						char *depname = GenerateFileName(version_num, item2, TRUE, NULL);
				
						if(depname)
						{
							ret = FALSE;
							do{
								if(first && !BIO_puts(repository_file1, "\n"))
									break;
								first = FALSE;
								if(!BIO_puts(repository_file1, "    <required-item"))
									break;
								if(current_item->before && BIO_printf(repository_file1, " before=\"%u\"", current_item->before) <= 0)
									break;
								if(current_item->after && BIO_printf(repository_file1, " after=\"%u\"", current_item->after) <= 0)
									break;
								if(!BIO_puts(repository_file1, ">"))
									break;
								if(depname && !BIO_puts(repository_file1, depname))
									break;
								if(!BIO_puts(repository_file1, "</required-item>\n"))
									break;
								ret = TRUE;
							}
							while(0);
							
							delete [] depname;
							depname = NULL;
							if(!ret)
								break;
						}
						ret = TRUE;
					}
					if(!ret)
						break;
					ret = FALSE;
				}
				if(!BIO_puts(repository_file1, "</repository-item>\n"))
					break;


				// Unmark visited list
				for(j=0; j< to_visit_len;j++)
					to_visit[j]=FALSE;

				// Fetch list of certificates to list in the file.
				for(j=0; j <current->GetItemCount(); j++)
				{
					ProduceCertificate_List(current->GetItem(j), to_visit, to_visit_len);
				}

				// Header before signature
				if(!BIO_puts(target, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"))
					break;

				// First part of signed content
				if(!BIO_puts(target1, "-->\n<!-- Copyright (C) 2008-2009 Opera Software ASA. All Rights Reserved. -->\n\n<certificates>\n"))
					break;

				// For each certificate to list, output the XML for the certificate
				for(j=0; j< to_visit_len;j++)
				{
					ret = FALSE;
					if(to_visit[j] && !ProduceCertificateXML(target1, GetRoot(j), key, sig_alg, version_num))
						break;
					ret = TRUE;
				}
				if(!ret)
					break;
				ret = FALSE;

				// Finish
				if(!BIO_puts(target1, "</certificates>\n"))
					break;
				if(!BIO_flush(target1))
					break;

				unsigned char *buffer = NULL;
				unsigned long buffer_len =0;

				// Get the memory buffer
				buffer_len = BIO_get_mem_data(target1, &buffer);
				if(buffer == NULL || buffer_len == 0)
					break;

				// Create the signed output
				if(!SignXMLData(target, buffer, buffer_len, key, sig_alg))
					break;

				// Cleanup
				if(!BIO_flush(target))
					break;
				BIO_free(target1);
				BIO_free(target);
				target = NULL;
				target1 = NULL;
			}

			ret = TRUE;
			current = current->Next();
		}
		if(!ret)
			break;
		ret = FALSE;
		
		// Are we ordering the deletion of any certificates?
		if(GetDeleteCertCount())
		{
			if(!BIO_puts(repository_file1, "\n<delete-list>\n"))
				break;
			
			int i,n=GetDeleteCertCount();
						
			ret = TRUE;
			for(i=0; i<n; i++)
			{
				const DEFCERT_DELETE_cert_st *item = GetDeleteCertSpec(i);
				
				if(!item)
					continue;
				
				// Construct a filename ID
				char *name = GenerateFileName(version_num, item->dercert_data, item->dercert_len, TRUE,NULL);
				if(!name)
					continue;

				ret = FALSE;
				// Identify item
				if(!BIO_puts(repository_file1, "<delete-item>"))
					break;
				if(!BIO_puts(repository_file1, name))
					break;
				if(item->reason && *item->reason)
				{
					// explain why, if needed.
					if(!BIO_puts(repository_file1, "\n    <delete-reason>"))
						break;
					if(!BIO_puts(repository_file1, item->reason))
						break;
					if(!BIO_puts(repository_file1, "</delete-reason>\n"))
						break;
				}
				if(!BIO_puts(repository_file1, "</delete-item>\n"))
					break;
				ret = TRUE;
			}
			if(!ret)
				break;
			ret = FALSE;
			
			if(!BIO_puts(repository_file1, "</delete-list>\n"))
				break;
		}

		// Any untrusted certs?
		if(GetUntrustedCertCount() || GetUntrustedRepositoryCertCount())
		{
			BOOL written_header = FALSE;
			
			int i,n=GetUntrustedCertCount();
						
			ret = TRUE;
			for(i=0; i<n; i++)
			{
				const DEFCERT_UNTRUSTED_cert_st *item = GetUntrustedCertSpec(i);
				
				if(!item)
					continue;
				if(!(version_num == 1  ||
					 (version_num != 1 && item->include_first_version <= version_num &&
						(item->include_last_version == 0 || item->include_last_version >= version_num)  )))
					continue;
				
				// Construct name ID
				char *name = GenerateFileName(version_num, item->dercert_data, item->dercert_len, TRUE,NULL);
				if(!name)
					continue;

				ret = FALSE;

				if(!written_header)
				{
					written_header = TRUE;
					if(!BIO_puts(repository_file1, "\n<untrusted-list>\n"))
						break;
				}

				if(!item->before || item->before>2 ||(!item->after &&  item->before>2)||  item->after >=2)
				{
					// identify the cert
					if(!BIO_puts(repository_file1, "<untrusted-item"))
						break;
					if(item->before && BIO_printf(repository_file1, " before=\"%u\"", item->before) <= 0)
						break;
					if(item->after && BIO_printf(repository_file1, " after=\"%u\"", item->after) <= 0)
						break;
					if(!BIO_puts(repository_file1, ">"))
						break;
					if(!BIO_puts(repository_file1, name))
						break;
					if(item->reason && *item->reason)
					{
						// Explain why
						if(!BIO_puts(repository_file1, "\n    <untrusted-reason>"))
							break;
						if(!BIO_puts(repository_file1, item->reason))
							break;
						if(!BIO_puts(repository_file1, "</untrusted-reason>\n"))
							break;
					}
					if(!BIO_puts(repository_file1, "</untrusted-item>\n"))
						break;

					// Create the file
					if(ProduceUntrustedCertificateXML_File(version, version_num,item,key,sig_alg))
						ret = TRUE;
				}

				BOOL gen_without_pubkey = FALSE;

				do {
					const unsigned char *data = item->dercert_data;
					X509 *cert = d2i_X509(NULL, &data, item->dercert_len);
					if(cert == NULL)
						break;

					if(X509_check_issued(cert,cert) != 0)
						gen_without_pubkey = TRUE; // Only generate without key included in hash if the cert is not selfsigned // Caused by mistake in pre 9-64 builds

					X509_free(cert);
				}while(0);

				//if(gen_without_pubkey)
				{
					// Construct name ID, without key if not selfsigned
					name = GenerateFileName(version_num, item->dercert_data, item->dercert_len, !gen_without_pubkey,NULL);
					if(!name)
						continue;

					ret = FALSE;
					// identify the cert
					if(!BIO_puts(repository_file1, "<untrusted-item before=\"2\">"))
						break;
					if(!BIO_puts(repository_file1, name))
						break;
					if(item->reason && *item->reason)
					{
						// Explain why
						if(!BIO_puts(repository_file1, "\n    <untrusted-reason>"))
							break;
						if(!BIO_puts(repository_file1, item->reason))
							break;
						if(!BIO_puts(repository_file1, "</untrusted-reason>\n"))
							break;
					}
					if(!BIO_puts(repository_file1, "</untrusted-item>\n"))
						break;
					
					// Create the file
					if(ProduceUntrustedCertificateXML_File(version, version_num,item,key,sig_alg, FALSE))
					ret = TRUE;
				}
			}
			if(!ret)
				break;
			ret = FALSE;
			
			if(GetUntrustedRepositoryCertCount())
			{
				int i,n=GetUntrustedRepositoryCertCount();

				ret = TRUE;
				for(i=0; i<n; i++)
				{
					const DEFCERT_UNTRUSTED_cert_st *item = GetUntrustedRepositoryCertSpec(i);

					if(!item)
						continue;

					if(!((version_num == 1 && item->include_last_version == 0) ||
						 (version_num != 1 && item->include_first_version <= version_num &&
							(item->include_last_version == 0 || item->include_last_version >= version_num)  )))
						continue;

					// Construct name ID
					char *name = GenerateFileName(version_num, item->dercert_data, item->dercert_len, TRUE,NULL);
					if(!name )
						continue;

					ret = FALSE;
					if(!written_header)
					{
						written_header = TRUE;
						if(!BIO_puts(repository_file1, "\n<untrusted-list>\n"))
							break;
					}

					// identify the cert
					if(!BIO_puts(repository_file1, "<untrusted-repository-item"))
						break;
					if(item->before && BIO_printf(repository_file1, " before=\"%u\"", item->before) <= 0)
						break;
					if(item->after && BIO_printf(repository_file1, " after=\"%u\"", item->after) <= 0)
						break;
					if(!BIO_puts(repository_file1, ">"))
						break;
					if(!BIO_puts(repository_file1, name))
						break;
					if(item->reason && *item->reason)
					{
						// Explain why
						if(!BIO_puts(repository_file1, "\n    <untrusted-reason>"))
							break;
						if(!BIO_puts(repository_file1, item->reason))
							break;
						if(!BIO_puts(repository_file1, "</untrusted-reason>\n"))
							break;
					}
					if(!BIO_puts(repository_file1, "</untrusted-repository-item>\n"))
						break;

					// Create the file
					if(ProduceUntrustedCertificateXML_File(version, version_num,item,key,sig_alg))
						ret = TRUE;
				}
			}
			if(!ret)
				break;
			ret = FALSE;

			if(!BIO_puts(repository_file1, "</untrusted-list>\n"))
				break;
		}

#define CRL_OVERRIDES
#ifdef CRL_OVERRIDES
		// Output the CRL overrides
		const DEFCERT_crl_overrides *crl_item = GetCRLOverride();
		ret = TRUE;
		while(crl_item->certificate || crl_item->name_field)
		{
			if(crl_item->certificate && crl_item->real_crl &&
				((version_num == 1 && crl_item->include_last_version == 0) ||
				  (version_num != 1 && crl_item->include_first_version <= version_num &&
					(crl_item->include_last_version == 0 || crl_item->include_last_version >= version_num) )) 
				)
			{
				// Which certificate ID?
				char *id = GenerateFileName(0, crl_item->certificate, crl_item->data_field_len, FALSE, NULL);
				if(id == NULL)
				{
					ret = FALSE;
					break;
				}

				do{
					ret = FALSE;
					printf("CRL %s , before=%d after=%d\n", crl_item->real_crl, crl_item->before, crl_item->after);

					// output ID, before and after version, and the override URL
					if(!BIO_puts(repository_file1, "<crl-location"))
						break;
					if(crl_item->before && BIO_printf(repository_file1, " before=\"%u\"", crl_item->before) <= 0)
						break;
					if(crl_item->after && BIO_printf(repository_file1, " after=\"%u\"", crl_item->after) <= 0)
						break;
					if(!BIO_puts(repository_file1, ">\n    <issuer-id>"))
						break;
					if(!BIO_puts(repository_file1, id))
						break;
					if(!BIO_puts(repository_file1, "</issuer-id>\n"))
						break;
					if(!BIO_puts(repository_file1, "    <crl-url>"))
						break;
					if(!BIO_puts(repository_file1, crl_item->real_crl))
						break;
					if(!BIO_puts(repository_file1, "</crl-url>\n"))
						break;
					if(!BIO_puts(repository_file1, "</crl-location>\n"))
						break;
					ret = TRUE;
				}while(0);

				delete [] id;

				if(!ret)
					break;
			}

			crl_item++;
		}

		if(!ret)
			break;

		// List OCSP overrides for URLs that require the POST method to be used.
		DEFCERT_ocsp_overrides *ocsp_item = GetOCSPOverride();
		while(*ocsp_item)
		{
			ret = FALSE;
					if(!BIO_puts(repository_file1, "<ocsp-override>"))
						break;
					if(!BIO_puts(repository_file1, *ocsp_item))
						break;
					if(!BIO_puts(repository_file1, "</ocsp-override>\n"))
						break;


			ret = TRUE;
			ocsp_item ++;

		}
#endif
		
		// End of repository list
		if(!BIO_puts(repository_file1, "</repository>\n"))
			break;
		
		if(!BIO_flush(repository_file1))
			break;

		unsigned char *buffer = NULL;
		unsigned long buffer_len =0;

		// Get memory buffer
		buffer_len = BIO_get_mem_data(repository_file1, &buffer);
		if(buffer == NULL || buffer_len == 0)
			break;

		// create signed content
		if(!SignXMLData(repository_file, buffer, buffer_len, key, sig_alg))
			break;

		// finished
		if(!BIO_flush(repository_file))
			break;
		ret = TRUE;
	}
	while(0);

	// Cleanup
	delete [] to_visit;
	BIO_free(target1);
	BIO_free(target);
	BIO_free(repository_file);
	BIO_free(repository_file1);
	repository_file = NULL;
	repository_file1 = NULL;
    return TRUE;

}

