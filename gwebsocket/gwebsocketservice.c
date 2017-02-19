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

#include "gwebsocketservice.h"

typedef struct _GWebSocketServicePrivate GWebSocketServicePrivate;
typedef struct _GWebSocketServiceIdleData GWebSocketServiceIdleData;

static GMutex g_websocket_service_mutex = G_STATIC_MUTEX_INIT;

struct _GWebSocketServicePrivate
{
  GMutex  mutex_internal;
  GList * clients;
};

struct _GWebSocketServiceIdleData
{
  GWebSocketService * service;
  GWebSocket * socket;
};

G_DEFINE_TYPE_WITH_PRIVATE(GWebSocketService,g_websocket_service,G_TYPE_THREADED_SOCKET_SERVICE)

void		_g_websocket_service_dispose(GObject * object);

gboolean	_g_websocket_complete(
		    GWebSocket * socket,
		    GSocketConnection * connection,
		    HttpRequest *  request,
		    const gchar *  key,
		    const gchar *  origin);

static gboolean	_g_websocket_service_run (
		    GThreadedSocketService *service,
		    GSocketConnection      *connection,
		    GObject                *source_object);


void		_g_websocket_service_client_message(
		    GWebSocket * socket,
		    GWebSocketMessage * message,
		    GWebSocketService * service);

void		_g_websocket_service_client_closed(
		    GWebSocket * socket,
		    GWebSocketService * service);

enum
{
	SIGNAL_CONNECTED = 0,
	SIGNAL_MESSAGE = 1,
	SIGNAL_CLOSED = 2,
	SIGNAL_REQUEST = 3,
	N_SIGNALS
};

//static GParamSpec * 	g_websocket_properties[N_PROPERTIES] = { NULL, };

static gint		g_websocket_service_signals[N_SIGNALS];

static void
g_websocket_service_init(GWebSocketService * self)
{
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(self);
  g_signal_connect(self,"run",G_CALLBACK(_g_websocket_service_run),NULL);
  g_mutex_init(&(priv->mutex_internal));
}

static void
g_websocket_service_class_init(GWebSocketServiceClass * klass)
{
  G_OBJECT_CLASS(klass)->dispose = _g_websocket_service_dispose;

  const GType message_params[2] = {G_TYPE_OBJECT,G_TYPE_POINTER};
  const GType socket_params[1] = {G_TYPE_OBJECT};
  const GType request_params[2] = {G_TYPE_OBJECT,G_TYPE_OBJECT};

  g_websocket_service_signals[SIGNAL_CONNECTED] =
     g_signal_newv ("connected",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      NULL /* closure */,
      NULL /* accumulator */,
      NULL /* accumulator data */,
      NULL /* C marshaller */,
      G_TYPE_NONE /* return_type */,
      1     /* n_params */,
      (GType*)socket_params  /* param_types */);

  g_websocket_service_signals[SIGNAL_MESSAGE] =
     g_signal_newv ("message",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      NULL /* closure */,
      NULL /* accumulator */,
      NULL /* accumulator data */,
      NULL /* C marshaller */,
      G_TYPE_NONE /* return_type */,
      2     /* n_params */,
      (GType*)message_params  /* param_types */);

  g_websocket_service_signals[SIGNAL_CLOSED] =
     g_signal_newv ("closed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      NULL /* closure */,
      NULL /* accumulator */,
      NULL /* accumulator data */,
      NULL /* C marshaller */,
      G_TYPE_NONE /* return_type */,
      1     /* n_params */,
      (GType*)socket_params  /* param_types */);

  g_websocket_service_signals[SIGNAL_REQUEST] =
     g_signal_newv ("request",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      NULL /* closure */,
      NULL /* accumulator */,
      NULL /* accumulator data */,
      NULL /* C marshaller */,
      G_TYPE_NONE /* return_type */,
      2     /* n_params */,
      (GType*)request_params  /* param_types */);
}

static gboolean	_g_websocket_service_run (
		  GThreadedSocketService *service,
		  GSocketConnection      *connection,
		  GObject                *source_object)
{
  g_mutex_lock(&g_websocket_service_mutex);
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  g_mutex_unlock(&g_websocket_service_mutex);
  GInputStream * input = g_io_stream_get_input_stream(G_IO_STREAM(connection));
  HttpRequest * request = http_request_new(HTTP_REQUEST_METHOD_GET,"",1.1);
  g_socket_set_timeout(g_socket_connection_get_socket(connection),10);
  GDataInputStream * data_stream = http_data_input_stream(input,NULL,NULL,NULL);

  g_socket_set_keepalive(g_socket_connection_get_socket(connection),TRUE);

  if(data_stream)
    {
      http_package_read_from_stream(HTTP_PACKAGE(request),data_stream,NULL,NULL,NULL);
      g_object_unref(data_stream);
      const gchar * key = NULL, *origin = NULL;
      gboolean  is_websocket = ((g_ascii_strcasecmp(http_package_get_string(HTTP_PACKAGE(request),"Upgrade",NULL),"websocket") == 0))
			    && (origin = http_package_get_string(HTTP_PACKAGE(request),"Origin",NULL))
			    && (key = http_package_get_string(HTTP_PACKAGE(request),"Sec-WebSocket-Key",NULL));
      if(is_websocket)
	{
	  g_socket_set_timeout(g_socket_connection_get_socket(connection),0);
	  GWebSocket * socket = g_websocket_new();
	   if(_g_websocket_complete(socket,connection,request,key,origin))
	     {
	       g_mutex_lock(&(priv->mutex_internal));
	       priv->clients = g_list_append(priv->clients,g_object_ref(socket));
	       g_signal_connect(G_OBJECT(socket),"message",G_CALLBACK(_g_websocket_service_client_message),service);
	       g_signal_connect(G_OBJECT(socket),"closed",G_CALLBACK(_g_websocket_service_client_closed),service);
	       g_mutex_unlock(&(priv->mutex_internal));
	       g_signal_emit (G_WEBSOCKET_SERVICE(service), g_websocket_service_signals[SIGNAL_CONNECTED],0,socket);
	     }
	   g_object_unref(socket);
	}
      else
      {
	g_mutex_lock(&(priv->mutex_internal));
	g_signal_emit(service,g_websocket_service_signals[SIGNAL_REQUEST],0,request,connection);
	g_mutex_unlock(&(priv->mutex_internal));
	if(g_socket_connection_is_connected(connection))
	  g_io_stream_close(G_IO_STREAM(connection),NULL,NULL);
      }
    }
  else
    {
      g_io_stream_close(G_IO_STREAM(connection),NULL,NULL);
    }
  g_object_unref(request);
  return FALSE;
}


gboolean
_g_websocket_service_client_closed_idle(gpointer data)
{
  GWebSocketServiceIdleData * idle_data = (GWebSocketServiceIdleData*)data;
  g_mutex_lock(&g_websocket_service_mutex);
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(idle_data->service));
  g_mutex_unlock(&g_websocket_service_mutex);
  gboolean result = G_SOURCE_CONTINUE;
  if(g_mutex_trylock(&(priv->mutex_internal)))
    {
      g_signal_emit (idle_data->service, g_websocket_service_signals[SIGNAL_CLOSED],0,idle_data->socket);
      priv->clients = g_list_remove(priv->clients,idle_data->socket);
      g_object_unref(idle_data->socket);
      g_mutex_unlock(&(priv->mutex_internal));
      result = G_SOURCE_REMOVE;
    }
  return result;
}


void
_g_websocket_service_client_message(
		  GWebSocket * socket,
		  GWebSocketMessage * message,
		  GWebSocketService * service)
{
  g_signal_emit (service, g_websocket_service_signals[SIGNAL_MESSAGE],0,socket,message);
}


void
_g_websocket_service_client_closed(
		  GWebSocket * socket,
		  GWebSocketService * service)
{
  GWebSocketServiceIdleData * data = g_new0(GWebSocketServiceIdleData,1);
  data->service = service;
  data->socket = socket;
  g_idle_add(_g_websocket_service_client_closed_idle,data);
}

GWebSocketService *
g_websocket_service_new(int max_threads)
{
  return G_WEBSOCKET_SERVICE(g_object_new(G_TYPE_WEBSOCKET_SERVICE,"max-threads",max_threads,NULL));
}

void
g_websocket_service_broadcast(GWebSocketService * service,GWebSocketBroadCastFunc func,gpointer data)
{
  g_mutex_lock(&g_websocket_service_mutex);
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  g_mutex_unlock(&g_websocket_service_mutex);
  g_mutex_lock(&(priv->mutex_internal));
  for(GList * iter = g_list_first(priv->clients);iter;iter = g_list_next(iter))
    {
      func(service,G_WEBSOCKET(iter->data),data);
    }
  g_mutex_unlock(&(priv->mutex_internal));
}

gsize
g_websocket_service_get_count(GWebSocketService * service)
{
  g_mutex_lock(&g_websocket_service_mutex);
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  g_mutex_unlock(&g_websocket_service_mutex);
  gsize count = 0;
  g_mutex_lock(&(priv->mutex_internal));
  count = g_list_length(priv->clients);
  g_mutex_unlock(&(priv->mutex_internal));
  return count;
}

void
_g_websocket_service_dispose(GObject * object)
{

}
