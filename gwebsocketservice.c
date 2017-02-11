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

#include "gwebsocketservice.h"

typedef struct _GWebSocketServicePrivate GWebSocketServicePrivate;

struct _GWebSocketServicePrivate
{
  GMutex  mutex_clients;
  GList * clients;
};

G_DEFINE_TYPE_WITH_PRIVATE(GWebSocketService,g_websocket_service,G_TYPE_THREADED_SOCKET_SERVICE)

void		_g_websocket_service_dispose(GObject * object);

gboolean	_g_websocket_complete(GWebSocket * socket,GSocketConnection * connection);

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
	N_SIGNALS
};

//static GParamSpec * 	g_websocket_properties[N_PROPERTIES] = { NULL, };

static gint		g_websocket_service_signals[N_SIGNALS];

static void
g_websocket_service_init(GWebSocketService * self)
{
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(self);
  g_signal_connect(self,"run",G_CALLBACK(_g_websocket_service_run),NULL);
  g_mutex_init(&(priv->mutex_clients));
}

static void
g_websocket_service_class_init(GWebSocketServiceClass * klass)
{
  G_OBJECT_CLASS(klass)->dispose = _g_websocket_service_dispose;

  const GType message_params[2] = {G_TYPE_OBJECT,G_TYPE_POINTER};
  const GType socket_params[2] = {G_TYPE_OBJECT};

  g_websocket_service_signals[SIGNAL_CONNECTED] =
     g_signal_newv ("connected",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
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
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
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
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      NULL /* closure */,
      NULL /* accumulator */,
      NULL /* accumulator data */,
      NULL /* C marshaller */,
      G_TYPE_NONE /* return_type */,
      1     /* n_params */,
      (GType*)socket_params  /* param_types */);
}

static gboolean	_g_websocket_service_run (
		  GThreadedSocketService *service,
		  GSocketConnection      *connection,
		  GObject                *source_object)
{
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  GWebSocket * socket = g_websocket_new();
  if(_g_websocket_complete(socket,connection))
    {
      g_mutex_lock(&(priv->mutex_clients));
      priv->clients = g_list_append(priv->clients,g_object_ref(socket));
      g_signal_connect(G_OBJECT(socket),"message",G_CALLBACK(_g_websocket_service_client_message),service);
      g_signal_connect(G_OBJECT(socket),"closed",G_CALLBACK(_g_websocket_service_client_closed),service);
      g_mutex_unlock(&(priv->mutex_clients));
      g_signal_emit (G_WEBSOCKET_SERVICE(service), g_websocket_service_signals[SIGNAL_CONNECTED],0,socket);
    }
  g_object_unref(socket);
  return FALSE;
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
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  g_signal_emit (service, g_websocket_service_signals[SIGNAL_CLOSED],0,socket);
  g_mutex_lock(&(priv->mutex_clients));
  priv->clients = g_list_remove(priv->clients,socket);
  g_object_unref(socket);
  g_mutex_unlock(&(priv->mutex_clients));
}

GWebSocketService *
g_websocket_service_new(int max_threads)
{
  return G_WEBSOCKET_SERVICE(g_object_new(G_TYPE_WEBSOCKET_SERVICE,"max-threads",max_threads,NULL));
}

void
g_websocket_service_broadcast(GWebSocketService * service,GWebSocketBroadCastFunc func,gpointer data)
{
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  g_mutex_lock(&(priv->mutex_clients));
  for(GList * iter = g_list_first(priv->clients);iter;iter = g_list_next(iter))
    {
      func(service,G_WEBSOCKET(iter->data),data);
    }
  g_mutex_unlock(&(priv->mutex_clients));
}

gsize
g_websocket_service_get_count(GWebSocketService * service)
{
  GWebSocketServicePrivate * priv = g_websocket_service_get_instance_private(G_WEBSOCKET_SERVICE(service));
  gsize count = 0;
  g_mutex_lock(&(priv->mutex_clients));
  count = g_list_length(priv->clients);
  g_mutex_unlock(&(priv->mutex_clients));
  return count;
}

void
_g_websocket_service_dispose(GObject * object)
{

}