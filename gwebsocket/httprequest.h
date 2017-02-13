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

#ifndef HTTP_REQUEST_
#define HTTP_REQUEST_

#include "httppackage.h"

#define HTTP_TYPE_REQUEST	(http_request_get_type())

G_DECLARE_DERIVABLE_TYPE(HttpRequest,http_request,HTTP,REQUEST,HttpPackage)

typedef enum _HttpRequestMethod HttpRequestMethod;

enum _HttpRequestMethod
{
	HTTP_REQUEST_METHOD_GET = 0,
	HTTP_REQUEST_METHOD_POST = 1,
	HTTP_REQUEST_METHOD_HEAD = 2,
	HTTP_REQUEST_METHOD_PUT = 3,
	HTTP_REQUEST_METHOD_DELETE = 4,
	HTTP_REQUEST_METHOD_TRACE = 5,
	HTTP_REQUEST_METHOD_OPTIONS = 6,
	HTTP_REQUEST_METHOD_CONNECT = 7,
	HTTP_REQUEST_METHOD_PATCH = 8,
	HTTP_REQUEST_METHOD_INVALID = 9
};

struct _HttpRequestClass
{
	/* parent class */
	HttpPackageClass parent_class;
	/* methods */
	gpointer padding[12];
};

G_BEGIN_DECLS

HttpRequest *		http_request_new(HttpRequestMethod method,const gchar * query,gdouble version);

HttpRequestMethod	http_request_get_method(HttpRequest * request);
void			http_request_set_method(HttpRequest * request,HttpRequestMethod method);

const gchar *		http_request_get_query(HttpRequest * request);
void			http_request_set_query(HttpRequest * request,const gchar * query);

gdouble			http_request_get_version(HttpRequest * request);
void			http_request_set_version(HttpRequest * request,gdouble version);

G_END_DECLS

#endif
