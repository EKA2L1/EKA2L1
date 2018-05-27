#pragma once

#include <epoc9/types.h>

namespace eka2l1 {
	namespace hle {
		namespace epoc9 {
			struct TRequestStatus {
				TRequestStatus();
				TRequestStatus(TInt aNewSts);

				TInt Int() const;

				TBool operator != (const TInt aRhs);
				TBool operator < (const TInt aRhs);
				TBool operator <= (const TInt aRhs);
				TBool operator == (const TInt aRhs);
				TBool operator > (const TInt aRhs);
				TBool operator >= (const TInt aRhs);

				TInt operator = (const TInt aNew);
			};
		}
	}
}