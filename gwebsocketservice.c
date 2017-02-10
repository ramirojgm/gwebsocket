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
  GList * clients;
};

G_DEFINE_TYPE_WITH_PRIVATE(GWebSocketService,g_websocket_service,G_TYPE_THREADED_SOCKET_SERVICE)

static void
g_websocket_service_init(GWebSocketService * self)
{

}

static void
g_websocket_service_class_init(GWebSocketServiceClass * klass)
{

}
