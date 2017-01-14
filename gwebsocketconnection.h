#ifndef _G_WEBSOCKET_CONNECTION_
#define _G_WEBSOCKET_CONNECTION_

#include "httprequest.h"
#include "httpresponse.h"

typedef struct _GWebSocketMessage GWebSocketMessage;

#define G_WEBSOCKET_TYPE_CONNECTION	(g_websocket_connection_get_type())

G_DECLARE_DERIVABLE_TYPE(GWebSocketConnection,g_websocket_connection,G_WEBSOCKET,CONNECTION,GObject)

typedef enum
{
	G_WEBSOCKET_MESSAGE_TEXT,
	G_WEBSOCKET_MESSAGE_BINARY
}GWebSocketMessageType;


typedef enum
{
    G_WEBSOCKET_SITE_SERVER,
    G_WEBSOCKET_SITE_CLIENT
}GWebSocketSite;

struct _GWebSocketMessage
{
    GWebSocketMessageType type;
    guint8 * data;
    gsize length;
};

struct _GWebSocketConnectionClass
{
	GObjectClass parent_class;
	/* default handlers */
	void 		(*message)(
				GWebSocketConnection * connection,
				GWebSocketMessage * message);

    void        (*connected)(
                GWebSocketConnection * connection);

	void		(*closed)(
				GWebSocketConnection * connection);
};

G_BEGIN_DECLS

GType			        g_websocket_connection_get_type(void);

GWebSocketConnection *	g_websocket_connection_new();

gboolean                g_websocket_connection_open(
                        GWebSocketConnection * connection,
                        GSocketAddress * address,
                        GCancellable * cancellable,
                        GError ** error);

gboolean                g_websocket_connection_open_url(
                        GWebSocketConnection * connection,
                        const gchar * url,
                        GCancellable * cancellable,
                        GError ** error);

gboolean                g_websocket_connection_is_open(
                        GWebSocketConnection * connection
                        );

GWebSocketConnection *	g_websocket_connection_factory_create_connection(
                        GWebSocketSite site,
						GSocketConnection * connection
						);

gboolean		        g_websocket_connection_send(
						GWebSocketConnection * object,
						GWebSocketMessage * message,
						GCancellable * cancellable,
						GError ** error);

gboolean                g_websocket_connection_reciv(
                        GWebSocketConnection * object,
                        GWebSocketMessage ** message,
                        GCancellable * cancellable,
                        GError ** error);

void			        g_websocket_connection_close(
						GWebSocketConnection * object);

GWebSocketMessage *     g_websocket_message_new(
                        GWebSocketMessageType type,
                        gpointer data,
                        gsize length);

GWebSocketMessage *     g_websocket_message_text_new(
                        const gchar * text,gssize length);

GWebSocketMessage *     g_websocket_message_binary_new(
                        const guint8 * data,gsize length);

const gchar *           g_websocket_message_get_text(
                        GWebSocketMessage * message);

const guint8 *          g_websocket_message_get_data(
                        GWebSocketMessage * message);

gsize                   g_websocket_message_get_length(
                        GWebSocketMessage * message);

void                    g_websocket_message_free(
                        GWebSocketMessage * message);

G_END_DECLS

#endif
