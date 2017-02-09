/*
	Copyright (C) 2016 Ramiro Jose Garcia Moraga

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "gwebsocket.h"

typedef struct _GWebSocketPrivate GWebSocketPrivate;

#define G_WEBSOCKET_KEY_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef enum
{
    G_WEBSOCKET_CODEOP_CONTINUE = 0x0,
    G_WEBSOCKET_CODEOP_TEXT = 0x1,
    G_WEBSOCKET_CODEOP_BINARY = 0x2,
    G_WEBSOCKET_CODEOP_CLOSE = 0x8,
    G_WEBSOCKET_CODEOP_PING = 0x9,
    G_WEBSOCKET_CODEOP_PONG = 0xA
}GWebSocketCodeOp;

struct _GWebSocketPrivate
{
  GSocketConnection *	connection;
  GMutex 		mutex_input;
  GMutex 		mutex_output;
  GThread *		recv_thread;
  GCancellable *  	recv_cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE(GWebSocket,g_websocket,G_TYPE_OBJECT)

static void	_g_websocket_dispose(GObject* object);
static void	_g_websocket_start(GWebSocket * self);
static void	_g_websocket_stop(GWebSocket * self);
static gpointer	_g_websocket_recv_thread(gpointer thread_data);

static gboolean _g_websocket_send(GWebSocket * socket,GWebSocketMessage * message,GError ** error);

static void	_g_websocket_message(GWebSocket * socket,GWebSocketMessage * message);
static void	_g_websocket_closed(GWebSocket * socket);


static void
g_websocket_init(GWebSocket * self)
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(self);
  g_mutex_init(&(priv->mutex_input));
  g_mutex_init(&(priv->mutex_output));
  priv->connection = NULL;
}

static void
g_websocket_class_init(GWebSocketClass * klass)
{
  G_OBJECT_CLASS(klass)->dispose = _g_websocket_dispose;
  /*<override>*/
  klass->send = _g_websocket_send;
  /*<signal>*/
  klass->closed = _g_websocket_closed;
  klass->message = _g_websocket_message;
}


static void
_g_websocket_dispose(GObject* object)
{
  GWebSocket * self = G_WEBSOCKET(object);
  GWebSocketPrivate * priv = g_websocket_get_instance_private(self);
  if(g_websocket_is_connected(self))
    g_websocket_close(self,NULL);
  g_mutex_clear(&priv->mutex_input);
  g_mutex_clear(&priv->mutex_output);
}

gchar *
g_websocket_generate_handshake(
    const gchar *key
    )
{
  gsize handshake_len = 20;
  guchar handshake[21];
  GChecksum *checksum;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, (guchar *)key, -1);
  g_checksum_update (checksum, (guchar *)G_WEBSOCKET_KEY_MAGIC, -1);

  g_checksum_get_digest (checksum, handshake, &handshake_len);
  g_checksum_free (checksum);

  g_assert (handshake_len == 20);

  return g_base64_encode (handshake, handshake_len);
}

gchar *
g_websocket_generate_key()
{
	guchar key[13];
	struct timespec tm_spec;
	guint8 index;
	if(!clock_gettime(0,&tm_spec))
	{
		srand(tm_spec.tv_nsec);
		for(index = 0;index <= 12;index ++)
			key[index] = (rand() % 256);
	}
	return g_base64_encode(key,12);
}

static void
_g_websocket_start(GWebSocket * self)
{

}

static void
_g_websocket_stop(GWebSocket * self)
{

}

static gpointer
_g_websocket_recv_thread(gpointer thread_data)
{

}

static gboolean
_g_websocket_send(
    GWebSocket * socket,
    GWebSocketMessage * message,
    GError ** error
    )
{

}

static void
_g_websocket_message(
    GWebSocket * socket,
    GWebSocketMessage * message
    )
{

}

static void
_g_websocket_closed(
    GWebSocket * socket
    )
{


}
