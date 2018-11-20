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

    MD__play_raw (MD__file, decoder, metadata_handle, playing_handle,
                  error_handle, buffer_underrun_handle, completion_handle);

    return true;
}

unsigned int MDFFT__lg_uint (unsigned int num) {

    unsigned int log_res = 1;

    while ((num = num >> 1) > 1) log_res++;

    return log_res;
}

#define MD__BIT_REVERSE_PRECALC_MAX 128

unsigned int MDFFT__bit_reverse (unsigned int num, unsigned int bits) {

    static unsigned int bit_rev_table[MD__BIT_REVERSE_PRECALC_MAX];
    static bool bit_rev_table_assigned[MD__BIT_REVERSE_PRECALC_MAX];

    static unsigned int bits_precalc = 0;

    if (bits_precalc != bits) {
        
        bits_precalc = bits;

        for (int i=0; i<bits_precalc; i++) bit_rev_table_assigned[i] = false;
    }

    if (num < bits_precalc && bit_rev_table_assigned[num]) return bit_rev_table[num];

    unsigned int reverse = 0;
    int curr_digit = 1;

    for (int i=0; i<bits; i++) {

        if ((curr_digit & num) != 0) reverse += 1;
        curr_digit <<= 1;
        if (i < bits - 1) reverse <<= 1;
    }

    bit_rev_table_assigned[num] = true;
    bit_rev_table[num] = reverse;

    return reverse;
}

void MDFFT__bit_reverse_copy (float complex v_in[], float complex v_out[], unsigned int count) {

    for (int i = 0; i < count; i++) v_out[i] = v_in[MDFFT__bit_reverse (i, MDFFT__lg_uint(count))];

    return;
}

void MDFFT__to_amp_surj (float complex v_in[], unsigned int count_in,
                         float v_out[], unsigned int count_out) {

// count_in = 128
// count_out = 8
// ratio = 16

    int ratio = count_in / count_out;

    for (int i=0; i<count_out; i++) {

        v_out[i] = 0;

        for (int j=0; j<ratio; j++) {
            
            // try this for maximum
            //float curr = cabs (v_in[i*ratio+j]);
            //if (curr > v_out[i]) v_out[i] = curr;
            
            v_out[i] += cabs (v_in[i*ratio+j]) / ratio;
        }
    }
}

#define MDFFT__MAX_LG_COUNT 512

void MDFFT__iterative (bool inverse, float complex v_in[], float complex v_out[], unsigned int count) {

    static unsigned int lg_uint_precalc_count_assign = 2;
    static unsigned int lg_uint_precalc_value = 1;

    static unsigned int roots_of_unity_precalc_count = 1;
    static float complex roots_of_unity_precalc_value [MDFFT__MAX_LG_COUNT];

    MDFFT__bit_reverse_copy (v_in, v_out, count);

    unsigned int m = 1;

    if (count != lg_uint_precalc_count_assign) {

        lg_uint_precalc_count_assign = count;

        lg_uint_precalc_value = MDFFT__lg_uint (count);
    }

    for (unsigned int s = 1; s <= lg_uint_precalc_value; s++) {

        m <<= 1;    // substitute with look-up

        float complex w = 0;

        if (roots_of_unity_precalc_count < lg_uint_precalc_value + 1) {

            float angle = (-2.0*M_PI)/m;
            w = cos(angle) + I * sin(angle);

            roots_of_unity_precalc_value[s-1] = w;

            roots_of_unity_precalc_count = s;
        }
        else w = roots_of_unity_precalc_value[s-1];

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