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
	gdouble			version;
};

G_DEFINE_TYPE_WITH_PRIVATE(HttpResponse,http_response,HTTP_TYPE_PACKAGE)

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
		static gchar
		version[10];
		static gint
		code = HTTP_RESPONSE_INVALID;
		sscanf(string_response,"%s %d",version,&code);
		int iresponse;
		for(iresponse = 0;iresponse < HTTP_RESPONSE_INVALID;iresponse++)
		{
			if(_http_response_codes[iresponse] == code)
			{
				priv->code = (HttpResponseCode)iresponse;
				break;
			}
		}
		gint version_first = 1,version_last = 0;
		sscanf(version + 5,"%d.%d",&version_first,&version_last);
		priv->version =	((gdouble)version_first) + (((gdouble)version_last) / (log(version_last) * 10));
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
	if(g_output_stream_printf(stream,&count,cancellable,error,"HTTP/%g.%g %d %s\r\n",version_first,version_last * 10,_http_response_codes[priv->code],_http_response_strings[priv->code]))
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
