//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_CHashedDistributions_H
#define GPOPT_CHashedDistributions_H

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpos/memory/IMemoryPool.h"
#include "gpopt/base/CColRef.h"
#include "gpopt/base/CUtils.h"

namespace gpopt
{
	// Build hashed distributions used in physical union all during
	// distribution derivation. The class is an array of hashed
	// distribution on input column of each child, and an output hashed
	// distribution on UnionAll output columns

	class CHashedDistributions : public CDistributionSpecArray
	{
		public:
			CHashedDistributions
			(
			IMemoryPool *mp,
			CColRefArray *pdrgpcrOutput,
			CColRefArrays *pdrgpdrgpcrInput
			);
	};
}

#endif //GPOPT_CHashedDistributions_H
