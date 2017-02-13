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

#include <glib.h>
#include <gtk/gtk.h>
#include <gwebsocket/gwebsocketservice.h>

typedef struct _GRemoteCapture GRemoteCapture;

struct _GRemoteCapture
{
  guint8 * data;
  gsize	count;
};

static void	g_remote_capture_request(GWebSocketService * service,HttpRequest * request,GSocketConnection * connection);
static gboolean	g_remote_capture_send(GWebSocketService * service);

gint
main(gint argc,gchar * argv[])
{
  gtk_init(&argc,&argv);
  GWebSocketService * service = g_websocket_service_new(30);
  g_socket_listener_add_inet_port(G_SOCKET_LISTENER(service),8080,G_OBJECT(service),NULL);
  g_signal_connect(service,"request",G_CALLBACK(g_remote_capture_request),NULL);
  g_socket_service_start(G_SOCKET_SERVICE(service));
  g_timeout_add(200,g_remote_capture_send,service);
  gtk_main();
  g_socket_service_stop(G_SOCKET_SERVICE(service));
  return 0;
}



static void
g_remote_capture_request(GWebSocketService * service,
			 HttpRequest * request,
			 GSocketConnection * connection)
{
  const gchar * query = http_request_get_query(request);
  gchar * filename = NULL;
  guint8 * content = NULL;
  gsize    length = 0;
  HttpResponse * response = http_response_new(HTTP_RESPONSE_OK,1.1);
  GOutputStream * output = g_io_stream_get_output_stream(G_IO_STREAM(connection));

  if(g_strcmp0(query,"/") == 0)
    query = "/index.htm";

  filename = g_build_filename("public",query,NULL);
  if(g_file_test(filename,G_FILE_TEST_IS_REGULAR))
    {
      if(!g_file_get_contents(filename,(guchar**)&content,&length,NULL))
	{
	  http_response_set_code(response,HTTP_RESPONSE_INTERNAL_ERROR);
	}
    }
  else
    {
      http_response_set_code(response,HTTP_RESPONSE_NOT_FOUND);
    }

  if(length > 0)
    http_package_set_int(HTTP_PACKAGE(response),"Content-Length",length);
  http_package_write_to_stream(HTTP_PACKAGE(response),output,NULL,NULL,NULL);
  if(content)
      g_output_stream_write_all(output,content,length,NULL,NULL,NULL);
  g_object_unref(response);
  g_free(filename);
  g_free(content);
}

static void
g_remote_capture_broadcast(GWebSocketService * service,GWebSocket * socket,gpointer data)
{
  GRemoteCapture * remote_data = (GRemoteCapture*)data;
  g_websocket_send_data(socket,remote_data->data,remote_data->count,NULL,NULL);
}

static gboolean
g_remote_capture_send(GWebSocketService * service)
{
  GdkWindow * root_window = gdk_screen_get_root_window(gdk_screen_get_default());
  GdkPixbuf * capture = gdk_pixbuf_get_from_window(root_window,0,0,gdk_window_get_width(root_window),gdk_window_get_width(root_window));
  guint8 * 	capture_data = NULL;
  gsize		capture_data_size = 0;
  if(capture)
    {
      if(gdk_pixbuf_save_to_buffer(capture,&capture_data,&capture_data_size,"png",NULL,NULL))
		{
		  GRemoteCapture * data = g_new0(GRemoteCapture,1);
		  data->data = capture_data;
		  data->count = capture_data_size;
		  g_websocket_service_broadcast(service,g_remote_capture_broadcast,data);
		  g_free(data);
		}
    }
  g_free(capture_data);
  g_object_unref(capture);
  return G_SOURCE_CONTINUE;
}
