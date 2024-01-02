#include "diplomat_index.h"

namespace diplomat {

    template<typename T>
    bool IndexItem<T>::sorter(const IndexItem& rhs, const IndexItem& lhs)
    {
        return rhs.file_id < lhs.file_id || rhs.line < lhs.line || rhs.ch < lhs.ch;
    }

    

}

