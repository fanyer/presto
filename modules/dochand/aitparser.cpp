/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef XML_AIT_SUPPORT

#include "modules/dochand/aitparser.h"
#include "modules/hardcore/base/baseincludes.h"

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlfragment.h"

//static
OP_STATUS AITParser::Parse(URL &url, AITData &ait_data)
{
	XMLFragment fragment;
	int retval = fragment.Parse(url);
	if (OpStatus::IsError(retval))
		return retval;

	fragment.EnterAnyElement();

	// Find all Application elemets that are present and collect their
	// information in one large list.
	do
	{
		while (fragment.HasCurrentElement() && !uni_str_eq(fragment.GetElementName().GetLocalPart(), "Application"))
		{
			if (!fragment.HasMoreElements())
				fragment.LeaveElement();

			fragment.EnterAnyElement();
		}

		// Found an application.
		if (fragment.HasCurrentElement() && uni_str_eq(fragment.GetElementName().GetLocalPart(), "Application"))
		{
			OP_STATUS status;

			AITApplication *application = OP_NEW(AITApplication, ());
			RETURN_OOM_IF_NULL(application);
			status = ait_data.applications.Add(application);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(application);
				return status;
			}

			TempBuffer buffer;
			// Parse the application data.
			do
			{
				if (!fragment.HasMoreElements())
					fragment.LeaveElement();

				fragment.EnterAnyElement();

				const uni_char *name = fragment.GetElementName().GetLocalPart();
				buffer.Clear();

				if (uni_str_eq(name, "appName"))
				{
					fragment.GetAllText(buffer);
					RETURN_IF_ERROR(application->name.Set(buffer.GetStorage()));
					buffer.Clear();
					fragment.LeaveElement();
				}
				else if (uni_str_eq(name, "applicationIdentifier"))
				{
					while (fragment.HasMoreElements())
					{
						if (fragment.EnterAnyElement())
						{
							fragment.GetAllText(buffer);

							const uni_char *tmp_name = fragment.GetElementName().GetLocalPart();
							if (uni_str_eq(tmp_name, "orgId"))
								application->org_id = uni_atoi(buffer.GetStorage());
							else if (uni_str_eq(tmp_name, "appId"))
								application->app_id = uni_atoi(buffer.GetStorage());

							buffer.Clear();
							fragment.LeaveElement();
						}
					}

					fragment.LeaveElement();

				}
				else if (uni_str_eq(name, "applicationDescriptor"))
				{
					while (fragment.HasMoreElements())
					{
						if (fragment.EnterAnyElement())
						{
							fragment.GetAllText(buffer);
							const uni_char *value = buffer.GetStorage();

							const uni_char *tmp_name = fragment.GetElementName().GetLocalPart();
							if (uni_str_eq(tmp_name, "controlCode") && value)
							{
								if (uni_str_eq(value, "AUTOSTART"))
									application->descriptor.control_code = AITApplicationDescriptor::CONTROL_CODE_AUTOSTART;
								else if (uni_str_eq(value, "PRESENT"))
									application->descriptor.control_code = AITApplicationDescriptor::CONTROL_CODE_PRESENT;
								else if (uni_str_eq(value, "KILL"))
									application->descriptor.control_code = AITApplicationDescriptor::CONTROL_CODE_KILL;
								else if (uni_str_eq(value, "DISABLED"))
									application->descriptor.control_code = AITApplicationDescriptor::CONTROL_CODE_DISABLED;
							}
							else if (uni_str_eq(tmp_name, "visibility") && value)
							{
								if (uni_str_eq(value, "NOT_VISIBLE_ALL"))
									application->descriptor.visibility = AITApplicationDescriptor::VISIBILITY_NOT_VISIBLE_ALL;
								else if (uni_str_eq(value, "NOT_VISIBLE_USERS"))
									application->descriptor.visibility = AITApplicationDescriptor::VISIBILITY_NOT_VISIBLE_USERS;
								else if (uni_str_eq(value, "VISIBLE_ALL"))
									application->descriptor.visibility = AITApplicationDescriptor::VISIBILITY_VISIBLE_ALL;
							}
							else if (uni_str_eq(tmp_name, "serviceBound") && value)
							{
								if (uni_str_eq(value, "false"))
									application->descriptor.service_bound = FALSE;
								else if (uni_str_eq(value, "true"))
									application->descriptor.service_bound = TRUE;
							}
							else if (uni_str_eq(tmp_name, "priority") && value)
							{
								application->descriptor.priority = static_cast<unsigned char>(uni_strtoul(value, NULL, 16, NULL));
							}
							else if (uni_str_eq(tmp_name, "mhpVersion"))
							{
								AITApplicationMhpVersion *mhp_version = OP_NEW(AITApplicationMhpVersion, ());
								RETURN_OOM_IF_NULL(mhp_version);
								status = application->descriptor.mhp_versions.Add(mhp_version);
								if (OpStatus::IsError(status))
								{
									OP_DELETE(mhp_version);
									return status;
								}

								TempBuffer buffer2;
								while (fragment.HasMoreElements())
								{
									fragment.EnterAnyElement();

									const uni_char *tag_name = fragment.GetElementName().GetLocalPart();
									if (uni_str_eq(tag_name, "profile"))
									{
										fragment.GetAllText(buffer2);
										mhp_version->profile = static_cast<unsigned short>(uni_atoi(buffer2.GetStorage()));
									}
									else if (uni_str_eq(tag_name, "versionMajor"))
									{
										fragment.GetAllText(buffer2);
										mhp_version->version_major = static_cast<unsigned char>(uni_atoi(buffer2.GetStorage()));
									}
									else if (uni_str_eq(tag_name, "versionMinor"))
									{
										fragment.GetAllText(buffer2);
										mhp_version->version_minor = static_cast<unsigned char>(uni_atoi(buffer2.GetStorage()));
									}
									else if (uni_str_eq(tag_name, "versionMicro"))
									{
										fragment.GetAllText(buffer2);
										mhp_version->version_micro = static_cast<unsigned char>(uni_atoi(buffer2.GetStorage()));
									}

									buffer2.Clear();
									fragment.LeaveElement();
								}
							}

							buffer.Clear();
							fragment.LeaveElement();
						}
					}
					fragment.LeaveElement();
				}
				else if (uni_str_eq(name, "applicationUsageDescriptor"))
				{
					while (fragment.HasMoreElements())
					{
						if (fragment.EnterAnyElement())
						{
							fragment.GetAllText(buffer);

							const uni_char *tmp_name = fragment.GetElementName().GetLocalPart();
							if (uni_str_eq(tmp_name, "ApplicationUsage"))
							{
								const uni_char *applicationUsage = buffer.GetStorage();
								if (applicationUsage && uni_str_eq(applicationUsage, "urn:dvb:mhp:2009:digitalText"))
									application->usage = 0x01;
							}

							buffer.Clear();
							fragment.LeaveElement();
						}
					}

					fragment.LeaveElement();
				}
				else if (uni_str_eq(name, "applicationTransport"))
				{
					AITApplicationTransport *transport = OP_NEW(AITApplicationTransport, ());
					RETURN_OOM_IF_NULL(transport);
					status = application->transports.Add(transport);
					if (OpStatus::IsError(status))
					{
						OP_DELETE(transport);
						return status;
					}

					const uni_char *transport_type = NULL;
					XMLCompleteName attr_name;
					const uni_char *attr_value;
					while(fragment.GetNextAttribute(attr_name, attr_value))
					{
						if (uni_str_eq(attr_name.GetLocalPart(), "type"))
						{
							transport_type = attr_value;
							break;
						}
					}

					if (!transport_type)
						transport_type = UNI_L("");

					if (uni_str_eq(transport_type, "mhp:HTTPTransportType"))
					{
						while (fragment.HasMoreElements())
						{
							fragment.EnterAnyElement();

							const uni_char *tag_name = fragment.GetElementName().GetLocalPart();
							if (uni_str_eq(tag_name, "URLBase"))
							{
								fragment.GetAllText(buffer);
								RETURN_IF_ERROR(transport->base_url.Set(buffer.GetStorage()));
								buffer.Clear();
							}

							fragment.LeaveElement();
						}

						transport->protocol = 0x0003;

					}
					else if (uni_str_eq(transport_type, "mhp:OCTransportType"))
					{
						while (fragment.HasMoreElements())
						{
							fragment.EnterAnyElement();

							const uni_char *tag_name = fragment.GetElementName().GetLocalPart();
							if (uni_str_eq(tag_name, "DVBTriplet"))
							{
								const uni_char *original_network_id = fragment.GetAttribute(UNI_L("OrigNetId"));
								const uni_char *transport_stream_id = fragment.GetAttribute(UNI_L("TSId"));
								const uni_char *service_id = fragment.GetAttribute(UNI_L("ServiceId"));

								transport->original_network_id = (original_network_id ? static_cast<unsigned short>(uni_strtoul(original_network_id, NULL, 10, NULL)) : 0);
								transport->transport_stream_id = (transport_stream_id ? static_cast<unsigned short>(uni_strtoul(transport_stream_id, NULL, 10, NULL)) : 0);
								transport->service_id = (service_id ? static_cast<unsigned short>(uni_strtoul(service_id, NULL, 10, NULL)) : 0);
								transport->remote = TRUE;
							}
							else if (uni_str_eq(tag_name, "ComponentTag"))
							{
								fragment.GetAllText(buffer);
								transport->component_tag = static_cast<unsigned char>(uni_strtoul(buffer.GetStorage(), NULL, 16, NULL));
								buffer.Clear();
							}

							fragment.LeaveElement();
						}

						transport->protocol = 0x0001;
					}

					fragment.LeaveElement();
				}
				else if (uni_str_eq(name, "applicationBoundary"))
				{
					while (fragment.HasMoreElements())
					{
						fragment.EnterAnyElement();
						const uni_char *tag_name = fragment.GetElementName().GetLocalPart();
						if (uni_str_eq(tag_name, "BoundaryExtension"))
						{
							fragment.GetAllText(buffer);
							const uni_char *value = buffer.GetStorage();

							if (value)
							{
								OpString *boundary = OP_NEW(OpString, ());
								RETURN_OOM_IF_NULL(boundary);
								status = application->boundaries.Add(boundary);
								if (OpStatus::IsError(status))
								{
									OP_DELETE(boundary);
									return status;
								}
								RETURN_IF_ERROR(boundary->Set(value));
							}
							buffer.Clear();

							fragment.LeaveElement();
						}

					}

					fragment.LeaveElement();

				}
				else if (uni_str_eq(name, "applicationLocation"))
				{
					fragment.GetAllText(buffer);
					RETURN_IF_ERROR(application->location.Set(buffer.GetStorage()));
					buffer.Clear();

					fragment.LeaveElement();
				}

			} while (uni_str_eq(fragment.GetElementName().GetLocalPart(), "Application"));

			fragment.LeaveElement();
		}

		// Continue parsing until we have no more elements to enter
		// and are at the root of the fragment.
	} while (fragment.HasMoreElements() || fragment.HasCurrentElement());

	return OpStatus::OK;
}

#endif // XML_AIT_SUPPORT
