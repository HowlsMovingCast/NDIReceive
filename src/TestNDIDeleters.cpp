#include "NDIDeleters.h"


struct countingInstances
{
    static int count;
    countingInstances() { count++; }
    ~countingInstances() = default;
};

void deductor(countingInstances* instance)
{
    instance->count--;
    delete instance;
}

int countingInstances::count = 0;

int main() 
{
    {
        countingInstances* pInstance = new countingInstances;
        deleteGuard guard(deductor, pInstance);

        if (countingInstances::count != 1) { return 1; }

        {
            countingInstances* pInstanceTwo = new countingInstances;
            deleteGuard guardTwo(deductor, pInstanceTwo);

            if (countingInstances::count != 2) { return 1; }

        }

        if (countingInstances::count != 1) { return 1; }
    }
    if (countingInstances::count != 0) { return 1; }
    return 0;
}