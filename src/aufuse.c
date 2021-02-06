#include "dskio.h"
#include "v6adapt.h"

int main(int argc, char const *argv[])
{
    if (!dsk_open("test.dsk"))
        return -1;

    v6_init();

    return 0;
}

