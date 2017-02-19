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

#include <stdio.h>
#include <math.h>
#include "httpresponse.h"

static const int _http_response_codes[] = {200,400,404,500,101,403,0};
static const gchar * _http_response_strings[] = {"OK","Bad Request","Not Found","Internal Error","Switching Protocols","Forbidden",NULL};

typedef struct _HttpResponsePrivate HttpResponsePrivate;

struct _HttpResponsePrivate
{
  HttpResponseCode	code;
  gdouble		version;
};

G_DEFINE_TYPE_WITH_PRIVATE(HttpResponse,http_response,HTTP_TYPE_PACKAGE)

static const gchar *
_http_response_get_code_description(HttpResponseCode code)
{
  switch(code)
  {
    case HTTP_RESPONSE_CONTINUE:
      return "Continue";
    case HTTP_RESPONSE_SWITCHING_PROTOCOLS:
      return "Switching Protocols";
    case HTTP_RESPONSE_PROCESSING:
      return "Processing";
    case HTTP_RESPONSE_OK:
      return "OK";
    case HTTP_RESPONSE_CREATED:
      return "Created";
    case HTTP_RESPONSE_ACCEPTED:
      return "Accepted";
    case HTTP_RESPONSE_NON_AUTHORITATIVE_INFORMATION:
      return "Non-Authoritative Information";
    case HTTP_RESPONSE_NO_CONTENT:
      return "No Content";
    case HTTP_RESPONSE_RESET_CONTENT:
      return "Reset Content";
    case HTTP_RESPONSE_PARTIAL_CONTENT:
      return "Partial Content";
    case HTTP_RESPONSE_MULTI_STATUS:
      return "Multi-Status";
    case HTTP_RESPONSE_ALREADY_REPORTED:
      return "Already Reported";
    case HTTP_RESPONSE_IM_USED:
      return "IM Used";
    case HTTP_RESPONSE_MULTIPLE_CHOICES:
      return "Multiple Choices";
    case HTTP_RESPONSE_MOVED_PERMANENTLY:
      return "Moved Permanently";
    case HTTP_RESPONSE_FOUND:
      return "Found";
    case HTTP_RESPONSE_SEE_OTHER:
      return "See Other";
    case HTTP_RESPONSE_NOT_MODIFIED:
      return "Not Modified";
    case HTTP_RESPONSE_USE_PROXY:
      return "Use Proxy";
    case HTTP_RESPONSE_SWITCH_PROXY:
      return "Switch Proxy";
    case HTTP_RESPONSE_TEMPORARY_REDIRECT:
      return "Temporary Redirect";
    case HTTP_RESPONSE_PERMANENT_REDIRECT:
      return "Permanent Redirect";
    case HTTP_RESPONSE_BAD_REQUEST:
      return "Bad Request";
    case HTTP_RESPONSE_UNAUTHORIZED:
      return "Unauthorized";
    case HTTP_RESPONSE_PAYMENT_REQUIRED:
      return "Payment Required";
    case HTTP_RESPONSE_FORBIDDEN:
      return "Forbidden";
    case HTTP_RESPONSE_NOT_FOUND:
      return "Not Found";
    case HTTP_RESPONSE_METHOD_NOT_ALLOWED:
      return "Method Not Allowed";
    case HTTP_RESPONSE_NOT_ACCEPTABLE:
      return "Not Acceptable";
    case HTTP_RESPONSE_PROXY_AUTHENTICATION_REQUIRED:
      return "Proxy Authentication Required";
    case HTTP_RESPONSE_REQUEST_TIME_OUT:
      return "Request Time-out";
    case HTTP_RESPONSE_CONFLICT:
      return "Conflict";
    case HTTP_RESPONSE_GONE:
      return "Gone";
    case HTTP_RESPONSE_LENGTH_REQUIRED:
      return "Length Required";
    case HTTP_RESPONSE_PRECONDITION_FAILED:
      return "Precondition Failed";
    case HTTP_RESPONSE_PLAYLOAD_TOO_LARGE:
      return "Payload Too Large";
    case HTTP_RESPONSE_URI_TOO_LONG:
      return "URI Too Long";
    case HTTP_RESPONSE_UNSUPORTED_MEDIA_TYPE:
      return "Unsupported Media Type";
    case HTTP_RESPONSE_RANGE_NOT_SATISFIABLE:
      return "Range Not Satisfiable";
    case HTTP_RESPONSE_EXPECTATION_FAILED:
      return "Expectation Failed";
    case HTTP_RESPONSE_IM_A_TEAPOT:
      return "I'm a teapot";
    case HTTP_RESPONSE_MISDIRECTED_REQUEST:
      return "Misdirected Request";
    case HTTP_RESPONSE_UNPROCESSABLE_ENTITY:
      return "Unprocessable Entity";
    case HTTP_RESPONSE_LOCKED:
      return "Locked";
    case HTTP_RESPONSE_FAILED_DEPENDENCY:
      return "Failed Dependency";
    case HTTP_RESPONSE_UPGRADE_REQUIRED:
      return "Upgrade Required";
    case HTTP_RESPONSE_PRECONDITION_REQUIRED:
      return "Precondition Required";
    case HTTP_RESPONSE_TOO_MANY_REQUESTS:
      return "Too Many Requests";
    case HTTP_RESPONSE_REQUEST_HEADER_FIELDS_TO_LARGE:
      return "Request Header Fields Too Large";
    case HTTP_RESPONSE_UNAVAILABLE_FOR_LEGAL_REASON:
      return "Unavailable For Legal Reasons";
    case HTTP_RESPONSE_INTERNAL_SERVER_ERROR:
      return "Internal Server Error";
    case HTTP_RESPONSE_NOT_IMPLEMENTED:
      return "Not Implemented";
    case HTTP_RESPONSE_BAD_GATEWAY:
      return "Bad Gateway";
    case HTTP_RESPONSE_SERVICE_UNAVAILABLE:
      return "Service Unavailable";
    case HTTP_RESPONSE_GATEWAY_TIME_OUT:
      return "Gateway Time-out";
    case HTTP_RESPONSE_HTTP_VERSION_NOT_SUPPORTED:
      return "HTTP Version Not Supported";
    case HTTP_RESPONSE_VARIANT_ALSO_NEGOTIATIES:
      return "Variant Also Negotiates";
    case HTTP_RESPONSE_INSUFFICIENT_STORAGE:
      return "Insufficient Storage";
    case HTTP_RESPONSE_LOOP_DETECTED:
      return "Loop Detected";
    case HTTP_RESPONSE_NOT_EXTENDED:
      return "Not Extended";
    case HTTP_RESPONSE_NETWORK_AUTHENTICATION_REQUIRED:
      return "Network Authentication Required";
    default:
      return "Invalid";
  }
}

gboolean	
_http_response_read_from_stream_real(HttpPackage * package,
				GDataInputStream * data_stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
  HttpResponsePrivate
  * priv = http_response_get_instance_private(HTTP_RESPONSE(package));
  gboolean
  done = TRUE;
  gsize
  count = 0,total_count = 0;
  gchar
  * string_response = g_data_input_stream_read_line(data_stream,&total_count,cancellable,error);
  if(string_response)
    {
      static gchar version[10];
      static gint  code = HTTP_RESPONSE_INVALID;
      gint version_first = 1,version_last = 0;
      sscanf(string_response,"%s %d",version,&code);
      priv->code = code;
      sscanf(version + 5,"%d.%d",&version_first,&version_last);
      priv->version =	((gdouble)version_first) + (((gdouble)version_last)  / 10);
      done = HTTP_PACKAGE_CLASS(http_response_parent_class)->read_from_stream(package,data_stream,&count,cancellable,error);
      total_count += count;
      if(length)
	*length = total_count;
      g_free(string_response);
    }
  else
    done = FALSE;
  return done;
}

gboolean	
_http_response_write_to_stream_real(HttpPackage * package,
				GOutputStream * stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
  g_return_val_if_fail(HTTP_IS_RESPONSE(package),FALSE);
  HttpResponsePrivate
  * priv = http_response_get_instance_private(HTTP_RESPONSE(package));
  g_return_val_if_fail(priv->code != HTTP_RESPONSE_INVALID,FALSE);
  g_return_val_if_fail(priv->version > 0,FALSE);
  gsize
  count = 0,total_count = 0;
  gboolean
  done = TRUE;
  gdouble version_first = 0;
  gdouble version_last = modf(priv->version,&version_first);
  if(g_output_stream_printf(stream,&count,cancellable,error,"HTTP/%g.%g %d %s\r\n",version_first,version_last * 10,priv->code,_http_response_get_code_description(priv->code)))
    {
      done = HTTP_PACKAGE_CLASS(http_response_parent_class)->write_to_stream(package,stream,&total_count,cancellable,error);
      if(length)
	*length = total_count + count;
    }
  else
    done = FALSE;
  return done;
}

void 
_http_response_finalize(GObject *  object)
{
	/*HttpResponsePrivate
	* priv = http_response_get_instance_private(HTTP_RESPONSE(object));*/
	G_OBJECT_CLASS(http_response_parent_class)->finalize(object);
}

static void
http_response_init(HttpResponse * self)
{
	HttpResponsePrivate
	* priv = http_response_get_instance_private(self);
	priv->code = HTTP_RESPONSE_INVALID;
	priv->version = 1.1;

}

static void
http_response_class_init(HttpResponseClass * klass)
{
	HTTP_PACKAGE_CLASS(klass)->read_from_stream = _http_response_read_from_stream_real;
	HTTP_PACKAGE_CLASS(klass)->write_to_stream = _http_response_write_to_stream_real;
	G_OBJECT_CLASS(klass)->finalize = _http_response_finalize;
}

HttpResponse *		
http_response_new(HttpResponseCode code,gdouble version)
{
	HttpResponse
	*  self = HTTP_RESPONSE(g_object_new(HTTP_TYPE_RESPONSE,NULL));
	HttpResponsePrivate
	* priv = http_response_get_instance_private(self);
	priv->code = code;
	priv->version = version;
	return self;
}

HttpResponseCode
http_response_get_code(HttpResponse * response)
{
	HttpResponsePrivate
	* priv = http_response_get_instance_private(response);
	return priv->code;
}

void			
http_response_set_code(HttpResponse * response,HttpResponseCode code)
{
	HttpResponsePrivate
	* priv = http_response_get_instance_private(response);
	priv->code = code;
}

gdouble			
http_response_get_version(HttpResponse * response)
{
	HttpResponsePrivate
	* priv = http_response_get_instance_private(response);
	return priv->version;
}

void			
http_response_set_version(HttpResponse * response,gdouble version)
{
	HttpResponsePrivate
	* priv = http_response_get_instance_private(response);
	priv->version = version;
}
