#include <pebble.h>
#include "cl_util.h"

#define KEY_START 0

#define NUM_SAMPLES 15

static Window *s_main_window;
static TextLayer *s_output_layer;
static TextLayer *x_layer, *y_layer, *z_layer;
static int latest_data[3 * NUM_SAMPLES];

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_output_layer, "Up pressed!");
    latest_data[3] = 1;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_output_layer, "Select pressed!");
  latest_data[4] = 1;
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_output_layer, "Down pressed!");
  latest_data[5] = 1;
}

static void click_config_provider(void *context) {
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create output TextLayer
  s_output_layer = text_layer_create(GRect(5, 0, window_bounds.size.w - 5, window_bounds.size.h));
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer, "No button pressed yet.");
  text_layer_set_overflow_mode(s_output_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer));
  x_layer = cl_init_text_layer(GRect(0, 0, 144, 24), GColorBlack, GColorClear, false, 0, FONT_KEY_GOTHIC_18, GTextAlignmentLeft);
    text_layer_set_text(x_layer, "Waiting for Android...");
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(x_layer));

    y_layer = cl_init_text_layer(GRect(0, 24, 144, 24), GColorBlack, GColorClear, false, 0, FONT_KEY_GOTHIC_18, GTextAlignmentLeft);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(y_layer));

    z_layer = cl_init_text_layer(GRect(0, 48, 144, 24), GColorBlack, GColorClear, false, 0, FONT_KEY_GOTHIC_18, GTextAlignmentLeft);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(z_layer));
}

static void main_window_unload(Window *window) {
  // Destroy output TextLayer
  text_layer_destroy(s_output_layer);
  text_layer_destroy(x_layer);
    text_layer_destroy(y_layer);
    text_layer_destroy(z_layer);
}

static void accel_new_data(AccelData *data, uint32_t num_samples)
{
    for(uint32_t i = 0; i < num_samples; i++)
    {
        latest_data[(i * 3) + 0] = (int)(0 + data[i].x);    //0, 3, 6
        latest_data[(i * 3) + 1] = (int)(0 + data[i].y);    //1, 4, 7
        latest_data[(i * 3) + 2] = (int)(0 + data[i].z);    //2, 5, 8
        latest_data[3] = 0;
        latest_data[4] = 0;
        latest_data[5] = 0;
    }
}

static void in_dropped_handler(AppMessageResult reason, void *context) 
{ 
    cl_interpret_message_result(reason);
}

static void send_next_data()
{
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    for(int i = 0; i < NUM_SAMPLES; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            int value = 0 + latest_data[(3 * i) + j];
            Tuplet t = TupletInteger((3 * i) + j, value);
            dict_write_tuplet(iter, &t);
        }
    }
    
    app_message_outbox_send();
}

static void out_sent_handler(DictionaryIterator *iter, void *context)
{
    //CAUTION - INFINITE LOOP
    send_next_data();

    // //Show on watch
    // static char buffs[3][32];
    // snprintf(buffs[0], sizeof("X: XXXXX"), "X: %d", latest_data[0]);
    // snprintf(buffs[1], sizeof("Y: YYYYY"), "Y: %d", latest_data[1]);
    // snprintf(buffs[2], sizeof("Z: ZZZZZ"), "Z: %d", latest_data[2]);
    // text_layer_set_text(x_layer, buffs[0]);
    // text_layer_set_text(y_layer, buffs[1]);
    // text_layer_set_text(z_layer, buffs[2]);
}

static void process_tuple(Tuple *t)
{
    switch(t->key)
    {
    case KEY_START: 
        text_layer_set_text(x_layer, "Connection established.");
        send_next_data();
    break;
    }
}

static void in_received_handler(DictionaryIterator *iter, void *context)
{   
    //Get data
    Tuple *t = dict_read_first(iter);
    if(t)
    {
        process_tuple(t);
    }
    
    //Get next
    while(t != NULL)
    {
        t = dict_read_next(iter);
        if(t)
        {
            process_tuple(t);
        }
    }
}

static void out_failed_handler(DictionaryIterator *iter, AppMessageResult result, void *context)
{
    cl_interpret_message_result(result);
}

static void init() {
  // Create main Window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_set_click_config_provider(s_main_window, click_config_provider);
 
  cl_set_debug(true);

    accel_data_service_subscribe(NUM_SAMPLES, (AccelDataHandler)accel_new_data);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);

    app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_sent(out_sent_handler);
    app_message_register_outbox_failed(out_failed_handler);

    int in_size = app_message_inbox_size_maximum();
    int out_size = app_message_outbox_size_maximum();
    app_log(APP_LOG_LEVEL_INFO, "C", 0, "I/O Buffer: %d/%d", in_size, out_size);
    app_message_open(in_size, out_size);
  
  window_stack_push(s_main_window, true);
  
}

static void deinit() {
  accel_data_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}