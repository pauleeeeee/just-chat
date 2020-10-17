#pragma once

#include <pebble.h>
#include <string.h>


// #define DOTSHEIGHT 28
#define MAX_MESSAGES 5
#define MAX_MESSAGE_LENGTH 512
#define MAX_MESSAGES_POOL_SIZE 2560

#define MessageText 100
#define MessageUser 101
#define ShouldVibrate 102
#define ShouldConfirmDictation 103

typedef struct {
  // the text displayed in the bubble
  char* text;
  int len;
  // user or other
  bool is_user;

} MessageBubble;


