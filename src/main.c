#include <pebble.h>

#include "analog.h"

static GBitmap *s_bitmap;
static BitmapLayer *s_bitmap_layer;
static Window *s_window;
static Layer *s_simple_bg_layer, *s_date_layer, *s_hands_layer;
static TextLayer *s_day_label, *s_num_label;

static GPath *s_min_tick_paths[NUM_MIN_TICKS];
static GPath *s_max_tick_paths[NUM_MAX_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_num_buffer[4], s_day_buffer[6];

static GPoint max_marks[NUM_MAX_TICKS][4];
static GPoint min_marks[NUM_MIN_TICKS][4];

static struct GPathInfo hours_array[NUM_MAX_TICKS];
static struct GPathInfo minutes_array[NUM_MIN_TICKS];
  
#define MinMarkRStart 84
#define MinMarkREnd 90
#define MinMarkWide 240
#define MaxMarkRStart 78
#define MaxMarkREnd 90
#define MaxMarkWide 340
   
// Matching values to those in 'Settings'
typedef enum {
  AppKeyShowHourTicks = 0,
  AppKeyShowMinTicks = 1
} AppKey;

static void bg_update_proc(Layer *layer, GContext *ctx) {
  //graphics_context_set_fill_color(ctx, GColorBlack);
  //graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int i = 0; i < NUM_MAX_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    //gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    #if defined(PBL_ROUND)
      gpath_draw_filled(ctx, s_max_tick_paths[i]);
    #endif
  }
  for (int i = 0; i < NUM_MIN_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    //gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    #if defined(PBL_ROUND)
      gpath_draw_filled(ctx, s_min_tick_paths[i]);
    #endif
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  const int16_t second_hand_length = PBL_IF_ROUND_ELSE((bounds.size.w / 2) - 19, bounds.size.w / 2);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };

  // second hand
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, second_hand, center);

  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorLightGray);

  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);

  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);

  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", t);  //%b for month
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_simple_bg_layer);

  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);

  s_day_label = text_layer_create(PBL_IF_ROUND_ELSE(
    //GRect(63, 114, 27, 20),
    //GRect(46, 114, 27, 20)));
    GRect(103, 114, 27, 20),
    GRect(86, 114, 27, 20)));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorWhite);
  text_layer_set_text_color(s_day_label, GColorBlack);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(PBL_IF_ROUND_ELSE(
    //GRect(90, 114, 18, 20),
    //GRect(73, 114, 18, 20)));
    GRect(130, 114, 18, 20),
    GRect(113, 114, 18, 20)));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorWhite);
  text_layer_set_text_color(s_num_label, GColorBlack);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_simple_bg_layer);
  layer_destroy(s_date_layer);

  text_layer_destroy(s_day_label);
  text_layer_destroy(s_num_label);

  layer_destroy(s_hands_layer);
}

static void init() {
  
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);

  s_bitmap_layer = bitmap_layer_create(bounds);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));
  s_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SPUR_IMAGE);
  bitmap_layer_set_compositing_mode(s_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);

   window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  //for (int i = 0; i < NUM_MAX_TICKS; ++i) {
  //  s_max_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  //}
  
    // init clock face paths
   for (int i = 0; i < NUM_MAX_TICKS; ++i) {
    int32_t mark_angle = TRIG_MAX_ANGLE * i / NUM_MAX_TICKS;
    max_marks[i][0].y= (int16_t)(-cos_lookup(mark_angle-MaxMarkWide) * (int32_t)MaxMarkRStart / TRIG_MAX_RATIO);
    max_marks[i][0].x= (int16_t)( sin_lookup(mark_angle-MaxMarkWide) * (int32_t)MaxMarkRStart / TRIG_MAX_RATIO);
    max_marks[i][1].y= (int16_t)(-cos_lookup(mark_angle-MaxMarkWide) * (int32_t)MaxMarkREnd   / TRIG_MAX_RATIO);
    max_marks[i][1].x= (int16_t)( sin_lookup(mark_angle-MaxMarkWide) * (int32_t)MaxMarkREnd   / TRIG_MAX_RATIO); 
    max_marks[i][2].y= (int16_t)(-cos_lookup(mark_angle+MaxMarkWide) * (int32_t)MaxMarkREnd   / TRIG_MAX_RATIO);
    max_marks[i][2].x= (int16_t)( sin_lookup(mark_angle+MaxMarkWide) * (int32_t)MaxMarkREnd   / TRIG_MAX_RATIO);  
    max_marks[i][3].y= (int16_t)(-cos_lookup(mark_angle+MaxMarkWide) * (int32_t)MaxMarkRStart / TRIG_MAX_RATIO);
    max_marks[i][3].x= (int16_t)( sin_lookup(mark_angle+MaxMarkWide) * (int32_t)MaxMarkRStart / TRIG_MAX_RATIO);
    hours_array[i].num_points = 4;
    hours_array[i].points = &max_marks[i][0];    
    s_max_tick_paths[i] = gpath_create(&hours_array[i]);  
    gpath_move_to(s_max_tick_paths[i], center);

  }

   // init clock face paths
   for (int i = 0; i < NUM_MIN_TICKS; ++i) {
    int32_t mark_angle = TRIG_MAX_ANGLE * i / NUM_MIN_TICKS;
    min_marks[i][0].y= (int16_t)(-cos_lookup(mark_angle-MinMarkWide) * (int32_t)MinMarkRStart / TRIG_MAX_RATIO);
    min_marks[i][0].x= (int16_t)( sin_lookup(mark_angle-MinMarkWide) * (int32_t)MinMarkRStart / TRIG_MAX_RATIO);
    min_marks[i][1].y= (int16_t)(-cos_lookup(mark_angle-MinMarkWide) * (int32_t)MinMarkREnd   / TRIG_MAX_RATIO);
    min_marks[i][1].x= (int16_t)( sin_lookup(mark_angle-MinMarkWide) * (int32_t)MinMarkREnd   / TRIG_MAX_RATIO); 
    min_marks[i][2].y= (int16_t)(-cos_lookup(mark_angle+MinMarkWide) * (int32_t)MinMarkREnd   / TRIG_MAX_RATIO);
    min_marks[i][2].x= (int16_t)( sin_lookup(mark_angle+MinMarkWide) * (int32_t)MinMarkREnd   / TRIG_MAX_RATIO);  
    min_marks[i][3].y= (int16_t)(-cos_lookup(mark_angle+MinMarkWide) * (int32_t)MinMarkRStart / TRIG_MAX_RATIO);
    min_marks[i][3].x= (int16_t)( sin_lookup(mark_angle+MinMarkWide) * (int32_t)MinMarkRStart / TRIG_MAX_RATIO);
    minutes_array[i].num_points = 4;
    minutes_array[i].points = &min_marks[i][0];
    //comment out to remove minute ticks
    s_min_tick_paths[i] = gpath_create(&minutes_array[i]);  
    gpath_move_to(s_min_tick_paths[i], center);

  }

  
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
  
}

static void deinit() {
  gbitmap_destroy(s_bitmap);
  bitmap_layer_destroy(s_bitmap_layer);
  
   gpath_destroy(s_hour_arrow);
  
  for (int i = 0; i < NUM_MAX_TICKS; ++i) {
    gpath_destroy(s_max_tick_paths[i]);
  }
  for (int i = 0; i < NUM_MIN_TICKS; ++i) {
    gpath_destroy(s_min_tick_paths[i]);
  }

  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}



