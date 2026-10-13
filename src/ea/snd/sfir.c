#include "ea/snd/smix.h"

// Windowed-sinc coefficients. type 2: low-pass at f2, 3: high-pass at f1 (at most 0.8),
// 4: band-pass f1..f2. Hamming window; normalised to unity gain at the pass band.
void calcFIRCoeffs(SNDFIRSTATE* fir, int type) {
    int k;
    if ((type == 3 || type == 4) && fir->f1 > 0.8f) {
        fir->f1 = 0.8f;
    }
    switch (type) {
    case 2:
        fir->c[4] = fir->f2;
        for (k = 1; k <= 4; k++) {
            f32 w = 3.1415927f * k;
            fir->c[4 - k] = SNDI_sin(fir->f2 * w) / w;
        }
        break;
    case 3:
        fir->c[4] = fir->f1;
        for (k = 1; k <= 4; k++) {
            f32 w = 3.1415927f * k;
            fir->c[4 - k] = SNDI_sin(fir->f1 * w) / w;
        }
        break;
    case 4:
        fir->c[4] = fir->f2 - fir->f1;
        for (k = 1; k <= 4; k++) {
            f32 w = 3.1415927f * k;
            f32 c = SNDI_cos(w * (0.5f * (fir->f2 + fir->f1)));
            fir->c[4 - k] = (2.0f * SNDI_sin(w * (0.5f * (fir->f2 - fir->f1))) * c) / w;
        }
        break;
    }
    f32 sum = 0.0f;
    for (k = 0; k <= 4; k++) {
        switch (type) {
        case 2:
        case 4:
            fir->c[k] *= 0.54f - 0.46f * SNDI_cos(0.7853982f * k);
            sum += fir->c[k];
            break;
        case 3:
            // Spectral inversion of the low-pass.
            fir->c[k] *= -(0.54f - 0.46f * SNDI_cos(0.7853982f * k));
            if (k % 2 == 0) {
                sum += fir->c[k];
            } else {
                sum -= fir->c[k];
            }
            break;
        }
    }
    if (type == 3) {
        sum += 1.0f;
        fir->c[4] += 1.0f;
    }
    f32 norm = 2.0f * sum - fir->c[4];
    if (norm < 0.0f) {
        norm *= -1.0f;
    }
    fir->c[0] /= norm;
    fir->c[1] /= norm;
    fir->c[2] /= norm;
    fir->c[3] /= norm;
    fir->c[4] /= norm;
}
