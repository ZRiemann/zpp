#include <zpp/namespace.h>

NSB_APP
class coroutine_examples{
public:
    coroutine_examples() = default;
    ~coroutine_examples() = default;

    void coro_sleep();
};
NSE_APP