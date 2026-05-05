#pragma once

#include <nozzle/nozzle_c.h>
#include <obs-module.h>
#include <stdint.h>
#include <string.h>

#define NZL_LOG(level, fmt, ...) \
    blog(level, "[nozzle] " fmt, ##__VA_ARGS__)

#define NZL_DEBUG(fmt, ...) NZL_LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define NZL_INFO(fmt, ...)  NZL_LOG(LOG_INFO, fmt, ##__VA_ARGS__)
#define NZL_WARN(fmt, ...)  NZL_LOG(LOG_WARNING, fmt, ##__VA_ARGS__)
#define NZL_ERROR(fmt, ...) NZL_LOG(LOG_ERROR, fmt, ##__VA_ARGS__)

struct nozzle_source_context {
    obs_source_t *source;
    NozzleReceiver *receiver;
    gs_texture_t *texture;
    uint32_t width;
    uint32_t height;
    NozzleTextureFormat format;
    char sender_name[256];
    uint32_t timeout_ms;
    bool initialized;
    bool connected;
};

struct nozzle_output_context {
    obs_output_t *output;
    NozzleSender *sender;
    char sender_name[256];
    char application_name[256];
    uint32_t width;
    uint32_t height;
    bool started;
};

static inline uint32_t nozzle_pixel_size(NozzleTextureFormat format)
{
    switch (format) {
    case NOZZLE_FORMAT_R8_UNORM:      return 1;
    case NOZZLE_FORMAT_RG8_UNORM:     return 2;
    case NOZZLE_FORMAT_R16_UNORM:     return 2;
    case NOZZLE_FORMAT_RG16_UNORM:    return 4;
    case NOZZLE_FORMAT_R16_FLOAT:     return 2;
    case NOZZLE_FORMAT_RG16_FLOAT:    return 4;
    case NOZZLE_FORMAT_R32_FLOAT:     return 4;
    case NOZZLE_FORMAT_RG32_FLOAT:    return 8;
    case NOZZLE_FORMAT_R32_UINT:      return 4;
    case NOZZLE_FORMAT_RGBA8_UNORM:
    case NOZZLE_FORMAT_BGRA8_UNORM:
    case NOZZLE_FORMAT_RGBA8_SRGB:
    case NOZZLE_FORMAT_BGRA8_SRGB:
        return 4;
    case NOZZLE_FORMAT_RGBA16_UNORM:  return 8;
    case NOZZLE_FORMAT_RGBA16_FLOAT:  return 8;
    case NOZZLE_FORMAT_RGBA32_FLOAT:  return 16;
    case NOZZLE_FORMAT_RGBA32_UINT:   return 16;
    case NOZZLE_FORMAT_DEPTH32_FLOAT: return 4;
    default:                          return 4;
    }
}

static inline enum gs_color_format nozzle_to_gs_format(NozzleTextureFormat format)
{
    switch (format) {
    case NOZZLE_FORMAT_RGBA8_UNORM:   return GS_RGBA;
    case NOZZLE_FORMAT_BGRA8_UNORM:   return GS_BGRX;
    case NOZZLE_FORMAT_RGBA8_SRGB:    return GS_RGBA;
    case NOZZLE_FORMAT_BGRA8_SRGB:    return GS_BGRX;
    case NOZZLE_FORMAT_R8_UNORM:      return GS_R8;
    case NOZZLE_FORMAT_RG8_UNORM:     return GS_R8G8;
    case NOZZLE_FORMAT_R16_UNORM:     return GS_R16;
    case NOZZLE_FORMAT_R32_FLOAT:     return GS_R32F;
    case NOZZLE_FORMAT_RGBA16_FLOAT:  return GS_RGBA16F;
    case NOZZLE_FORMAT_RGBA32_FLOAT:  return GS_RGBA32F;
    case NOZZLE_FORMAT_R16_FLOAT:     return GS_R16F;
    case NOZZLE_FORMAT_RG16_FLOAT:    return GS_RG16F;
    case NOZZLE_FORMAT_RG32_FLOAT:    return GS_RG32F;
    case NOZZLE_FORMAT_RG16_UNORM:    return GS_RG16;
    case NOZZLE_FORMAT_RGBA16_UNORM:  return GS_RGBA16;
    case NOZZLE_FORMAT_R32_UINT:      return GS_R32F;
    case NOZZLE_FORMAT_RGBA32_UINT:   return GS_RGBA32F;
    case NOZZLE_FORMAT_DEPTH32_FLOAT: return GS_R32F;
    default:                          return GS_RGBA;
    }
}
