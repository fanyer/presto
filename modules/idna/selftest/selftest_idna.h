#ifndef _SELFTEST_IDNA_H_
#define _SELFTEST_IDNA_H_

#define IDNA_PREFIX "xn--"
#define IDNA_PREFIX_L UNI_L("xn--")
#define IMAA_PREFIX "iesg--"
#define IMAA_PREFIX_L UNI_L("iesg--")

#include "modules/idna/idna.h" 

#define SELFTEST_PVALID IDNALabelValidator::IDNA_PVALID
#define SELFTEST_CONTEXTJ IDNALabelValidator::IDNA_CONTEXTJ
#define SELFTEST_CONTEXTO IDNALabelValidator::IDNA_CONTEXTO
#define SELFTEST_DISALLOWED IDNALabelValidator::IDNA_DISALLOWED
#define SELFTEST_UNASSIGNED IDNALabelValidator::IDNA_UNASSIGNED

#endif
