#include "nozzle-plugin.h"

#include <stdlib.h>
#include <string.h>

#define NZL_SENDER_LIST "sender_list"
#define NZL_TIMEOUT_MS  "timeout_ms"
#define NZL_TIMEOUT_DEFAULT 100

static void nozzle_source_destroy_receiver(nozzle_source_context *ctx)
{
    if (!ctx)
        return;

    ctx->connected = false;
    ctx->initialized = false;

    if (ctx->texture) {
        obs_enter_graphics();
        gs_texture_destroy(ctx->texture);
        obs_leave_graphics();
        ctx->texture = nullptr;
    }

    if (ctx->receiver) {
        nozzle_receiver_destroy(ctx->receiver);
        ctx->receiver = nullptr;
    }
}

static bool nozzle_source_try_connect(nozzle_source_context *ctx)
{
    if (ctx->receiver) {
        nozzle_source_destroy_receiver(ctx);
    }

    NozzleReceiverDesc desc{};
    desc.name = ctx->sender_name;
    desc.application_name = "OBS Studio";
    desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;

    NozzleReceiver *receiver = nullptr;
    NozzleErrorCode err = nozzle_receiver_create(&desc, &receiver);
    if (err != NOZZLE_OK || !receiver) {
        NZL_WARN("failed to create receiver for '%s' (error %d)", ctx->sender_name, (int)err);
        return false;
    }

    ctx->receiver = receiver;

    NozzleConnectedSenderInfo info{};
    err = nozzle_receiver_get_connected_info(receiver, &info);
    if (err != NOZZLE_OK) {
        NZL_WARN("receiver created but not connected to '%s'", ctx->sender_name);
        ctx->connected = false;
        ctx->initialized = false;
        return false;
    }

    ctx->width = info.width;
    ctx->height = info.height;
    ctx->format = info.format;
    ctx->connected = true;
    ctx->initialized = true;

    NZL_INFO("connected to sender '%s' (%ux%u)", ctx->sender_name, ctx->width, ctx->height);
    return true;
}

static void nozzle_source_update_texture(nozzle_source_context *ctx, NozzleFrame *frame)
{
    NozzleFrameInfo frame_info{};
    nozzle_frame_get_info(frame, &frame_info);

    if (frame_info.width != ctx->width || frame_info.height != ctx->height) {
        ctx->width = frame_info.width;
        ctx->height = frame_info.height;

        obs_enter_graphics();
        if (ctx->texture) {
            gs_texture_destroy(ctx->texture);
        }
        ctx->texture = gs_texture_create(
            ctx->width, ctx->height,
            nozzle_to_gs_format(ctx->format),
            1, nullptr, 0);
        obs_leave_graphics();
    }

    if (!ctx->texture)
        return;

    NozzleMappedPixels pixels{};
    NozzleErrorCode err = nozzle_frame_lock_pixels_with_origin(frame, NOZZLE_ORIGIN_TOP_LEFT, &pixels);
    if (err != NOZZLE_OK) {
        NZL_WARN("failed to lock frame pixels (error %d)", (int)err);
        return;
    }

    const uint8_t *image_data = (const uint8_t *)pixels.data;
    uint32_t pixel_count = pixels.width * pixels.height;
    uint32_t channels = (pixels.format == NOZZLE_FORMAT_RGBA32_UINT) ? 4 : 1;
    uint32_t element_count = pixel_count * channels;

    uint32_t *convert_buf = nullptr;

    if (nozzle_needs_uint_to_float(pixels.format)) {
        convert_buf = (uint32_t *)bmalloc(element_count * sizeof(float));
        auto *src = (const uint32_t *)pixels.data;
        auto *dst = (float *)convert_buf;
        for (uint32_t i = 0; i < element_count; ++i) {
            dst[i] = static_cast<float>(src[i]);
        }
        image_data = (const uint8_t *)convert_buf;
    }

    obs_enter_graphics();
    gs_texture_set_image(
        ctx->texture,
        image_data,
        pixels.row_stride_bytes,
        false);
    obs_leave_graphics();

    if (convert_buf) {
        bfree(convert_buf);
    }

    nozzle_frame_unlock_pixels(frame);
}

static const char *nozzle_source_get_name(void *unused)
{
    (void)unused;
    return obs_module_text("NozzleSource");
}

static void *nozzle_source_create(obs_data_t *settings, obs_source_t *source)
{
    auto *ctx = (nozzle_source_context *)bzalloc(sizeof(nozzle_source_context));
    if (!ctx)
        return nullptr;

    ctx->source = source;
    ctx->width = 1;
    ctx->height = 1;
    ctx->timeout_ms = NZL_TIMEOUT_DEFAULT;
    ctx->texture = nullptr;
    ctx->receiver = nullptr;
    ctx->initialized = false;
    ctx->connected = false;

    const char *name = obs_data_get_string(settings, NZL_SENDER_LIST);
    if (name && *name) {
        strncpy(ctx->sender_name, name, sizeof(ctx->sender_name) - 1);
    }
    ctx->timeout_ms = (uint32_t)obs_data_get_int(settings, NZL_TIMEOUT_MS);

    return ctx;
}

static void nozzle_source_destroy(void *data)
{
    auto *ctx = (nozzle_source_context *)data;
    nozzle_source_destroy_receiver(ctx);
    bfree(ctx);
}

static void nozzle_source_update(void *data, obs_data_t *settings)
{
    auto *ctx = (nozzle_source_context *)data;

    const char *name = obs_data_get_string(settings, NZL_SENDER_LIST);
    uint32_t timeout = (uint32_t)obs_data_get_int(settings, NZL_TIMEOUT_MS);

    bool name_changed = strncmp(ctx->sender_name, name ? name : "", sizeof(ctx->sender_name) - 1) != 0;
    bool timeout_changed = ctx->timeout_ms != timeout;

    if (name && *name) {
        strncpy(ctx->sender_name, name, sizeof(ctx->sender_name) - 1);
    }
    ctx->timeout_ms = timeout;

    if (name_changed && ctx->sender_name[0]) {
        nozzle_source_destroy_receiver(ctx);
        nozzle_source_try_connect(ctx);
    }
}

static void nozzle_source_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, NZL_SENDER_LIST, "");
    obs_data_set_default_int(settings, NZL_TIMEOUT_MS, NZL_TIMEOUT_DEFAULT);
}

static uint32_t nozzle_source_get_width(void *data)
{
    auto *ctx = (nozzle_source_context *)data;
    return ctx->width;
}

static uint32_t nozzle_source_get_height(void *data)
{
    auto *ctx = (nozzle_source_context *)data;
    return ctx->height;
}

static void nozzle_source_video_tick(void *data, float seconds)
{
    (void)seconds;
    auto *ctx = (nozzle_source_context *)data;

    if (!ctx->sender_name[0])
        return;

    if (!ctx->receiver || !ctx->connected) {
        nozzle_source_try_connect(ctx);
        return;
    }

    NozzleAcquireDesc acquire_desc{};
    acquire_desc.timeout_ms = ctx->timeout_ms;

    NozzleFrame *frame = nullptr;
    NozzleErrorCode err = nozzle_receiver_acquire_frame(ctx->receiver, &acquire_desc, &frame);
    if (err != NOZZLE_OK || !frame) {
        if (err == NOZZLE_ERROR_SENDER_CLOSED) {
            NZL_WARN("sender '%s' closed, reconnecting...", ctx->sender_name);
            nozzle_source_destroy_receiver(ctx);
        }
        return;
    }

    NozzleFrameInfo frame_info{};
    nozzle_frame_get_info(frame, &frame_info);

    if (frame_info.frame_index == 0 && ctx->texture) {
        nozzle_frame_release(frame);
        return;
    }

    nozzle_source_update_texture(ctx, frame);
    nozzle_frame_release(frame);
}

static void nozzle_source_video_render(void *data, gs_effect_t *effect)
{
    (void)data;
    auto *ctx = (nozzle_source_context *)data;

    if (!ctx->texture)
        return;

    effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

    while (gs_effect_loop(effect, "Draw")) {
        obs_source_draw(ctx->texture, 0, 0, 0, 0, false);
    }
}

static void fill_sender_list(obs_property_t *list, const char *current)
{
    obs_property_list_clear(list);
    obs_property_list_add_string(list, obs_module_text("NozzleSource.SelectSender"), "");

    NozzleSenderInfoArray senders{};
    NozzleErrorCode err = nozzle_enumerate_senders(&senders);
    if (err != NOZZLE_OK || !senders.items)
        return;

    int current_idx = 0;
    for (uint32_t i = 0; i < senders.count; i++) {
        const char *name = senders.items[i].name ? senders.items[i].name : "";
        obs_property_list_add_string(list, name, name);
        if (current && strcmp(name, current) == 0)
            current_idx = (int)i + 1;
    }

    nozzle_free_sender_info_array(&senders);
}

static obs_properties_t *nozzle_source_get_properties(void *data)
{
    auto *ctx = (nozzle_source_context *)data;

    obs_properties_t *props = obs_properties_create();

    obs_property_t *sender_list = obs_properties_add_list(
        props, NZL_SENDER_LIST,
        obs_module_text("NozzleSource.SenderName"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

    fill_sender_list(sender_list, ctx->sender_name);

    obs_properties_add_int(
        props, NZL_TIMEOUT_MS,
        obs_module_text("NozzleSource.Timeout"),
        1, 5000, 10);

    return props;
}

obs_source_info create_nozzle_source_info()
{
    obs_source_info info{};
    info.id = "nozzle_source";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    info.get_name = nozzle_source_get_name;
    info.create = nozzle_source_create;
    info.destroy = nozzle_source_destroy;
    info.update = nozzle_source_update;
    info.get_defaults = nozzle_source_defaults;
    info.get_width = nozzle_source_get_width;
    info.get_height = nozzle_source_get_height;
    info.video_tick = nozzle_source_video_tick;
    info.video_render = nozzle_source_video_render;
    info.get_properties = nozzle_source_get_properties;
    return info;
}
