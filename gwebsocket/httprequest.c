#include <stdio.h>
#include "httprequest.h"

const gchar * _http_string_method[] = {"GET","POST","HEAD","PUT","DELETE","TRACE","OPTIONS","CONNECT","PATCH",NULL};

typedef struct _HttpRequestPrivate HttpRequestPrivate;

struct _HttpRequestPrivate
{
	HttpRequestMethod	method;
	gchar *			query;
	gdouble			version;
};

G_DEFINE_TYPE_WITH_PRIVATE(HttpRequest,http_request,HTTP_TYPE_PACKAGE)

gboolean	
_http_request_read_from_stream_real(HttpPackage * package,
				GDataInputStream * data_stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(HTTP_REQUEST(package));
	gboolean
	done = TRUE;
	gsize
	count = 0,total_count = 0;
	gchar
	* string_query = g_data_input_stream_read_line(data_stream,&total_count,cancellable,error);
	if(string_query)
	{
		static gchar
		method[10],
		version[10];
		gchar
		* query = g_new0(gchar,total_count);
		sscanf(string_query,"%s %s %s",method,query,version);
		g_clear_pointer(&(priv->query),g_free);
		priv->method = HTTP_REQUEST_METHOD_INVALID;
		int imethod;
		for(imethod = 0;imethod < HTTP_REQUEST_METHOD_INVALID;imethod++)
		{
			if(g_ascii_strcasecmp(_http_string_method[imethod],method) == 0)
			{
				priv->method = (HttpRequestMethod)imethod;
				break;
			}
		}
		priv->query = g_strdup(query);
		priv->version = g_strtod(version + 5,NULL);
		done = HTTP_PACKAGE_CLASS(http_request_parent_class)->read_from_stream(package,data_stream,&count,cancellable,error);
		total_count += count;
		if(length)
			*length = total_count;
		g_free(string_query);
		g_free(query);
	}
	else
		done = FALSE;
	return done;
}

gboolean	
_http_request_write_to_stream_real(HttpPackage * package,
				GOutputStream * stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
	g_return_val_if_fail(HTTP_IS_REQUEST(package),FALSE);
	HttpRequestPrivate
	* priv = http_request_get_instance_private(HTTP_REQUEST(package));
	g_return_val_if_fail(priv->method != HTTP_REQUEST_METHOD_INVALID,FALSE);
	g_return_val_if_fail(priv->query != NULL,FALSE);
	g_return_val_if_fail(priv->version > 0,FALSE);

	gsize
	count = 0,total_count = 0;
	gboolean
	done = TRUE;
	if(g_output_stream_printf(stream,&count,cancellable,error,"%s %s HTTP/%0.1f\r\n",_http_string_method[priv->method],priv->query,priv->version))
	{
		done = HTTP_PACKAGE_CLASS(http_request_parent_class)->write_to_stream(package,stream,&total_count,cancellable,error);
		if(length)
			*length = total_count + count;		
	}
	else
		done = FALSE;	
	return done;
}

void 
_http_request_finalize(GObject *  object)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(HTTP_REQUEST(object));
	g_clear_pointer(&(priv->query),g_free);
	G_OBJECT_CLASS(http_request_parent_class)->finalize(object);
}

static void
http_request_init(HttpRequest * self)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(self);
	priv->method = HTTP_REQUEST_METHOD_INVALID;
	priv->query = NULL;
	priv->version = 1.1;

}

static void
http_request_class_init(HttpRequestClass * klass)
{
	HTTP_PACKAGE_CLASS(klass)->read_from_stream = _http_request_read_from_stream_real;
	HTTP_PACKAGE_CLASS(klass)->write_to_stream = _http_request_write_to_stream_real;
	G_OBJECT_CLASS(klass)->finalize = _http_request_finalize;
}

HttpRequest *		
http_request_new(HttpRequestMethod method,const gchar * query,gdouble version)
{
	HttpRequest
	*  self = HTTP_REQUEST(g_object_new(HTTP_TYPE_REQUEST,NULL));
	HttpRequestPrivate
	* priv = http_request_get_instance_private(self);
	priv->method = method;
	priv->query = g_strdup(query);
	priv->version = version;
	return self;
}

HttpRequestMethod	
http_request_get_method(HttpRequest * request)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(request);
	return priv->method;
}

void			
http_request_set_method(HttpRequest * request,HttpRequestMethod method)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(request);
	priv->method = method;
}

const gchar *		
http_request_get_query(HttpRequest * request)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(request);
	return priv->query;
}

void			
http_request_set_query(HttpRequest * request,const gchar * query)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(request);
	g_clear_pointer(&(priv->query),g_free);
	priv->query = g_strdup(query);
}

gdouble			
http_request_get_version(HttpRequest * request)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(request);
	return priv->version;
}

void			
http_request_set_version(HttpRequest * request,gdouble version)
{
	HttpRequestPrivate
	* priv = http_request_get_instance_private(request);
	priv->version = version;
}
