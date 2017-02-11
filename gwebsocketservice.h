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

#ifndef GWEBSOCKETSERVICE_H_
#define GWEBSOCKETSERVICE_H_

#include "gwebsocket.h"


#define G_TYPE_WEBSOCKET_SERVICE	(g_websocket_service_get_type())
G_DECLARE_DERIVABLE_TYPE(GWebSocketService,g_websocket_service,G,WEBSOCKET_SERVICE,GThreadedSocketService)

typedef void (*GWebSocketBroadCastFunc)(GWebSocketService * service,GWebSocket * socket,gpointer data);

struct _GWebSocketServiceClass
{
  GThreadedSocketServiceClass parent_class;
};

GType			g_websocket_service_get_type(void);

GWebSocketService *	g_websocket_service_new(int max_threads);

void			g_websocket_service_broadcast(GWebSocketService * service,GWebSocketBroadCastFunc func,gpointer data);

gsize			g_websocket_service_get_count(GWebSocketService * service);

#endif /* GWEBSOCKETSERVICE_H_ */
