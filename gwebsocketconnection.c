#include "gwebsocketconnection.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

int clock_gettime(int clk_id, struct timespec *tp);

typedef struct _GWebSocketConnectionPrivate	GWebSocketConnectionPrivate;
typedef struct _GWebSocketDatagram GWebSocketDatagram;

struct _GWebSocketDatagram
{
    GWebSocketConnection *  source;
    GWebSocketCodeOp        op;
    gboolean                fin;
    guint8 *                data;
    guint32                 mask;
    gsize                   size;
};

struct _GWebSocketConnectionPrivate
{
	GSocketConnection * connection;
	GWebSocketSite      site;
	GThread *           recv_thread;
    GCancellable *      recv_thread_cancel;
	GMutex              recv_mutex;
	gboolean            recv_wait;
};

G_DEFINE_TYPE_WITH_PRIVATE(GWebSocketConnection,g_websocket_connection,G_TYPE_OBJECT)

#define G_WEBSOCKET_CONNECTION_KEY_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define G_WEBSOCKET_MASK(site) ((site == G_WEBSOCKET_SITE_CLIENT) ? 0x00000000 : _g_websocket_connection_generate_mask())

typedef enum
{
	G_WEBSOCKET_CODEOP_CONTINUE = 0x0,
	G_WEBSOCKET_CODEOP_TEXT = 0x1,
	G_WEBSOCKET_CODEOP_BINARY = 0x2,
	G_WEBSOCKET_CODEOP_CLOSE = 0x8,
	G_WEBSOCKET_CODEOP_PING = 0x9,
	G_WEBSOCKET_CODEOP_PONG = 0xA
}GWebSocketCodeOp;

guint32
_g_websocket_connection_generate_mask()
{
    return 0xAABBCCDD;
}

gchar *
_g_websocket_connection_generate_handshake(const gchar *key)
{
  gsize handshake_len = 20;
  guchar handshake[21];
  GChecksum *checksum;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, (guchar *)key, -1);
  g_checksum_update (checksum, (guchar *)G_WEBSOCKET_CONNECTION_KEY_MAGIC, -1);

  g_checksum_get_digest (checksum, handshake, &handshake_len);
  g_checksum_free (checksum);

  g_assert (handshake_len == 20);

  return g_base64_encode (handshake, handshake_len);
}

gchar *
_g_websocket_connection_generate_key()
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

gboolean
_g_websocket_connection_read(
		GInputStream * input,
		GWebSocketDatagram ** datagram,
		GCancellable * cancellable,
		GError ** error)
{

	const guint64 max_frame_size = 15728640L; //-> 15MB
	gboolean      done = FALSE;

	gboolean fin = TRUE;
	GWebSocketCodeOp code = G_WEBSOCKET_CODEOP_CONTINUE;
	guint8 * buf = NULL;
	gsize count = 0;

	guint8 header[2] = {0,};

	done = g_input_stream_read(input,&header,2,cancellable,error) == 2;
	if(done)
	{
		fin = header[0] & 0b10000000;
		gboolean have_mask = header[1] & 0b10000000;
		code = header[0] & 0b00001111;
		guint8 header_size = header[1] & 0b01111111;
		if(header_size < 126)
			count = header_size;
		else if(header_size == 126)
		{
			done = g_input_stream_read(input,&count,2,cancellable,error) == 2;
			count = GUINT16_FROM_BE(count);
		}
		else if(header_size == 127)
		{
			done = g_input_stream_read(input,&count,8,cancellable,error) == 8;
			count = GUINT64_FROM_BE(*((guint64*)count));
		}
		if((done) && (count > 0) && (count <= max_frame_size))
		{
			guint8 mask[4] = {0,};
			if(have_mask)
				done = g_input_stream_read(input,mask,4,cancellable,error) == 4;
			buf = g_new0(guint8,count + 1);
			done = g_input_stream_read(input,buf,count,cancellable,error) == count;
			if(have_mask)
			{
				for(int i = 0;i< count;i++)
					buf[i] ^= mask[i % 4];
			}
		}
		else
		{
		    if(buf)
                g_free(buf);
		    if(error)
                *error = g_error_new(g_quark_from_static_string("g-websocket-error-quark"),0,"bad frame size (%d)",(int)*count);
			done = FALSE;
		}
	}
	if(done)
    {
        *datagram = g_new0(GWebSocketDatagram,1);
        *datagram->fin = fin;
        *datagram->op = code;
        *datagram->size = count;
        *datagram->data = buf;
    }
	return done;
}

gboolean
_g_websocket_connection_write(
		GOutputStream * output,
		GWebSocketDatagram * datagram,
		GCancellable * cancellable,
		GError ** error)
{
	guint8 header[16];
	size_t p;

	gboolean mid_header = datagram->size > 125 && datagram->size <= 65535;
	gboolean long_header = datagram->size > 65535;

	/* NB. big-endian spec => bit 0 == MSB */
	header[0] = (datagram->fin ? 0b10000000:0b00000000)|(((guint8)datagram->op) & 0b00001111);
	header[1] = (datagram->mask ? 0b10000000:0b00000000)|(((guint8)(mid_header ? 126 : long_header ? 127 : datagram->size)) & 0b01111111);
	p = 2;
	if (mid_header)
	{
	  *(guint16 *)(header + p) = GUINT16_TO_BE( (guint16)datagram->size );
	  p += 2;
	}
	else if (long_header)
	{
	  *(guint64 *)(header + p) = GUINT64_TO_BE( datagram->size);
	  p += 8;
	}
	guint8 *
	masked_buf = NULL;
	if(datagram->mask)
	{
		guint8 * mask_val = ((guint8*)&(datagram->mask);
		*((guint8*)(header + p)) = mask_val[0];
		*((guint8*)(header + p + 1)) = mask_val[1];
		*((guint8*)(header + p + 2)) = mask_val[2];
		*((guint8*)(header + p + 3)) = mask_val[3];

		p += 4;
		masked_buf = g_malloc(datagram->size);

		gint32 index;
		for(index = 0;index < datagram->size;index++)
			masked_buf[index] = datagram->data[index] ^ mask_val[index % 4];
	}
	gboolean done = g_output_stream_write (output, header, p, cancellable,error) == p;
	if(!datagram->mask)
		done = done && g_output_stream_write(output, datagram->data, datagram->size,cancellable, error) == datagram->size ;
	else
	{
		done = done && g_output_stream_write(output, masked_buf, datagram->size, cancellable, error) == datagram->size;
		g_free(masked_buf);
	}
	return done;
}

typedef struct
{
	GWebSocketConnection * connection;
	GWebSocketCodeOp    code;
	guint8 *            data;
	gsize               length;
}GWebSocketConnectionMessage;

enum
{
	PROP_CONNECTION = 1,
	PROP_SITE,
	N_PROPERTIES
};

enum
{
	SIGNAL_MESSAGE = 0,
	SIGNAL_CONNECTED = 1,
	SIGNAL_CLOSED = 2,
	N_SIGNALS
};

static GParamSpec *
g_websocket_connection_properties[N_PROPERTIES] = { NULL, };

int
g_websocket_connection_signals[N_SIGNALS];

gboolean
_g_websocket_connection_catch_message(gpointer data)
{
	GWebSocketConnectionMessage
	* message = (GWebSocketConnectionMessage*)(data);

	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(message->connection);

	GOutputStream
	* output = NULL;

	if(priv->connection)
		output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));

	switch(message->code)
	{
	case G_WEBSOCKET_CODEOP_CLOSE:
		g_websocket_connection_close(message->connection);
		break;
	case G_WEBSOCKET_CODEOP_TEXT:
	case G_WEBSOCKET_CODEOP_BINARY:
        GWebSocketMessage * msg = g_websocket_message_new((message->code == G_WEBSOCKET_CODEOP_BINARY) ? G_WEBSOCKET_MESSAGE_BINARY : G_WEBSOCKET_MESSAGE_TEXT, message->data,message->length);
		g_signal_emit (message->connection, g_websocket_connection_signals[SIGNAL_MESSAGE],0,msg);
		g_websocket_message_free(msg);
		break;
	case G_WEBSOCKET_CODEOP_CONTINUE:
		break;
	case G_WEBSOCKET_CODEOP_PING:
	    //TODO: Add generate random encrypt mask
		if(output){
            g_websocket_connection_write_frame(output,TRUE,G_WEBSOCKET_CODEOP_PONG,G_WEBSOCKET_MASK(priv->site),(guint8*)"Hey! PONG",9,NULL,NULL);
		}
		break;
	default:
		break;
	}

	g_free(message->data);
	g_free(message);
	return G_SOURCE_REMOVE;
}

gboolean
g_websocket_connection_event_close(gpointer data)
{
	g_websocket_connection_close(G_WEBSOCKET_CONNECTION(data));
	return FALSE;
}

static gpointer
g_websocket_connection_event_thread(gpointer ws)
{
	GWebSocketConnection
	* self = G_WEBSOCKET_CONNECTION(ws);

	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(self);

	GCancellable * cancellable = priv->recv_thread_cancel;

	GInputStream
	* input = g_io_stream_get_input_stream(G_IO_STREAM(priv->connection));

	GWebSocketCodeOp 	codeop;
	guint8 * 			data = NULL;
	gsize 				data_size = 0;
	gboolean			fin = TRUE;

	while(g_websocket_connection_read_frame(input,&fin,&codeop,&data,&data_size,cancellable,NULL))
	{
		GWebSocketConnectionMessage
		* msg = g_new0(GWebSocketConnectionMessage,1);
		msg->code = codeop;
		msg->connection = self;
		msg->data = data;
		msg->data_size = data_size;
		g_mutex_lock(&(priv->recv_mutex));
		priv->recv_wait = FALSE;
		g_mutex_unlock(&(priv->recv_mutex));
		g_idle_add((GSourceFunc)g_websocket_connection_catch_message,msg);
	}
	g_idle_add((GSourceFunc)g_websocket_connection_event_close,self);
	return NULL;
}

gboolean
g_websocket_connection_send_ping(GWebSocketConnection * connection)
{

	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(connection);
	GOutputStream
	* output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
	g_output_stream_flush(output,NULL,NULL);
	gboolean done = g_websocket_connection_write_frame(output,TRUE,G_WEBSOCKET_CODEOP_PING,priv->incoming ? 0x0:G_WEBSOCKET_MASK,(guint8*)"Hey! Ping",9,priv->cancellable,NULL);
	if(!done){
		priv->ping_timeout = 0;
		g_websocket_connection_close(connection);
	}
	return done;
}

static void
g_websocket_connection_constructed(GObject * self)
{
	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(G_WEBSOCKET_CONNECTION(self));
	g_socket_set_keepalive(g_socket_connection_get_socket(priv->connection),TRUE);
	GInputStream
	* input = g_io_stream_get_input_stream(G_IO_STREAM(priv->connection));
	GOutputStream
	* output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
	HttpRequest
	* request = http_request_new(HTTP_REQUEST_METHOD_GET,"",1.1);
	HttpResponse
	* response = http_response_new(HTTP_RESPONSE_SWITCHING_PROTOCOLS,1.1);
	if(priv->incoming)
	{
		GDataInputStream
		* data_stream = http_data_input_stream(input,NULL,NULL,NULL);
		if(data_stream)
		{
			http_package_read_from_stream(HTTP_PACKAGE(request),data_stream,NULL,NULL,NULL);
			g_object_unref(data_stream);
			const gchar
			*key = NULL, *origin = NULL;
			gboolean
			is_valid = ((g_ascii_strcasecmp(http_package_get_string(HTTP_PACKAGE(request),"Upgrade",NULL),"websocket") == 0))
						&& (origin = http_package_get_string(HTTP_PACKAGE(request),"Origin",NULL))
						&& (key = http_package_get_string(HTTP_PACKAGE(request),"Sec-WebSocket-Key",NULL));
			if(is_valid)
			{
				gchar *
				handshake = g_websocket_connection_generate_handshake(key);
				http_package_set_string(HTTP_PACKAGE(response),"Upgrade","websocket",-1);
				http_package_set_string(HTTP_PACKAGE(response),"Connection","upgrade",-1);
				http_package_set_string(HTTP_PACKAGE(response),"Sec-WebSocket-Accept",handshake,-1);
				http_package_set_string(HTTP_PACKAGE(response),"Sec-WebSocket-Origin",origin,-1);
				http_package_write_to_stream(HTTP_PACKAGE(response),output,NULL,NULL,NULL);
				priv->cancellable = g_cancellable_new();
				priv->ping_timeout = g_timeout_add(5000,(GSourceFunc)g_websocket_connection_send_ping,(gpointer)self);
				priv->recv_thread = g_thread_new("websocket-reciv-thread",g_websocket_connection_event_thread,self);
				g_free(handshake);
			}
			else
			{
				http_response_set_code(response,HTTP_RESPONSE_BAD_REQUEST);
				http_package_write_to_stream(HTTP_PACKAGE(response),output,NULL,NULL,NULL);
				g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
			}
		}
		else
			g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
	}
	else
	{
		gchar 	* key = g_websocket_connection_generate_key(),
			* handshake = g_websocket_connection_generate_handshake(key);
		http_request_set_query(request,priv->query);
		http_request_set_method(request,HTTP_REQUEST_METHOD_GET);
		http_package_set_string(HTTP_PACKAGE(request),"Upgrade","websocket",-1);
		http_package_set_string(HTTP_PACKAGE(request),"Origin",priv->hostname,-1);
		http_package_set_string(HTTP_PACKAGE(request),"Sec-WebSocket-Key",key,-1);
		if(http_package_write_to_stream(HTTP_PACKAGE(request),output,NULL,NULL,NULL))
		{
			GDataInputStream
			* data_stream = http_data_input_stream(input,NULL,NULL,NULL);
			if(data_stream)
			{
				http_package_read_from_stream(HTTP_PACKAGE(response),data_stream,NULL,NULL,NULL);
				if((http_response_get_code(response) == HTTP_RESPONSE_SWITCHING_PROTOCOLS) && (g_strcmp0(handshake,http_package_get_string(HTTP_PACKAGE(response),"Sec-WebSocket-Accept",NULL)) == 0))
				{
					priv->cancellable = g_cancellable_new();
					priv->recv_thread = g_thread_new("websocket-reciv-thread",g_websocket_connection_event_thread,self);
				}
				else
					g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
			}
			else
				g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
		}

		g_free(key);
		g_free(handshake);

	}
	g_object_unref(request);
	g_object_unref(response);
}

static void
g_websocket_connection_init(GWebSocketConnection * self)
{
	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(self);
	priv->connection = NULL;
	priv->site = G_WEBSOCKET_SITE_CLIENT;
	priv->recv_thread = NULL;
	priv->recv_thread_cancel = NULL;
	priv->recv_wait = FALSE;
	g_mutex_init(&(priv->recv_mutex));
}

static void
g_websocket_connection_set_property (
		GObject      *object,
		guint         property_id,
		const GValue *value,
		GParamSpec   *pspec)
{
	GWebSocketConnectionPrivate *
	priv = g_websocket_connection_get_instance_private(G_WEBSOCKET_CONNECTION(object));

	switch (property_id)
	{
	case PROP_CONNECTION:
		g_clear_object(&(priv->connection));
		priv->connection = g_value_get_object(value);
		break;
	case PROP_SITE:
		priv->site = g_value_get_uint(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
g_websocket_connection_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
	GWebSocketConnectionPrivate *
	priv = g_websocket_connection_get_instance_private(G_WEBSOCKET_CONNECTION(object));

	switch (property_id)
	{
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	case PROP_SITE:
		g_value_set_uint (value, priv->site);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

void
g_websocket_connection_close(GWebSocketConnection * self)
{
	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(self);
	if(priv->ping_timeout > 0)
	{
		g_source_remove(priv->ping_timeout);
		priv->ping_timeout = 0;
	}
	if(priv->connection)
	{
		g_cancellable_cancel(priv->cancellable);
		if(priv->recv_thread)
			g_thread_join(priv->recv_thread);
		if(!g_io_stream_is_closed(G_IO_STREAM(priv->connection)))
		{
			GOutputStream
			* output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
			g_websocket_connection_write_frame(output,TRUE,G_WEBSOCKET_CODEOP_CLOSE,priv->incoming ? 0x0:G_WEBSOCKET_MASK,(guint8*)"By!",3,NULL,NULL);
			g_io_stream_close(G_IO_STREAM(priv->connection),NULL,NULL);
			g_signal_emit (self, g_websocket_connection_signals[SIGNAL_CLOSED],0,NULL);
		}
		g_clear_object(&(priv->connection));
	}
}


void
g_websocket_connection_dispose(GObject * object)
{
	g_websocket_connection_close(G_WEBSOCKET_CONNECTION(object));
}

static void
g_websocket_connection_class_init(GWebSocketConnectionClass * klass)
{
	G_OBJECT_CLASS(klass)->constructed = g_websocket_connection_constructed;
	g_websocket_connection_properties[PROP_CONNECTION] = g_param_spec_object(
		"connection",
		"Connection",
		"WebSocket Connection",
		G_TYPE_SOCKET_CONNECTION,
		G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE
	);

	g_websocket_connection_properties[PROP_SITE] = g_param_spec_uint(
		"site",
		"Site",
		"The site of creation for connection ",
		FALSE,
		G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE
	);

	G_OBJECT_CLASS(klass)->get_property = g_websocket_connection_get_property;
	G_OBJECT_CLASS(klass)->set_property = g_websocket_connection_set_property;
	G_OBJECT_CLASS(klass)->dispose = g_websocket_connection_dispose;

	g_object_class_install_properties(
			G_OBJECT_CLASS(klass),
			N_PROPERTIES,
			g_websocket_connection_properties);
	const GType
	message_params[1] = {G_TYPE_POINTER};

	g_websocket_connection_signals[SIGNAL_MESSAGE] =
			g_signal_newv ("message",
	                 G_TYPE_FROM_CLASS (klass),
	                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
	                 NULL /* closure */,
	                 NULL /* accumulator */,
	                 NULL /* accumulator data */,
	                 NULL /* C marshaller */,
	                 G_TYPE_NONE /* return_type */,
	                 1     /* n_params */,
	                 (GType*)message_params  /* param_types */);

    g_websocket_connection_signals[SIGNAL_CONNECTED] =
				g_signal_newv ("connected",
		                 G_TYPE_FROM_CLASS (klass),
		                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		                 NULL /* closure */,
		                 NULL /* accumulator */,
		                 NULL /* accumulator data */,
		                 NULL /* C marshaller */,
		                 G_TYPE_NONE /* return_type */,
		                 0     /* n_params */,
		                 NULL  /* param_types */);

	g_websocket_connection_signals[SIGNAL_CLOSED] =
				g_signal_newv ("closed",
		                 G_TYPE_FROM_CLASS (klass),
		                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		                 NULL /* closure */,
		                 NULL /* accumulator */,
		                 NULL /* accumulator data */,
		                 NULL /* C marshaller */,
		                 G_TYPE_NONE /* return_type */,
		                 0     /* n_params */,
		                 NULL  /* param_types */);
}

/*GWebSocketConnection *	g_websocket_connection_new(
						const gchar * url,
						GCancellable * cancellable,
						GError ** error
						)
{
		if(g_str_has_prefix(url,"ws://") || g_str_has_prefix(url,"WS://") || g_str_has_prefix(url,"Ws://") || g_str_has_prefix(url,"wS://"))
			url += 5;
		gchar
		*hostname = NULL,
		*query = "/";
		gchar **
		url_part = g_strsplit(url,"/",2);
		gint
		url_part_count = g_strv_length(url_part);

		GSocketClient
		* client = g_socket_client_new();
		if(url_part_count > 0)
			hostname = url_part[0];

		if(url_part_count == 2)
			query = url_part[1];

		GSocketConnection
		* connection = NULL;
		GWebSocketConnection
		*ws = NULL;
		if(hostname)
		{
			if((connection = g_socket_client_connect_to_host(client,hostname,8081,cancellable,error)))
			{
				ws = G_WEBSOCKET_CONNECTION(
						g_object_new(G_WEBSOCKET_TYPE_CONNECTION,
								"connection",connection,
								"incoming",FALSE,
								"query",query,
								"hostname",hostname,
								NULL
								)
				);
			}
		}
		g_clear_object(&client);
		g_strfreev(url_part);
		if(g_io_stream_is_closed(G_IO_STREAM(connection)))
		{
			g_object_unref(ws);
			ws = NULL;
		}
		return ws;
}*/



GWebSocketConnection *
g_websocket_connection_factory_create_connection(
        GWebSocketSite site
		GSocketConnection * connection
		)
{
    g_return_if_val_fail(connection != NULL,NULL);
    g_return_val_if_fail(g_socket_connection_is_connected(connection) != TRUE,NULL);

	GWebSocketConnection
	* ws = G_WEBSOCKET_CONNECTION(
			g_object_new(G_WEBSOCKET_TYPE_CONNECTION,
					"connection",connection,
					"site",site,
					NULL)
	);
	return ws;
}

/*gboolean
g_websocket_connection_send_string(
						GWebSocketConnection * object,
						const gchar * str,
						gsize length,
						GCancellable * cancellable,
						GError ** error)
{
	if((gint64)length < 0)
		length = strlen(str);
	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(object);
	if(priv->connection)
	{
		GOutputStream
		* output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
		return g_websocket_connection_write_frame(output,TRUE,G_WEBSOCKET_CODEOP_TEXT,priv->incoming ? 0x0:G_WEBSOCKET_MASK,(guint8*) str,length,cancellable,error);
	}
	else
		return FALSE;
}

gboolean
g_websocket_connection_send_data(
						GWebSocketConnection * object,
						gpointer data,
						gsize length,
						GCancellable * cancellable,
						GError ** error)
{
	GWebSocketConnectionPrivate
	* priv = g_websocket_connection_get_instance_private(object);
	GOutputStream
	* output = g_io_stream_get_output_stream(G_IO_STREAM(priv->connection));
	return g_websocket_connection_write_frame(output,TRUE,G_WEBSOCKET_CODEOP_BINARY,priv->incoming ? 0x0:G_WEBSOCKET_MASK, data,length,cancellable,error);
}*/

GWebSocketMessage *
g_websocket_message_new(
                        GWebSocketMessageType type,
                        gpointer data,
                        gsize length)
{
    GWebSocketMessage * message = g_new0(GWebSocketMessage,1);
    message->data = g_memdup(data,length);
    message->length = length;
    message->type = type;
    return message;
}

GWebSocketMessage *
g_websocket_message_text_new(
                        const gchar * text,gssize length)
{
    GWebSocketMessage * message = g_new0(GWebSocketMessage,1);
    if(length < 0)
        length = g_utf8_strlen(text,G_MAXINT16);
    message->data = g_strndup(text,length);
    message->length = length;
    message->type = G_WEBSOCKET_MESSAGE_TEXT;
    return message;
}

GWebSocketMessage *     g_websocket_message_binary_new(
                        const guint8 * data,gsize length)
{
    GWebSocketMessage * message = g_new0(GWebSocketMessage,1);
    message->data = g_memdup(data,length);
    message->length = length;
    message->type = G_WEBSOCKET_MESSAGE_BINARY;
    return message;
}

const gchar *           g_websocket_message_get_text(
                        GWebSocketMessage * message)
{
    g_return_val_if_fail(message != NULL,NULL);
    g_return_val_if_fail(message->type == G_WEBSOCKET_MESSAGE_TEXT,NULL);
    return (const gchar*)message->data;
}

const guint8 *          g_websocket_message_get_data(
                        GWebSocketMessage * message)
{
    g_return_val_if_fail(message != NULL,NULL);
    g_return_val_if_fail(message->type == G_WEBSOCKET_MESSAGE_BINARY,NULL);
    return (const guint8*)message->data;
}

gsize
g_websocket_message_get_length(
                        GWebSocketMessage * message)
{
    g_return_val_if_fail(message != NULL,0);
    return (const gchar*)message->length;
}

void
g_websocket_message_free(
                        GWebSocketMessage * message)
{
    g_return_if_fail(message != NULL);
    g_free(message->data);
    g_free(message);
}
