#include "mdlame.h"
#include "mdcore.h"

#include <stdio.h>

hip_t MDLAME__gf = 0;

void *MDLAME__decoder (void *filename)
{
    unsigned int buff_size = 4096;

    MDLAME__gf = hip_decode_init ();

    FILE *mp3fp = fopen ((char *) filename, "r");

    unsigned char *mp3buffer = malloc (buff_size);

    short *pcm_l = malloc (buff_size * 100);    // this is horrible, but the best
    short *pcm_r = malloc (buff_size * 100);    // solution I could find on the net
                                                // (so far) - working only with
                                                // 4096 ... I really don't like mp3,
                                                // so if anybody wants support, DIY

    mp3data_struct* mp3data = malloc (sizeof (mp3data_struct));

    bool init = false;

    while (!feof(mp3fp)) {

        short mp3read_size = fread (mp3buffer, 1, buff_size, mp3fp);
        short mp3ret = hip_decode_headers (MDLAME__gf, mp3buffer,
                                           mp3read_size, pcm_l, pcm_r, mp3data);

        if (mp3ret == -1)
        {
            printf ("Error decoding mp3.\n");
            break;
        }
        else if (mp3ret > 0)
        {
            if (!init) {

                MD__set_metadata (mp3data->samplerate,
                                  mp3data->stereo, 16, 0);
                init = true;
            }

            for (int i=0; i<mp3ret; i++) {
                // would have to check if this is the right order
                MD__add_to_buffer (pcm_l[i]);
                MD__add_to_buffer (pcm_l[i]>>8);
                MD__add_to_buffer (pcm_r[i]);
                MD__add_to_buffer (pcm_r[i]>>8);
            }
        }
    }

    fclose (mp3fp);

    free (pcm_l);
    free (pcm_r);
    free (mp3buffer);

    MD__decoding_done_signal ();

    hip_decode_exit (MDLAME__gf);

    MD__exit_decoder ();
}
