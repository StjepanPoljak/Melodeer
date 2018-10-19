#include "mdutils.h"

#include "mdflac.h"
#include "mdwav.h"
#include "mdmpg123.h"

// MD__play_raw wrappers

bool MD__play_raw_with_decoder (MD__file_t *MD__file,
                                void (*metadata_handle) (MD__metadata_t, void *),
                                void (*playing_handle) (void *),
                                void (*error_handle) (char *, void *),
                                void (*buffer_underrun_handle) (void *),
                                void (*completion_handle) (void *)) {

    void *(* decoder)(void *) = NULL;

    MD__filetype type = MD__get_filetype (MD__file->filename);

    switch (type) {

    case MD__FLAC:
        decoder = MDFLAC__start_decoding;
        break;

    case MD__WAV:
        decoder = MDWAV__parse;
        break;

    case MD__MP3:
        decoder = MDMPG123__decoder;
        break;

    default:
        return false;
    }

    MD__play_raw (MD__file, decoder, metadata_handle, playing_handle, error_handle, buffer_underrun_handle, completion_handle);

    return true;
}

unsigned int MDFFT__lg_uint (unsigned int num) {

    unsigned int log_res = 1;

    while ((num = num >> 1) > 1) log_res++;

    return log_res;
}

unsigned int MDFFT__bit_reverse (unsigned int num, unsigned int bits) {

    unsigned int reverse = 0;
    int curr_digit = 1;

    for (int i=0; i<bits; i++) {

        if ((curr_digit & num) != 0) reverse += 1;
        curr_digit <<= 1;
        if (i < bits - 1) reverse <<= 1;
    }

    return reverse;
}

void MDFFT__bit_reverse_copy (float complex v_in[], float complex v_out[], int count) {

    for (int i = 0; i < count; i++) v_out[i] = v_in[MDFFT__bit_reverse (i, MDFFT__lg_uint(count))];

    return;
}

void MDFFT__to_amp_surj (float complex v_in[], unsigned int count_in,
                         float v_out[], unsigned int count_out) {

    unsigned int ratio = count_in / count_out;

    for (int i=0; i<count_out; i++) {

        v_out[i] = 0;

        for (int j=0; j<ratio*i; j++) v_out[i] += cabs (v_in[i+j]);
    }
}

void MDFFT__iterative (bool inverse, float complex v_in[], float complex v_out[], int count) {

    MDFFT__bit_reverse_copy (v_in, v_out, count);

    unsigned int m = 1;

    for (unsigned int s = 1; s <= MDFFT__lg_uint (count); s++) {

        m <<= 1;    // substitute with look-up

        float angle = (-2.0*M_PI)/m;
        float complex w = cos(angle) + I * sin(angle);  // also substitute with lookup

        if (inverse) w = conj(w);

        for (unsigned int k=0; k<count; k+=m) {

            float complex v=1;
            for (unsigned int j=0; j<m/2; j++) {

                float complex t = v * v_out[k+j+m/2];
                float complex u = v_out[k+j];
                v_out[k+j] = u + t;
                v_out[k+j+m/2] = u - t;
                v = v * w;
            }
        }
    }
}

void MDFFT__apply_hanning (float complex v[], unsigned int count) {

    for (unsigned int i=0; i<count; i++)

        v[i] = (0.5 - 0.5*cos((2*M_PI*i)/count))*v[i];
}

void tests () {

    float complex *v_in = malloc(8*sizeof(v_in));
    float complex v_out[8];

    for (int i=0; i<8; i++) {
        v_in[i] = cos(2*M_PI*i / 8);
    }

    MDFFT__apply_hanning(v_in, 8);

    MDFFT__iterative(false, v_in, v_out, 8);

    for(int i=0; i<8; i++) {
        v_out[i] = v_out[i] / 8;
        printf("[%d] = %.4f+%.4fi\n", i, creal(v_out[i]), cimag(v_out[i]));
    }

    float complex v_in2[8];

    MDFFT__iterative(true, v_out, v_in2, 8);

    for(int i=0; i<8; i++) {

        printf("%.4f+%.4fi\n", creal(v_in2[i]), cimag(v_in2[i]));
    }
}

unsigned int MD__get_curr_seconds (MD__file_t *MD__file) {

    return (MD__file->MD__current_chunk->order * MD__file->MD__current_chunk->size
         / (MD__file->MD__metadata.bps * MD__file->MD__metadata.channels / 8))
          / MD__file->MD__metadata.sample_rate;
}

MD__filetype MD__get_filetype (const char *filename) {

    char curr = filename [0];
    unsigned int last_dot_position = -1;
    unsigned int i = 0;

    while (curr != 0) {

        if (curr == '.') last_dot_position = i;

        curr = filename [i++];
    }

    if (last_dot_position == -1) return MD__UNKNOWN;

    unsigned int diff = i - last_dot_position;

    if (diff == 5) {

        if (filename [last_dot_position]     == 'f'
         && filename [last_dot_position + 1] == 'l'
         && filename [last_dot_position + 2] == 'a'
         && filename [last_dot_position + 3] == 'c') {

            return MD__FLAC;
        }
    }

    if (diff == 4) {

        if (filename [last_dot_position]     == 'w'
         && filename [last_dot_position + 1] == 'a'
         && filename [last_dot_position + 2] == 'v') {

            return MD__WAV;
        }

        if (filename [last_dot_position]     == 'm'
         && filename [last_dot_position + 1] == 'p'
         && filename [last_dot_position + 2] == '3') {

            return MD__MP3;
        }
    }

    return MD__UNKNOWN;
}
