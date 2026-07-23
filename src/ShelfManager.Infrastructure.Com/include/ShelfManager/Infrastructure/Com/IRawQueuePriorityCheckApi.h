#pragma once

#include <Windows.h>
#include <OleAuto.h>

namespace ShelfManager::Infrastructure::Com {

class IRawQueuePriorityCheckApi {
public:
    virtual ~IRawQueuePriorityCheckApi() = default;

    // SOURCE: The exact vendor declaration is not available yet. This seam
    // models the supplied BSTR input/output behavior and is replaced if the
    // official header uses different parameter modifiers or ownership rules.
    virtual HRESULT Check(BSTR input, BSTR* output) = 0;
};

}  // namespace ShelfManager::Infrastructure::Com
