#include <pebble.h>

#define KEY_DATA 5
#define KEY_TIME 6

static Window* window;
static TextLayer* text_layer;
static int sensitivity;
static bool recording = true;

static void sendData(int keyData, int keyTime, int data, time_t timestamp) {
    recording = false;
    DictionaryIterator *iter;
  
    app_message_outbox_begin(&iter);
    
    dict_write_int(iter, KEY_DATA, &data, sizeof(data), true);
    dict_write_int(iter, KEY_TIME, &timestamp, sizeof(timestamp), true);
    const uint32_t final_size = dict_write_end(iter);
    app_message_outbox_send(); 
    
    APP_LOG(APP_LOG_LEVEL_INFO, "Size of object sent: %d", final_size);
}

static void accel_data_handler(AccelData* data, uint32_t num_samples) {
    if(recording) {
      // calculate y-axis derivative
      static char buf[128];
      int dy = data[num_samples-1].y - data[0].y;
      if (dy > sensitivity) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Detected!");
        time_t timestamp = time(NULL);
        snprintf(buf, sizeof(buf), "Threshold: %d\nHandshake!", sensitivity);
        text_layer_set_text(text_layer, buf);
        sendData(KEY_DATA, KEY_TIME, 1, timestamp);
        vibes_double_pulse();
      } else {
        snprintf(buf, sizeof(buf), "Threshold: %d", sensitivity);
        text_layer_set_text(text_layer, buf);
      }
    }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Increase threshold
    static char buf[128];
    sensitivity += 25;
    snprintf(buf, sizeof(buf), "Threshold: %d", sensitivity);
    text_layer_set_text(text_layer, buf);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Resume shock display
    static char buf[128];
    sensitivity -= 25;
    snprintf(buf, sizeof(buf), "Threshold: %d", sensitivity);
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
    GRect bounds = layer_get_bounds(window_layer);

    text_layer = text_layer_create((GRect) { .origin = { 0, 30 }, .size = { bounds.size.w, 60 } });
    text_layer_set_text(text_layer, "Threshold: 850");
    layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
    text_layer_destroy(text_layer);
}

static void init(void) {
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    window = window_create();
    window_set_click_config_provider(window, click_config_provider);
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
    window_destroy(window);
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}
