#include <pebble.h>

#define KEY_TIME 5
#define KEY_DIRECTION 6

static Window* window;
static TextLayer* text_layer;
static ActionBarLayer* action_bar;
static int sensitivity;
static bool recording = true;
static CompassHeadingData direction;
static GBitmap *status_icon;
static GBitmap *up_icon;
static GBitmap *down_icon;
static GBitmap *user_icon;


static void sendData(int keyTime, int keyDir, time_t timestamp, int dir) {
    recording = false;
    DictionaryIterator *iter;
  
    app_message_outbox_begin(&iter);
    dict_write_int(iter, keyTime, &timestamp, sizeof(timestamp), true);
    dict_write_int(iter, keyDir, &direction, sizeof(direction), true);
    dict_write_end(iter);
    app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO, "Direction of handshake: %d", dir);
}

static void accel_data_handler(AccelData* data, uint32_t num_samples) {
    if(recording) {
      // calculate y-axis derivative
      static char buf[128];
      int dy = data[num_samples-1].y - data[0].y;
      if (dy > sensitivity) {
        time_t timestamp = time(NULL);
        compass_service_peek(&direction);
        int angle = (int) TRIGANGLE_TO_DEG(direction.magnetic_heading);
        sendData(KEY_TIME, KEY_DIRECTION, timestamp, angle);
        vibes_double_pulse();
      } else {
        snprintf(buf, sizeof(buf), "Sensitivity: %d", sensitivity);
        text_layer_set_text(text_layer, buf);
      }
    }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Increase threshold
    static char buf[128];
    sensitivity += 25;
    snprintf(buf, sizeof(buf), "Sensitivity: %d", sensitivity);
    text_layer_set_text(text_layer, buf);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Resume shock display
    static char buf[128];
    sensitivity -= 25;
    snprintf(buf, sizeof(buf), "Sensitivity: %d", sensitivity);
    text_layer_set_text(text_layer, buf);
}
static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");
}
 
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
 
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
    APP_LOG(APP_LOG_LEVEL_ERROR, "%d", reason);
    recording = true;
}
 
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
    recording = true;
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    status_icon = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON);
    window_set_status_bar_icon(window, status_icon);
    GRect bounds = layer_get_bounds(window_layer);
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, window);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
    up_icon = gbitmap_create_with_resource(RESOURCE_ID_UP);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, up_icon);
    down_icon = gbitmap_create_with_resource(RESOURCE_ID_DOWN);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, down_icon);
    user_icon = gbitmap_create_with_resource(RESOURCE_ID_USER);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, user_icon);
    text_layer = text_layer_create((GRect) { .origin = { 0, 63 }, .size = { bounds.size.w, 60 } });
    text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
    text_layer_set_background_color(text_layer, GColorClear);
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text(text_layer, "Threshold: 850");
    layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
    gbitmap_destroy(status_icon);
    gbitmap_destroy(up_icon);
    gbitmap_destroy(down_icon);
    text_layer_destroy(text_layer);
    action_bar_layer_destroy(action_bar);
}

static void init(void) {
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
            .load = window_load,
            .unload = window_unload,
            });
    sensitivity = 850;
    accel_data_service_subscribe(5, accel_data_handler);
    const bool animated = true;
    window_stack_push(window, animated);
}

static void deinit(void) {
    accel_data_service_unsubscribe();
    window_destroy(window);
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}
