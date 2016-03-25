#include "messagebox.h"

#include "pebble.h"

static Window *messageBoxWindow;
static TextLayer *labelLayer;
static ActionBarLayer *actionBarLayer;

static GBitmap * okBitmap;
static GBitmap * nokBitmap;

static const char * message_   = 0;
static uint32_t okResourceId_  = 0;
static uint32_t nokResourceId_ = 0;
static VoidFnc okFunction_ = 0;
static VoidFnc nokFunction_ = 0;

static void onOkClicked(ClickRecognizerRef recognizer, void * context)
{
    window_stack_pop(true);
    if (okFunction_) okFunction_();
}
static void onNOkClicked(ClickRecognizerRef recognizer, void * context)
{
    window_stack_pop(true);
    if (nokFunction_) nokFunction_();
}

static void actionBarClickConfigProvider(void * context)
{
    window_single_click_subscribe(BUTTON_ID_UP,   (ClickHandler) onOkClicked);
    window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) onNOkClicked);
}

static void verticalAlignTextLayer(TextLayer * textLayer)
{
    const GRect frame = layer_get_frame(text_layer_get_layer(textLayer));
    const GSize content = text_layer_get_content_size(textLayer);
    layer_set_frame(text_layer_get_layer(textLayer),
                    GRect(frame.origin.x, frame.origin.y + (frame.size.h - content.h - 5) / 2,
                          frame.size.w, frame.size.h));
}

static void onMessageBoxLoad(Window *window)
{
    Layer * windowRootLayer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(windowRootLayer);

    labelLayer = text_layer_create(GRect(0, 0, bounds.size.w - ACTION_BAR_WIDTH, bounds.size.h));
    text_layer_set_text(labelLayer, message_);
    text_layer_set_background_color(labelLayer, GColorClear);
    text_layer_set_text_alignment(labelLayer, GTextAlignmentCenter);
    text_layer_set_font(labelLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    layer_add_child(windowRootLayer, text_layer_get_layer(labelLayer));

    verticalAlignTextLayer(labelLayer);

    okBitmap  = gbitmap_create_with_resource(okResourceId_);
    nokBitmap = gbitmap_create_with_resource(nokResourceId_);

    actionBarLayer = action_bar_layer_create();
    action_bar_layer_set_icon(actionBarLayer, BUTTON_ID_UP, okBitmap);
    action_bar_layer_set_icon(actionBarLayer, BUTTON_ID_DOWN, nokBitmap);
    action_bar_layer_add_to_window(actionBarLayer, window);
    action_bar_layer_set_click_config_provider(actionBarLayer, actionBarClickConfigProvider);
}

static void onMessageBoxUnload(Window *window)
{
    text_layer_destroy(labelLayer);
    action_bar_layer_destroy(actionBarLayer);

    gbitmap_destroy(okBitmap);
    gbitmap_destroy(nokBitmap);

    window_destroy(window);
    messageBoxWindow = NULL;
}

void showMessageBox(const char * msg, VoidFnc okFunction, VoidFnc nokFunction, uint32_t okResourceId, uint32_t nokResourceId)
{
    message_       = msg;
    okFunction_    = okFunction;
    nokFunction_   = nokFunction;
    okResourceId_  = okResourceId;
    nokResourceId_ = nokResourceId;

    if (!messageBoxWindow)
    {
        messageBoxWindow = window_create();
        window_set_background_color(messageBoxWindow, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
        window_set_window_handlers(messageBoxWindow, (WindowHandlers){
                                       .load   = onMessageBoxLoad,
                                       .unload = onMessageBoxUnload,
                                   });
    }
    window_stack_push(messageBoxWindow, true);
}
