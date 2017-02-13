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

#ifndef GWEBSOCKET_H_
#define GWEBSOCKET_H_

#include "httprequest.h"
#include "httpresponse.h"

typedef enum	_GWebSocketMessageType	GWebSocketMessageType;
typedef struct	_GWebSocketMessage 	GWebSocketMessage;

#define G_TYPE_WEBSOCKET	(g_websocket_get_type())
G_DECLARE_DERIVABLE_TYPE	(GWebSocket,g_websocket,G,WEBSOCKET,GObject)

enum _GWebSocketMessageType
{
  G_WEBSOCKET_MESSAGE_TEXT,
  G_WEBSOCKET_MESSAGE_BINARY
};

struct _GWebSocketClass
{
  GObjectClass 	parent_class;

  /*<override>*/
  gboolean 	(*send)(GWebSocket * socket,GWebSocketMessage * message,GCancellable * cancellable,GError ** error);
};

gboolean	g_websocket_uri_parse(const gchar * uri,gchar ** scheme,gchar ** hostname,gchar ** query);

GType		g_websocket_get_type(void);

gchar *		g_websocketgenerate_handshake(
		    const gchar *key
		    );

gchar *		g_websocket_generate_key();

GWebSocket *	g_websocket_new();


gboolean	g_websocket_is_connected(
		    GWebSocket * socket
		    );

gboolean	g_websocket_connect(
		    GWebSocket * socket,
		    const gchar * uri,
		    guint16 default_port,
		    GCancellable * cancellable,
		    GError ** error);

HttpRequest *	g_websocket_get_request(
		    GWebSocket * socket);

gboolean	g_websocket_send(
		    GWebSocket * socket,
		    GWebSocketMessage * message,
		    GError ** error
		    );

gboolean	g_websocket_send_text(
		    GWebSocket * socket,
		    const gchar * text,
		    gssize length,
		    GError ** error
		    );

gboolean	g_websocket_send_data(
		    GWebSocket * socket,
		    const guint8 * data,
		    gssize length,
		    GError ** error
		    );

void		g_websocket_close(
		    GWebSocket * socket,
		    GError ** error
		    );


GWebSocketMessage *	g_websocket_message_new_text(
			    const gchar * text,
			    gssize length
			    );

GWebSocketMessage *	g_websocket_message_new_data(
			    const guint8 * data,
			    gsize length
			    );

GWebSocketMessageType	g_websocket_message_get_type(
			    GWebSocketMessage * message
			    );

const gchar *		g_websocket_message_get_text(
			    GWebSocketMessage * message
			    );

const guint8 *		g_websocket_message_get_data(
			    GWebSocketMessage * message
			    );

gsize			g_websocket_message_get_length(
			    GWebSocketMessage * message
			    );

void			g_websocket_message_free(
			    GWebSocketMessage * message
			    );

#endif /* GWEBSOCKET_H_ */
