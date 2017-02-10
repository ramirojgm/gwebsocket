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
typedef struct _GWebSocketDatagram GWebSocketDatagram;
typedef struct _GWebSocketIdleData GWebSocketIdleData;

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
  gboolean		use_mask;
};

struct _GWebSocketDatagram
{
  gboolean fin;
  GWebSocketCodeOp code;
  guint32 mask;
  guint8 * buffer;
  gsize count;
};

struct _GWebSocketIdleData
{
  GWebSocket * socket;
  GWebSocketDatagram * datagram;
};

G_DEFINE_TYPE_WITH_PRIVATE(GWebSocket,g_websocket,G_TYPE_OBJECT)

static void	_g_websocket_dispose(GObject* object);
static void	_g_websocket_start(GWebSocket * self);
static void	_g_websocket_stop(GWebSocket * self);

static gboolean	_g_websocket_read(GInputStream * input,GWebSocketDatagram ** datagram,GCancellable * cancellable,GError ** error);
static gboolean	_g_websocket_write(GOutputStream * output,GWebSocketDatagram * datagram,GCancellable * cancellable,GError ** error);

static gboolean _g_websocket_recv_idle(gpointer idle_data);
static gpointer	_g_websocket_recv_thread(gpointer thread_data);

static gboolean _g_websocket_send(GWebSocket * socket,GWebSocketMessage * message,GCancellable * cancellable,GError ** error);

enum
{
	PROP_CONNECTION = 1,
	PROP_USE_MASK = 2,
	N_PROPERTIES
};

enum
{
	SIGNAL_MESSAGE = 0,
	SIGNAL_CLOSED = 1,
	N_SIGNALS
};

//static GParamSpec * 	g_websocket_properties[N_PROPERTIES] = { NULL, };

static gint		g_websocket_signals[N_SIGNALS];

gboolean
g_websocket_uri_parse(const gchar * uri,gchar ** scheme,gchar ** hostname,gchar ** query)
{
  gchar * uri_scheme = g_uri_parse_scheme(uri);
  if(scheme)
    {
      *scheme = uri_scheme;
      *hostname = NULL;
      *query = NULL;

      gchar ** split = g_strsplit(uri + (g_utf8_strlen(uri_scheme,20) + 3),"/",2);
      if(g_strv_length(split) >= 1)
		*hostname = g_strdup(split[0]);
	  if(g_strv_length(split) == 2)
		*query = g_strdup_printf("/%s",split[1]);
	  g_strfreev(split);
      return *hostname != NULL;
    }
  else
    {
      return FALSE;
    }
}

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
  klass->closed = NULL;
  klass->message = NULL;

  const GType message_params[1] = {G_TYPE_POINTER};

  g_websocket_signals[SIGNAL_MESSAGE] =
      g_signal_newv ("message",
       G_TYPE_FROM_CLASS (klass),
       G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
       G_STRUCT_OFFSET(GWebSocketClass,message) /* closure */,
       NULL /* accumulator */,
       NULL /* accumulator data */,
       NULL /* C marshaller */,
       G_TYPE_NONE /* return_type */,
       1     /* n_params */,
       (GType*)message_params  /* param_types */);

  g_websocket_signals[SIGNAL_CLOSED] =
      g_signal_newv ("closed",
       G_TYPE_FROM_CLASS (klass),
       G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
       G_STRUCT_OFFSET(GWebSocketClass,closed) /* closure */,
       NULL /* accumulator */,
       NULL /* accumulator data */,
       NULL /* C marshaller */,
       G_TYPE_NONE /* return_type */,
       0     /* n_params */,
       NULL  /* param_types */);
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

guint32
g_websocket_generate_mask()
{
  struct timespec tm_spec;
  guint32 mask = 0;
  if(!clock_gettime(0,&tm_spec))
  {
    srand(tm_spec.tv_nsec);
    mask = rand();
  }
  return mask;
}

void
_g_websocket_start(GWebSocket * self)
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(self);
  g_return_if_fail(priv->connection != NULL);
  g_return_if_fail(g_socket_connection_is_connected(priv->connection) == TRUE);
  priv->recv_cancellable = g_cancellable_new();
  priv->recv_thread = g_thread_new("g_websocket_recv_thread",_g_websocket_recv_thread,self);
}

void
_g_websocket_stop(GWebSocket * self)
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(self);
  g_return_if_fail(priv->connection != NULL);
  g_return_if_fail(priv->recv_thread != NULL);
  g_return_if_fail(priv->recv_cancellable != NULL);
  g_return_if_fail(g_socket_connection_is_connected(priv->connection) == TRUE);
  g_return_if_fail(g_cancellable_is_cancelled(priv->recv_cancellable) == FALSE);

  g_cancellable_cancel(priv->recv_cancellable);
  g_thread_join(priv->recv_thread);

  if(g_socket_connection_is_connected(priv->connection))
    g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);

  g_clear_object(&(priv->connection));
  g_clear_object(&(priv->recv_cancellable));
}

static gpointer
_g_websocket_recv_thread(
    gpointer thread_data
    )
{
  GWebSocket * socket = G_WEBSOCKET(thread_data);
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);

  GMutex * mutex_input = &(priv->mutex_input);
  GCancellable * cancellable = priv->recv_cancellable;
  GInputStream * stream = g_io_stream_get_input_stream(G_IO_STREAM(priv->connection));
  GWebSocketDatagram * datagram = NULL;

  while(_g_websocket_read(stream,&datagram,cancellable,NULL))
    {
      g_mutex_lock(mutex_input);
      if(!g_cancellable_is_cancelled(cancellable) && !g_input_stream_is_closed(stream))
	{
	  GWebSocketIdleData * data = g_new0(GWebSocketIdleData,1);
	  data->datagram = datagram;
	  data->socket = socket;
	  g_idle_add(_g_websocket_recv_idle,data);
	}
      else
	{
	  if(datagram)
	    {
	      g_free(datagram->buffer);
	      g_free(datagram);
	    }
	}
      g_mutex_unlock(mutex_input);
    }
  return NULL;
}

static gboolean
_g_websocket_recv_idle(
    gpointer idle_data
    )
{
  GWebSocketIdleData * data = (GWebSocketIdleData*)idle_data;
  GWebSocketPrivate * priv = g_websocket_get_instance_private(data->socket);

  GOutputStream * output = NULL;
  if(g_socket_connection_is_connected(G_IO_STREAM(priv->connection)))
    output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));

  switch(data->datagram->code)
  {
  case G_WEBSOCKET_CODEOP_CLOSE:
    g_websocket_close(data->socket,NULL);
    break;
  case G_WEBSOCKET_CODEOP_TEXT:
    {
      GWebSocketMessage * message = g_websocket_message_new_text((const gchar*)data->datagram->buffer,data->datagram->count);
      g_signal_emit (data->socket, g_websocket_signals[SIGNAL_MESSAGE],0,message);
      g_websocket_message_free(message);
    }
    break;
  case G_WEBSOCKET_CODEOP_BINARY:
    {
      GWebSocketMessage * message = g_websocket_message_new_data(data->datagram->buffer,data->datagram->count);
       g_signal_emit (data->socket, g_websocket_signals[SIGNAL_MESSAGE],0);
       g_websocket_message_free(message);
    }
    break;
  case G_WEBSOCKET_CODEOP_CONTINUE:
    break;
  case G_WEBSOCKET_CODEOP_PING:
    if(output)
      {
	GWebSocketDatagram * pong = g_new0(GWebSocketDatagram,1);
	pong->code = G_WEBSOCKET_CODEOP_PONG;
	pong->count = data->datagram->count;
	pong->buffer = data->datagram->buffer;
	pong->fin = TRUE;
	pong->mask = 0;
	_g_websocket_write(output,pong,NULL,NULL);
	g_free(pong);
      }
    break;
  default:
    break;
  }
  g_free(data->datagram->buffer);
  g_free(data->datagram);
  g_free(data);
  return G_SOURCE_REMOVE;
}

static gboolean
_g_websocket_send(
    GWebSocket * socket,
    GWebSocketMessage * message,
    GCancellable * cancellable,
    GError ** error
    )
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);
  g_return_val_if_fail(priv->connection != NULL,FALSE);

  if(g_socket_connection_is_connected(priv->connection))
    {
      GOutputStream * output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
      GWebSocketDatagram * msg = g_new0(GWebSocketDatagram,1);
      msg->fin = TRUE;
      if(g_websocket_message_get_type(message) == G_WEBSOCKET_MESSAGE_TEXT)
	{
	  msg->code = G_WEBSOCKET_CODEOP_TEXT;
	  msg->buffer = (guint8*)g_websocket_message_get_data(message);
	}
      else
	{
	  msg->code = G_WEBSOCKET_CODEOP_BINARY;
	  msg->buffer = (guint8*)g_websocket_message_get_text(message);
	}
      msg->count = g_websocket_message_get_length(message);

      if(priv->use_mask)
	msg->mask = g_websocket_generate_mask();
      else
	msg->mask = 0;

      gboolean done = _g_websocket_write(output,msg,cancellable,error);
      g_free(msg);
      return done;
    }
  else
    {
      //TODO: is not connected
      return FALSE;
    }
}

static gboolean
_g_websocket_read(
      GInputStream * input,
      GWebSocketDatagram ** datagram,
      GCancellable * cancellable,
      GError ** error)
{
  GWebSocketDatagram * result = g_new0(GWebSocketDatagram,1);

  const guint64 max_frame_size = 15728640L; //-> 15MB
  gboolean done = FALSE;
  result->code = G_WEBSOCKET_CODEOP_CONTINUE;
  result->buffer = NULL;
  result->count = 0;

  guint8 header[2] = {0,};

  done = g_input_stream_read_all(input,&header,2,NULL,cancellable,error);
  if(done)
    {
      result->fin = header[0] & 0b10000000;
      result->code = header[0] & 0b00001111;

      gboolean have_mask = header[1] & 0b10000000;
      guint8 header_size = header[1] & 0b01111111;

      if(header_size < 126)
	{
	  result->count = header_size;
	}
      else if(header_size == 126)
	{
	  done = g_input_stream_read_all(input,&(result->count),2,NULL,cancellable,error);
	  result->count = GUINT16_FROM_BE((guint16)(result->count));
	}
      else if(header_size == 127)
	{
	  done = g_input_stream_read_all(input,&(result->count),8,NULL,cancellable,error);
	  result->count = GUINT64_FROM_BE((guint64)(result->count));
	}

      if(done)
	{
	  if((result->count > 0) && (result->count <= max_frame_size))
	    {
	      guint8 mask[4] = {0,};
	      if(have_mask)
		{
		  done = g_input_stream_read_all(input,mask,4,NULL,cancellable,error);
		  result->mask = *((guint32 *)(mask));
		}

	      result->buffer = g_new0(guint8,(result->count) + 1);
	      done = g_input_stream_read_all(input,result->buffer,result->count,NULL,cancellable,error);
	      if(have_mask)
		{
		  for(guint i = 0;i<result->count;i++)
		    (result->buffer)[i] ^= mask[i % 4];
		}
	    }
	  else if(result->code == G_WEBSOCKET_CODEOP_BINARY || result->code == G_WEBSOCKET_CODEOP_TEXT)
	    {
		//TODO: bad size datagram and free it
	      done = FALSE;

	    }
	  else
	    {
	      *datagram = result;
	    }
	}
      else
	{
	  //TODO: free
	}
    }
  return done;
}

static gboolean
_g_websocket_write(
    GOutputStream * output,
    GWebSocketDatagram * datagram,
    GCancellable * cancellable,
    GError ** error)
{
  guint8 header[16];
  size_t p;

  gboolean mid_header = datagram->count > 125 && datagram->count <= 65535;
  gboolean long_header = datagram->count > 65535;

  /* NB. big-endian spec => bit 0 == MSB */
  header[0] = (datagram->fin ? 0b10000000:0b00000000)|(((guint8)datagram->code) & 0b00001111);
  header[1] = (datagram->mask ? 0b10000000:0b00000000)|(((guint8)(mid_header ? 126 : long_header ? 127 : datagram->count)) & 0b01111111);

  p = 2;
  if (mid_header)
    {
      *(guint16 *)(header + p) = GUINT16_TO_BE( (guint16)datagram->count );
      p += 2;
    }
  else if (long_header)
    {
      *(guint64 *)(header + p) = GUINT64_TO_BE( datagram->count );
      p += 8;
    }
  guint8 *
  masked_buf = NULL;
  if(datagram->mask)
    {
      guint8 * mask_val = ((guint8*)&(datagram->mask));
      *((guint8*)(header + p)) = mask_val[0];
      *((guint8*)(header + p + 1)) = mask_val[1];
      *((guint8*)(header + p + 2)) = mask_val[2];
      *((guint8*)(header + p + 3)) = mask_val[3];

      p += 4;
      masked_buf = g_malloc(datagram->count);

      for(guint32 index = 0;index < datagram->count;index++)
	masked_buf[index] = datagram->buffer[index] ^ mask_val[index % 4];
    }

  gboolean done = FALSE;
  done = g_output_stream_write_all (output, header, p, NULL, cancellable,error);
  if(!(datagram->mask))
    {
      done = done && g_output_stream_write_all (output, datagram->buffer, datagram->count, NULL, cancellable, error);
    }
  else
    {
      done = done && g_output_stream_write_all (output, masked_buf, datagram->count, NULL, cancellable, error);
      g_free(masked_buf);
    }
  return done;
}

gboolean
_g_websocket_complete(GWebSocket * socket,GSocketConnection * connection)
{
  gboolean done = FALSE;
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);
  priv->connection = connection;
  GInputStream * input = g_io_stream_get_input_stream(G_IO_STREAM(priv->connection));
  GOutputStream * output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
  g_socket_set_keepalive(g_socket_connection_get_socket(priv->connection),TRUE);

  HttpRequest * request = http_request_new(HTTP_REQUEST_METHOD_GET,"",1.1);
  HttpResponse* response = http_response_new(HTTP_RESPONSE_SWITCHING_PROTOCOLS,1.1);
  GDataInputStream * data_stream = http_data_input_stream(input,NULL,NULL,NULL);

  if(data_stream)
    {
      http_package_read_from_stream(HTTP_PACKAGE(request),data_stream,NULL,NULL,NULL);
      g_object_unref(data_stream);
      const gchar * key = NULL, *origin = NULL;
      gboolean  is_valid = ((g_ascii_strcasecmp(http_package_get_string(HTTP_PACKAGE(request),"Upgrade",NULL),"websocket") == 0))
			      && (origin = http_package_get_string(HTTP_PACKAGE(request),"Origin",NULL))
			      && (key = http_package_get_string(HTTP_PACKAGE(request),"Sec-WebSocket-Key",NULL));
      if(is_valid)
	{
	  gchar *  handshake = g_websocket_generate_handshake(key);
	  http_package_set_string(HTTP_PACKAGE(response),"Upgrade","websocket",-1);
	  http_package_set_string(HTTP_PACKAGE(response),"Connection","upgrade",-1);
	  http_package_set_string(HTTP_PACKAGE(response),"Sec-WebSocket-Accept",handshake,-1);
	  http_package_set_string(HTTP_PACKAGE(response),"Sec-WebSocket-Origin",origin,-1);
	  http_package_write_to_stream(HTTP_PACKAGE(response),output,NULL,NULL,NULL);
	  g_free(handshake);
	  done = TRUE;
	}
      else
	{
	  http_response_set_code(response,HTTP_RESPONSE_BAD_REQUEST);
	  http_package_write_to_stream(HTTP_PACKAGE(response),output,NULL,NULL,NULL);
	  g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
	  g_clear_object(&(priv->connection));
	}
    }
  else
    {
      g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
      g_clear_object(&(priv->connection));
    }

  g_object_unref(request);
  g_object_unref(response);
  return done;
}

static gboolean
_g_websocket_complete_client(GWebSocket * socket,const gchar * hostname,const gchar * query)
{
  gboolean done = FALSE;
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);
  GInputStream * input = g_io_stream_get_input_stream(G_IO_STREAM(priv->connection));
  GOutputStream * output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
  priv->use_mask = TRUE;

  g_socket_set_keepalive(g_socket_connection_get_socket(priv->connection),TRUE);

  HttpRequest * request = http_request_new(HTTP_REQUEST_METHOD_GET,"",1.1);
  HttpResponse* response = http_response_new(HTTP_RESPONSE_SWITCHING_PROTOCOLS,1.1);
  gchar   * key = g_websocket_generate_key(),
	  * handshake = g_websocket_generate_handshake(key);

  http_request_set_query(request,query);
  http_request_set_method(request,HTTP_REQUEST_METHOD_GET);

  http_package_set_string(HTTP_PACKAGE(request),"Upgrade","websocket",-1);
  http_package_set_string(HTTP_PACKAGE(request),"Origin",hostname,-1);
  http_package_set_string(HTTP_PACKAGE(request),"Sec-WebSocket-Key",key,-1);

  if(http_package_write_to_stream(HTTP_PACKAGE(request),output,NULL,NULL,NULL))
  {
    GDataInputStream * data_stream = http_data_input_stream(input,NULL,NULL,NULL);
    if(data_stream)
      {
	http_package_read_from_stream(HTTP_PACKAGE(response),data_stream,NULL,NULL,NULL);
	g_input_stream_close(G_INPUT_STREAM(data_stream),NULL,NULL);
	g_object_unref(data_stream);
	if((http_response_get_code(response) == HTTP_RESPONSE_SWITCHING_PROTOCOLS) && (g_strcmp0(handshake,http_package_get_string(HTTP_PACKAGE(response),"Sec-WebSocket-Accept",NULL)) == 0))
	  {
	    _g_websocket_start(socket);
	    done = TRUE;
	  }
	else
	  {
	    g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
	    g_clear_object(&(priv->connection));
	    done = FALSE;
	  }
      }
    else
      {
	g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
	g_clear_object(&(priv->connection));
	done = FALSE;
      }
  }

  g_free(key);
  g_free(handshake);

  g_object_unref(request);
  g_object_unref(response);

  return done;
}

GWebSocket *
g_websocket_new()
{
  GWebSocket * self = G_WEBSOCKET(g_object_new(G_TYPE_WEBSOCKET,NULL));
  return self;
}


gboolean
g_websocket_is_connected(
    GWebSocket * socket
    )
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);
  if(priv->connection)
    {
      return g_socket_connection_is_connected(priv->connection);
    }
  else
    {
      return FALSE;
    }
}

gboolean
g_websocket_connect(
    GWebSocket * socket,
    const gchar * uri,
    guint16 default_port,
    GCancellable * cancellable,
    GError ** error)
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);
  GSocketClient * client = g_socket_client_new();
  gchar *scheme = NULL, *hostname = NULL, *query = NULL;
  gboolean done = FALSE;
  g_websocket_uri_parse(uri,&scheme,&hostname,&query);
  priv->connection = g_socket_client_connect_to_host(client,hostname,default_port,cancellable,error);
  if(priv->connection)
    {
      done = _g_websocket_complete_client(socket,hostname,query);
    }
  g_free(scheme);
  g_free(hostname);
  g_free(query);
  g_object_unref(client);
  return done;
}

gboolean
g_websocket_send(
    GWebSocket * socket,
    GWebSocketMessage * message,
    GCancellable * cancellable,
    GError ** error
    )
{
  GWebSocketClass * klass = G_WEBSOCKET_GET_CLASS(socket);
  return klass->send(socket,message,cancellable,error);
}

gboolean
g_websocket_send_text(
    GWebSocket * socket,
    const gchar * text,
    gssize length,
    GCancellable * cancellable,
    GError ** error
    )
{
  GWebSocketMessage * msg = g_websocket_message_new_text(text,length);
  gboolean done = g_websocket_send(socket,msg,cancellable,error);
  g_websocket_message_free(msg);
  return done;
}

gboolean
g_websocket_send_data(
    GWebSocket * socket,
    const guint8 * data,
    gssize length,
    GCancellable * cancellable,
    GError ** error
    )
{
  GWebSocketMessage * msg = g_websocket_message_new_data(data,length);
  gboolean done = g_websocket_send(socket,msg,cancellable,error);
  g_websocket_message_free(msg);
  return done;
}

void
g_websocket_close(
    GWebSocket * socket,
    GError ** error
    )
{
  GWebSocketPrivate * priv = g_websocket_get_instance_private(socket);
  if(g_websocket_is_connected(socket))
    {
      GWebSocketDatagram * datagram = g_new0(GWebSocketDatagram,1);
      datagram->code = G_WEBSOCKET_CODEOP_CLOSE;
      datagram->buffer = NULL;
      datagram->count = 0;
      datagram->fin = TRUE;
      datagram->mask = 0;
      _g_websocket_write(g_io_stream_get_output_stream(G_IO_STREAM(priv->connection)),datagram,NULL,error);
      g_free(datagram);
      _g_websocket_stop(socket);
    }
}
