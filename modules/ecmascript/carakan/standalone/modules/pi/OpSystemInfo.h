#ifndef OP_SYSTEM_INFO_H
#define OP_SYSTEM_INFO_H

class OpSystemInfo
{
public:
#if defined OPSYSTEMINFO_CPU_FEATURES
	enum CPUFeatures
	{
		CPU_FEATURES_NONE = 0

#if defined ARCHITECTURE_ARM
		, CPU_FEATURES_ARM_VFPV2 = (1 << 0)	///< ARM VFPv2 floating point
		, CPU_FEATURES_ARM_VFPV3 = (1 << 1)	///< ARM VFPv3 floating point
		, CPU_FEATURES_ARM_NEON  = (1 << 2)	///< Support for ARM NEON SIMD instructions
#elif defined ARCHITECTURE_IA32
		, CPU_FEATURES_IA32_SSE2   = (1 << 0)
		, CPU_FEATURES_IA32_SSE3   = (1 << 1)
		, CPU_FEATURES_IA32_SSSE3  = (1 << 2)
		, CPU_FEATURES_IA32_SSE4_1 = (1 << 3)
		, CPU_FEATURES_IA32_SSE4_2 = (1 << 4)
#endif
	};
	/** Returns an bit-field of available CPU features.
	 *
	 * @return A bit-wise OR of the CPUFeature flags of features the platform
	 *         knows to be available on the CPU on which this method is queried.
	 *
	 *         If more than one CPU is available only the features available on
	 *         all CPUs are returned.
	 */
	virtual unsigned int GetCPUFeatures()
        {
# if defined ARCHITECTURE_ARM_VFP
	    return CPU_FEATURES_ARM_VFPV3;
# elif defined ARCHITECTURE_IA32
	    return CPU_FEATURES_IA32_SSE4_2;
# else
	    return CPU_FEATURES_NONE;
# endif // ARCHITECTURE_ARM_VFP
	}
#endif // OPSYSTEMINFO_CPU_FEATURES
};

#endif
