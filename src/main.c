#include <pebble.h>

#define COLORS       false
#define ANTIALIASING true

#define HAND_MARGIN  15
#define FINAL_RADIUS 70

#define ANIMATION_DURATION 5
#define ANIMATION_DELAY    6

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static TextLayer *s_date_layer;
static GFont s_date_font;


static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0;
static bool s_animating = false;

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;
  
  layer_mark_dirty(s_canvas_layer);
  
 /* for(int i = 0; i < 3; i++) {
    s_color_channels[i] = rand() % 256;
  } */
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 8);
  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // White clockface - Just changed it to Clear
  //graphics_context_set_fill_color(ctx, GColorClear);
  //graphics_fill_circle(ctx, s_center, s_radius);

  // Draw outline
  // graphics_draw_circle(ctx, s_center, s_radius); 

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  // Draw hands with positive length only
  if(s_radius > 2 * HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, hour_hand);
  } 
  if(s_radius > HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, minute_hand);
  }
}


// Make the date show
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
    
  // Copy date into buffer from tm structure
static char date_buffer[16];
strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);

// Show the date
text_layer_set_text(s_date_layer, date_buffer);
}


static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

// Create GBitmap
s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BIOSHOCK_ROUND);

// Create BitmapLayer to display the GBitmap
s_background_layer = bitmap_layer_create(window_bounds);

// Set the bitmap onto the layer and add to the window
bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));

  s_center = grect_center_point(&window_bounds);
  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer); 

// Create date TextLayer
s_date_layer = text_layer_create(GRect(0, 140, 144, 30));
text_layer_set_text_color(s_date_layer, GColorWhite);
text_layer_set_background_color(s_date_layer, GColorClear);
text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ANDES_24));
text_layer_set_font(s_date_layer, s_date_font);

// Add to Window
layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window) {
  // Destroy Date Layer
  text_layer_destroy(s_date_layer);
  
  // Destroy Date Font
  fonts_unload_custom_font(s_date_font);
  
  // Destroy Canvas
  layer_destroy(s_canvas_layer);
  
  // Destroy GBitmap and BitmapLayer
  gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);


}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}

static void init() {
  srand(time(NULL));

 time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);
  
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
  
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Make sure the time is displayed from the start
  update_time();

  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}