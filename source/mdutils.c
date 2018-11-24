#include "mdutils.h"

#include "mdflac.h"
#include "mdwav.h"
#include "mdmpg123.h"

#define MDFFT__MAX_LG_COUNT 512

#define MD__MAX_FFT_COUNT 65536

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

    static unsigned int bit_rev_table[MD__MAX_FFT_COUNT] = { 0 };
    static bool bit_rev_table_assigned[MD__MAX_FFT_COUNT] = { false };

    static unsigned int bits_precalc = 0;

    if (bit_rev_table_assigned[num] && (bits == bits_precalc) && (num < MD__MAX_FFT_COUNT))

        return bit_rev_table[num];

    if (bits_precalc != bits) {

        bits_precalc = bits;

        for (int i=0; i<MD__MAX_FFT_COUNT; i++) bit_rev_table_assigned[i] = false;
    }

    unsigned int reverse = 0;
    int curr_digit = 1;

    for (int i=0; i<bits; i++) {

        if ((curr_digit & num) != 0) reverse += 1;

        curr_digit <<= 1;

        if (i < bits - 1) reverse <<= 1;
    }

    if (num < MD__MAX_FFT_COUNT) {

        bit_rev_table_assigned[num] = true;
        bit_rev_table[num] = reverse;
    }

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

        for (int j=0; j<ratio; j++) v_out[i] += cabs (v_in[i*ratio+j]) / ratio;
    }
}

void MDFFT__iterative (bool inverse, float complex v_in[], float complex v_out[], unsigned int count) {

    static unsigned int lg_uint_precalc_count_assign = 2;
    static unsigned int lg_uint_precalc_value = 1;

    static unsigned int roots_of_unity_precalc_count = 0;
    static float complex roots_of_unity_precalc_value[MDFFT__MAX_LG_COUNT] = { 0 };

    MDFFT__bit_reverse_copy (v_in, v_out, count);

    unsigned int m = 1;

    if (count != lg_uint_precalc_count_assign) {

        lg_uint_precalc_count_assign = count;
        lg_uint_precalc_value = MDFFT__lg_uint (count);
    }

    for (unsigned int s = 1; s <= lg_uint_precalc_value; s++) {

        m <<= 1;

        float complex w = 0;

        if (lg_uint_precalc_value <= MDFFT__MAX_LG_COUNT) {

            if (roots_of_unity_precalc_count != lg_uint_precalc_value) {

                float angle = (-2.0 * M_PI) / m;
                w = cos (angle) + I * sin (angle);

                roots_of_unity_precalc_value[s-1] = w;
            }
            else w = roots_of_unity_precalc_value[s-1];
        }
        else {

            float angle = (-2.0 * M_PI) / m;
            w = cos (angle) + I * sin (angle);
        }

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

    roots_of_unity_precalc_count = lg_uint_precalc_value;

}

void MDFFT__apply_hanning (float complex v[], unsigned int count) {

    static float complex hanning_precalc[MD__MAX_FFT_COUNT] = { 0 };
    static unsigned int hanning_precalc_count = 0;

    if (count == hanning_precalc_count && count <= MD__MAX_FFT_COUNT) {

        for (unsigned int i=0; i<count; i++) {

            v[i] = hanning_precalc[i] * v[i];
        }
    }
    else {

        for (unsigned int i=0; i<count; i++) {

            float complex hanning_i = 0.5 - 0.5 * cos ((2*M_PI*i) / count);

            v[i] = hanning_i * v[i];

            if (i < MD__MAX_FFT_COUNT) hanning_precalc[i] = hanning_i;
        }

        hanning_precalc_count = count;
    }
}

void tests () {

    for (int i=0; i<5; i++) {

        for (int j=0; j<16; j++) {

            printf("%d, ", MDFFT__bit_reverse (j, 4));
        }
        printf("\n");
    }

    return;

    float complex *v_in = malloc(8*sizeof(*v_in));
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

    float *res = malloc(2*sizeof(*res));

    MDFFT__to_amp_surj(v_out, 8, res, 2);

    printf("%02.2f, %02.2f\n", res[0], res[1]);

    //
    // float complex v_in2[8];
    //
    // MDFFT__iterative(true, v_out, v_in2, 8);
    //
    // for(int i=0; i<8; i++) {
    //
    //     printf("%.4f+%.4fi\n", creal(v_in2[i]), cimag(v_in2[i]));
    // }
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
