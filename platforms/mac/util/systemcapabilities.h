#if !defined(SYSTEMCAPABILITIES_H)
#define SYSTEMCAPABILITIES_H

#ifdef __cplusplus
extern "C" {
#endif

void InitOperatingSystemVersion();
SInt32 GetOSVersion();
void GetDetailedOSVersion(int &major, int &minor, int &bugfix);

#ifdef __cplusplus
}
#endif

#endif
