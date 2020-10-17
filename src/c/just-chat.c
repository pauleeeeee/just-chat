#include "./just-chat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Window *s_window;
static DictationSession *s_dictation_session;
//holder char for dictation session initialization
static char s_sent_message[512];
static int s_num_messages;
//array of message bubbles
static MessageBubble s_message_bubbles[MAX_MESSAGES];
//array of text layers that hold the bounds of each message bubble
static TextLayer *s_text_layers[MAX_MESSAGES];
//all text layers are added as children to the scroll layer
static ScrollLayer *s_scroll_layer;

// static int timeout = 30000;
static int should_vibrate = 1;
static int should_confirm_dictation = 0;

// static void timeout_and_exit(){
//   exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
//   window_stack_remove(s_window, false);
// }

static int msgsDataAvailable = MAX_MESSAGES_POOL_SIZE;

static int msgsTop = 0;
static int msgsCount = 0;


// int get_num_messages() {
//   return msgsCount;
//   // return s_num_messages;
// }

// MessageBubble *get_message_by_id(int id) {
//   return &s_message_bubbles[id];
// }


//big thanks to @UDXS for this array / memory management code for MessageBubble!

int msgs_get_idx(int idx){
	int trueIdx = msgsTop - 1 - idx;
	if(trueIdx < 0) trueIdx += MAX_MESSAGES;
	return trueIdx;
}

MessageBubble* msgs_get(int idx){
  printf("getting message idx %d", idx);
	return &s_message_bubbles[msgs_get_idx(idx)];
}

void msg_delete_last(){
	int idx = msgs_get_idx(--msgsCount);
	printf("DelLast at %d\n", idx);

	msgsDataAvailable += s_message_bubbles[idx].len;
	free(s_message_bubbles[idx].text);
}

void msg_gc(int bytes){ // Garbage Collect.
	if(bytes > MAX_MESSAGES_POOL_SIZE) {
    printf("MAX POOL limit hit");
		return; // Error.
	}
	while(msgsDataAvailable < bytes){ // Just keep deleting message entries until we have space.
		msg_delete_last();
	}
}

MessageBubble* msg_push(MessageBubble msg){
	if(msg.len > MAX_MESSAGES_POOL_SIZE){ // Message too long for buffer.
    printf("message too long for buffer"); 
		return NULL; // Error.
	}
	msg_gc(msg.len); // Ensure there is room in the message string data buffer for this new message.

	if(msgsCount - 1 == MAX_MESSAGES){ // No more space in the messages list. Make room for new message metadata.
		msg_delete_last();
	}

	s_message_bubbles[msgsTop++] = msg;
	msgsCount++;
	msgsDataAvailable -= msg.len;

	printf("Left = %d. NewTop = %d\n", msgsDataAvailable, msgsTop);
  
  return NULL;
}

//creates a text layer that is appropriately sized AND placed for the message.
TextLayer *render_new_bubble(int index, GRect bounds) {
  //get text from message array
  // char *text = s_message_bubbles[index].text;
  char *text = msgs_get(index)->text;
  //set font
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  //set bold if message is from user
  if (msgs_get(index)->is_user){
    font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  } 
  //create a holder GRect to use to measure the size of the final textlayer for the message all integers like '5' and '10' are for padding
  GRect shrinking_rect = GRect(5, 0, bounds.size.w - 10, 2000);
  GSize text_size = graphics_text_layout_get_content_size(text, font, shrinking_rect, GTextOverflowModeWordWrap, GTextAlignmentLeft);
  GRect text_bounds = bounds;

  //set the starting y bounds to the height of the total bounds which was passed into this function by the other functions
  text_bounds.origin.y = bounds.size.h;
  text_bounds.size.h = text_size.h + 5;

  //creates the textlayer with the bounds caluclated above. Size AND position are set this way. This is how we get all the messages stacked on top of each other like a chat
  TextLayer *text_layer = text_layer_create(text_bounds);
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(text_layer, GColorClear);

  //set alignment depending on whether the message was made by the user or the assistant
  if (msgs_get(index)->is_user){
    text_layer_set_text_alignment(text_layer, GTextAlignmentRight);
  } else {
    text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
  }

  //do not get creative here
  text_layer_set_font(text_layer, font);
  text_layer_set_text(text_layer, text);

  return text_layer;
}

//this function loops through all messages and passes their data to the render_new_bubble function which actually draws each textlayer
static void draw_message_bubbles(){
  //get bounds of the pebble
  Layer *window_layer = window_get_root_layer(s_window);

  //this bounds GRect is very important. It gets modified on each pass through the loop. As new bubbles are created, the bounds are adjusted. The bounds supply information both for the actual size of the scroll layer but also for positioning of each textlayer
  //again, adjusted for padding with 5 and -10
  GRect bounds = GRect(5, 0, (layer_get_bounds(window_layer).size.w - 10), 5);

    //for each message, render a new bubble (stored in the text layers array), adjust the important bounds object, and update the scroll layer's size
    for(int index = msgsCount - 1; index >= 0; index--) {
      // printf("index number %d in the for loop", index);
      //render the bubble for the message
      s_text_layers[index] = render_new_bubble(index, bounds);
      //adjust bounds based on the height of the bubble rendered
      bounds.size.h = bounds.size.h + text_layer_get_content_size(s_text_layers[index]).h + 10;
      //set scroll layer content size so everything is shown and is scrollable
      scroll_layer_set_content_size(s_scroll_layer, bounds.size);
      //add the newly rendered bubble to the scroll layer. Again, its position was set by passing the bounds to the render_new_bubble function
      scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_text_layers[index]));
    }
  //after the bubbles have been drawn and added to the scroll layer, this function scrolls the scroll layer to the given offset. In this case: the bottom of the screen (to the y coordinate of the bounds object)
  scroll_layer_set_content_offset(s_scroll_layer, GPoint(0,-bounds.size.h), true);
}



//adds a new message to the messages array but does not render anything
static void add_new_message(char *text, bool is_user){
		msg_push((MessageBubble){text, strlen(text) + 1, is_user});
}




//standard dictation callback
static void handle_transcription(char *transcription_text) {
  //makes a dictionary which is used to send the phone app the results of the transcription
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  //writes the transcription to the dictionary
  dict_write_cstring(iter, 0, transcription_text);

  //sends the dictionary through an appmessage to the pebble phone app
  app_message_outbox_send();

  //adds the transcription to the messages array so it is displayed as a bubble. true indicates that this message is from a user
  char* text = malloc(strlen(transcription_text + 1));
  strcpy(text, transcription_text);
  add_new_message(text, true);

  //update the view so new message is drawn
  draw_message_bubbles();

}

static void dictation_session_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if(status == DictationSessionStatusSuccess) {
    handle_transcription(transcription);
  } else {
  }
}

//starts dictation if the user short presses select.
static void select_callback(ClickRecognizerRef recognizer, void *context) {
  if (s_dictation_session){
    dictation_session_start(s_dictation_session);
  } else {
    s_dictation_session = dictation_session_create(sizeof(s_sent_message), dictation_session_callback, NULL);
    dictation_session_enable_confirmation(s_dictation_session, false);
    dictation_session_start(s_dictation_session);
  }
}

//open pre-filled action menu
static void long_select_callback(ClickRecognizerRef recognizer, void *context) {
  //future feature
}

//this click config provider is not really used
static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_callback);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, long_select_callback, NULL);
}

//click config provider for the scroll window, setting both short and long select pushes.
static void prv_scroll_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_callback);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, long_select_callback, NULL);
}

//when the phone app sends the pebble app an appmessage, this function is called.
static void in_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *message_text_tuple = dict_find( iter, MessageText );
  if (message_text_tuple) {
    //if an appmessage has a ActionResponse key, read the value and add a new message. Since this is the assistant, the is_user flag is set to false
    char* text = malloc(strlen(message_text_tuple->value->cstring) + 1);
    strcpy(text, message_text_tuple->value->cstring);
    add_new_message(text, false);
    // add_new_message(message_text_tuple->value->cstring, false);
    draw_message_bubbles();
    vibes_short_pulse();
  }

  Tuple *message_user_tuple = dict_find( iter, MessageUser);
  if (message_user_tuple) {
    //future feature
  }

  Tuple *should_vibrate_tuple = dict_find( iter, ShouldVibrate );
  if (should_vibrate_tuple) {
    should_vibrate = should_vibrate_tuple->value->int32;
    persist_write_int(ShouldVibrate, should_vibrate_tuple->value->int32);
  }
  
  Tuple *should_confirm_dictation_tuple = dict_find( iter, ShouldConfirmDictation );
  if (should_confirm_dictation_tuple) {
    should_confirm_dictation = should_confirm_dictation_tuple->value->int32;
    persist_write_int(ShouldVibrate, should_confirm_dictation_tuple->value->int32);
    dictation_session_enable_confirmation(s_dictation_session, should_confirm_dictation);
  }
  
}

static void in_dropped_handler(AppMessageResult reason, void *context){
  //handle failed message
  add_new_message("Message was dropped between the phone and the Pebble", false);
  draw_message_bubbles();

}

static void scroll_layer_scrolled_handler(struct ScrollLayer *scroll_layer, void *context){
  //fire when scroll event
}

static void prv_window_load(Window *window) {
  //root layer
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);


  //recall settinngs from persistent storage
  should_vibrate = persist_read_int(ShouldVibrate);
  should_confirm_dictation = persist_read_int(ShouldConfirmDictation);

  //create dictation and set confirmation mode to false
  s_dictation_session = dictation_session_create(sizeof(s_sent_message), dictation_session_callback, NULL);
  dictation_session_enable_confirmation(s_dictation_session, should_confirm_dictation);

  //create scroll layer 
  GRect scroll_bounds = GRect(0, 0, bounds.size.w, bounds.size.h);
  s_scroll_layer = scroll_layer_create(scroll_bounds);
  // Set the scrolling content size
  scroll_layer_set_content_size(s_scroll_layer, GSize(scroll_bounds.size.w, scroll_bounds.size.h-2));
  //make the scroll layer page instead of single click scroll but sometimes it might lead to UI problems
  scroll_layer_set_paging(s_scroll_layer, false);
  //hide content indicator shadow
  scroll_layer_set_shadow_hidden(s_scroll_layer, true);
  // set the click config provider of the scroll layer. This adds the short and long select press callbacks. it silently preserves scrolling functionality
  scroll_layer_set_callbacks(s_scroll_layer, (ScrollLayerCallbacks){
    .click_config_provider = prv_scroll_click_config_provider,
    .content_offset_changed_handler = scroll_layer_scrolled_handler
  });
  //take the scroll layer's click config and place it onto the main window
  scroll_layer_set_click_config_onto_window(s_scroll_layer, window);

  //add scroll layer to the root window
  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));

  //testing
  // add_new_message("test test test test test test test test test test test test test test test test test test ", true);
  // add_new_message("test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test ", false);
  draw_message_bubbles();

}

static void prv_window_unload(Window *window) {
  for (int i = 0; i < MAX_MESSAGES; i++){
    text_layer_destroy(s_text_layers[i]);
  }
  dictation_session_destroy(s_dictation_session);
  scroll_layer_destroy(s_scroll_layer);
  //probably need to destroy other stuff here like the s_message_bubbles and the s_text_layers arrays
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  //instantiate appmessages
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

