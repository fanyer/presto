/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _ROOTSTORE_TABLE_H_
#define _ROOTSTORE_TABLE_H_

#ifdef ROOTSTORE_DISTRIBUTION_BASE
#define USE_OLD_VERISIGN_CLASS3
#endif
#define USE_NEW_VERISIGN_CLASS3

#include "modules/rootstore/defcert/thaw_pcasha.h"
#include "modules/rootstore/defcert/thaw_sca_sha.h"
#ifdef USE_OLD_VERISIGN_CLASS3
#include "modules/rootstore/defcert/vsign3g1.h"
#endif
#ifdef USE_NEW_VERISIGN_CLASS3
#include "modules/rootstore/defcert/verisign_pca3_sha.h"
#include "modules/rootstore/defcert/vsign_i2009_csig.h"
#include "modules/rootstore/defcert/vsign_i2009_ev1024.h"
#include "modules/rootstore/defcert/vsign_i2009_ofx.h"
#include "modules/rootstore/defcert/vsign_i2009_ss1024.h"
#include "modules/rootstore/defcert/vsign_i2009_ssca.h"
#endif
#include "modules/rootstore/defcert/vsign_c3g1_svrint.h"
#include "modules/rootstore/defcert/vrsgn_g5.h"

#include "modules/rootstore/defcert/GTECTGlobalRoot.h"
#include "modules/rootstore/defcert/Equifax_CA.h"

#include "modules/rootstore/defcert/geotrust_primary.h"
#include "modules/rootstore/defcert/thawte_primary_root.h"

//#include "modules/rootstore/defcert/entgssl_ca_cert.h"

#include "modules/rootstore/defcert/valicert_class2.h"
#include "modules/rootstore/defcert/starfield_issuingca.h"

#include "modules/rootstore/defcert/AddtrustExtCA.h"

#include "modules/rootstore/defcert/entSSL_ca_cert.h"
#include "modules/rootstore/defcert/entrust_ev.h"

#include "modules/rootstore/defcert/globalsign_r1.h"
#include "modules/rootstore/defcert/globalsign_r2.h"

#include "modules/rootstore/defcert/digicert_ev.h"
#include "modules/rootstore/defcert/comodo_ev.h"
//#include "modules/rootstore/defcert/entca2048.h"
#include "modules/rootstore/defcert/entrust2048-2012.h"


#if !defined LIBSSL_AUTO_UPDATE_ROOTS || defined ROOTSTORE_DISTRIBUTION_BASE || defined ROOTSTORE_USE_LOCALFILE_REPOSITORY
//#include "modules/rootstore/defcert/thaw1.h"
//#include "modules/rootstore/defcert/thaw2.h"
//#include "modules/rootstore/defcert/thaw3.h"
#include "modules/rootstore/defcert/thawte_g3_sha256.h"

// Baltimore certificates
#include "modules/rootstore/defcert/BTCTcodert.h"
#include "modules/rootstore/defcert/BTCTmort.h"
#include "modules/rootstore/defcert/BTCTRoot.h"
#include "modules/rootstore/defcert/GTECTRoot.h"
//#include "modules/rootstore/defcert/PostenCA.h"

#include "modules/rootstore/defcert/verisign_pca1_sha.h"
#include "modules/rootstore/defcert/verisign_pca2_sha.h"

#include "modules/rootstore/defcert/vsign1g2.h"
#include "modules/rootstore/defcert/vsign2g2.h"
#include "modules/rootstore/defcert/vsign3g2.h"
#include "modules/rootstore/defcert/vsign4g2.h"

#include "modules/rootstore/defcert/vsign1g3.h"
#include "modules/rootstore/defcert/vsign2g3.h"
#include "modules/rootstore/defcert/vsign3g3.h"
#include "modules/rootstore/defcert/vsign4g3.h"

#include "modules/rootstore/defcert/verisign_universial_sha256.h"

//#include "modules/rootstore/defcert/vsignssc.h"
//#include "modules/rootstore/defcert/vsign_rsassca_2009.h"

//#include "modules/rootstore/defcert/entclient_ca_cert.h"
//#include "modules/rootstore/defcert/entgclient_ca_cert.h"
//#include "modules/rootstore/defcert/entwap_ca_cert.h"
#include "modules/rootstore/defcert/entrust-g2.h"

#include "modules/rootstore/defcert/certum1.h"
#include "modules/rootstore/defcert/certum2.h"
#include "modules/rootstore/defcert/certum3.h"
#include "modules/rootstore/defcert/certum4.h"
#include "modules/rootstore/defcert/certum5.h"
#include "modules/rootstore/defcert/certum_TN.h"

//#include "modules/rootstore/defcert/crenca2003.h"

//#include "modules/rootstore/defcert/globalsign_root.h"
#include "modules/rootstore/defcert/globalsign_r3_sha256.h"

//#include "modules/rootstore/defcert/Equifax_CA1.h"
#include "modules/rootstore/defcert/Equifax_CA2.h"
//#include "modules/rootstore/defcert/Equifax_GCA1.h"
#include "modules/rootstore/defcert/Geotrust_CA.h"
#include "modules/rootstore/defcert/UserTrust_CA.h"
#include "modules/rootstore/defcert/GeoTrust_UCA.h"
//#include "modules/rootstore/defcert/Equifax_SCA2.h"
#include "modules/rootstore/defcert/Geotrust_GCA2.h"
#include "modules/rootstore/defcert/GeoTrust_UCA2.h"
#include "modules/rootstore/defcert/geotrust_g3_sha256.h"
#include "modules/rootstore/defcert/equifax_sbca_sha.h"
#include "modules/rootstore/defcert/equifax_sgbca_sha.h"

//#include "modules/rootstore/defcert/ActalisRootCA.h"
#include "modules/rootstore/defcert/Actalis_2012_sha256.h"

#include "modules/rootstore/defcert/GDClass2Root.h"
#include "modules/rootstore/defcert/SFClass2Root.h"
#include "modules/rootstore/defcert/godaddy_g2_root.h"
#include "modules/rootstore/defcert/starfield_g2_root.h"
#include "modules/rootstore/defcert/starfield_services_g2.h"

//#include "modules/rootstore/defcert/Deutsche_Telekom_Root_CA_1.h"
#include "modules/rootstore/defcert/Deutsche_Telekom_Root_CA_2.h"
#include "modules/rootstore/defcert/t_system_global_c2.h"
#include "modules/rootstore/defcert/t_system_global_c3.h"

#include "modules/rootstore/defcert/Sonera_Class1_CA.h"
#include "modules/rootstore/defcert/Sonera_Class2_CA.h"

#include "modules/rootstore/defcert/UTN-USERFirst-Object.h"
#include "modules/rootstore/defcert/TrustedCertificateServices.h"
#include "modules/rootstore/defcert/SecureCertificateServices.h"
#include "modules/rootstore/defcert/AAACertificateServices.h"
#include "modules/rootstore/defcert/UTN-DATACorpSGC.h"
#include "modules/rootstore/defcert/UTN-USERFirst-ClientAuthenticationandEmail.h"
#include "modules/rootstore/defcert/UTN-USERFirst-Hardware.h"
#include "modules/rootstore/defcert/AddtrustClass1.h"
#include "modules/rootstore/defcert/AddtrustPubCA.h"
#include "modules/rootstore/defcert/AddtrustQCA.h"

#include "modules/rootstore/defcert/RSA2048Root.h"
#include "modules/rootstore/defcert/ValicertClass3Root.h"

//#include "modules/rootstore/defcert/trustcenter_class_1.h"
//#include "modules/rootstore/defcert/trustcenter_class_2.h"
//#include "modules/rootstore/defcert/trustcenter_class_3.h"
#include "modules/rootstore/defcert/trustcenter_2048.h"
#include "modules/rootstore/defcert/trustcenter_class_3_ii.h"
#include "modules/rootstore/defcert/tc_universial_1.h"
#include "modules/rootstore/defcert/tc_universial_3.h"

#include "modules/rootstore/defcert/nederland_ca.h"
#include "modules/rootstore/defcert/nederland_ca2.h"

#include "modules/rootstore/defcert/qv_root_ca1.h"
#include "modules/rootstore/defcert/qv_root_ca2.h"
#include "modules/rootstore/defcert/qv_root_ca3.h"

#include "modules/rootstore/defcert/SecComRootca.h"
#include "modules/rootstore/defcert/SecComRoot1ca.h"
#include "modules/rootstore/defcert/secom_ev_root.h"
#include "modules/rootstore/defcert/secom_2_sha256.h"

#include "modules/rootstore/defcert/swissign_gold_g2.h"
#include "modules/rootstore/defcert/swissign_platinum_g2.h"
#include "modules/rootstore/defcert/swissign_silver_g2.h"

#include "modules/rootstore/defcert/AmericaOnline1.h"
#include "modules/rootstore/defcert/AmericaOnline2.h"

#include "modules/rootstore/defcert/cisco_2048.h"

#include "modules/rootstore/defcert/digcert_aid.h"
#include "modules/rootstore/defcert/digicert_global.h"

#include "modules/rootstore/defcert/kisa_3280.h"
#include "modules/rootstore/defcert/kisa_wrsa.h"

#include "modules/rootstore/defcert/trustwave_scga.h"
#include "modules/rootstore/defcert/trustwave_stca.h"
#include "modules/rootstore/defcert/trustwave_xgca.h"

#include "modules/rootstore/defcert/diginotar2007.h"

#include "modules/rootstore/defcert/swisscom_ca1.h"

#include "modules/rootstore/defcert/turktrust_ca1.h"
#include "modules/rootstore/defcert/turktrust_ca3.h"
#include "modules/rootstore/defcert/turktrust_ca2.h"

#include "modules/rootstore/defcert/keynectis_ca2.h"

#include "modules/rootstore/defcert/izenpe_csrs.h"
#include "modules/rootstore/defcert/izenpe_sha256.h"

#include "modules/rootstore/defcert/wisekey.h"

#include "modules/rootstore/defcert/BuypassClass2.h"
#include "modules/rootstore/defcert/BuypassClass3.h"
#include "modules/rootstore/defcert/BuypassClass2_sha256.h"
#include "modules/rootstore/defcert/BuypassClass3_sha256.h"

#include "modules/rootstore/defcert/China_cnnic_root.h"
#include "modules/rootstore/defcert/cnnic_ev_2011.h"

#include "modules/rootstore/defcert/twca_root.h"

#include "modules/rootstore/defcert/affirmtrust_com_root.h"
#include "modules/rootstore/defcert/affirmtrust_netm_root.h"
#include "modules/rootstore/defcert/affirmtrust_premium_root.h"

#include "modules/rootstore/defcert/startcom_root.h"

#endif // autoupdate

#include "modules/rootstore/rootstore_base.h"

#ifdef ROOTSTORE_DISTRIBUTION_BASE
#include "modules/rootstore/root_depend.h"
#include "modules/rootstore/root_overrides.h"
#endif

LOCAL_PREFIX_CONST_ARRAY (static, DEFCERT_st, defcerts)
	//DEFCERT_ITEM(Thawte_Personal_Free,TRUE,TRUE,NULL, SSL_Options_Version_3_50)
	//DEFCERT_ITEM(Thawte_Personal_Basic,TRUE,TRUE,NULL, SSL_Options_Version_3_50)
	//DEFCERT_ITEM(Thawte_Personal_Premium,TRUE,TRUE,NULL, SSL_Options_Version_3_50)
	DEFCERT_REPLACEITEM_PRESHIP(Thawte_Server_Basic_sha1,FALSE,FALSE,NULL, SSL_Options_Version_12_00)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(Thawte_Server_Premium_sha1,FALSE,FALSE,NULL, SSL_Options_Version_12_00, &THAWTE_DEP_old)
	DEFCERT_ITEM(THAWTE_G3_SHA_256, FALSE, FALSE , NULL, SSL_Options_Version_9_65)

	DEFCERT_REPLACEITEM(VERISIGN_PCA1_SHA, FALSE, FALSE , "Verisign Class 1 Public Primary Certification Authority", SSL_Options_Version_9_65)
	DEFCERT_REPLACEITEM(VERISIGN_PCA2_SHA, FALSE, FALSE , "Verisign Class 2 Public Primary Certification Authority", SSL_Options_Version_9_65)
#ifdef USE_OLD_VERISIGN_CLASS3
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(Verisign_Class3_G1,FALSE,FALSE, "Verisign Class 3 Public Primary Certification Authority", SSL_Options_Version_5_03b, &VERISIGN_DEP_g1_c3_old)
#endif
#ifdef USE_NEW_VERISIGN_CLASS3
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_PCA3_SHA, FALSE, FALSE , "Verisign Class 3 Public Primary Certification Authority", SSL_Options_Version_10_0, &VERISIGN_DEP_g1_c3)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_INTERMEDIATE_2009_CSIG, FALSE, FALSE , NULL, SSL_Options_Version_10_0, &VERISIGN_DEP_g1_c3_intermediates)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_INTERMEDIATE_2009_EV1024, FALSE, FALSE , NULL, SSL_Options_Version_10_0, &VERISIGN_DEP_g1_c3_intermediates)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_INTERMEDIATE_2009_OFX, FALSE, FALSE , NULL, SSL_Options_Version_10_0, &VERISIGN_DEP_g1_c3_intermediates)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_INTERMEDIATE_2009_SS1024, FALSE, FALSE , NULL, SSL_Options_Version_10_0, &VERISIGN_DEP_g1_c3_intermediates)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_INTERMEDIATE_2009_SSCA, FALSE, FALSE , NULL, SSL_Options_Version_10_0, &VERISIGN_DEP_g1_c3_intermediates)
#endif
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(VERISIGN_SVR_INTL_2, FALSE, FALSE , "VeriSign International Server CA", SSL_Options_Version_10_01, NULL /* no dependency*/)

	DEFCERT_ITEM(Verisign_Class1_G2,FALSE,FALSE, "Verisign Class 1 Public Primary Certification Authority - G2", SSL_Options_Version_5_03b)
	DEFCERT_ITEM(Verisign_Class2_G2,FALSE,FALSE, "Verisign Class 2 Public Primary Certification Authority - G2", SSL_Options_Version_5_03b)
	DEFCERT_ITEM(Verisign_Class3_G2,FALSE,FALSE, "Verisign Class 3 Public Primary Certification Authority - G2", SSL_Options_Version_5_03b)
	DEFCERT_ITEM(Verisign_Class4_G2,FALSE,FALSE, "Verisign Class 4 Public Primary Certification Authority - G2", SSL_Options_Version_5_03b)

	DEFCERT_ITEM(Verisign_Class1_G3,FALSE,FALSE,NULL, SSL_Options_Version_5_03b)
	DEFCERT_ITEM(Verisign_Class2_G3,FALSE,FALSE,NULL, SSL_Options_Version_5_03b)
	DEFCERT_ITEM(Verisign_Class3_G3,FALSE,FALSE,NULL, SSL_Options_Version_5_03b)
	DEFCERT_ITEM(Verisign_Class4_G3,FALSE,FALSE,NULL, SSL_Options_Version_5_03b)

	//DEFCERT_REPLACEITEM(VERISIGN_RSA_SSCA_2009_SHA1,FALSE,FALSE,NULL, SSL_Options_Version_10_0)
	DEFCERT_ITEM(VERISIGN_UNIVERSIAL_SHA256, FALSE, FALSE , NULL, SSL_Options_Version_9_65)
	//DEFCERT_ITEM(Verisign_TimeStampCA,FALSE,FALSE,NULL, SSL_Options_Version_5_03b)

	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(EQUIFAX_CA,FALSE,FALSE	,"Equifax Secure Certificate Authority", SSL_Options_Version_7_00b, &GEOTRUST_DEP_old)
	//DEFCERT_ITEM(EQUIFAX_CA1,FALSE,FALSE	,"Equifax Secure eBusiness CA-1", SSL_Options_Version_7_00b)
	DEFCERT_ITEM(EQUIFAX_CA2,FALSE,FALSE	,"Equifax Secure eBusiness CA-2", SSL_Options_Version_7_00b)
	//DEFCERT_ITEM(EQUIFAX_GCA1,FALSE,FALSE	,"Equifax Secure Global eBusiness CA-1", SSL_Options_Version_7_00b)
	DEFCERT_ITEM(GEOTRUST_CA,FALSE,FALSE	,"GeoTrust Global CA", SSL_Options_Version_7_00b)
	DEFCERT_ITEM(USERTRUST_CA,FALSE,FALSE	,"UTN-USERFirst-Network Applications", SSL_Options_Version_7_00b)
	DEFCERT_REPLACEITEM(Equifax_Sec_eBusiness_SHA, FALSE, FALSE , NULL, SSL_Options_Version_12_00)
	DEFCERT_REPLACEITEM(Equifax_SecGlobal_eBusiness_SHA, FALSE, FALSE , NULL, SSL_Options_Version_12_00)

	//DEFCERT_ITEM(Equifax_SecureCA_2,FALSE,FALSE	,"Equifax Secure Certificate Authority 2", SSL_Options_Version_8_00a)
	DEFCERT_ITEM(GeoTrust_Universial_CA,FALSE,FALSE	,"GeoTrust Universal CA", SSL_Options_Version_8_00a)
	DEFCERT_ITEM(GeoTrust_Universial_CA2,FALSE,FALSE	,"GeoTrust Universal CA-2", SSL_Options_Version_8_00a)
	DEFCERT_ITEM(Geotrust_GlobalCA2,FALSE,FALSE	,"GeoTrust Global CA-2", SSL_Options_Version_8_00a)
	DEFCERT_ITEM(GEOTRUST_PCA_G3_SHA256, FALSE, FALSE , NULL, SSL_Options_Version_9_65)

	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(VERISIGN_G5_CLASS3, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &VERISIGN_DEP_g5_c3)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(THAWTE_PRIMARY_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &THAWTE_DEP_primary)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(GEOTRUST_PRIMARY_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &GEOTRUST_DEP_primary)

	//DEFCERT_REPLACEITEM_WITH_DEPEND(TRUSTCENTER_CLASS_1_2011,TRUE,TRUE,"TC TrustCenter, Germany, Class 1 CA", SSL_Options_Version_9_00a, &TrustCenter_old_dep)
	//DEFCERT_REPLACEITEM_WITH_DEPEND(TRUSTCENTER_CLASS_2_2011,FALSE,FALSE,"TC TrustCenter, Germany, Class 2 CA", SSL_Options_Version_9_00a, &TrustCenter_c2_old_dep)
	//DEFCERT_REPLACEITEM_WITH_DEPEND(TRUSTCENTER_CLASS_3_2011,FALSE,FALSE,"TC TrustCenter, Germany, Class 3 CA", SSL_Options_Version_9_00a, &TrustCenter_old_dep)
	DEFCERT_ITEM(TRUSTCENTER_CLASS_3_G2,FALSE,FALSE, NULL, SSL_Options_Version_9_22)
	DEFCERT_REPLACEITEM(TRUSTCENTER_2048,FALSE,FALSE, NULL, SSL_Options_Version_9_00b)
	DEFCERT_ITEM(TRUSTCENTER_UNIVERSIAL_1, FALSE, FALSE , NULL, SSL_Options_Version_10_61)
	DEFCERT_ITEM_WITH_DEPEND(TRUSTCENTER_UNIVERSIAL_3, FALSE, FALSE , NULL, SSL_Options_Version_10_61, &TrustCenter_EV_dep)

	// Baltimore Certifcicates
	DEFCERT_ITEM_PRESHIP(BALTIMORE_TECHNOLOGIES_GTECTGlobalRoot,FALSE,FALSE,NULL, SSL_Options_Version_4_10)
	DEFCERT_ITEM(BALTIMORE_TECHNOLOGIES_BTCTRoot,FALSE,FALSE,NULL, SSL_Options_Version_4_10)
	DEFCERT_ITEM(BALTIMORE_TECHNOLOGIES_BTCTcodert,FALSE,FALSE,NULL, SSL_Options_Version_4_10)
	DEFCERT_ITEM(BALTIMORE_TECHNOLOGIES_BTCTmort,FALSE,FALSE,NULL, SSL_Options_Version_4_10)

	// Posten Norge BA root certificate
	//DEFCERT_ITEM(Posten_Norge_BA_Root,FALSE,FALSE,NULL, SSL_Options_Version_5_03b)

	DEFCERT_REPLACEITEM_PRESHIP(ENTRUST2048_2012,FALSE,FALSE,"Entrust 2048 Secure Server Certification Authority", SSL_Options_Version_12_00)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(Entrust_SSL_CA,FALSE,FALSE,"Entrust Secure Server Certification Authority", SSL_Options_Version_6_02, &Entrust_dep2)
	//DEFCERT_ITEM_WITH_DEPEND_PRESHIP(Entrust_GSSL_CA,FALSE,FALSE,"Entrust Global Secure Server Certification Authority ", SSL_Options_Version_6_02, &Entrust_dep)
	//DEFCERT_ITEM(Entrust_client_CA,FALSE,FALSE,"Entrust Client Certification Authority", SSL_Options_Version_6_02)
	//DEFCERT_ITEM(Entrust_GClient_CA,FALSE,FALSE,"Entrust Global Client Certification Authority ", SSL_Options_Version_6_02)
	//DEFCERT_ITEM(Entrust_WAP_CA,FALSE,FALSE,"Entrust WAP Certification Authority", SSL_Options_Version_6_02)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(ENTRUST_EV_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &Entrust_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(ENTRUST_G2, FALSE, FALSE , NULL, SSL_Options_Version_12_00, &Entrust_EV_dep)

	DEFCERT_ITEM(Certum_Level5,FALSE,FALSE,"Certum CA", SSL_Options_Version_6_05)
	DEFCERT_REPLACEITEM_WITH_DEPEND(Certum_Level1,FALSE, FALSE,"Certum CA Level I", SSL_Options_Version_6_05, &Certum_dep_other)
	DEFCERT_REPLACEITEM_WITH_DEPEND(Certum_Level2,FALSE, FALSE,"Certum CA Level II", SSL_Options_Version_6_05, &Certum_dep_other)
	DEFCERT_REPLACEITEM_WITH_DEPEND(Certum_Level3,FALSE,FALSE,"Certum CA Level III", SSL_Options_Version_6_05, &Certum_dep_other)
	DEFCERT_REPLACEITEM_WITH_DEPEND(Certum_Level4,FALSE,FALSE,"Certum CA Level IV", SSL_Options_Version_6_05, &Certum_dep_other)
	DEFCERT_ITEM_WITH_DEPEND(CERTUM_TN_ROOT, FALSE, FALSE , "Certum Trusted Network CA", SSL_Options_Version_9_64, &Certum_dep_EV)

	//DEFCERT_REPLACEITEMFLAGS(CREN_CLIENT_CA_2003,FALSE,FALSE,"CREN Higher Education Root", SSL_Options_Version_7_20a)

	//DEFCERT_ITEM(ACTALIS_ROOT_CA,FALSE,FALSE	,"Actalis Root CA", SSL_Options_Version_7_20)
	DEFCERT_ITEM(ACTALIS_ROOT_2012_SHA256, FALSE, FALSE , NULL, SSL_Options_Version_11_62)

	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(VALICERT_CLASS_2_CA,FALSE,FALSE	,"Valicert Class 2 CA", SSL_Options_Version_7_50, &Godaddy_dep)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(STARFIELD_ISSUING_CA,FALSE,FALSE	,"Starfield Issuing CA", SSL_Options_Version_7_50, &Godaddy_dep1)
	DEFCERT_ITEM_WITH_DEPEND(GoDaddy_Class2,FALSE,FALSE	,"Go Daddy 2048 Class 2 CA", SSL_Options_Version_8_00a, &Godaddy_EVdep)
	DEFCERT_ITEM_WITH_DEPEND(StarField_Class2,FALSE,FALSE	,"Starfield 2048 Class 2 CA", SSL_Options_Version_8_00a, &Godaddy_EVdep2)
	DEFCERT_ITEM_WITH_DEPEND(GODADDY_G2_SHA256_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_52, &Godaddy_EVdep)
	DEFCERT_ITEM_WITH_DEPEND(GODADDY_STARFIELD_G2_SHA256_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_52, &Godaddy_EVdep2)
	DEFCERT_ITEM_WITH_DEPEND(GODADDY_STARFIELD_SERV_G2_SHA256_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_52, &Godaddy_EVdep3)

	//DEFCERT_ITEM(DEUTSCHE_TELEKOM_ROOT_CA_1, FALSE, FALSE, "Deutsche Telekom Root CA 1", SSL_Options_Version_7_52)
	DEFCERT_ITEM_WITH_DEPEND(DEUTSCHE_TELEKOM_ROOT_CA_2, FALSE, FALSE, "Deutsche Telekom Root CA 2", SSL_Options_Version_7_52, &T_System_EV_dep)
	DEFCERT_ITEM(T_SYSTEM_GLOBAL_ROOT_C2, FALSE, FALSE , NULL, SSL_Options_Version_11_51)
	DEFCERT_ITEM_WITH_DEPEND(T_SYSTEM_GLOBAL_ROOT_C3, FALSE, FALSE , NULL, SSL_Options_Version_11_51, &T_System_EV_dep2)

	DEFCERT_ITEM(SONERA_CLASS1_CA, TRUE, TRUE, "Sonera Class 1 CA", SSL_Options_Version_7_60)
	DEFCERT_ITEM(SONERA_CLASS2_CA, FALSE, FALSE, "Sonera Class 2 CA", SSL_Options_Version_7_60)

	DEFCERT_ITEM(SecComRootCA, FALSE, FALSE, "ValiCert Class 1 Policy Validation Authority", SSL_Options_Version_7_60a	)
	DEFCERT_ITEM_WITH_DEPEND(SecComRoot1CA, FALSE, FALSE, "Security Communication Root CA 1", SSL_Options_Version_8_00, &Secom_Cert_dep)
	DEFCERT_ITEM_WITH_DEPEND(SECOM_EV_ROOT, FALSE, FALSE , "Security Communication EV Root CA 1", SSL_Options_Version_9_50a, &Secom_EV_dep)
	DEFCERT_ITEM(SECOM_2_SHA_256, FALSE, FALSE , "Security Communication Root CA 2", SSL_Options_Version_10_01)

	DEFCERT_ITEM_WITH_DEPEND(Komodo_SecureCertificateServices, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_AAACertificateServices, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_TrustedCertificateServices, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_UTN_USERFirst_Object, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_UTN_USERFirst_Hardware, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_UTN_USERFirst_ClientAuthenticationandEmail, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_UTN_DATACorpSGC, FALSE, FALSE , NULL, SSL_Options_Version_8_00, &Comodo_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_AddTrust_Class1, FALSE, FALSE , NULL, SSL_Options_Version_8_00a, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(Komodo_AddTrust_ExtCA, FALSE, FALSE , NULL, SSL_Options_Version_8_00a, &Comodo_EV_dep2)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_AddTrust_PubCA, FALSE, FALSE , NULL, SSL_Options_Version_8_00a, &Comodo_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(Komodo_AddTrust_QCA, FALSE, FALSE , NULL, SSL_Options_Version_8_00a, &Comodo_EV_dep)

	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(COMODO_EV_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50b, &Comodo_EV_dep)

	DEFCERT_ITEM(RSA_ValicertClass3Root, FALSE, FALSE , "RSA Security 1024 V1", SSL_Options_Version_8_00a)
	DEFCERT_ITEM(RSA_2048_Root, FALSE, FALSE , "RSA Security 2048 V3", SSL_Options_Version_8_00a)

	DEFCERT_ITEM(NEDERLAND_CA, FALSE, FALSE , NULL, SSL_Options_Version_9_00d)
	DEFCERT_ITEM(NEDERLAND_CA_4096, FALSE, FALSE , NULL, SSL_Options_Version_9_64)

	//DEFCERT_ITEM_PRESHIP(GLOBALSIGN_ROOT,FALSE,FALSE	,"GlobalSign Root", SSL_Options_Version_7_00a)
	DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(GLOBALSIGN_ROOT1, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &Globalsign_dep)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(GLOBALSIGN_ROOT2, FALSE, FALSE , "GlobalSign Root CA - R2", SSL_Options_Version_9_50a, &Globalsign_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(GLOBALSIGN_R3_SHA256, FALSE, FALSE , "GlobalSign Root CA - R3", SSL_Options_Version_10_0, &Globalsign_EV_dep)

	DEFCERT_ITEM_WITH_DEPEND(QV_ROOT_CA1, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &QuoVadis_dep)
	DEFCERT_ITEM_WITH_DEPEND(QV_ROOT_CA2, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &QuoVadis_EV_dep)
	DEFCERT_ITEM(QV_ROOT_CA3, FALSE, FALSE , NULL, SSL_Options_Version_9_50a)

	DEFCERT_ITEM_WITH_DEPEND(SWISS_SIGN_GOLD_G2, FALSE, FALSE , NULL, SSL_Options_Version_9_50a, &SwissSign_EV_dep)
	DEFCERT_ITEM(SWISS_SIGN_PLATINUM_G2, FALSE, FALSE , NULL, SSL_Options_Version_9_50a)
	DEFCERT_ITEM(SWISS_SIGN_SILVER_G2, FALSE, FALSE , NULL, SSL_Options_Version_9_50a)

	DEFCERT_ITEM(AOL_CERT_1, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)
	DEFCERT_ITEM(AOL_CERT_2, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)

	DEFCERT_ITEM(CISCO_ROOT_2048, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)

	DEFCERT_ITEM(DIGICERT_AID_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)
	DEFCERT_ITEM(DIGICERT_GLOBAL_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)
	DEFCERT_ITEM_WITH_DEPEND_PRESHIP(DIGICERT_HA_EV_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50b, &Digicert_EV_dep)

	DEFCERT_ITEM(KISA_3280_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)
	DEFCERT_ITEM(KISA_WRSA_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_9_50b)

	DEFCERT_ITEM_WITH_DEPEND(TRUSTWAVE_SGCA_CERT_1, FALSE, FALSE , "Trustwave Secure Global CA", SSL_Options_Version_9_51, &XRamp_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(TRUSTWAVE_STCA_CERT_2, FALSE, FALSE , "Trustwave SecureTrust CA", SSL_Options_Version_9_51, &XRamp_EV_dep)
	DEFCERT_ITEM_WITH_DEPEND(TRUSTWAVE_XGCA_CERT_3, FALSE, FALSE , "Trustwave XRamp Global CA", SSL_Options_Version_9_51, &XRamp_EV_dep)

	DEFCERT_ITEM(SWISSCOM_1, FALSE, FALSE , NULL, SSL_Options_Version_9_51)

	DEFCERT_ITEM(TURK_TRUST_1, FALSE, FALSE , NULL, SSL_Options_Version_9_51)
	DEFCERT_ITEM(TURK_TRUST_3, FALSE, FALSE , NULL, SSL_Options_Version_9_51)
	DEFCERT_ITEM(TURK_TRUST_2, FALSE, FALSE , NULL, SSL_Options_Version_9_51)

	DEFCERT_ITEM_WITH_DEPEND(KEYNECTIS_CLASS_2, FALSE, FALSE , "KeyNectis Class 2 Primary CA", SSL_Options_Version_9_51, &Keynectis_EV_dep)

	DEFCERT_ITEM(IZENPE_G1, FALSE, FALSE , "Izenpe2003", SSL_Options_Version_9_52)
	DEFCERT_ITEM_WITH_DEPEND(IZENPE_SHA256_ROOT, FALSE, FALSE , "Izenpe2007", SSL_Options_Version_9_52, &Izenpe_EV_dep)

	DEFCERT_ITEM(WISEKEY_ROOT_CA, FALSE, FALSE , "WISeKey Global Root CA", SSL_Options_Version_9_52)

	DEFCERT_ITEM(BUYPASS_CLASS_2, FALSE, FALSE , NULL, SSL_Options_Version_10_01)
	DEFCERT_ITEM_WITH_DEPEND(BUYPASS_CLASS_3, FALSE, FALSE , NULL, SSL_Options_Version_10_01, &Buypass_EV_dep)
	DEFCERT_ITEM(BUYPASS_CLASS2_ROOT_SHA256, FALSE, FALSE , NULL, SSL_Options_Version_11_60)
	DEFCERT_ITEM_WITH_DEPEND(BUYPASS_CLASS3_ROOT_SHA256, FALSE, FALSE , NULL, SSL_Options_Version_11_60, &Buypass_EV_dep)

	DEFCERT_ITEM(CHINA_CNNICROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_01)
	DEFCERT_ITEM_WITH_DEPEND(CHINA_CNNIC_EV_ROOT_2011, FALSE, FALSE , NULL, SSL_Options_Version_11_10,&CNNIC_EV_dep)

	DEFCERT_ITEM(TAIWAN_CA_ROOT_2010, FALSE, FALSE , NULL, SSL_Options_Version_10_52)

	DEFCERT_ITEM(AFFIRMTRUST_COMMERCIAL_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_52)
	DEFCERT_ITEM(AFFIRMTRUST_NETWORKING_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_52)
	DEFCERT_ITEM(AFFIRMTRUST_PREMIUM_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_52)

	DEFCERT_ITEM_WITH_DEPEND(STARTCOM_ROOT, FALSE, FALSE , NULL, SSL_Options_Version_10_61, &Startcom_EV_dep)

	// NOTE: Untrusted certificates, marked as Deny and warn
	DEFCERT_REPLACEITEMFLAGS(DIGINOTAR_2007, TRUE, TRUE , NULL, SSL_Options_Version_11_52) // Denied trust due to being hacked and having issued an unknown number of fraudulent certificates

	// NOTE: ALL new items **above** this line!!
	DEFCERT_ITEM_BASE(NULL, 0, FALSE, FALSE, FALSE, FALSE, NULL, 0, NULL)      // Lastitem;
CONST_END(defcerts)

#ifdef ROOTSTORE_DISTRIBUTION_BASE
#include "modules/rootstore/root_aux.h"
#endif

#endif // _ROOTSTORE_TABLE_H_
