#include "picasso.h"

const char *picasso_icc_profile_name(picasso_icc_profile profile) {
    switch (profile) {
        case PICASSO_PROFILE_NONE: return "None";
        case PICASSO_PROFILE_ACESCG_LINEAR: return "ACESCG Linear";
        case PICASSO_PROFILE_ADOBERGB1998: return "AdobeRGB1998";
        case PICASSO_PROFILE_DCI_P3_RGB: return "DCI(P3) RGB";
        case PICASSO_PROFILE_DISPLAY_P3: return "Display P3";
        case PICASSO_PROFILE_GENERIC_CMYK_PROFILE: return "Generic CMYK Profile";
        case PICASSO_PROFILE_GENERIC_GRAY_GAMMA_2_2_PROFILE: return "Generic Gray Gamma 2.2 Profile";
        case PICASSO_PROFILE_GENERIC_GRAY_PROFILE: return "Generic Gray Profile";
        case PICASSO_PROFILE_GENERIC_LAB_PROFILE: return "Generic Lab Profile";
        case PICASSO_PROFILE_GENERIC_RGB_PROFILE: return "Generic RGB Profile";
        case PICASSO_PROFILE_GENERIC_XYZ_PROFILE: return "Generic XYZ Profile";
        case PICASSO_PROFILE_ITU_2020: return "ITU-2020";
        case PICASSO_PROFILE_ITU_709: return "ITU-709";
        case PICASSO_PROFILE_ROMM_RGB: return "ROMM RGB";
        case PICASSO_PROFILE_SRGB: return "sRGB Profile";
        default: return "Unknown";
    }
}
