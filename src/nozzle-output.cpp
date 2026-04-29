#include "nozzle-plugin.h"

#include <stdlib.h>
#include <string.h>

#define NZL_OUTPUT_SENDER_NAME     "output_sender_name"
#define NZL_OUTPUT_APPLICATION_NAME "output_application_name"

static bool nozzle_output_create_sender(nozzle_output_context *ctx)
{
    if (ctx->sender) {
        nozzle_sender_destroy(ctx->sender);
        ctx->sender = nullptr;
    }

    NozzleSenderDesc desc{};
    desc.name = ctx->sender_name[0] ? ctx->sender_name : "OBS Nozzle Output";
    desc.application_name = ctx->application_name[0] ? ctx->application_name : "OBS Studio";
    desc.ring_buffer_size = 3;
    desc.allow_format_fallback = 1;

    NozzleSender *sender = nullptr;
    NozzleErrorCode err = nozzle_sender_create(&desc, &sender);
    if (err != NOZZLE_OK || !sender) {
        NZL_ERROR("failed to create nozzle sender (error %d)", (int)err);
        return false;
    }

    ctx->sender = sender;
    NZL_INFO("sender created: '%s' (%ux%u)", desc.name, ctx->width, ctx->height);
    return true;
}

static const char *nozzle_output_get_name(void *unused)
{
    (void)unused;
    return obs_module_text("NozzleOutput");
}

static void *nozzle_output_create(obs_data_t *settings, obs_output_t *output)
{
    auto *ctx = (nozzle_output_context *)bzalloc(sizeof(nozzle_output_context));
    if (!ctx)
        return nullptr;

    ctx->output = output;
    ctx->sender = nullptr;
    ctx->started = false;
    ctx->width = 0;
    ctx->height = 0;

    const char *name = obs_data_get_string(settings, NZL_OUTPUT_SENDER_NAME);
    const char *app = obs_data_get_string(settings, NZL_OUTPUT_APPLICATION_NAME);

    if (name && *name) {
        strncpy(ctx->sender_name, name, sizeof(ctx->sender_name) - 1);
    }
    if (app && *app) {
        strncpy(ctx->application_name, app, sizeof(ctx->application_name) - 1);
    } else {
        strncpy(ctx->application_name, "OBS Studio", sizeof(ctx->application_name) - 1);
    }

    return ctx;
}

static void nozzle_output_destroy(void *data)
{
    auto *ctx = (nozzle_output_context *)data;
    if (!ctx)
        return;

    if (ctx->sender) {
        nozzle_sender_destroy(ctx->sender);
        ctx->sender = nullptr;
    }

    bfree(ctx);
}

static void nozzle_output_update(void *data, obs_data_t *settings)
{
    auto *ctx = (nozzle_output_context *)data;

    const char *name = obs_data_get_string(settings, NZL_OUTPUT_SENDER_NAME);
    const char *app = obs_data_get_string(settings, NZL_OUTPUT_APPLICATION_NAME);

    if (name && *name) {
        strncpy(ctx->sender_name, name, sizeof(ctx->sender_name) - 1);
    }
    if (app && *app) {
        strncpy(ctx->application_name, app, sizeof(ctx->application_name) - 1);
    } else {
        strncpy(ctx->application_name, "OBS Studio", sizeof(ctx->application_name) - 1);
    }
}

static bool nozzle_output_start(void *data)
{
    auto *ctx = (nozzle_output_context *)data;

    if (!ctx->output) {
        NZL_ERROR("output_start called with no output");
        return false;
    }

    uint32_t width = obs_output_get_width(ctx->output);
    uint32_t height = obs_output_get_height(ctx->output);

    if (width == 0 || height == 0) {
        NZL_ERROR("output has invalid dimensions: %ux%u", width, height);
        return false;
    }

    ctx->width = width;
    ctx->height = height;

    video_t *video = obs_output_video(ctx->output);
    if (!video) {
        NZL_ERROR("output has no video");
        return false;
    }

    if (!obs_output_can_begin_data_capture(ctx->output, 0)) {
        NZL_ERROR("cannot begin data capture");
        return false;
    }

    video_scale_info info{};
    info.format = VIDEO_FORMAT_BGRA;
    info.width = width;
    info.height = height;
    obs_output_set_video_conversion(ctx->output, &info);

    if (!nozzle_output_create_sender(ctx)) {
        NZL_ERROR("failed to create nozzle sender");
        return false;
    }

    bool started = obs_output_begin_data_capture(ctx->output, 0);
    if (!started) {
        NZL_ERROR("failed to begin data capture");
        nozzle_sender_destroy(ctx->sender);
        ctx->sender = nullptr;
        return false;
    }

    ctx->started = true;
    NZL_INFO("output started: '%s' %ux%u", ctx->sender_name, width, height);
    return true;
}

static void nozzle_output_stop(void *data, uint64_t ts)
{
    (void)ts;
    auto *ctx = (nozzle_output_context *)data;

    if (ctx->started) {
        obs_output_end_data_capture(ctx->output);
        ctx->started = false;
    }

    if (ctx->sender) {
        nozzle_sender_destroy(ctx->sender);
        ctx->sender = nullptr;
    }

    NZL_INFO("output stopped");
}

static void nozzle_output_raw_video(void *data, struct video_data *frame)
{
    auto *ctx = (nozzle_output_context *)data;

    if (!ctx->started || !ctx->sender)
        return;

    NozzleFrame *writable_frame = nullptr;
    NozzleErrorCode err = nozzle_sender_acquire_writable_frame(
        ctx->sender,
        ctx->width,
        ctx->height,
        NOZZLE_FORMAT_BGRA8_UNORM,
        &writable_frame);

    if (err != NOZZLE_OK || !writable_frame) {
        NZL_WARN("failed to acquire writable frame (error %d)", (int)err);
        return;
    }

    NozzleMappedPixels pixels{};
    err = nozzle_frame_lock_writable_pixels(writable_frame, &pixels);
    if (err != NOZZLE_OK) {
        NZL_WARN("failed to lock writable pixels (error %d)", (int)err);
        nozzle_frame_release(writable_frame);
        return;
    }

    uint32_t copy_height = pixels.height < ctx->height ? pixels.height : ctx->height;
    uint32_t src_row_bytes = ctx->width * 4;
    uint32_t dst_row_bytes = pixels.row_bytes;

    if (src_row_bytes == dst_row_bytes) {
        memcpy(pixels.data, frame->data[0], src_row_bytes * copy_height);
    } else {
        uint32_t min_row = src_row_bytes < dst_row_bytes ? src_row_bytes : dst_row_bytes;
        const uint8_t *src = (const uint8_t *)frame->data[0];
        uint8_t *dst = (uint8_t *)pixels.data;
        for (uint32_t y = 0; y < copy_height; y++) {
            memcpy(dst, src, min_row);
            src += src_row_bytes;
            dst += dst_row_bytes;
        }
    }

    nozzle_frame_unlock_writable_pixels(writable_frame);

    err = nozzle_sender_commit_frame(ctx->sender, writable_frame);
    if (err != NOZZLE_OK) {
        NZL_WARN("failed to commit frame (error %d)", (int)err);
    }
}

static obs_properties_t *nozzle_output_get_properties(void *data)
{
    (void)data;
    obs_properties_t *props = obs_properties_create();

    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

    obs_properties_add_text(
        props, NZL_OUTPUT_SENDER_NAME,
        obs_module_text("NozzleOutput.SenderName"),
        OBS_TEXT_DEFAULT);

    obs_properties_add_text(
        props, NZL_OUTPUT_APPLICATION_NAME,
        obs_module_text("NozzleOutput.ApplicationName"),
        OBS_TEXT_DEFAULT);

    return props;
}

obs_output_info create_nozzle_output_info()
{
    obs_output_info info{};
    info.id = "nozzle_output";
    info.flags = OBS_OUTPUT_VIDEO;
    info.get_name = nozzle_output_get_name;
    info.create = nozzle_output_create;
    info.destroy = nozzle_output_destroy;
    info.update = nozzle_output_update;
    info.start = nozzle_output_start;
    info.stop = nozzle_output_stop;
    info.raw_video = nozzle_output_raw_video;
    info.get_properties = nozzle_output_get_properties;
    return info;
}
