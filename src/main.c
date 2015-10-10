#include <pebble.h>
#include <math.h>
#include <shapes.h>

static Window *s_main_window;
static Layer *s_canvas_layer, *s_cover_layer, *s_tap_layer;
static TextLayer *s_date_layer, *s_battery_layer, *s_month_day_layer;
static GFont s_square_font_small, s_square_font_large;
static char s_battery_buffer[16];

//static int8_t s_animation_stage = 0;

bool slanted_to_right;

static PropertyAnimation *s_cover_wipe_animation;
static PropertyAnimation *s_batt_layer_animation,*s_batt_layer_reverse_animation;

int8_t currentHour, currentMinute, currentMonthDay;
static char month_and_weekday_buffer[50];
static char abbrv_month[4], day_of_month[3], full_weekday[10];

//create pointers for path info
static GPath *s_dotpath_info_ptr, *s_hour_tens_info_ptr, *s_hour_ones_info_ptr, *s_minute_tens_info_ptr, *s_minute_ones_info_ptr;

// use a common angle for all rotations
float ROTATION_ANGLE;

// function prototypes
static void battery_layer_animation();
static void tap_handler(AccelAxisType axis, int32_t direction);

static void draw_time_dot(GContext *ctx, int8_t x, int8_t y){
  gpath_move_to(s_dotpath_info_ptr, GPoint(x,y));
  gpath_rotate_to(s_dotpath_info_ptr, ROTATION_ANGLE);
  gpath_draw_filled(ctx, s_dotpath_info_ptr);
}

static void update_numbers_from_current_time(){  
  s_hour_tens_info_ptr = gpath_create(time_digit_info((int)floor(currentHour / 10)));
  s_hour_ones_info_ptr = gpath_create(time_digit_info(currentHour % 10));
  s_minute_tens_info_ptr = gpath_create(time_digit_info((int)floor(currentMinute / 10)));
  s_minute_ones_info_ptr = gpath_create(time_digit_info(currentMinute % 10));
}

static void clean_up_number_gpaths(){
  gpath_destroy(s_hour_tens_info_ptr);
  gpath_destroy(s_hour_ones_info_ptr);
  gpath_destroy(s_minute_tens_info_ptr);
  gpath_destroy(s_minute_ones_info_ptr);
}

static void wipe_anim_stopped_handler(Animation *animation, bool finished, void *context) {
#ifdef PBL_SDK_2
  // Free the animation only on SDK 2.x
  property_animation_destroy(s_cover_wipe_animation);
#endif
}

static void startup_animation(){
  // Determine start and finish positions
  GRect start, finish;
  
  start = GRect(0, 0, 144, 168);
  finish = GRect(-144, 0, 144, 168);
  
  // Schedule the animation
  s_cover_wipe_animation = property_animation_create_layer_frame(s_cover_layer, &start, &finish);
  animation_set_duration((Animation*)s_cover_wipe_animation, 2400);
  animation_set_delay((Animation*)s_cover_wipe_animation, 500);
  animation_set_curve((Animation*)s_cover_wipe_animation, AnimationCurveLinear);
  animation_set_handlers((Animation*)s_cover_wipe_animation, (AnimationHandlers) {
    .stopped = wipe_anim_stopped_handler
  }, NULL);
  animation_schedule((Animation*)s_cover_wipe_animation);
}

static void batt_layer_anim_reverse_stopped_handler(Animation *animation, bool finished, void *context){
  #ifdef PBL_SDK_2
    // Free the animation only on SDK 2.x
    property_animation_destroy(s_batt_layer_reverse_animation);
  #endif
  
  if (finished){
    accel_tap_service_subscribe(tap_handler);
  }
}

static void battery_layer_animation_reverse(){
  GRect rstart, rfinish;
    
  if(slanted_to_right){
    rstart = GRect(143 - 40, 0, 38, 25);
    rfinish = GRect(143 - 40, -25, 38, 25);
  } else{
    rstart = GRect(4, 0, 38, 25);
    rfinish = GRect(4, -25, 38, 25);
  }
  
  // Schedule the animation
  s_batt_layer_reverse_animation = property_animation_create_layer_frame(s_tap_layer, &rstart, &rfinish);
  animation_set_duration((Animation*)s_batt_layer_reverse_animation, 1000);
  animation_set_delay((Animation*)s_batt_layer_reverse_animation, 2000);
  animation_set_curve((Animation*)s_batt_layer_reverse_animation, AnimationCurveEaseIn);
  animation_set_handlers((Animation*)s_batt_layer_reverse_animation, (AnimationHandlers) {
    .stopped = batt_layer_anim_reverse_stopped_handler
  }, NULL);
  animation_schedule((Animation*)s_batt_layer_reverse_animation);
}

static void tap_anim_stopped_handler(Animation *animation, bool finished, void *context){
  #ifdef PBL_SDK_2
  // Free the animation only on SDK 2.x
    property_animation_destroy(s_batt_layer_animation);
  #endif
    
  // Schedule the next one, unless the app is exiting
  if (finished) {
    battery_layer_animation_reverse();
  }
}

static void battery_layer_animation(){
    
  GRect start, finish;
  
  // get battery info
  BatteryChargeState charge_state = battery_state_service_peek();
  if (charge_state.is_charging) {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "Chg");
  } else {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
  }
  
  text_layer_set_text_color(s_battery_layer, GColorBlack);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentCenter);
  text_layer_set_text(s_battery_layer,s_battery_buffer);
  
    
  if(slanted_to_right){
    start = GRect(143 - 40, -25, 38, 25);
    finish = GRect(143 - 40, 0, 38, 25);    
  } else{
    start = GRect(4, -25, 38, 25);
    finish = GRect(4, 0, 38, 25);
  }
  
    // Schedule the animation
  s_batt_layer_animation = property_animation_create_layer_frame(s_tap_layer, &start, &finish);
  animation_set_duration((Animation*)s_batt_layer_animation, 1000);
  animation_set_delay((Animation*)s_batt_layer_animation, 300);
  animation_set_curve((Animation*)s_batt_layer_animation, AnimationCurveEaseOut);
  animation_set_handlers((Animation*)s_batt_layer_animation, (AnimationHandlers) {
    .stopped = tap_anim_stopped_handler
  }, NULL);
  animation_schedule((Animation*)s_batt_layer_animation);
  
}

static void battery_handler(BatteryChargeState charge_state) {  
  // if charging state changed, do the battery layer animation
  //if (charge_state.is_charging || charge_state.is_plugged || !charge_state.is_plugged || !charge_state.is_charging) {
  battery_layer_animation();
  //}
}

static void cover_update_proc(Layer *this_layer, GContext *ctx){
  // create a black grect the size of the whole screen
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(this_layer), 0, 0);
}

static void taplayer_update_proc(Layer *this_layer, GContext *ctx){
    // draw time dots
  #ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorBrightGreen);
  #else
    graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
    
  graphics_fill_rect(ctx, layer_get_bounds(this_layer), 0, 0);
}

static void canvas_update_proc(Layer *this_layer, GContext *ctx) {
  // draw time dots
  #ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorBrightGreen);
  #else
    graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  
  // set up gpoints as starting locations for digits
  GPoint HOUR_TENS;
  GPoint HOUR_ONES;
  GPoint MINUTE_TENS;
  GPoint MINUTE_ONES;  
  
  // draw the time and date according to which way it should be slanted
  if(slanted_to_right){ 
    ROTATION_ANGLE = TRIG_MAX_ANGLE * 0.073;
      
    HOUR_TENS = GPoint(28,21);  
    HOUR_ONES = GPoint(57,35);
    MINUTE_TENS = GPoint(91,53);
    MINUTE_ONES = GPoint(119,67);
    
    draw_time_dot(ctx, 74,63);
    draw_time_dot(ctx, 65,82);
    
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
    text_layer_set_text_alignment(s_month_day_layer, GTextAlignmentLeft);
  } else{
    ROTATION_ANGLE = TRIG_MAX_ANGLE * (1 - 0.073);
    
    HOUR_TENS = GPoint(3,79);
    HOUR_ONES = GPoint(32,65);
    MINUTE_TENS = GPoint(66,47);
    MINUTE_ONES = GPoint(94,33);
    
    draw_time_dot(ctx, 63,65);
    draw_time_dot(ctx, 72,84);
    
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_month_day_layer, GTextAlignmentRight);
  }
  clean_up_number_gpaths();            // destory all previous gpaths for numbers
  update_numbers_from_current_time();  // set up gpaths for current time numbers
  
         //draw time digits
  
  // draw the hour tens digit
  if(currentHour > 9 || currentHour == 0){
    gpath_move_to(s_hour_tens_info_ptr, HOUR_TENS);
    gpath_rotate_to(s_hour_tens_info_ptr, ROTATION_ANGLE);
    gpath_draw_filled(ctx, s_hour_tens_info_ptr);
  }
  
  // draw the hour ones digit
  gpath_move_to(s_hour_ones_info_ptr, HOUR_ONES);
  gpath_rotate_to(s_hour_ones_info_ptr, ROTATION_ANGLE);
  gpath_draw_filled(ctx, s_hour_ones_info_ptr);
  
  //draw the minute tens digit
  gpath_move_to(s_minute_tens_info_ptr, MINUTE_TENS);
  gpath_rotate_to(s_minute_tens_info_ptr, ROTATION_ANGLE);
  gpath_draw_filled(ctx, s_minute_tens_info_ptr);
    
  //draw the minute ones digit
  gpath_move_to(s_minute_ones_info_ptr, MINUTE_ONES);
  gpath_rotate_to(s_minute_ones_info_ptr, ROTATION_ANGLE);
  gpath_draw_filled(ctx, s_minute_ones_info_ptr);
  
  // print the date 
  #ifdef PBL_COLOR
    text_layer_set_text_color(s_date_layer, GColorBrightGreen);
    text_layer_set_text_color(s_month_day_layer, GColorBrightGreen);
  #else
    text_layer_set_text_color(s_date_layer, GColorWhite);
    text_layer_set_text_color(s_month_day_layer, GColorWhite);
  #endif
    
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text(s_date_layer,month_and_weekday_buffer);
  
  text_layer_set_background_color(s_month_day_layer, GColorClear);
  text_layer_set_text(s_month_day_layer,day_of_month);  
}

static void update_time(struct tm *tick_time) {
  // if 24 hour time not set, make necessary adjustements
  if(!clock_is_24h_style() && tick_time->tm_hour > 12){
    currentHour = tick_time->tm_hour - 12;
  } else {
    currentHour = tick_time->tm_hour;
  }
  currentMinute = tick_time->tm_min;
  
  currentMonthDay = tick_time->tm_mday;
  
  //testing:
  //currentMonthDay = 30;
  
  if(currentMonthDay > 9){
    strftime(month_and_weekday_buffer,sizeof(month_and_weekday_buffer),"%b    %n%A",tick_time);
  } else{
    strftime(month_and_weekday_buffer,sizeof(month_and_weekday_buffer),"%b  %n%A",tick_time);
  }
  
  strftime(abbrv_month,sizeof(abbrv_month),"%b",tick_time);
  //strftime(day_of_month,sizeof(day_of_month),"%e",tick_time); // put it back to this after testing
  snprintf(day_of_month,sizeof(day_of_month),"%d",currentMonthDay);
  
  strftime(full_weekday,sizeof(full_weekday),"%A",tick_time);
  
  //APP_LOG(APP_LOG_LEVEL_INFO,"month-day-weekday: %s-%s-%s", abbrv_month, day_of_month, full_weekday);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  layer_mark_dirty(s_canvas_layer);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  accel_tap_service_unsubscribe();
  // start the animation
  battery_layer_animation();
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);
  
  // Create Layers
  s_canvas_layer = layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  layer_add_child(window_layer, s_canvas_layer);

  //
  s_date_layer = text_layer_create(GRect(window_bounds.origin.x + 4, window_bounds.size.h - 48, window_bounds.size.w - 5, 48));
  s_square_font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_22));
  text_layer_set_font(s_date_layer, s_square_font_small);
  layer_add_child(window_layer,text_layer_get_layer(s_date_layer));
  
  GRect date_layer_bounds = layer_get_bounds(text_layer_get_layer(s_date_layer));
  
  if (slanted_to_right){
    s_month_day_layer = text_layer_create(GRect(46, -6, 28, 29));
  }else{
    if(currentMonthDay > 9){ /// check for 2-digit date, adjust positioning of date number
      s_month_day_layer = text_layer_create(GRect(143-51, -6, 48, 29));
    } else{
      s_month_day_layer = text_layer_create(GRect(143-32, -6, 28, 29));
    }
  }
  s_square_font_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_28));
  text_layer_set_font(s_month_day_layer, s_square_font_large);
  layer_add_child(text_layer_get_layer(s_date_layer), text_layer_get_layer(s_month_day_layer));
  
  s_cover_layer = layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  layer_add_child(window_layer, s_cover_layer);
  
  if (slanted_to_right){
    s_tap_layer = layer_create(GRect(143-44, -23, 40, 23));  
  }else{
    s_tap_layer = layer_create(GRect(4, -25, 40, 23));
  }
    
  layer_add_child(window_layer, s_tap_layer);

  GRect tap_layer_bounds = layer_get_bounds(s_tap_layer);
 
  s_battery_layer = text_layer_create(GRect(tap_layer_bounds.origin.x+1,tap_layer_bounds.origin.y-4,tap_layer_bounds.size.w-1,tap_layer_bounds.size.h+4));
  
  text_layer_set_font(s_battery_layer, s_square_font_small);
  //layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  layer_add_child(s_tap_layer, text_layer_get_layer(s_battery_layer));
    
  // set bg color
  window_set_background_color(s_main_window, GColorBlack);
    
  // Set the update_procs
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_set_update_proc(s_cover_layer, cover_update_proc);
  layer_set_update_proc(s_tap_layer, taplayer_update_proc);
  
}

static void main_window_unload(Window *window) {
  // Destroy Layers
  layer_destroy(s_canvas_layer);
  fonts_unload_custom_font(s_square_font_small);
  fonts_unload_custom_font(s_square_font_large);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_cover_layer);
  layer_destroy(s_tap_layer);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_month_day_layer);
}

static void init(void) {
  slanted_to_right = true;
  //slanted_to_right = false;
  
    // get initial time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_time(t);
   
  // Create main Window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
   
  // initialize the path info for the time dots
  s_dotpath_info_ptr = gpath_create(&TIME_DOT_INFO);
  
  // initialize all the number path info
  update_numbers_from_current_time();
                          
  // we want the minutes.  give us the minutes.
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  //tap handler service
  accel_tap_service_subscribe(tap_handler);
  
  // battery state service
  battery_state_service_subscribe(battery_handler);
  
  // start animation on startup
  startup_animation();
}

static void deinit(void) {
   // destroy dynamically allocated gpaths
  gpath_destroy(s_dotpath_info_ptr);
  clean_up_number_gpaths();
  
  // Stop any animation in progress
  animation_unschedule_all();
  
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
   
  // Destroy main Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
