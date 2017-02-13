/*
	Copyright (C) 2017 Ramiro Jose Garcia Moraga

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gwebsocket.h"

struct _GWebSocketMessage
{
  GWebSocketMessageType type;
  union{
    guint8 * data;
    gchar * text;
  } content;
  gsize length;
};

GWebSocketMessage *
g_websocket_message_new_text(
			    const gchar * text,
			    gssize length
			    )
{
  GWebSocketMessage * message = g_new0(GWebSocketMessage,1);
  if(length < 0)
    length = g_utf8_strlen(text,1024);

  message->content.text = g_strndup(text,length);
  message->type = G_WEBSOCKET_MESSAGE_TEXT;
  message->length = length;
  return message;
}

GWebSocketMessage *
g_websocket_message_new_data(
			    const guint8 * data,
			    gsize length
			    )
{
  GWebSocketMessage * message = g_new0(GWebSocketMessage,1);
  message->content.data = g_memdup(data,length);
  message->type = G_WEBSOCKET_MESSAGE_BINARY;
  message->length = length;
  return message;
}

GWebSocketMessageType
g_websocket_message_get_type(
			    GWebSocketMessage * message
			    )
{
  return message->type;
}

const gchar *
g_websocket_message_get_text(
			    GWebSocketMessage * message
			    )
{
  g_return_val_if_fail(message->type == G_WEBSOCKET_MESSAGE_TEXT,NULL);
  return message->content.text;
}

const guint8 *
g_websocket_message_get_data(
			    GWebSocketMessage * message
			    )
{
  g_return_val_if_fail(message->type == G_WEBSOCKET_MESSAGE_BINARY,NULL);
  return message->content.data;
}

gsize
g_websocket_message_get_length(
			    GWebSocketMessage * message
			    )
{
  return message->length;
}

void
g_websocket_message_free(
			    GWebSocketMessage * message
			    )
{
  g_free(message->content.data);
  g_free(message);
}
