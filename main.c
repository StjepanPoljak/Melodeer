#include <stdlib.h>
#include <stdio.h>

#include "mdcore.h"
#include "mdflac.h"

int main (int argc, char *argv[])
{
    MD__initialize();
    MDAL__initialize();

    MD__play(argv[1], MDFLAC__start_decoding);

    MDAL__close ();

    exit(0);
}
